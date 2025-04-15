#ifndef WATERPAL_MODEM_H
#define WATERPAL_MODEM_H

#include <TinyGsmClient.h> //  Purpose:  header file in the TinyGSM library, for communicating with various GSM modules. The library provides an abstraction layer that simplifies the process of sending AT commands.
#include "waterpal_error_logging.h"
#include "waterpal_clock.h"

// These functions are all related to the modem, and are used to interact with it in various ways. They are all part of the firmware for the WaterPAL device, which is designed to monitor water usage and send SMS messages with relevant data. The functions are used to gather information from the modem, send messages, and manage the modem's power state.

// Reading various information from the modem (battery level, tower info, signal strength, timestamp, GPS)
// Sending SMS messages
// Starting up the modem
// Powering down the modem

int64_t modem_get_IMEI();
int modem_clear_buffer();

static bool _modem_is_on = false; // 0 = off, 1 = on

bool modem_on(bool full_restart = true)
{
  if (_modem_is_on)
  {
    Serial.println("Modem is already on");
    return true;
  }

  // Start the cell antenna
  pinMode(PWR_PIN, OUTPUT);    // Set power pin to output needed to START modem on power pin 4
  digitalWrite(PWR_PIN, HIGH); // Set power pin high (on), which when inverted is low
  delay(1000);                 // Docs note: "Starting the machine requires at least 1 second of low level, and with a level conversion, the levels are opposite"
  // NOTE: Some docs say 300ms is sufficient, but we're using 1s to be safe.
  digitalWrite(PWR_PIN, LOW);  // Set power pin low (off), which when inverted is high

  SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX); // Set conditions for serial port to read and write

  bool success = false;
  // There are two ways to initialize the modem -- restart, or simple init.
  if (full_restart) {
    Serial.println("Initializing modem via restart..."); // Start modem on next line
    if (!modem.restart())
    { //  Command to start modem, see extended notes tab
      Serial.println("Failed to restart modem, attempting to continue without restarting");
    } else {
      Serial.println("Modem restarted");
      success = true;
      _modem_is_on = true;
    }
  } else {
    Serial.println("Initializing modem via initialize..."); // Start modem on next line
    if (!modem.init())
    { //  Command to start modem, see extended notes tab
      Serial.println("Failed to init modem, attempting to continue...");
    } else {
      Serial.println("Modem initialized");
      success = true;
    }
  }

  return success;
}

int64_t _imei;

int64_t modem_on_get_imei()
{
  if (_modem_is_on)
  {
    Serial.println("Modem is already on");
    return _imei;
  }

  // Start the cell antenna
  pinMode(PWR_PIN, OUTPUT);    // Set power pin to output needed to START modem on power pin 4
  digitalWrite(PWR_PIN, HIGH); // Set power pin high (on), which when inverted is low
  delay(1000);                 // Docs note: "Starting the machine requires at least 1 second of low level, and with a level conversion, the levels are opposite"
  // NOTE: Some docs say 300ms is sufficient, but we're using 1s to be safe.
  digitalWrite(PWR_PIN, LOW);  // Set power pin low (off), which when inverted is high

  SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX); // Set conditions for serial port to read and write

  bool success = false;

  // Start out without doing a full restart
  bool full_restart = false;

  // Start the modem

  do {
    // There are two ways to initialize the modem -- restart, or simple init.
    if (full_restart) {
      Serial.println("Initializing modem via restart..."); // Start modem on next line
      if (!modem.restart())
      { //  Command to start modem, see extended notes tab
        Serial.println("Failed to restart modem, attempting to continue without restarting");
      } else {
        Serial.println("Modem restarted");
      }
    } else {
      Serial.println("Initializing modem via initialize..."); // Start modem on next line
      if (!modem.init())
      { //  Command to start modem, see extended notes tab
        Serial.println("Failed to init modem, attempting to continue...");
      } else {
        Serial.println("Modem initialized");
      }
    }
    // If we failed to initialize the modem, we should try a full restart next time
    full_restart = true;

    _imei = modem_get_IMEI();

    for (int i = 0; i < 10; i++)
    {
      if (_imei != 0)
      {
        Serial.print("Successfully retrievied IMEI: ");
        Serial.println(_imei);
        success = true;
        break;
      }
      // Wait a bit
      delay(1000);
      // Clear our buffer
      int bytes_cleared = modem_clear_buffer();
      Serial.println("Failed to get IMEI. Cleared " + String(bytes_cleared) + " bytes from buffer. Retrying...");
      // Try again
      _imei = modem_get_IMEI();
    }

  } while (!success);

  _modem_is_on = true;
 
  return _imei;
}

