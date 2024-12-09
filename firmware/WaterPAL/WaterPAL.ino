// WaterPAL - Water Pump Activity Logger
// Copyright (c) 2024 Clint Herron and Stephen Peacock
// MIT License

#define TINY_GSM_MODEM_SIM7000  //  Purpose:  inform the TinyGSM library which GSM module you are using. This allows the library to tailor its operations, such as AT commands and responses
#define TINY_GSM_RX_BUFFER 1024 //  Purpose:  determines how much data can be stored temporarily while it is being received from the GSM module.

#define GSM_PIN ""              //  NOTE: not sure if needed or not

#define SerialAT Serial1   //  Purpose:  Select hardware serial port for AT commands
#define DUMP_AT_COMMANDS   //  Purpose:  you can see every AT command that the TinyGSM library sends to the GSM module, as well as the responses received.

#include <TinyGsmClient.h> //  Purpose:  header file in the TinyGSM library, for communicating with various GSM modules. The library provides an abstraction layer that simplifies the process of sending AT commands.
#include <SPI.h>           //  Purpose:  header file for the SPI (Serial Peripheral Interface) library in Arduino for communicating to SD cards. SPI devices etc
#include <Ticker.h>        //  Purpose:  header file for the Ticker library, which is used in Arduino and ESP8266/ESP32 platforms to perform periodic tasks at specified intervals without using delay functions

#include <esp_sleep.h>
#include "waterpal_config.h"

#ifdef DUMP_AT_COMMANDS                    //  NOTE: If enabled it requires the streamDebugger library
#include <StreamDebugger.h>                //  Purpose:  to check data streams back and forth
StreamDebugger debugger(SerialAT, Serial); //  Serial ports monitored?
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif


// Modem communication definitions
#define UART_BAUD 9600 //  Baud rate dencreased from 115200 to 9600. This is for the SIM part only.
#define PIN_DTR 25     //
#define PIN_TX 27      //  Communication out
#define PIN_RX 26      //  Commnuncation in
#define PWR_PIN 4      //  Power on pin (needs to go high)

// All RTC_DATA_ATTR variables are persisted while the device is in deep sleep
volatile RTC_DATA_ATTR int64_t bootCount = 0;
volatile RTC_DATA_ATTR int64_t total_water_usage_time_s = 0; // Total time in seconds that the water pump has been in use
volatile RTC_DATA_ATTR int64_t last_rising_edge_time_s = 0;  // Seconds since epoch of the last rising edge

volatile RTC_DATA_ATTR int64_t total_sms_send_count = 0; // Total number of SMS messages sent

volatile RTC_DATA_ATTR int64_t last_sms_send_time_s = 0;         // Seconds since epoch of the last SMS send time
volatile RTC_DATA_ATTR int64_t last_extra_sensor_read_time_s = 0;// Seconds since epoch of the last extra sensor read


// How frequently do we want to log from peripheral sensors? (temp, humidity, etc)
#define EXTRA_SENSOR_READ_INTERVAL ((24l * 60l * 60l) / NUM_EXTRA_SENSOR_READS_PER_DAY)

// Array to store the last read values from the extra sensors
volatile RTC_DATA_ATTR float extra_sensor_values[NUM_EXTRA_SENSORS * NUM_EXTRA_SENSOR_READS_PER_DAY];
volatile RTC_DATA_ATTR int extra_sensor_read_count = 0;
volatile RTC_DATA_ATTR int64_t last_time_drift_val_s = 0; // Time drift in seconds at the last check

// The current value of the input pin
int input_pin_value = 0;

//  GPS lat/lon
float lat;
float lon;

char sms_buffer[256];

#include "waterpal_error_logging.h"
#include "waterpal_modem.h"
#include "waterpal_sensors.h"
#include "waterpal_clock.h"

