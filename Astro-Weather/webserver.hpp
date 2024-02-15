//--------------------------------------------
//  HTTP POST
//--------------------------------------------
void sendHTTP (String httpPost){
  WiFiClient client;
  HTTPClient http;
  DebugLn(httpPost);
  http.begin(client, httpPost);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int httpCode = http.POST("");
  DebugLn(httpCode);    
  http.end();
}

//--------------------------------------------
//  HTTP GET
//--------------------------------------------
void sendHTTPGet (String httpGet){
  WiFiClient client;
  HTTPClient http;
  DebugLn(httpGet);
  http.begin(client, httpGet);
  int httpCode = http.GET();
  DebugLn(httpCode);    
  http.end();
}

//--------------------------------------------
//  HTTPs GET
//--------------------------------------------
void sendHTTPsGet (String httpGet){
  HTTPClient https;

  std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);

  // fingerprint not needed
  client->setInsecure();
  // send data
  DebugLn("HTTPsGet: " + httpGet);
  https.begin(*client, httpGet);
  // http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int httpCode = https.GET();
  String payload = https.getString();

  DebugLn("HTTPsGet return code: " + String(httpCode));    // this return 200 when success
  DebugLn(payload);     // this will get the response
  https.end();

}

//--------------------------------------------
//  web server page returning raw data (json)
//--------------------------------------------
void handleRowData() {
  DebugLn("handleRowData");

  time(&now);                       // read the current time
  localtime_r(&now, &tm);           // update the structure tm with the current time

  httpUpdateResponse = "";
  getSensorData();
#ifdef  SEND_BY_MODULE
  String message =  "{ \"time\": \"" + String(now) + "\"" + 
                    ", \"ipaddress\": \"" + String(ipaddress) + "\"" + 
                    ", \"version\": \"" + String(FVERSION) + "\"" + 
                    ", " + bme280Json() + 
                    ", " + gy906Json() + 
                    ", " + SQMJson() +
# ifdef W_RAIN
                    ", " + rainJson() +
# endif
# ifdef W_WIND
                    ", " + windJson() +
# endif
                    ", " + safeJson() +
                    " }"; 
  server.send(200, "text/html", message + "\r\n");

#else
  server.send(200, "text/html", prepareJsonData() + "\r\n");
#endif
}

#ifdef  W_BOLTWOOD
//-----------------------------------------------------
//  web server page returning Boltwood II format data
//-----------------------------------------------------
void handleBoltwood(){

  DebugLn("handleBoltwood");

  getSensorData();

  String data = boltwoodData();

  String UserAgent = server.header("User-Agent");
  DebugLn("User Agent:" + UserAgent);
  if (UserAgent.indexOf("curl") == -1) {
    //browswer is not curl, replace spaces by &nbsp;
    data.replace(" ","&nbsp;");
  }

  server.send(200, "text/html", data);

}

#endif

