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

TinyGsmClientSecure client(modem, 0);
HttpClient http(client, server, port);

#if WATERPAL_USE_DESIGNOUTREACH_HTTP
// Server details for Design Outreach servers
const char server_designoutreach[] = "devdo.bridgetreedcc.com";
const int port_designoutreach = 443;

// NOTE: "To have more than one client of any type, you need to create them on different sockets." c.f. https://github.com/vshymanskyy/TinyGSM/issues/292#issuecomment-496014840
TinyGsmClientSecure client_designoutreach(modem, 1);
HttpClient http_designoutreach(client_designoutreach, server_designoutreach, port_designoutreach);
#endif

int gprs_connected = 0;

int gprs_connect()
{
  // Don't connect twice
  if (gprs_connected)
  {
    Serial.println(F("GPRS already connected"));
    return 1;
  }

  int bytes_cleared = modem_clear_buffer();
  if (bytes_cleared > 0) {
    Serial.println("Cleared " + String(bytes_cleared) + " bytes from buffer prior to GPRS connect.");
  }

  // Wait a maximum of 1 minute to connect to the network
  Serial.println(F("GPRS connecting..."));
  if (!modem.waitForNetwork(60L * 1000L))
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
  gprs_connected = 1;
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

  #if WATERPAL_USE_DESIGNOUTREACH_HTTP
  http_designoutreach.connectionKeepAlive();
  #endif // WATERPAL_USE_DESIGNOUTREACH_HTTP

  return 1;
}