// Function prototypes
void doTimeChecks();
void doDeepSleep(time_t nextWakeTime);
//void doFirstTimeInitialization();
void doLogRisingEdge();
void doLogFallingEdge();
void doSendSMS();
void printLocalTime();

void print_extra_sensor_vals();
float get_extra_sensor_min(int sensor_index);
float get_extra_sensor_max(int sensor_index);
float get_extra_sensor_avg(int sensor_index);

#define GET_LOCALTIME_NOW struct timeval tv; gettimeofday(&tv, NULL); time_t now = tv.tv_sec; struct tm timeinfo; localtime_r(&now, &timeinfo);

// Part 1 is choosing what triggered the wakeup cycle, then drop to specific case
void setup()
{
  // Configure the input pin with a pulldown resistor
  pinMode(WATERPAL_FLOAT_SWITCH_INPUT_PIN, INPUT_PULLUP); // Steve - Aug 7 - pullup two x 10k resistor added, which also drains while switch is closed.

  bootCount++;

  Serial.begin(115200); // Serial port baud rate

  if (bootCount == 1)
  {
    // If this is the first time booting up, then we need to wait a bit for the serial port to initialize
    delay(1000);
  }

  Serial.println("setup()");
  Serial.println("  Reading from pin " + String(WATERPAL_FLOAT_SWITCH_INPUT_PIN));
  Serial.println("  Boot number: " + String(bootCount));

  // Read from inputPin X times and debounce by taking the majority reading from X readings with a 50ms delay in between each read.
  int totalReading = 0; // start with no count
  int NUM_READS = 5;    // count X times, should always be an odd number.

  for (int cnt = 0; cnt < NUM_READS; cnt++)
  {
    totalReading += digitalRead(WATERPAL_FLOAT_SWITCH_INPUT_PIN); // Accumulate readings from the input pin
    delay(50);                              // Delay for a bit between each read.
  }

  // Democratically vote for the value of the input pin.
  //  If we have a majority of readings that are HIGH, then set the input pin value to HIGH. If we have a majority of readings that are LOW, then set the input pin value to LOW.
  input_pin_value = (totalReading * 2 > NUM_READS);

  Serial.println("  Current input pin value: " + String(input_pin_value));

  if (totalReading > 0 && totalReading < NUM_READS)
  {
    // If we have a mix of readings, then note it in the log so that we know the debounce code did something
    Serial.println("   Mixed readings detected -- debouncing code worked! Total readings: " + String(totalReading) + " out of " + String(NUM_READS));
  }

  // Check the wakeup reason
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  Serial.println("  Wakeup reason: " + String(wakeup_reason));

  // If it's powering on for the first time, then do all of our initialization
  if (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED || (total_sms_send_count % 8 == 0))
  {
    bool doSetNetworkMode = (bootCount == 1);
    doExtendedSelfCheck(doSetNetworkMode);
  }
  else if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0)
  {
    // If it's waking up from deep sleep due to an edge on the water sensor input, determine which edge it is.
    if (input_pin_value == HIGH)
    {
      // If it's waking up from deep sleep due to a rising edge on the water sensor input pin, then log the current time of the rising edge, and go back into deep sleep.
      doLogRisingEdge();
    }
    else
    {
      // If it's waking up from deep sleep due to a falling edge on the water sensor input pin, then log the current time of the falling edge, calculate the time span between the rising and falling edges, and add that time span to our day's total accumulation. Then go back into deep sleep.
      doLogFallingEdge();
    }
  }
  else if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER)
  {
    Serial.println("  Waking up from timer");
  }

  // Even if we woke up for a rising edge, we should still check to see if it's time to send an SMS.

  // No matter how we woke up, always go back to sleep at the end.
  doTimeChecks();
}


