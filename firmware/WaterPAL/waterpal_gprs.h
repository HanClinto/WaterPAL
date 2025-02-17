#ifndef WATERPAL_GPRS_H
#define WATERPAL_GPRS_H

// HTTP functions over GPRS
#define LOGGING  // <- Optional logging is for the HTTP library
// Add a reception delay, if needed.
// This may be needed for a fast processor at a slow baud rate.
#define TINY_GSM_YIELD() { delay(2); }
// flag to force SSL client authentication, if needed
#define TINY_GSM_SSL_CLIENT_AUTHENTICATION

#include <ArduinoHttpClient.h>
#include <UrlEncode.h>
#include <TinyGsmClient.h>

// Server details
const char server[] = "script.google.com";
const int port = 443;

TinyGsmClientSecure client(modem);
HttpClient http(client, server, port);

int gprs_connect()
{
  Serial.println(F("GPRS connecting..."));
  if (!modem.waitForNetwork(600000L))
  {
    Serial.println(F("Failed to wait for network"));
    return 0;
  }

  modem.gprsConnect(WATERPAL_APN, WATERPAL_GPRS_USER, WATERPAL_GPRS_PASS);

  if (!modem.isNetworkConnected())
  {
    Serial.println(F("Network failed to connect"));
    return 0;
  }

  Serial.println(F("GPRS connected"));

  // Set CNACT=1
  //modem.sendAT("+CNACT=1");
  //if (modem.waitResponse() != 1)
  //{
  //  Serial.println(F("Failed to set CNACT=1"));
  //  return 0;
  //}

  Serial.println("Keeping connection alive...");
  http.connectionKeepAlive(); // Currently, this is needed for HTTPS

  return 1;
}

int gprs_disconnect()
{
  Serial.println(F("GPRS disconnecting..."));
  if (!modem.gprsDisconnect())
  {
    Serial.println(F("GPRS disconnection failed"));
    return 0;
  }
  Serial.println(F("GPRS disconnected"));
  return 1;
}

