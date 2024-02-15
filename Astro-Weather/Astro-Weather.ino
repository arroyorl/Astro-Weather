/*
 *  This sketch runs an IoT for check weather conditions for
 *  a home astronomical observatory.
 *  Gives information about cloud coverage, sky quality (mpsas),
 *  ambient temperature, humidity and pression, wind speed and rain
 *  During the setup the device will act as AP mode so that you can set the
 *  Wifi connection and MQTT parameters, then it will try to connect to the WiFi. 
 *  - While serving the webserver it will also post the data to MQTT broker.
 */
///////////////////////////////////////////////////////////////
//           History                                         //
///////////////////////////////////////////////////////////////
//  1.0 first version  (= AirQuality v6.8)
//      code based on measure.ino from Dirk O. Kaar <dok@dok-net.net>
//
//  1.1 don't change spaces by &nbsp; in handlerBoltwood() if browser is 'curl'
//
///////////////////////////////////////////////////////////////
 

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>  

#include <time.h> 

#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>   // v 1.9.12

#define FVERSION  "v1.0"
#define PNAME "ESP8266 Astro Weather"

///////////////////////////////////////////////////
// Comment for final release
#define RDEBUG
///////////////////////////////////////////////////

// modules included
#define W_MQTT        // MQTT module
#define W_RAIN        // rain sensor
#define W_WIND        // anemomether
#define W_BOLTWOOD    // Boltwood II format data submission


#define SEND_BY_MODULE  // send a MQTT message for each module
                        // if not defined will send a message integrating all data
                        // except for rain and wind for which separate messages are sent

#include "Rdebug.h"
#include "settings.h"

#define GPIO0 0
#define GPIO1 1
#define GPIO2 2
#define GPIO3 3
#define GPIO4 4
#define GPIO5 5
#define GPIO6 6
#define GPIO7 7
#define GPIO8 8
#define GPIO9 9
#define GPIO10 10
#define GPIO11 11
#define GPIO12 12
#define GPIO13 13
#define GPIO14 14
#define GPIO15 15
#define GPIO16 16

#define LEDON LOW
#define LEDOFF HIGH
#define SETUP_PIN GPIO0
#define WAKEUP_PIN GPIO16

// see board names in C:\Users\ricardo\AppData\Local\Arduino15\packages\esp8266\hardware\esp8266\3.1.2\boards.txt
// Define Red and green LEDs
#ifdef ARDUINO_ESP8266_NODEMCU_ESP12E
# define RED_LED    GPIO2   // D4
# define GREEN_LED  GPIO15  // D8
#endif
#ifdef ARDUINO_ESP8266_GENERIC
# define RED_LED    GPIO12 //    antes GPIO9
# define GREEN_LED  GPIO10
#endif

//Struct to store setting in EEPROM
Settings    settings;

//Global Variables
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
String      data;
String      httpUpdateResponse;
const char* ap_ssid = "ESP-SKYWEATHER";
const char* ap_password = ""; 
int         ap_setup_done = 0;
rst_info*   myResetInfo;
String      ipaddress;
unsigned long        lastTime; //The millis counter to see when a second rolls by

// global variables for time
time_t      now;                    // this are the seconds since Epoch (1970) - UTC
tm          tm;                     // the structure tm holds time information in a more convenient way
char        hhmm[6];                // variable to store time in format HH:MM

// MQTT functions
#ifdef W_MQTT
#   include  "mqtt.h"
#endif

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//  Include functions for sensors
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#include "bme280.hpp"           // temperature, humidity, pression
#include "gy906.hpp"            // sky temperature, clouds
#include "rain.hpp"             // rain
#include "wind.hpp"             // wind speed
#include "sqm-tsl2591.hpp"      // SQM (TSL2591)
#include "safe.hpp"             // check safe conditions
#include "boltwood.hpp"         // prepare data in Boltwood II format

// function prototypes
String  prepareJsonData();
void    getSensorData();
int     setupSTA();

ESP8266WebServer server(80);
// root web page -> usage
#include "rootPage.hpp"
// Web page for setup configuration
#include "setupPage.hpp"
// web page for parameters configuration
#include "parametersPage.hpp"
// Web server for respond to rawdata web requestes and send data to wonderground
#include "webserver.hpp"

/////////////////////////////////////////
// blink LED
/////////////////////////////////////////
void blinkLed(unsigned long time){
  digitalWrite(RED_LED, LEDON);
  delay(time);
  digitalWrite(RED_LED, LEDOFF);
  delay(time);
}

/////////////////////////////////////////
// setup WiFi configuration
/////////////////////////////////////////
void firstSetup(){
  DebugLn("First Setup ->");
  DebugLn(String(settings.data.magic));
  DebugLn("Setting up AP");
  WiFi.mode(WIFI_AP);             //Only Access point
  WiFi.softAP(ap_ssid, NULL, 8);  //Start HOTspot removing password will disable security
  delay(50);
  server.on("/", handleSetup);
  server.on("/setupform", handleSetupForm);
  DebugLn("Server Begin");
  server.begin(); 
  delay(100);
  do {
    server.handleClient(); 
    blinkLed(200);
    Debug(".");
  }
  while (!ap_setup_done);
    
  settings.data.magic[0] = MAGIC[0];
  settings.data.magic[1] = MAGIC[1];
  settings.data.magic[2] = MAGIC[2];
  settings.data.magic[3] = MAGIC[3];
  server.stop();
  WiFi.disconnect();
  settings.Save();
  DebugLn("First Setup Done");
}