// We only do an extended self-check every X boots, to save power.
void doExtendedSelfCheck(bool doSetNetworkMode = false)
{
  Serial.println("doExtendedSelfCheck()");
  printLocalTime(); // NOTE: This should print an uninitialized time (1970) until we set the time from the cell tower.

  // Turn on the modem
  Serial.println("Powering on cell modem...");
  bool init_success = modem_on();

  // Update system time and check for drift
  Serial.println("\n---Getting Tower Timestamp---\n");
  modem_setLocalTimeFromCCLK(); //  Set local time from CCLK

  // Now get the local time again and print it as a check
  printLocalTime();

  // Optional: Set Network Mode
  if (doSetNetworkMode)
  {
    Serial.println("Setting network mode...");
    // 2 Automatic
    // 13 GSM only
    // 38 LTE only
    // 51 GSM and LTE only
    modem.setNetworkMode(WATERPAL_NETWORK_MODE);
  }

  // Check IMEI
  String imei_base64 = modem_get_IMEI_base64();

  Serial.print("IMEI (base64 encoded): " + imei_base64 + "\n");
  int64_t imei = _base64_to_int64(imei_base64);
  Serial.print("IMEI (decoded): " + String(imei) + "\n");

  gpsInfo gps_data;

  // Check GPS (optional)
  #if WATERPAL_USE_GPS

  // Turn GPS on
  bool gps_res = modem_gps_on();

  // Get GPS data, with a timeout of 60 seconds.
  if (modem_get_gps(gps_data, 60))
  {
    Serial.printf("GPS Lat:%f Lon:%f\n", gps_data.lat, gps_data.lon);
    Serial.printf("GPS Time: %d/%d/%d %d:%d:%d\n", gps_data.year, gps_data.month, gps_data.day, gps_data.hour, gps_data.minute, gps_data.second);
  }
  else
  {
    logError(ERROR_GPS_FAIL); // , "Failed to get GPS data");
  }

  // Turn GPS off
  gps_res = modem_gps_off();

  #endif // WATERPAL_USE_GPS

  // Get Cell Tower Info
  String cpsi = modem_get_cpsi();
  if (cpsi.length() == 0)
  {
    logError(ERROR_MODEM_FAIL); //, "Failed to get cell tower info");
  } else {
    Serial.println("Cell Tower Info: " + cpsi);
  }

  // Send extended info SMS
  Serial.println("Sending extended info SMS...");

  // Format of the extended data SMS message
  // Header: "1,IMEI,sms_count,X"
  // Body: "lat (6 decimals),lon (6 decimals),cpsi"
  // NOTE: Accuracy of the lat/lon is 6 decimal places, which is about 0.11 meters.
  // Reference for accuracy: https://gis.stackexchange.com/questions/8650/measuring-accuracy-of-latitude-and-longitude
  //                         https://en.wikipedia.org/wiki/Decimal_degrees#Precision

  // Format the message
  snprintf(sms_buffer, sizeof(sms_buffer),
           "1,%s,%lld,X,%.6f,%.6f,%s",
           // Header:
             // Version (1)
             imei_base64.c_str(),
             total_sms_send_count,
             // Packet type (X)
           // Body:
             gps_data.lat,
             gps_data.lon,
             cpsi.c_str());

  // Send the SMS
  bool sms_res = modem_broadcast_sms(sms_buffer, 30);

  if (sms_res)
  {
    Serial.println("Extended data SMS sent successfully");
    total_sms_send_count++;
  }
  else
  {
    logError(ERROR_SMS_FAIL); // , "Failed to send SMS message");
  }

}

void doLogRisingEdge()
{
  Serial.println("doLogRisingEdge()");

  // Log the current time of the rising edge, and go back into deep sleep.
  GET_LOCALTIME_NOW; // populate now and timeinfo

  last_rising_edge_time_s = now;

  Serial.println("  Rising edge detected at " + String(timeinfo.tm_hour) + ":" + String(timeinfo.tm_min) + ":" + String(timeinfo.tm_sec));
}