/* Weekly data parameters:
    data.IMEI,           // IMEI
    data.totalSMSCount,  // total SMS send count
    data.GPSLat,         // GPS Latitude
    data.GPSLong,        // GPS Longitude
    data.CPSI            // CPSI information (debug string)
*/
int gprs_send_data_weekly(String imei, int totalSMSCount, float GPSLat, float GPSLong, String CPSI)
{
  // Send data to the server in the style of:
  //  https://script.google.com/macros/s/AKfycbzc-xMFUDC5eisYN_rSOkV5UM0mTLd9s9ssqyqLW0LbzR1giPq5MqnKENSFYLHzFPvGUg/exec?IMEI=123456789012345&totalSMSCount=100&GPSLat=37.7749&GPSLong=-122.4194&CPSI=123456789012345
  //            /macros/s/AKfycbzc-xMFUDC5eisYN_rSOkV5UM0mTLd9s9ssqyqLW0LbzR1giPq5MqnKENSFYLHzFPvGUg/exec
  // https://script.googleusercontent.com/macros/echo?user_content_key=tBLjPdLV768fRCXKnjybjSXHf8MDnv_7F6gm3tw1v6mIub01mNvPjKVLpRw3ddWZdpdlS5wBAVhdh60BKs31Ik2H4B8R9ymWOJmA1Yb3SEsKFZqtv3DaNYcMrmhZHmUMWojr9NvTBuBLhyHCd5hHa6_AjzMEeZLPwpHOTeu5EjB0sDqZoFgVb03d_qyaNh157TCeshrO4LuhD7gWIsDLt-QjiL-BjcmvNbraE5--3IeBon6lfM2ckhzqF8ZARUsqutNVihpc7C-Dib38DDsdM26OZQqZIj2ott0usYC-tSid4Enrv_t2Gr_uf3VRE-HNrS-LTtTpPlJNUnnJM1QN20H9g71u62u-n7kaGU_oWbcdlBr5LOSoHPnkWsEC9MTFUaUDoaBHoGk_xGmh2k5VgZ6Y3U2YOVIIJ80TIiRQSQUay_phO-_QvOQv5e2uqcfSBUbgkya8KRuQ7MvWEirwyfiCQdLX5wkT-NG1O8mQJDo&lib=M4gCo7tC_8DJRAzIeZQdTh9nsRX4jpA1g
  // Prepare the URL
  // String url = "/macros/echo?user_content_key=tBLjPdLV768fRCXKnjybjSXHf8MDnv_7F6gm3tw1v6mIub01mNvPjKVLpRw3ddWZdpdlS5wBAVhdh60BKs31Ik2H4B8R9ymWOJmA1Yb3SEsKFZqtv3DaNYcMrmhZHmUMWojr9NvTBuBLhyHCd5hHa6_AjzMEeZLPwpHOTeu5EjB0sDqZoFgVb03d_qyaNh157TCeshrO4LuhD7gWIsDLt-QjiL-BjcmvNbraE5--3IeBon6lfM2ckhzqF8ZARUsqutNVihpc7C-Dib38DDsdM26OZQqZIj2ott0usYC-tSid4Enrv_t2Gr_uf3VRE-HNrS-LTtTpPlJNUnnJM1QN20H9g71u62u-n7kaGU_oWbcdlBr5LOSoHPnkWsEC9MTFUaUDoaBHoGk_xGmh2k5VgZ6Y3U2YOVIIJ80TIiRQSQUay_phO-_QvOQv5e2uqcfSBUbgkya8KRuQ7MvWEirwyfiCQdLX5wkT-NG1O8mQJDo&lib=M4gCo7tC_8DJRAzIeZQdTh9nsRX4jpA1g&";
  String url = "/macros/s/AKfycbzc-xMFUDC5eisYN_rSOkV5UM0mTLd9s9ssqyqLW0LbzR1giPq5MqnKENSFYLHzFPvGUg/exec?";
  url += "IMEI=" + imei;
  url += "&totalSMSCount=" + String(totalSMSCount);
  url += "&GPSLat=" + String(GPSLat, 6);
  url += "&GPSLong=" + String(GPSLong, 6);
  url += "&CPSI=" + urlEncode(CPSI);

  Serial.print(F("Requesting URL: "));
  Serial.println(url);

  // Send the request
  int err = http.get(url);

  if (err != 0)
  {
    Serial.print(F("HTTP GET failed, error: "));
    Serial.println(err);
    return 0;
  }

  // Read the status code and body of the response
  int status = http.responseStatusCode();
  Serial.print(F("Response status code: "));
  Serial.println(status);
  if (!status)
  {
    delay(10000);
    return 0;
  }

  Serial.println(F("Response Headers:"));
  while (http.headerAvailable())
  {
    String headerName = http.readHeaderName();
    String headerValue = http.readHeaderValue();
    Serial.println("    " + headerName + " : " + headerValue);
  }

  // Accept 200 or 302 as a valid response code.
  if (status != 200 && status != 302)
  {
    Serial.print(F("HTTP GET returned invalid response code: "));
    Serial.println(status);
    return 0;
  }

  int length = http.contentLength();
  if (length >= 0) {
    Serial.print(F("Content length is: "));
    Serial.println(length);
  }
  if (http.isResponseChunked()) {
    Serial.println(F("The response is chunked"));
  }

  String body = http.responseBody();
  Serial.println(F("Response:"));
  Serial.println(body);

  Serial.print(F("Body length is: "));
  Serial.println(body.length());

  // Close the connection
  http.stop();
  Serial.println(F("Server disconnected"));

  return 1;
}


