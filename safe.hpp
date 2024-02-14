/*************************************************
          safe / unsafe conditions check
*************************************************/

bool safePrev = false;
bool safeNow  = false;

bool isSafe(){

  bool res = true;

  if (skyTemp > settings.data.cloudytemp)
    res = false;
  else if (humidity > settings.data.humiditythreshold)
    res = false;
#ifdef  W_WIND
  if (maxwindspeed > settings.data.windthreshold)
    res = false;
#endif
#ifdef  W_RAIN
  if (rainA < settings.data.rainthreshold)
    res = false;
#endif
  if (cloudI > settings.data.cloudthreshold)
    res = false;
  if (sqmMpsas < settings.data.sqmthreshold)
    res = false;
  else if (sqmLux > settings.data.luxthreshold)
    res = false;

  safePrev = safeNow;   // store previous safe condition
  safeNow = res;        // store current safe condition
  return res;
}

String safeJson()
{

  String jsonMsg =  String("\"safe\": ") + String(isSafe());
  return jsonMsg;

}

void sendSafedata()
{

#ifdef  W_MQTT
  mqttSendSensorData(safeJson());
#endif
}