void doLogFallingEdge()
{
  Serial.println("doLogFallingEdge()");
  // Log the current time of the falling edge, calculate the time span between the rising and falling edges, and add that time span to our day's total accumulation. Then go back into deep sleep.
  GET_LOCALTIME_NOW; // populate now and timeinfo

  if (last_rising_edge_time_s == 0)
  { //  only zero if falling edge
    Serial.println(">>> WARNING: No rising edge time logged! Invalid reading (noise in the line, or switch triggered too quickly?)");
  }
  else
  {
    time_t then = last_rising_edge_time_s; // get start time
    struct tm then_timeinfo;
    localtime_r(&then, &then_timeinfo); // turn into hr, min
    Serial.println("  Rising edge was detected at " + String(then_timeinfo.tm_hour) + ":" + String(then_timeinfo.tm_min) + ":" + String(then_timeinfo.tm_sec));

    int64_t time_diff_s = (tv.tv_sec - last_rising_edge_time_s); //  subtract start from now elapsed

    //  Check for negative time differences (noise in the line, or switch triggered too quickly?)
    if (time_diff_s < 0)
    {
      Serial.println(">>> WARNING: Negative time difference detected -- invalid reading (noise in the line, or switch triggered too quickly?)");
      Serial.println("  Time difference: " + String(time_diff_s) + " seconds");
      // Set the time difference to zero to "ratchet" so that we can't actually count downwards.
      time_diff_s = 0;
    }

    total_water_usage_time_s += time_diff_s;

    // Reset our last rising edge time to zero
    last_rising_edge_time_s = 0; //  reset clock to zero

    Serial.println("  Falling edge detected at " + String(timeinfo.tm_hour) + ":" + String(timeinfo.tm_min) + ":" + String(timeinfo.tm_sec));
    Serial.println("  Time difference: " + String(time_diff_s) + " seconds");
    Serial.println("  Total water usage time: " + String(total_water_usage_time_s) + " seconds");
  }
}

void doSendSMS()
{
  Serial.println("doSendSMS()");
  // Send a text message (to a configured number) with the the previous day's cumulative water usage time. If the message is sent successfully, then clear the cumulative amount and go back to sleep for another 24 hrs.
  GET_LOCALTIME_NOW; // populate now and timeinfo

  // Power up the modem (take it out of airplane / low-power mode, or whatever's needed)
  bool init_success = modem_on();

  // Get our modem identification
  String imei_base64 = modem_get_IMEI_base64();

  // Get the battery level
  Serial.println("Reading battery level...");
  batteryInfo batt_val = modem_get_batt_val(); //  Get battery value from modem
  Serial.println( "Battery level: charge status: " + String(batt_val.charging) + " percentage: " + String(batt_val.percentage) + " mV: " + String(batt_val.voltage_mV));

  // Get the signal quality
  int8_t signal_quality = modem_get_signal_quality();
  Serial.println("Signal quality: " + String(signal_quality) + "%");

  Serial.println("  >> Sending SMS with water usage time: " + String(total_water_usage_time_s) + " seconds");
  Serial.println("  >> SMS sent at " + String(timeinfo.tm_hour) + ":" + String(timeinfo.tm_min) + ":" + String(timeinfo.tm_sec));

  // Calculate extra sensor values
  print_extra_sensor_vals();
  float humidity_min = get_extra_sensor_min(0);
  float humidity_max = get_extra_sensor_max(0);
  float humidity_avg = get_extra_sensor_avg(0);
  float temp_min = get_extra_sensor_min(1);
  float temp_max = get_extra_sensor_max(1);
  float temp_avg = get_extra_sensor_avg(1);

  Serial.println("  Calculated extra sensor values:");
  Serial.println("    Humidity: Min: " + String(humidity_min, 2) + " - Avg: " + String(humidity_avg, 2) + " - Max: " + String(humidity_max, 2));
  Serial.println("    Temperature: Min: " + String(temp_min, 2) + " - Avg: " + String(temp_avg, 2) + " - Max: " + String(temp_max, 2));

  // Confirm that it sent correctly, and if so, clear the total water usage time.
  bool success = false;

  snprintf(sms_buffer, sizeof(sms_buffer), "1,%s,%lld,R,%lld,%lld,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
           // Header:
             // Version (1)
             imei_base64.c_str(),
             total_sms_send_count,
             // Packet type (R)
           // Body
             total_water_usage_time_s, // Total water usage time (s)
             last_time_drift_val_s, // Time drift (s)
             int(get_extra_sensor_min(1) + 0.5f), // Temperature C (Low)
             int(get_extra_sensor_avg(1) + 0.5f), // Temperature C (Avg)
             int(get_extra_sensor_max(1) + 0.5f), // Temperature C (High)
             int(get_extra_sensor_min(0) + 0.5f), // Humidity (Low)
             int(get_extra_sensor_avg(0) + 0.5f), // Humidity (Avg)
             int(get_extra_sensor_max(0) + 0.5f), // Humidity (High)
             signal_quality, // Signal Strength Pct
             batt_val.charging, // Battery Charge Status
             batt_val.percentage, // Battery Charge
             batt_val.voltage_mV, // Battery Voltage (mV)
             bootCount, // Boot Count
             0); // Flutter Count (TODO)

  // Send the SMS
  success = modem_broadcast_sms(sms_buffer, 30);

  if (success)
  {
    Serial.println("SMS sent successfully");
    // Clear the total water usage time
    total_water_usage_time_s = 0;
    // Clear our extra sensor data
    for (int i = 0; i < NUM_EXTRA_SENSORS * NUM_EXTRA_SENSOR_READS_PER_DAY; i++)
    {
      extra_sensor_values[i] = 0;
    }
    extra_sensor_read_count = 0;

    // Save our last send time
    last_sms_send_time_s = tv.tv_sec;
    total_sms_send_count++;
  }
  else
  {
    Serial.println("Regular SMS failed to send");
    logError(ERROR_SMS_FAIL); // , "SMS failed to send");
  }

}