bool modem_off()
{
  // Send the shutdown command
  modem.sendAT("+CPOWD=1"); // Power down the modem
  delay(1000);
  // TODO: Do we want to check for a response here?

  // TODO: Should we sleep the modem instead of turning it off entirely?
  // Power down the modem
  pinMode(PWR_PIN, OUTPUT);    // Set power pin to output needed to START modem on power pin 4
  digitalWrite(PWR_PIN, HIGH); // Set power pin high (on), which when inverted is low

  SerialAT.end(); // End serial port communication

  _modem_is_on = false;

  return true;
}

typedef struct batteryInfo
{
  int charging;
  int percentage;
  int voltage_mV;
} batteryInfo;

batteryInfo modem_get_batt_val()
{
  batteryInfo battInfo;

  // This function checks battery level.  See page 58 of SIM manual.  Output from CBC is (battery charging on or off 0,1,2),(percentage capacity),(voltage in mV)
  // NOTE: This does not work if plugged into USB power, so need to connect to (unpowered) FTDI serial port monitor to actually test this.
  modem.sendAT("+CBC");
  // TODO: Simplify this as   if (!modem.waitResponse(1000L, res, "+CBC: ")) ?
  if (!modem.waitResponse("+CBC: "))
  {
    logError(ERROR_BATTERY_READ); //, "Failed to get battery level");
    return battInfo;
  }
  String battLoop = modem.stream.readStringUntil('\n');
  battLoop.trim();
  modem.waitResponse();

  // Parse the battery level response
  if (!sscanf(battLoop.c_str(),
              "%d,%d,%d",
              &battInfo.charging,
              &battInfo.percentage,
              &battInfo.voltage_mV))
  {
    logError(ERROR_BATTERY_READ);
  }

  return battInfo;
}

batteryInfo modem_get_batt_val_retry() {
  batteryInfo battInfo;
  for (int i = 0; i < 10; i++)
  {
    battInfo = modem_get_batt_val();
    // Percentage should be between 0 and 100, but let's be safe and check for 0-1000
    if (battInfo.percentage > 0 && battInfo.percentage < 1000)
    {
      return battInfo;
    }
    // Wait a bit
    delay(1000);
    // Clear our buffer
    int bytes_cleared = modem_clear_buffer();
    Serial.println("Failed to get battery levels. Cleared " + String(bytes_cleared) + " bytes from buffer. Retrying...");
  }
  return battInfo;
}

int8_t modem_get_signal_quality()
{
  Serial.println("\n---Getting Signal Quality---\n");

  // Read tower signal strength
  int csq = modem.getSignalQuality();
  if (csq == 99) {
    Serial.println(">> Failed to get signal quality");
    csq = 0;
  }
  // getSignalQuality returns 99 if it fails, or 0-31 if it succeeds, so we check for failure and map return values to percentage from 0-100.
  csq = map(csq, 0, 31, 0, 100);
  Serial.println("Signal quality: " + String(csq) + "%");

  return csq;
}

int8_t modem_get_signal_quality_retry()
{
  int8_t csq = 0;
  for (int i = 0; i < 10; i++)
  {
    csq = modem_get_signal_quality();
    if (csq > 0)
    {
      return csq;
    }
    // Wait a bit
    delay(1000);
    // Clear our buffer
    int bytes_cleared = modem_clear_buffer();
    Serial.println("Failed to get signal quality. Cleared " + String(bytes_cleared) + " bytes from buffer. Retrying...");
  }
  return csq;
}

