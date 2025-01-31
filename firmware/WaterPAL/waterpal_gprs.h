#ifndef WATERPAL_GPRS_H
#define WATERPAL_GPRS_H

// HTTP functions over GPRS

#include <ArduinoHttpClient.h>
#include <UrlEncode.h>

// #define LOGGING  // <- Optional logging is for the HTTP library

// Add a reception delay, if needed.
// This may be needed for a fast processor at a slow baud rate.
// #define TINY_GSM_YIELD() { delay(2); }

// flag to force SSL client authentication, if needed
// #define TINY_GSM_SSL_CLIENT_AUTHENTICATION

// Server details
const char server[] = "script.google.com";
const int port = 443;

TinyGsmClientSecure client(modem);
HttpClient http(client, server, port);

int gprs_connect()
{
  Serial.println(F("GPRS connecting..."));
  modem.gprsConnect(WATERPAL_APN, WATERPAL_GPRS_USER, WATERPAL_GPRS_PASS);

  if (!modem.waitForNetwork())
  {
    Serial.println(F("Failed to wait for network"));
    return 0;
  }

  if (!modem.isNetworkConnected())
  {
    Serial.println(F("Network failed to connect"));
    return 0;
  }

  Serial.println(F("GPRS connected"));

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

int gprs_send_data(String imei, int totalSMSCount, int dailyWaterUsageTime, int detectedClockTimeDrift)
{
  // Send data to the server in the style of:
  //  https://script.google.com/macros/s/AKfycbzc-xMFUDC5eisYN_rSOkV5UM0mTLd9s9ssqyqLW0LbzR1giPq5MqnKENSFYLHzFPvGUg/exec?IMEI=123456789012345&totalSMSCount=100&dailyWaterUsageTime=3600&detectedClockTimeDrift=5

  // Prepare the URL
  String url = "/macros/s/AKfycbzc-xMFUDC5eisYN_rSOkV5UM0mTLd9s9ssqyqLW0LbzR1giPq5MqnKENSFYLHzFPvGUg/exec?";
  url += "IMEI=" + imei;
  url += "&totalSMSCount=" + String(totalSMSCount);
  url += "&dailyWaterUsageTime=" + String(dailyWaterUsageTime);
  url += "&detectedClockTimeDrift=" + String(detectedClockTimeDrift);

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
  SerialMon.print(F("Response status code: "));
  SerialMon.println(status);
  if (!status)
  {
    delay(10000);
    return;
  }

  SerialMon.println(F("Response Headers:"));
  while (http.headerAvailable())
  {
    String headerName = http.readHeaderName();
    String headerValue = http.readHeaderValue();
    SerialMon.println("    " + headerName + " : " + headerValue);
  }

  if (status != 200)
  {
    Serial.print(F("HTTP GET returned invalid response code: "));
    Serial.println(status);
    return 0;
  }


  int length = http.contentLength();
  if (length >= 0) {
    SerialMon.print(F("Content length is: "));
    SerialMon.println(length);
  }
  if (http.isResponseChunked()) {
    SerialMon.println(F("The response is chunked"));
  }

  String body = http.responseBody();
  SerialMon.println(F("Response:"));
  SerialMon.println(body);

  SerialMon.print(F("Body length is: "));
  SerialMon.println(body.length());

  // Close the connection
  http.stop();
  SerialMon.println(F("Server disconnected"));
}

#endif // WATERPAL_GPRS_H