void doReadExtraSensors() {
  // Read the extra sensors here (such as temperature, humidity, etc)
  sensors_setup();

  // Read humidity
  float humidity = sensors_read_humidity();
  // Read temperature as Celsius (the default)
  float temp_c = sensors_read_temp_c();

  // Print sensor readings (to 2 decimal places)
  Serial.println("   > Sensor reading: " + String(extra_sensor_read_count) + ":  Humidity: " + String(humidity, 2) + "%, Temp: " + String(temp_c, 2) + "°C");

  // Store the sensor values in the array
  extra_sensor_values[extra_sensor_read_count * NUM_EXTRA_SENSORS] = humidity;
  extra_sensor_values[extra_sensor_read_count * NUM_EXTRA_SENSORS + 1] = temp_c;

  extra_sensor_read_count++;
}

void print_extra_sensor_vals()
{
  Serial.println(" **> Debug extra sensor values (" + String(extra_sensor_read_count) + " readings):");
  for (int i = 0; i < extra_sensor_read_count; i++)
  {
    Serial.println("  Reading " + String(i) + ": Humidity: " + String(extra_sensor_values[i * NUM_EXTRA_SENSORS], 2) + "%, Temp: " + String(extra_sensor_values[i * NUM_EXTRA_SENSORS + 1], 2) + " °C");
  }
}

float get_extra_sensor_min(int sensor_index)
{
  float min_val = extra_sensor_values[sensor_index];
  for (int i = 0; i < extra_sensor_read_count; i++)
  {
    if (extra_sensor_values[i * NUM_EXTRA_SENSORS + sensor_index] < min_val)
    {
      min_val = extra_sensor_values[i * NUM_EXTRA_SENSORS + sensor_index];
    }
  }
  return min_val;
}