// Clear the input buffer and return the number of bytes cleared
int modem_clear_buffer()
{
  int bytes_cleared = 0;
  while (SerialAT.available())
  {
    SerialAT.read();
    bytes_cleared++;
  }
  return bytes_cleared;
}

String modem_get_cpsi()
{
  String cpsi;
  modem.sendAT("+CPSI?"); //  test cell provider info
  if (modem.waitResponse("+CPSI: ") == 1)
  {
    cpsi = modem.stream.readStringUntil('\n');
    cpsi.trim();
    modem.waitResponse();
    Serial.println(">> The current network parameters are: '" + cpsi + "'");
  } else {
    Serial.println(">> No network parameters found");
  }

  int bytes_cleared = modem_clear_buffer();
  if (bytes_cleared > 0) {
    Serial.println("Cleared " + String(bytes_cleared) + " bytes from buffer after querying network parameters.");
  }

  return cpsi;
}

// **********
// IMEI Functions
// **********

int64_t _str_to_int64(const String& str)
{
  int64_t val = 0;
  for (int i = 0; i < str.length(); i++)
  {
    // Check for non-numeric characters
    if (str[i] < '0' || str[i] > '9')
    {
      // Skip over potential hyphens or other non-numeric characters
      continue;
    }
    val = val * 10 + (str[i] - '0');
  }

  return val;
}

String _int64_to_base64(int64_t val)
{
  // Ex: 869951037053562 -> "DFzdCiRp6"
  // Base64 encoding
  const char *b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"; // NOTE: URLs don't like + or /, so change to _. for our purposes
  String res = "";
  while (val > 0)
  {
    res = b64[val & 0x3F] + res;
    val >>= 6;
  }
  return res;
}

int64_t _base64_to_int64(const String& b64_str)
{
  // Base64 decoding
  const char *b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"; // NOTE: URLs don't like + or /, so change to _. for our purposes
  int64_t val = 0;
  for (int i = 0; i < b64_str.length(); i++)
  {
    val = val << 6;
    val += strchr(b64, b64_str[i]) - b64;
  }
  return val;
}

int64_t modem_get_IMEI()
{
  // The IMEI is either 15 or 16 digits long, so we need to store it as a string.
  // "The IMEI (15 decimal digits: 14 digits plus a check digit) or IMEISV (16 decimal digits: 14 digits plus two software version digits) includes information on the origin, model, and serial number of the device."
  // Ex: "869951037053562"
  String imei_str = modem.getIMEI();

  //if (imei_str.length() == 0)
  //{
  //  imei_str = "0000000000000000"; // Default if we cannot get an IMEI
  //}

  Serial.println("IMEI: " + imei_str);

  // Parse the IMEI string into a 64-bit integer
  return _str_to_int64(imei_str);
}

String modem_get_IMEI_base64()
{
  return _int64_to_base64(modem_get_IMEI());
}

// AT+CLTS Get Local Timestamp
// NOTE: Not sure if needed, but may add it later if AT+CCLK? is not sufficient.
/*
int8_t getLocalTimestampUTC() {
  modem.sendAT("+CLTS?");
  if (modem.waitResponse("+CSQ:") != 1) { return 99; }
  int8_t res = modem.streamGetIntBefore(',');
  modem.waitResponse();
  return res;
}
*/