int gprs_disconnect()
{
  // Don't disconnect twice
  if (!gprs_connected)
  {
    Serial.println(F("GPRS already disconnected"));
    return 1;
  }
  
  Serial.println(F("GPRS disconnecting..."));
  if (!modem.gprsDisconnect())
  {
    Serial.println(F("GPRS disconnection failed"));
    return 0;
  }
  Serial.println(F("GPRS disconnected"));
  gprs_connected = 0;
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

  // Set our device timeout
  http.setHttpResponseTimeout(WATERPAL_HTTP_TIMEOUT_MS);

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
  if (status < 0)
  {
    Serial.println("Response " + String(status) + " from server. Waiting and trying again...");
    delay(1000);
    return 0;
  }

  Serial.println(F("Response Headers:"));
  while (http.headerAvailable())
  {
    String headerName = http.readHeaderName();
    if (http.headerAvailable())
    {
      String headerValue = http.readHeaderValue();
      Serial.println("    " + headerName + " : " + headerValue);
    }
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

  // Set our device timeout
  http.setHttpResponseTimeout(WATERPAL_HTTP_TIMEOUT_MS);

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
  if (status < 0)
  {
    Serial.println("Response " + String(status) + " from server. Waiting and trying again...");
    delay(1000);
    return 0;
  }

  Serial.println(F("Response Headers:"));
  while (http.headerAvailable())
  {
    String headerName = http.readHeaderName();
    if (http.headerAvailable())
    {
      String headerValue = http.readHeaderValue();
      Serial.println("    " + headerName + " : " + headerValue);
    }
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

#if WATERPAL_USE_DESIGNOUTREACH_HTTP

const char header_a[] = { 0x30, 0x36, 0x64, 0x65, 0x37, 0x37, 0x65, 0x34, 0x37, 0x30, 0x35, 0x37, 0x32, 0x30, 0x35, 0x31, 0x61, 0x33, 0x33, 0x30, 0x63, 0x33, 0x62, 0x39, 0x32, 0x30, 0x33, 0x61, 0x34, 0x64, 0x31, 0x32, 0x00 };

int gprs_post_data_daily_designoutreach(String imei, int totalSMSCount, int dailyWaterUsageTime, int detectedClockTimeDrift, int temperatureLow, int temperatureAvg, int temperatureHigh, int humidityLow, int humidityAvg, int humidityHigh, int signalStrength, int batteryChargeStatus, int batteryChargePercent, float batteryVoltage, int bootCount)
{
  // Get the time of the request
  timeval tv;
  gettimeofday(&tv, NULL);
  String time_iso8601 = timevalToISO8601(tv);

  // NOTE: Convert liters per hour into gallons per day.
  // 1 liter = 0.264172 gallons
  // dailyWaterUsageTime is in seconds
  // Assuming 950 liters is transferred in 1 hr of water usage, convert to total gallons for the day's usage
  float gallons = ((dailyWaterUsageTime * WATERPAL_LITERS_PER_HR) * 0.264172) / 3600;

  // Create the JSON payload
  String jsonPayload = "{";
  jsonPayload += "\"sensor_id\": \"" + imei + "\", ";
  jsonPayload += "\"Imei_number\": \"" + imei + "\", ";

  jsonPayload += "\"timestamp\": \"" + time_iso8601 + "\", ";

  jsonPayload += "\"daily_water_usage_second\": " + String(dailyWaterUsageTime) + ", ";
  jsonPayload += "\"battery_voltage\": " + String(batteryVoltage) + ", ";
  jsonPayload += "\"gallons\": " + String(gallons) + ", ";
  jsonPayload += "\"period\": \"24 Hours\", ";
  jsonPayload += "\"boot_count\": \"" + String(bootCount) + "\", ";
  jsonPayload += "\"battery_charge\": \"" + String(batteryChargePercent) + "\", ";
  jsonPayload += "\"signal_strength\": \"" + String(signalStrength) + "\", ";
  jsonPayload += "\"humid\": \"" + String(humidityAvg) + "\", ";  // Using avg humidity as the example only has one field
  jsonPayload += "\"temperature\": \"" + String(temperatureAvg) + "\", ";  // Using avg temperature
  jsonPayload += "\"detected_clock\": \"" + String(detectedClockTimeDrift) + "\", ";
  jsonPayload += "\"total_sms_count\": \"" + String(totalSMSCount) + "\" ";
  jsonPayload += "}";

  Serial.print(F("Prepared JSON payload (length: "));
  Serial.print(jsonPayload.length());
  Serial.println(F("):"));
  Serial.println(jsonPayload);

  // Define the endpoint URL (without query parameters now)
  String url = "ulcs/usagedata";

  Serial.print(F("Requesting POST to URL: "));
  Serial.println(url);

  // Set our device timeout
  http_designoutreach.setHttpResponseTimeout(WATERPAL_HTTP_TIMEOUT_MS);

  // Add authentication header
  http_designoutreach.beginRequest();
  http_designoutreach.post(url);
  http_designoutreach.sendHeader("Content-Type", "application/json");
  http_designoutreach.sendHeader("Key", header_a);
  http_designoutreach.sendHeader("Content-Length", jsonPayload.length());

  // Send the JSON payload
  http_designoutreach.beginBody();
  http_designoutreach.print(jsonPayload);
  http_designoutreach.endRequest();

  // Read the status code and body of the response
  int status = http_designoutreach.responseStatusCode();
  Serial.print("Response status code: ");
  Serial.println(status);
  if (status < 0)
  {
    Serial.println("Response " + String(status) + " from server. Waiting and trying again...");
    delay(1000);
    return 0;
  }

  Serial.println("Response Headers:");
  while (http_designoutreach.headerAvailable())
  {
    String headerName = http_designoutreach.readHeaderName();
    if (http_designoutreach.headerAvailable())
    {
      String headerValue = http_designoutreach.readHeaderValue();
      Serial.println("    " + headerName + " : " + headerValue);
    }
  }

  // Accept 200 or 201 as a valid response code for POST
  if (status != 200 && status != 201)
  {
    Serial.print(F("HTTP POST returned invalid response code: "));
    Serial.println(status);
    return 0;
  }

  int length = http_designoutreach.contentLength();
  if (length >= 0) {
    Serial.print(F("Content length is: "));
    Serial.println(length);
  }
  if (http_designoutreach.isResponseChunked()) {
    Serial.println(F("The response is chunked"));
  }

  String body = http_designoutreach.responseBody();
  Serial.print(F("Response body length is: "));
  Serial.println(body.length());

  Serial.println(F("Response:"));
  Serial.println(body);

  // Close the connection
  http_designoutreach.stop();
  Serial.println(F("Server disconnected"));

  return 1;
}
#endif // WATERPAL_USE_DESIGNOUTREACH_HTTP

#endif // WATERPAL_GPRS_H