/* Daily data parameters:
    data.IMEI,                             // IMEI
    data.totalSMSCount,                    // total SMS send count
    data.dailyWaterUsageTime,              // daily water usage time (s)
    data.detectedClockTimeDrift,           // detected clock time drift
    data.temperatureLow,                   // temp low
    data.temperatureAvg,                   // temp avg
    data.temperatureHigh,                  // temp high
    data.humidityLow,                      // humidity low
    data.humidityAvg,                      // humidity avg
    data.humidityHigh,                     // humidity high
    data.signalStrength,                   // signal strength (%)
    data.batteryChargeStatus,              // battery charge status
    data.batteryChargePercent,             // battery charge (%)
    data.batteryVoltage,                   // battery voltage (mV)
    data.bootCount                         // boot count
    */
int gprs_send_data_daily(String imei, int totalSMSCount, int dailyWaterUsageTime, int detectedClockTimeDrift, int temperatureLow, int temperatureAvg, int temperatureHigh, int humidityLow, int humidityAvg, int humidityHigh, int signalStrength, int batteryChargeStatus, int batteryChargePercent, int batteryVoltage, int bootCount)
{
  // Send data to the server in the style of:
  //  https://script.google.com/macros/s/AKfycbzc-xMFUDC5eisYN_rSOkV5UM0mTLd9s9ssqyqLW0LbzR1giPq5MqnKENSFYLHzFPvGUg/exec?IMEI=123456789012345&totalSMSCount=100&dailyWaterUsageTime=3600&detectedClockTimeDrift=5

  // Prepare the URL
  String url = "/macros/s/AKfycbzc-xMFUDC5eisYN_rSOkV5UM0mTLd9s9ssqyqLW0LbzR1giPq5MqnKENSFYLHzFPvGUg/exec?";
  url += "IMEI=" + imei;
  url += "&totalSMSCount=" + String(totalSMSCount);
  url += "&dailyWaterUsageTime=" + String(dailyWaterUsageTime);
  url += "&detectedClockTimeDrift=" + String(detectedClockTimeDrift);
  url += "&temperatureLow=" + String(temperatureLow);
  url += "&temperatureAvg=" + String(temperatureAvg);
  url += "&temperatureHigh=" + String(temperatureHigh);
  url += "&humidityLow=" + String(humidityLow);
  url += "&humidityAvg=" + String(humidityAvg);
  url += "&humidityHigh=" + String(humidityHigh);
  url += "&signalStrength=" + String(signalStrength);
  url += "&batteryChargeStatus=" + String(batteryChargeStatus);
  url += "&batteryChargePercent=" + String(batteryChargePercent);
  url += "&batteryVoltage=" + String(batteryVoltage);
  url += "&bootCount=" + String(bootCount);

  Serial.print(F("Requesting URL: "));
  Serial.println(url);

  Serial.print(F("Requesting URL: "));
  Serial.println(url);

  // Send the request
  int err = http.get(url);

  if (err != 0)
  {
    Serial.print(F("HTTP GET failed, error: "));
    Serial.println(err);
    return 0;
  }

  // Read the status code and body of the response
  int status = http.responseStatusCode();
  Serial.print(F("Response status code: "));
  Serial.println(status);
  if (!status)
  {
    delay(10000);
    return 0;
  }

  Serial.println(F("Response Headers:"));
  while (http.headerAvailable())
  {
    String headerName = http.readHeaderName();
    String headerValue = http.readHeaderValue();
    Serial.println("    " + headerName + " : " + headerValue);
  }

  // Accept 200 or 302 as a valid response code.
  if (status != 200 && status != 302)
  {
    Serial.print(F("HTTP GET returned invalid response code: "));
    Serial.println(status);
    return 0;
  }


  int length = http.contentLength();
  if (length >= 0) {
    Serial.print(F("Content length is: "));
    Serial.println(length);
  }
  if (http.isResponseChunked()) {
    Serial.println(F("The response is chunked"));
  }

  String body = http.responseBody();
  Serial.println(F("Response:"));
  Serial.println(body);

  Serial.print(F("Body length is: "));
  Serial.println(body.length());

  // Close the connection
  http.stop();
  Serial.println(F("Server disconnected"));

  return 1;
}

#endif // WATERPAL_GPRS_H