int8_t modem_setLocalTimeFromCCLK() {
  // Send:
    // AT+CCLK?
  // Receive
    // -> +CCLK: "24/10/08,23:39:49-16"
    // ->
    // -> OK

  modem.sendAT("+CCLK?");
  if (modem.waitResponse("+CCLK:") != 1) { return 0; }

  // Request the current system time so that we can calculate the drift between the modem's time and the system time.
  struct timeval tv_orig;
  gettimeofday(&tv_orig, NULL);

  // Receive the timestamp string from the modem
  String timestamp = SerialAT.readString();
  timestamp.trim();

  struct tm timeinfo;
  // Zero-out the struct
  memset(&timeinfo, 0, sizeof(timeinfo));

  int16_t timezone_quarterHourOffset; // tm struct does not have a space for the timezone offset, so store it in a separate number for now.

  // Note that the timezone offset is in quarter-hour increments, which can handle things like India's 5.5 hour offset.
  if (!parseTimestamp(timestamp, timeinfo, timezone_quarterHourOffset)) {
    Serial.println(">>> Failed to parse timestamp: " + timestamp);
    logError(ERROR_TIMESTAMP_FAIL); //, "Failed to parse timestamp");
    return 0;
  }

  // TODO: How to properly use timezone information?
  //  NOTE: May not be needed if we deal primarily in local time. Important thing is to send SMS messages at 10pm local, so if UTC is not set correctly, that's probably fine.
  // TODO: How to populate timeinfo.tm_isdst properly?
  //  NOTE: Most developing countries do not use DST, so we can default to 0 for now.

  // Set the system time
  time_t t_of_day = mktime(&timeinfo);
  struct timeval tv_new;
  tv_new.tv_sec = t_of_day;
  tv_new.tv_usec = 0;
  settimeofday(&tv_new, NULL); // Update the RTC with the new time epoch offset.

  int8_t res = modem.waitResponse(); // Clear the OK

  // Only calculate drift if this is not our first time waking up
  if (bootCount > 1) {
    // Calculate the offset between the modem's time and the original system time
    int64_t time_diff_s = tv_new.tv_sec - tv_orig.tv_sec;

    Serial.println(">> TIME DRIFT: Drift between modem and system time: " + String(time_diff_s) + " seconds");

    last_time_drift_val_s = time_diff_s;
  } else {
    Serial.println(">> TIME DRIFT: First time setting time from modem -- no drift calculation needed.");
  }

  return res;
}

// **********
// SMS Functions
// **********

// SMS packets have a header and a body.

// Header:
//  Version (integer, 1 byte)
//  Packet type (integer, 1 byte)
//  SMS Index (integer, 4 bytes)
//  Identity (IMEI, base64 encoded, 


bool modem_broadcast_sms(const String& message, const int num_retries = 10)
{
  bool error = false;

  const int num_phone_numbers = sizeof(WATERPAL_DEST_PHONE_NUMBERS) / sizeof(WATERPAL_DEST_PHONE_NUMBERS[0]);

  Serial.println("Broadcasting SMS message to " + String(num_phone_numbers) + " numbers: '" + message + "'");

  // Send the SMS message to all the phone numbers in the list
  for (int i = 0; i < num_phone_numbers; i++)
  {
    int retry_cnt = 0;

    // Retry sending the SMS message up to num_retries times
    while (retry_cnt < num_retries)
    {
      if (modem.sendSMS(WATERPAL_DEST_PHONE_NUMBERS[i], message))
      {
        Serial.println(" SMS message sent successfully to number " + String(WATERPAL_DEST_PHONE_NUMBERS[i]) + " [" + String(i) + "]");
        break;
      }
      retry_cnt++;

      int signal_quality = modem_get_signal_quality();

      Serial.println(" Failed to send SMS message to number " + String(WATERPAL_DEST_PHONE_NUMBERS[i]) + " [" + String(i) + "]. Signal quality: " + String(signal_quality) + ". Retrying... (attempt " + String(retry_cnt) + " of " + String(num_retries) + ")");

      delay(1000);
    }
    if (retry_cnt == num_retries)
    {
      logError(ERROR_SMS_FAIL); //, "Failed to send SMS message");
      error = true;
    }
  }

  return !error;
}

bool modem_broadcast_sms_sprintf(const char* format, ...)
{
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  return modem_broadcast_sms(buffer);
}

