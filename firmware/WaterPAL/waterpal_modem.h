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

#endif // WATERPAL_MODEM_H