/////////////////////////////////////////
// connect to WiFi
/////////////////////////////////////////
int setupSTA()
{
  int timeOut=0;

  for (int retry=0; retry<=3; retry++) {
    WiFi.disconnect();
    WiFi.hostname("ESP-" + String(settings.data.name)) ;
    WiFi.mode(WIFI_STA);
    DebugLn("Connecting to "+String(settings.data.ssid));
    DebugLn("Connecting to "+String(settings.data.psk));
    
    if (String(settings.data.psk).length()) {
      WiFi.begin(String(settings.data.ssid), String(settings.data.psk));
    } else {
      WiFi.begin(String(settings.data.ssid));
    }
    timeOut=0;
    retry=0;    
     while (WiFi.status() != WL_CONNECTED) {
      if (timeOut < 10){          // if not timeout, keep trying
        delay(100);
        Debug(String(WiFi.status()));
        blinkLed(500);
        timeOut ++;
      } else{
        timeOut = 0;
        DebugLn("-Wifi connection timeout");
        blinkLed(500);
        retry++;
        if (retry == 3) { 
          return 0;
        }
      }
    }
    DebugLn(" Connected");

    // Print the IP address
    ipaddress = WiFi.localIP().toString();
    DebugLn(ipaddress); 
    DebugLn(WiFi.hostname().c_str());
    break; 
    }
    return 1;
}

/////////////////////////////////////////
// get data from sensors
/////////////////////////////////////////
void  getSensorData(){

  getBME280data();
#ifdef W_RAIN
  readRainSensor();
#endif
  get_gy906data();
  getSQMdata();

}

/////////////////////////////////////////
// prepare all sensor data in JSON format
/////////////////////////////////////////
String prepareJsonData() {

  ipaddress = WiFi.localIP().toString();

  time(&now);  // read the current time

  String res =  "{ \"time\": \"" + String(now) + "\"" + 
                ", \"ipaddress\": \"" + String(ipaddress) + "\"" + 
                ", \"version\": \"" + String(FVERSION) + "\"" + 
                ",\"sensors\":" +
                  "{ \"temperature\":" + String(temperature) + 
                  ", \"humidity\":" + String(humidity) + 
                  ", \"dewpoint\":" + String(dewpoint) +
                  ", \"pressure\":" + String(pressure) + 
                  ", \"sealevelpressure\":" + String(sealevelpressure) + 
                  ", \"rainD\":" + String(rainD) + 
                  ", \"rainA\":" + String(rainA) + 
                  ", \"windspeed\":" + String(windspeedNow) +
                  ", \"maxwindspeed\":" + String(maxwindspeedNow) +
                  ", \"skyT\":" + String(skyTemp) + 
                  ", \"ambientT\":" + String(ambientTemp) + 
                  ", \"cloudIndex\":" + String(cloudI) +    
                  ", \"clouds\":" + String(clouds) +    
                  ", \"sqmfull\":" + String(sqmFull) +
                  ", \"sqmIR\":" + String(sqmIr) +
                  ", \"sqmVis\":" + String(sqmVis) +
                  ", \"mpsas\":" + String(sqmMpsas) +
                  ", \"dmpsas\":" + String(sqmDmpsas) +
                  "}" +
                "}";

  return res;
}

/////////////////////////////////////////
// send sensors data
/////////////////////////////////////////
void sendData() {
  DebugLn("sendData");

  // get Temperature, humidity and Pressure
  getSensorData();
  DebugLn("Temp: "+String(temperature)+" ºC, Humid: "+String(humidity)+"%");

  if ( isnan(temperature) ) {
    // temperature value is nan, reset device
    DebugLn("Failed temp reading [nan], reseting device");
    delay(2000);
    ESP.restart();
  }


#ifdef W_MQTT
# ifdef SEND_BY_MODULE
  // send one MQTT message for each module data
      sendBME280data();
      sendGY906data();
      sendSQMdata();
      sendSafedata();
#   ifdef W_BOLTWOOD  
      sendBoltwood();
#   endif
# else
  mqtt_send("data", prepareJsonData(), false);
# endif
#endif
  delay(100);

}

