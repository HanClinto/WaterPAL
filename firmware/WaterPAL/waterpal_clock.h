#ifndef WATERPAL_CLOCK_H
#define WATERPAL_CLOCK_H

#include <esp_attr.h>

bool parseTimestamp(const String& timestamp, struct tm& timeinfo, int16_t& quarterHourOffset) {
    int year, month, day, hour, minute, second;
    char tzSign;
    int tzOffset;

    Serial.println(">> Parsing timestamp: " + timestamp);

    // Try parsing with different potential formats
    if (sscanf(timestamp.c_str(), "\"%d/%d/%d,%d:%d:%d%c%d\"",
               &year, &month, &day, &hour, &minute, &second, &tzSign, &tzOffset) == 8) {
        // Successfully parsed
        Serial.println(">> Parsed timestamp with timezone information: " + String(year) + "/" + String(month) + "/" + String(day) + " " + String(hour) + ":" + String(minute) + ":" + String(second) + " " + String(tzSign) + String(tzOffset));
    } else if (sscanf(timestamp.c_str(), "\"%d/%d/%d,%d:%d:%d\"",
                      &year, &month, &day, &hour, &minute, &second) == 6) {
        // Timestamp without timezone information
        tzSign = '+';
        tzOffset = 0;

        Serial.println(">> Parsed timestamp WITHOUT timezone information: " + String(year) + "/" + String(month) + "/" + String(day) + " " + String(hour) + ":" + String(minute) + ":" + String(second) + " " + String(tzSign) + String(tzOffset));
    } else {
        return false; // Parsing failed
    }

    // Fill the tm struct
    timeinfo.tm_year = year + 2000 - 1900;
    timeinfo.tm_mon = month - 1;
    timeinfo.tm_mday = day;
    timeinfo.tm_hour = hour;
    timeinfo.tm_min = minute;
    timeinfo.tm_sec = second;

    // Convert timezone offset
    quarterHourOffset = (tzSign == '-' ? 1 : -1) * tzOffset;

    return true;
}

// Function to convert timeval to ISO 8601 format string
String timevalToISO8601(struct timeval tv) {
  char buffer[30];
  
  // Convert seconds to a time structure
  time_t nowtime = tv.tv_sec;
  struct tm *nowtm = gmtime(&nowtime);
  
  // Format the date and time including milliseconds
  // Format: YYYY-MM-DDThh:mm:ssZ
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", nowtm);
  
  // Return as a String
  return String(buffer) + "Z";
  
  // If you need milliseconds precision, uncomment these lines:
  /*
  char millisec[5];
  sprintf(millisec, ".%03d", (int)(tv.tv_usec / 1000));
  return String(buffer) + String(millisec) + "Z";
  */
}

#endif // WATERPAL_CLOCK_H