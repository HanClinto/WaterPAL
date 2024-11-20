#ifndef WATERPAL_MODEM_H
#define WATERPAL_MODEM_H

#include <TinyGsmClient.h> //  Purpose:  header file in the TinyGSM library, for communicating with various GSM modules. The library provides an abstraction layer that simplifies the process of sending AT commands.
#include "waterpal_error_logging.h"

// These functions are all related to the modem, and are used to interact with it in various ways. They are all part of the firmware for the WaterPAL device, which is designed to monitor water usage and send SMS messages with relevant data. The functions are used to gather information from the modem, send messages, and manage the modem's power state.

// Reading various information from the modem (battery level, tower info, signal strength, timestamp, GPS)
// Sending SMS messages
// Starting up the modem
// Powering down the modem

bool modem_init(bool full_restart = true)
{
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

int8_t modem_set_local_time_from_cclk() {
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


#endif // WATERPAL_MODEM_H