String modem_read_sms()
{
  // Configure the modem to read SMS messages
  modem.sendAT("+CMGF=1"); // Set SMS mode to text
  if (modem.waitResponse() != 1)
  {
    logError(ERROR_SMS_FAIL); //, "Failed to set SMS mode to text");
    return "";
  }

  // Read the SMS message
  modem.sendAT("+CMGR=1"); // Read SMS message at index 1
  if (modem.waitResponse("+CMGR: ") != 1)
  {
    logError(ERROR_SMS_FAIL); //, "Failed to read SMS message");
    return "";
  }

  // Parse the SMS message
  String sms = modem.stream.readStringUntil('\n');
  sms.trim();
  modem.waitResponse();

  // Delete all read SMS messages
  modem.sendAT("+CMGD=1,4"); // Delete all read SMS messages
  if (modem.waitResponse() != 1)
  {
    logError(ERROR_SMS_FAIL); //, "Failed to delete SMS messages");
  }

  return sms;
}

// **********
// GPS Functions
// **********

bool modem_gps_on()
{
  // Set Modem GPS Power Control Pin to HIGH ,turn on GPS power
  // Only in version 20200415 is there a function to control GPS power
  modem.sendAT("+CGPIO=0,48,1,1");
  if (modem.waitResponse(10000L) != 1)
  {
    logError(ERROR_GPS_FAIL); //, "Failed to turn on GPS");
  }

  Serial.println("\nEnabling GPS...\n");

  return modem.enableGPS();
}

struct gpsInfo
{
  float lat;
  float lon;
  // Time info
  int year;
  int month;
  int day;
  int hour;
  int minute;
  int second;
};

// Get GPS info (with optional timeout, default to 60 seconds)
bool modem_get_gps(struct gpsInfo& gps, uint32_t timeout_s = 60)
{
  bool success = false;

  struct timeval gps_start;
  gettimeofday(&gps_start, NULL);
  struct timeval gps_now = gps_start;
  int attempt_cnt = 0;

  while (gps_now.tv_sec - gps_start.tv_sec < timeout_s)
  {
    if (modem.getGPS(&gps.lat, &gps.lon, 
                     0, 0, 0, 0, 0,
                     &gps.year, &gps.month, &gps.day,
                     &gps.hour, &gps.minute, &gps.second))
    {
      success = true;

      // TODO: Should we use this to set the system time?
      //  Note that this is the UTC time, so we would need to adjust for the local timezone.
      //  The cell tower gives us the local time (with timezone info), so it may be better for setting the time of the device.
      //  Even so, it would be good to have the GPS time as a backup, and to check for drift / compare accuracy.
      break;
    }
    else
    {
      Serial.print("getGPS attempt " + String(++attempt_cnt) + " unsuccessful. Is your antenna plugged in? Time: ");
      Serial.println(millis());
    }
    gettimeofday(&gps_now, NULL);
    // Try again later
    delay(1000);
  }

  if (!success)
  {
    Serial.println("getGPS failed. Is your antenna plugged in?");
    logError(ERROR_GPS_FAIL); //, "Failed to get GPS data");
  } else {
    Serial.println("GPS data received successfully in " + String(gps_now.tv_sec - gps_start.tv_sec) + " seconds");
  }

  int bytes_cleared = modem_clear_buffer();
  if (bytes_cleared > 0) {
    Serial.println("GPS: Cleared " + String(bytes_cleared) + " bytes from buffer after GPS read.");
  }


  return success;
}

bool modem_gps_off()
{
  bool success = modem.disableGPS();

  // Set Modem GPS Power Control Pin to LOW ,turn off GPS power
  // Only in version 20200415 is there a function to control GPS power
  modem.sendAT("+CGPIO=0,48,1,0");
  if (modem.waitResponse(10000L) != 1)
  {
    DBG("Set GPS Power LOW Failed");
  }

  int bytes_cleared = modem_clear_buffer();
  if (bytes_cleared > 0) {
    Serial.println("GPS: Cleared " + String(bytes_cleared) + " bytes from buffer after GPS shutdown.");
  }

  return success;
}


#endif // WATERPAL_MODEM_H
