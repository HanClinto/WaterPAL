#ifndef WATERPAL_ERROR_LOGGING_H
#define WATERPAL_ERROR_LOGGING_H

#include <esp_attr.h>

// Storage for error codes and messages to assist in debugging
volatile RTC_DATA_ATTR int last_error_code = 0;       // Last error code
volatile RTC_DATA_ATTR int64_t last_error_time_s = 0; // Time of the last error in seconds since epoch

// Error codes
#define ERROR_NONE 0
#define ERROR_UNKNOWN 1
#define ERROR_GPS_FAIL 2
#define ERROR_SMS_FAIL 3
#define ERROR_MODEM_FAIL 4
#define ERROR_SENSOR_FAIL 5
#define ERROR_WATER_SENSOR_FAIL 6
#define ERROR_BATTERY_READ 7
#define ERROR_TIMESTAMP_FAIL 8 // Failed to parse timestamp

String getError()
{
  if (last_error_code == ERROR_NONE)
  {
    return "";
  }
  return "ERROR: " + String(last_error_code) + " (at " + String(last_error_time_s) + ")";
}

void logError(int error_code)
{
  timeval tv;
  gettimeofday(&tv, NULL);

  last_error_code = error_code;
  last_error_time_s = tv.tv_sec;

  Serial.println("  >>> " + getError());
}

void clearError()
{
  last_error_code = ERROR_NONE;
  last_error_time_s = 0;
}

#endif // WATERPAL_ERROR_LOGGING_H