float get_extra_sensor_max(int sensor_index)
{
  float max_val = extra_sensor_values[sensor_index];
  for (int i = 0; i < extra_sensor_read_count; i++)
  {
    if (extra_sensor_values[i * NUM_EXTRA_SENSORS + sensor_index] > max_val)
    {
      max_val = extra_sensor_values[i * NUM_EXTRA_SENSORS + sensor_index];
    }
  }
  return max_val;
}

float get_extra_sensor_avg(int sensor_index)
{
  if (extra_sensor_read_count == 0)
  {
    return 0;
  }

  float sum = 0;
  for (int i = 0; i < extra_sensor_read_count; i++)
  {
    sum += extra_sensor_values[i * NUM_EXTRA_SENSORS + sensor_index];
  }
  return sum / extra_sensor_read_count;
}

// doTimeChecks() takes care of all time-based housekeeping tasks, such as reading the extra sensors, sending SMS messages, and going back to sleep.
void doTimeChecks() {
  // Get the current system time
  GET_LOCALTIME_NOW; // populate now and timeinfo

  Serial.println("doTimeChecks()");
  Serial.println("  Current time of day: " + String(timeinfo.tm_hour) + ":" + String(timeinfo.tm_min) + ":" + String(timeinfo.tm_sec) + " (" + String(now) + ")");

  struct tm midnight = timeinfo;
  midnight.tm_hour = 0;
  midnight.tm_min = 0;
  midnight.tm_sec = 0;
  // What is the epoch time of the previous midnight?
  time_t prev_midnight = mktime(&midnight);

  Serial.println("  Previous midnight: " + String(prev_midnight));

  if (now < prev_midnight) {
    Serial.println("**** Time is before midnight -- something is wrong!");
    prev_midnight -= 86400; // Subtract one day in seconds
  }

  // What is our current time since midnight?
  long seconds_since_midnight = now - prev_midnight;

  Serial.println("  Seconds since midnight: " + String(seconds_since_midnight));

  // When was the last time that we should have read the sensors?
  time_t prev_scheduled_sensor_read_time = prev_midnight + long(seconds_since_midnight / EXTRA_SENSOR_READ_INTERVAL) * EXTRA_SENSOR_READ_INTERVAL;

  Serial.println("  Previous scheduled sensor read time: " + String(prev_scheduled_sensor_read_time));

  // If the previous sensor read time is newer than the last time we read the sensors, then we should read the sensors now.
  // TODO: Add a grace period here, so that if we're within X minutes of the target time, then do the send / read anyways?
  if (prev_scheduled_sensor_read_time > last_extra_sensor_read_time_s)
  {
    Serial.println("    !Time to read extra sensors!");
    doReadExtraSensors();

    last_extra_sensor_read_time_s = now;

    print_extra_sensor_vals();
    float humidity_min = get_extra_sensor_min(0);
    float humidity_max = get_extra_sensor_max(0);
    float humidity_avg = get_extra_sensor_avg(0);
    float temp_min = get_extra_sensor_min(1);
    float temp_max = get_extra_sensor_max(1);
    float temp_avg = get_extra_sensor_avg(1);

    Serial.println("  Calculated extra sensor values:");
    Serial.println("    Humidity: Min: " + String(humidity_min, 2) + " - Avg: " + String(humidity_avg, 2) + " - Max: " + String(humidity_max, 2));
    Serial.println("    Temperature: Min: " + String(temp_min, 2) + " - Avg: " + String(temp_avg, 2) + " - Max: " + String(temp_max, 2));

  }

  // When was the previous time that we should have sent an SMS today?
  time_t prev_scheduled_sms_send_time = prev_midnight + long(seconds_since_midnight / SMS_DAILY_SEND_INTERVAL) * SMS_DAILY_SEND_INTERVAL;

  Serial.println("  Previous scheduled SMS send time: " + String(prev_scheduled_sms_send_time));

  // If the previous due SMS send time is newer than the last time we sent an SMS, then we should send an SMS now.
  // TODO: Add a grace period here, so that if we're within X minutes of the target time, then do the send / read anyways?
  if (prev_scheduled_sms_send_time > last_sms_send_time_s)
  {
    // Send our SMS and clear our accumulated data readings
    doSendSMS();
  }

  time_t next_scheduled_sms_send_time = prev_scheduled_sms_send_time + SMS_DAILY_SEND_INTERVAL;
  time_t next_scheduled_sensor_read_time = prev_scheduled_sensor_read_time + EXTRA_SENSOR_READ_INTERVAL;

  Serial.println("  Next scheduled sensor read time: " + String(next_scheduled_sensor_read_time) + " (delta: " + String(next_scheduled_sensor_read_time - now) + ")");
  Serial.println("  Next scheduled SMS send time: " + String(next_scheduled_sms_send_time) + " (delta: " + String(next_scheduled_sms_send_time - now) + ")");

  // Figure out which of the two times is closer, and set the next wake up time to be that time.
  time_t next_wake_time = next_scheduled_sms_send_time;
  if (next_scheduled_sensor_read_time < next_scheduled_sms_send_time)
  {
    Serial.println("   Next wake time is the sensor read time");
    next_wake_time = next_scheduled_sensor_read_time;
  } else {
    Serial.println("   Next wake time is the SMS send time");
  }

  Serial.println("  Next wake time: " + String(next_wake_time) + " (delta: " + String(next_wake_time - now) + ")");

  doDeepSleep(next_wake_time);
}