/////////////////////////////////////////
// module setup
/////////////////////////////////////////
void setup() {
  DebugStart();
  DebugLn("\nStart ->");

  myResetInfo = ESP.getResetInfoPtr();
  DebugLn("myResetInfo->reason "+String(myResetInfo->reason)); // reason is uint32
                                                                 // 0 = power down
                                                                 // 6 = reset button
                                                                 // 5 = restart from deepsleep


  //************** load settings **********************************
  settings.Load();

  //************** setup LED and DHT pins, and switch OFF LEDs **************
  DebugLn("Setup ->");
  pinMode(RED_LED, FUNCTION_3);
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, FUNCTION_3);
  pinMode(GREEN_LED, OUTPUT);

  // ******** initiallize LEDs and wait for setup pin *****************
  DebugLn("-> Check if SETUP_PIN is low");
  digitalWrite(RED_LED, LEDON);
  digitalWrite(GREEN_LED, LEDON);
  // Wait up to 5s for SETUP_PIN to go low to enter AP/setup mode.
  pinMode(SETUP_PIN, INPUT);      //Configure setup pin as input
  digitalWrite(SETUP_PIN, HIGH);  //Enable internal pooling
  delay(5000);  
  DebugLn("Magic ->"+String(settings.data.magic));

  // if NO MAGIC Or SETUP PIN enter hard config...
  if (String(settings.data.magic) != MAGIC||!digitalRead(SETUP_PIN)){
    digitalWrite(GREEN_LED, LEDOFF);
    digitalWrite(RED_LED, LEDON);
    DebugLn("First Setup ->");
    firstSetup();
  }
  else {
    // initial setup is done
    ap_setup_done = 1;
  }

  // NO SETUP, switch off both LEDs
  digitalWrite(GREEN_LED, LEDOFF);
  digitalWrite(RED_LED, LEDOFF);

  //********* initialize sensors *****************
  BME280setup();
#ifdef W_RAIN
  setupRain();
#endif
#ifdef W_WIND
  setupWind();
#endif
  gy906setup();
  SQMsetup();

  // ********** set timezone (do it before WiFi setup) *******************
  configTime(settings.data.timezone, settings.data.ntpserver);

  // *********** setup STA mode and connect to WiFi ************
  if (setupSTA() == 0) { // SetupSTA mode
    DebugLn("Wifi no connected, wait 60 sec. and restart");
    delay(60*1000);
    ESP.restart();
  }

  // ********** initialize OTA *******************
  ArduinoOTA.begin();

#ifdef W_MQTT
  // ********* initialize MQTT ******************
  if (strlen(settings.data.mqttbroker) > 0 ) {
    // MQTT broker defined, initialize MQTT
    mqtt_init();
    delay(100);
  }
#endif
#ifdef  W_TS
  ThingSpeaksetup();
#endif

  //********** switch ON GREEN LED, initialize web servers and switch OFF LEDs
  digitalWrite(GREEN_LED, LEDON);
  digitalWrite(RED_LED, LEDOFF);
  DebugLn("-> Initiate WebServer");
  server.on("/",handleRoot);
  server.onNotFound(handleRoot);
  server.on("/parameters",handleParameters);
  server.on("/setup", handleSetup);
  server.on("/data",handleRowData);
  server.on("/setupform", handleSetupForm);
  server.on("/parametersform", handleParametersForm);
  server.collectHeaders("User-Agent","Content-Type");
#ifdef  W_BOLTWOOD
  server.on("/boltwood", handleBoltwood);
#endif
  delay(100);
  server.begin(); 
  delay(3000);
  digitalWrite(GREEN_LED, LEDOFF);
  digitalWrite(RED_LED, LEDOFF);  

  // ********* initialize counters
  lastTime = millis();

  // read current time'
  time(&now);                       // read the current time
  Debug("-> Time sync to ");
  DebugLn(ctime(&now));

  // send data (first interval)
  sendData();

  DebugLn("-> End Setup");
}

/////////////////////////////////////////
// main loop
/////////////////////////////////////////
void loop() {

  // handle http client while waiting
  server.handleClient();    
  
  // handle OTA
  ArduinoOTA.handle();

  // handle MQTT
  handleMQTT();

  // normal loop

  if ( (millis() - lastTime) > (settings.data.poolinterval * 1000) ) {
      // check if has been elapsed poolinterval seconds
      lastTime = millis();
      sendData();
  }

#ifdef  W_RAIN
  // send rain data every 30 seconds
  if( (millis() - lastSecondR) >= 30000)
  {
    lastSecondR = millis();
    sendRainData();
#ifdef  W_BOLTWOOD
    isSafe();
    if(safeNow != safePrev) {
      sendBoltwood;
    }
#endif
  }
#endif

#ifdef W_WIND
  // calculate wind speed every 5 seconds
  if(millis() - lastSecondW >= 5000)
  {
      lastSecondW = millis();

      //Get wind speed 
      windspeed = get_wind_speed(); //Wind speed in Km/h
      sumwindspeed += windspeed;
      maxwindspeed = max(windspeed, maxwindspeed);
      numsampleswind++;
      if (numsampleswind == settings.data.numsampleswind) {
        windspeed = (sumwindspeed / (float)numsampleswind);  
        windspeedNow = windspeed;
        maxwindspeedNow = maxwindspeed;
        sendWindData();
        sumwindspeed = 0.0;
        maxwindspeed = 0.0;
        numsampleswind = 0;
#ifdef  W_BOLTWOOD
        isSafe();
        if(safeNow != safePrev) {
          sendBoltwood;
        }
#endif
      }
  }
#endif

  // reset every 24 h 
  if (millis() > (24*60*60*1000ul)) {
    ESP.restart();
  }
}
