# Astro-Weather
ESP8266 sketch for check weather conditions for a home astronomical observatory.  Gives information about cloud coverage, sky quality (mpsas), ambient temperature, humidity and pression, wind speed and rain.


Sensor uses a BME280 for Temperature and humitity measurement, a MLX90614 to measure sky temperature ad calculate cloud coverage, a TSL2591 to get luminosity and calculate sky quality (SQM measured in mpsas) and an anemomether and a rain sensor

All data is transmitted over MQTT.

The MLX90614 sensor reads sky temperature (object temperature) and applies a correction factor factor based on a formula from indi-duino Meteo: [https://github.com/LabAixBidouille/Indi/blob/master/3rdparty/indi-duino/devices/Firmwares/indiduinoMETEO/indiduinoMETEO.ino](https://github.com/LabAixBidouille/Indi/blob/master/3rdparty/indi-duino/devices/Firmwares/indiduinoMETEO/indiduinoMETEO.ino) , also calculates a cloud coverage percentage based on a formula from same program.

Sky quality measure is based on SQM_example by Gabe Shaughnessy on library [https://github.com/gshau/SQM_TSL2591](https://github.com/gshau/SQM_TSL2591)

The program sends an MQTT message for each sensor reading in JSON format:
**{"time": unix_time, "sensor_name": {"sensor_data1": value, "sensor_data2": value, ...}}**

Also send a message for astronomy safe conditions with sensor name "safe" and value 1 or 0, and sends a message in Boltwood II format with the weather data and flags.

> [!NOTE]
> On first run, the program creates an access point with name "ESP-SKYWEATHER" creating a web server accessible on http://192.168.4.1 which allow to configure SSID/password, node name, and MQTT configuration. Once configured these parameters, the program connects to the configured WiFi and starts data capturing and submission. This AP can also been activated after a reset if the SETUP_PIN (GPIO 0) is down during a lapse of 5 seconds after reset.

on normal operation, the program presents 4 web pages on the device IP address:
-	**http://ip_address/setup** - the setup page mentioned above
-	**http://ip_address/parameters** - which allows to configure several parameters of the program
-	**http://ip_address/data** - which returns all sensors data in JSON format
-	**http://ip_address/boltwood** - returning data in Boltwood II format

## Configurable parameters are:
- **Pooling interval:** num of seconds elapsed between sensors data reading and submission
- **Altitude:** altitude in meters. it is used for calculate sea level pressure
- **Temp. Adj.:** temperature offset.
- **Num samples WindSpeed:** number of samples to take on wind speed calculation periods of 5 secs.
The program calculates the maximum wind speed during the period and average of wind speed and submits the data.


### Sky temperature parameters
- **Clear sky temp.**, defines the temperature of the limit of the sky temperature below what is considered clear sky (default -5ºC), above this temperature is considered cloudy, and above 0 ºC is considered overcast. Cloud index is calculated as a linear percentage of actual sky temp between 0ºC and "Clear sky temp".
- **K1 to K7** parametes, see [apendixes 6 and 7 of CloudWatcher](https://lunaticoastro.com/aagcw/enhelp/)


### Safe status parameters
The "/parameters" section defines the thresholds for the safe / unsafe condition:
- **Humidity**	safe if humidity is below this value, unsafe otherwise.
- **Wind**	safe if max wind speed (gust) is below this value, unsafe otherwise.
- **Rain**	safe if the analogic value of rain sensor (rainA) is above this value, unsafe otherwise.
- **Cloud index**	safe if cloudI is below this value, unsafe otherwise.
- **SQM**		safe if sky quality (mpsas) is above this value, unsafe otherwise.
- **Light**	safe if luminosity (lux) is below this value, unsafe otherwise.


## Conditional compilation:
There are three defines, for including (or exclude if is commented), some sensors:
```
    #define W_RAIN      includes code for rain sensor with an YL-80 or FC-37 sensor
    #define W_WIND      includes code for wind sensor
    #define W_BOLTWOOD  includes code for data submission in Boltwood II format
    #define W_MQTT      includes code for data submission over MQTT
```
Notice that the corresponding .hpp file is always included, but not used.

> [!NOTE]
> If you comment or delete the #define W_MQTT, no MQTT messages are sent, although it still apperas on **/setup** page

**SEND_BY_MODULE** define: is defined, send an MQTT separate message for each sensor. If not defined, send only one JSON message including all sensors on it. Despite it is defined or not, the rain and wind info is always submitted when captured (30 secs for rain, Num_samples_windspeed * 5 secs for wind)

> [!IMPORTANT]
> check all defines regarding GPIOs for leds, wing gauge and I2C bus.


## Leds behavior:
On start RED and GREEN leds are on for 5 secs. If SETUP_PIN (GPIO 0) is down at end of this period, both leds are off and the program enters in AP mode for configuration, otherwise goes to normal operation mode (STA mode).

Initial setup (AP mode), RED led blinks quickly (200 ms).

During WiFi conecction (setup STA), RED led bilnks slowly (500 ms), after the program is connected to WiFi, RED led is off and GREEN led is on during 3 secs.

On normal operation, both leds are off.

## Bridge to ASCOM Boltwood Weather Monitor 
The directory [ASCOM bridge](ASCOM%20bridge) contains a .bat file which access the /boltwood web page of the sketch server and saves the response on a local file in the windows machine. This file may be used by the [ASCOM Boltwood Weather Monitor](https://github.com/ascomghostdrivers/legacydrivers/tree/main/ObservingConditionsDrivers) legacy driver<br>

**Usage:** 
```
boltwood.bat server_url [timeout  filename]
        default filename: %homedrive%%homepath%\downloads\boltwood.txt
        defailt time: 30 secs.
```