// This function takes care of all housekeeping needed to go to deep sleep and save our battery.
void doDeepSleep(time_t nextWakeTime)
{
  Serial.println("doDeepSleep()");
  // Calculate the amount of time remaining until our target SMS send time (10pm)
  // Set wake conditions of the device to be either the target SMS send time or a rising edge on the water sensor input pin -- whichever comes first.

  // Calculate the time until the next wakeup time. Get current RTC time via gettimeofday()
  GET_LOCALTIME_NOW; // populate `now` and `timeinfo`

  // Shut off the modem (TODO: Perhaps only put it into sleep / low-power mode?)
  modem_off();

  // Calculate the time until the next wakeup
  time_t seconds_until_wakeup = nextWakeTime - now;

  Serial.println("  Current time of day: " + String(timeinfo.tm_hour) + ":" + String(timeinfo.tm_min) + ":" + String(timeinfo.tm_sec) + " (" + String(now) + ")");
  Serial.println("  Next wake time: " + String(nextWakeTime));

  Serial.println("  Seconds until next wake up: " + String(seconds_until_wakeup));

  int triggerOnEdge = 1; // Default to triggering on a rising edge.

  // If we're currently tracking a rising edge, then configure to trigger on a falling edge instead.
  if (last_rising_edge_time_s > 0)
  {
    triggerOnEdge = 0;
    Serial.println("  Configuring trigger for falling edge");
  }
  else
  {
    Serial.println("  Configuring trigger for rising edge");
  }

  // Configure the deep sleep wakeup
  esp_sleep_enable_ext0_wakeup(WATERPAL_FLOAT_SWITCH_INPUT_PIN, triggerOnEdge);

  //  Configure the deep sleep timer
  esp_sleep_enable_timer_wakeup(seconds_until_wakeup * 1000000ull);

  // Log some information for debugging purposes:
  Serial.println("  Total water usage time: " + String(total_water_usage_time_s) + " seconds");

  // Go to sleep
  Serial.println("  Going to sleep now for " + String(seconds_until_wakeup) + " seconds until next scheduled wake up");
  esp_deep_sleep_start();
}

void printLocalTime(){
  GET_LOCALTIME_NOW; // populate `now` and `timeinfo`

  Serial.print(">> Current system time: ");
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  Serial.println();
}

void loop()
{
  // Our code shouldn't ever get here, but if we do, then go immediately into deep sleep.
  Serial.println("loop() -- SHOULD NOT BE HERE");
  doTimeChecks();
}
