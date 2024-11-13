// WaterPAL - Water Pump Activity Logger
// Copyright (c) 2024 Clint Herron and Stephen Peacock
// MIT License

#define DEST_PHONE_NUMBER "+1987654321" // Update phone numbers here.

#define TINY_GSM_MODEM_SIM7000  //  Purpose:  inform the TinyGSM library which GSM module you are using. This allows the library to tailor its operations, such as AT commands and responses
#define TINY_GSM_RX_BUFFER 1024 //  Purpose:  determines how much data can be stored temporarily while it is being received from the GSM module.

#define TINY_GSM_TEST_GPS false //  Use true or false to include or exclude GPS search
#define GSM_PIN ""              //  NOTE: not sure if needed or not

#define SerialAT Serial1 //  Purpose:  defined and initialized with specific RX and TX pins to communicate with the GSM module.
#define DUMP_AT_COMMANDS //  Purpose:  you can see every AT command that the TinyGSM library sends to the GSM module, as well as the responses received.

#include <TinyGsmClient.h> //  Purpose:  header file in the TinyGSM library, for communicating with various GSM modules. The library provides an abstraction layer that simplifies the process of sending AT commands.
#include <SPI.h>           //  Purpose:  a header file for the SPI (Serial Peripheral Interface) library in Arduino for communicating to SD cards. SPI devices etc
#include <Ticker.h>        //  Purpose:   header file for the Ticker library, which is used in Arduino and ESP8266/ESP32 platforms to perform periodic tasks at specified intervals without using delay functions

#include <esp_sleep.h>

#ifdef DUMP_AT_COMMANDS                    //  NOTE: If enabled it requires the streamDebugger library
#include <StreamDebugger.h>                //  Purpose:  to check data streams back and forth
StreamDebugger debugger(SerialAT, Serial); //  Serial ports monitored?
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

/*
Note: you can only use pins that are RTC GPIOs with this wake-up source. Here’s a list of the RTC GPIOs for different ESP32 chip models:
    ESP32-S3: 0-21;
    ESP32: 0, 2, 4, 12-15, 25-27, 32-39;
    ESP32-S2: 0-21;
*/
#define INPUT_PIN GPIO_NUM_14

// How frequently do we want to send an SMS message?
//#define SMS_DAILY_SEND_INTERVAL 22 * (60 * 60) // 22 hours in seconds
// Set to 1 hour for testing purposes
// #define SMS_DAILY_SEND_INTERVAL 1 * (60 * 60) // 1 hour in seconds
// Set to 5 minutes for testing purposes
 #define SMS_DAILY_SEND_INTERVAL (5 * 60l) // 5 minutes in seconds

// Modem communication definitions
#define UART_BAUD 9600 //  Baud rate dencreased from 115200 to 9600. This is for the SIM part only.
#define PIN_DTR 25     //
#define PIN_TX 27      //  Communication out
#define PIN_RX 26      //  Commnuncation in
#define PWR_PIN 4      //  Power on pin (needs to go high)

// All RTC_DATA_ATTR variables are persisted while the device is in deep sleep
volatile RTC_DATA_ATTR int bootCount = 0;
volatile RTC_DATA_ATTR int64_t total_water_usage_time_s = 0; // Total time in seconds that the water pump has been in use
volatile RTC_DATA_ATTR int64_t last_rising_edge_time_s = 0;  // Seconds since epoch of the last rising edge

volatile RTC_DATA_ATTR int64_t last_sms_send_time_s = 0;         // Seconds since epoch of the last SMS send time
volatile RTC_DATA_ATTR int64_t last_extra_sensor_read_time_s = 0;// Seconds since epoch of the last extra sensor read

volatile RTC_DATA_ATTR int last_batt_val_charging;
volatile RTC_DATA_ATTR int last_batt_val_percentage;
volatile RTC_DATA_ATTR int last_batt_val_voltage_mV;

#define NUM_EXTRA_SENSORS 2
#define NUM_EXTRA_SENSOR_READS_PER_DAY 24

// How frequently do we want to log from peripheral sensors? (temp, humidity, etc)
#define EXTRA_SENSOR_READ_INTERVAL ((24 * 60 * 60l) / NUM_EXTRA_SENSOR_READS_PER_DAY)

// Array to store the last read values from the extra sensors
volatile RTC_DATA_ATTR float extra_sensor_values[NUM_EXTRA_SENSORS * NUM_EXTRA_SENSOR_READS_PER_DAY];

volatile RTC_DATA_ATTR int64_t last_time_drift_val_s = 0; // Time drift in seconds at the last check

// Extra Sensors: DHT11 / DHT22
#include "DHT.h"
#define DHTPIN 32      // Digital pin connected to the DHT sensor

// NOTE: Uncomment the correct line for the DHT sensor you are using.
#define DHTTYPE DHT11  // DHT 11
//#define DHTTYPE DHT22  // DHT 22  (AM2302), AM2321
//#define DHTTYPE DHT21  // DHT 21 (AM2301)

DHT dht(DHTPIN, DHTTYPE);

// The current value of the input pin
int input_pin_value = 0;

//  September 19 - add GPS float for lat/lon
float lat;
float lon;

#include "waterpal_error_logging.h"
#include "waterpal_modem.h"

// Function prototypes
void doTimeChecks();
void doDeepSleep(time_t nextWakeTime);
void doFirstTimeInitialization();
void doLogRisingEdge();
void doLogFallingEdge();
void doSendSMS();

#define GET_LOCALTIME_NOW struct timeval tv; gettimeofday(&tv, NULL); time_t now = tv.tv_sec; struct tm timeinfo; localtime_r(&now, &timeinfo);

// Part 1 is choosing what triggered the wakeup cycle, then drop to specific case
void setup()
{
  // Configure the input pin with a pulldown resistor
  pinMode(INPUT_PIN, INPUT); // Steve - Aug 7 - pullup two x 10k resistor added, which also drains while switch is closed.
  // TODO: Audit potentially unnecessary delays
  delay(100);                // Steve - Aug 7 - added 1/10 second delay

  bootCount++;

  Serial.begin(115200); // Serial port baud rate

  // TODO: Audit potentially unnecessary delays
  delay(1000);

  Serial.println("setup()");
  Serial.println("  Reading from pin " + String(INPUT_PIN));
  Serial.println("  Boot number: " + String(bootCount));

  // Read from inputPin X times and debounce by taking the majority reading from X readings with a 50ms delay in between each read.
  int totalReading = 0; // start with no count
  int NUM_READS = 5;    // count X times, should always be an odd number.

  for (int cnt = 0; cnt < NUM_READS; cnt++)
  {
    totalReading += digitalRead(INPUT_PIN); // Accumulate readings from the input pin
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
  if (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED)
  {
    doFirstTimeInitialization();
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

void printLocalTime(){
  GET_LOCALTIME_NOW; // populate `now` and `timeinfo`

  Serial.print(">> Current system time: ");
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  Serial.println();
}

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

int8_t setLocalTimeFromCCLK() {
  // Send:
    // AT+CCLK?
  // Receive
    // -> +CCLK: "24/10/08,23:39:49-16"
    // ->
    // -> OK

  // TODO: Read the local time, then calculate and log the drift if we are in debug mode.

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

// TODO: Many of the modem functions are growing large enough that we can probably break them out into a supplementary source file soon, so that they don't continue to clutter up the rest of the program flow.
void doFirstTimeInitialization()
{
  Serial.println("doFirstTimeInitialization()");
  printLocalTime(); // NOTE: This should print an uninitialized time (1970) until we set the time from the cell tower.

  //delay(1000); // TODO: Unneeded?
  // Ensure that we can communicate with the cell modem, and that it can connect with the tower
  // If it can connect with the tower, then set the internal RTC according to the cell tower's information so that we have correct local time.
  // Send an SMS with a "powered on" message including our GPS (if available), battery voltage, tower information, etc.

  Serial.println("Powering on cell modem...");

  // Start the cell antenna
  pinMode(PWR_PIN, OUTPUT);    // Set power pin to output needed to START modem on power pin 4
  digitalWrite(PWR_PIN, HIGH); // Set power pin high (on)
  delay(300);
  digitalWrite(PWR_PIN, LOW);  // Not sure why it gets toggled off, but all documentation says to do this routine for powerup.

  SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX); // Set conditions for serial port to read and write

  Serial.println("Enabling SMS message indications.");

  // TODO: Is +CNMI= needed? It seems like this might be already being sent by the restart code that happens later.
  modem.sendAT("+CNMI=1,2,0,0,0"); //    Enable new SMS message indications.  Buffer / storage or not?
  if (SerialAT.available())
  { // Listen for incoming SMS messages
    // TODO: I don't think we need to look for SMS messages here, since we shouldn't be able to receive them...? - CH, 2024-11-13
    String sms = SerialAT.readString();
    Serial.println("SMS Received: " + sms);
  }

  // There are two ways to initialize the modem -- restart, or simple init.
  if (true) {
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

  // RF antenna should be started by now
  Serial.println("Println: Your boot count start number is: " + String(bootCount));

  // Texts to send on initialization loop:
  // Test: modem.sendSMS(DEST_PHONE_NUMBER, String("SMS from doFirstTimeInitialization block"));  //send SMS
  modem.sendSMS(DEST_PHONE_NUMBER, "SMS: Your boot count start number is: " + String(+bootCount)); // send SMS
  if (modem.waitResponse(10000L) != 1)
  { // wait 10 seconds for tower ping
    Serial.println("Counter send failed");
  } else {
    Serial.println("Counter send successful");
  }

  Serial.println("Reading battery level...");
  batteryInfo last_batt_val = modem_get_batt_val(); //  Get battery value from modem
  last_batt_val_charging = last_batt_val.charging;
  last_batt_val_percentage = last_batt_val.percentage;
  last_batt_val_voltage_mV = last_batt_val.voltage_mV;
  Serial.println( "Battery level: charge status: " + String(last_batt_val_charging) + " percentage: " + String(last_batt_val_percentage) + " mV: " + String(last_batt_val_voltage_mV));

  modem.sendSMS(DEST_PHONE_NUMBER, "Battery level: charge status:" + String(last_batt_val_charging) + " percentage: " + String(last_batt_val_percentage) + " mV: " + String(last_batt_val_voltage_mV)); // send cell tower strength to text
  if (modem.waitResponse(10000L) != 1)
  { // ping tower for ten seconds
    DBG("Battery level send failed");
  }
  // End battery check section

  // This section checks cell tower info
  String res = modem.getIMEI();
  Serial.print("IMEI:");
  Serial.println(res);
  Serial.println();

  modem.sendAT("+CPSI?"); //  test cell provider info
  if (modem.waitResponse("+CPSI: ") == 1)
  {
    res = modem.stream.readStringUntil('\n');
    res.trim();
    modem.waitResponse();
    Serial.println(">> The current network parameters are: '" + res + "'");
  } else {
    Serial.println(">> No network parameters found");
  }

  Serial.println("Sending network parameters via SMS...");
  // Send tower info
  if (!modem.sendSMS(DEST_PHONE_NUMBER, String(res)))
  {
    DBG("Network parameter send failed");
  }
  // End tower info section

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

  modem.sendSMS(DEST_PHONE_NUMBER, String(" SMS: Your signal strength: " + String(csq) + "%"));
  if (modem.waitResponse(10000L) != 1)
  { // wait 10 seconds for tower ping
    DBG("Signal strength send failed");
  }
  //   End tower signal strength section

  // This section gathers timestamp
  Serial.println("\n---Getting Tower Timestamp---\n");
  setLocalTimeFromCCLK(); //  Set local time from CCLK

  // Now get the local time again and print it
  printLocalTime();

//  End timestamp and cell tower strength section

#if TINY_GSM_TEST_GPS
  // TODO: This section loops endlessly if we don't have a GPS antenna connected. Need to add a timeout // fallback.
  Serial.println("\n---Starting GPS TEST---\n");
  // Set Modem GPS Power Control Pin to HIGH ,turn on GPS power
  // Only in version 20200415 is there a function to control GPS power
  modem.sendAT("+CGPIO=0,48,1,1");
  if (modem.waitResponse(10000L) != 1)
  {
    DBG("Set GPS Power HIGH Failed");
  }

  Serial.println("\nEnabling GPS...\n");
  modem.enableGPS();

  float lat, lon; //  Check float has more than two decimal places in SMS?
  while (1)
  {
    if (modem.getGPS(&lat, &lon))
    {
      Serial.printf("lat:%f lon:%f\n", lat, lon);
      break;
    }
    else
    {
      Serial.print("getGPS failed. Is your antenna plugged in? Time: ");
      Serial.println(millis());
    }
    delay(2000);
  }
  modem.disableGPS();

  // Set Modem GPS Power Control Pin to LOW ,turn off GPS power
  // Only in version 20200415 is there a function to control GPS power
  modem.sendAT("+CGPIO=0,48,1,0");
  if (modem.waitResponse(10000L) != 1)
  {
    DBG("Set GPS Power LOW Failed");
  }

  //  Send GPS info to SMS target
  modem.sendSMS(DEST_PHONE_NUMBER, String(" Lat " + String(+lat))); //  Send latitude to text, but more than two decimal places?
  modem.sendSMS(DEST_PHONE_NUMBER, String(" Lon " + String(+lon))); //  Send longitude to text, but more than two decimal places?
  if (modem.waitResponse(10000L) != 1)
  {
    DBG("Lat fail");
  }

  Serial.println("\n---End of GPRS TEST---\n");
#endif // TINY_GSM_TEST_GPS

  // TODO: Needed delay?
  delay(1000); // delay 1 second before powering antenna down

  //  This section powers down RF (cell) antenna
  SerialAT.println("AT+CPOWD=1"); // see page 83

  // TODO: Needed delay?
  delay(1000);
  while (SerialAT.available())
  {
    Serial.write(SerialAT.read());
  }
  // End RF antenna power down section

  Serial.println("\n---End of doFirstTimeInitialization()---");
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

  // TODO: Power up the modem (take it out of airplane / low-power mode, or whatever's needed)

  // TODO: Send the SMS
  // TODO: Add the extra sensor data to the SMS message

  Serial.println("  >> Sending SMS with water usage time: " + String(total_water_usage_time_s) + " seconds");
  Serial.println("  >> SMS sent at " + String(timeinfo.tm_hour) + ":" + String(timeinfo.tm_min) + ":" + String(timeinfo.tm_sec));

  // TODO: Confirm that it sent correctly, and if so, clear the total water usage time.
  bool success = true;

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

    // Save our last send time
    last_sms_send_time_s = tv.tv_sec;
  }
  else
  {
    logError(ERROR_SMS_FAIL); // , "SMS failed to send");
  }

  // TODO: Power down the modem, or put it back into low-power mode
}

void doReadExtraSensors(int sensorReadIndex) {
  // Read the extra sensors here (such as temperature, humidity, etc)

  // Read humidity
  float humidity = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float temp_c = dht.readTemperature();

  // Print sensor readings (to 2 decimal places)
  Serial.println("  Humidity: " + String(humidity, 2) + "%, Temp: " + String(temp_c, 2) + "°C");

  // Store the sensor values in the array
  extra_sensor_values[sensorReadIndex * NUM_EXTRA_SENSORS] = humidity;
  extra_sensor_values[sensorReadIndex * NUM_EXTRA_SENSORS + 1] = temp_c;
}

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
    // Which index should we use to store this reading in today's array?
    int sensorReadIndex = int(seconds_since_midnight / EXTRA_SENSOR_READ_INTERVAL);

    doReadExtraSensors(sensorReadIndex);

    last_extra_sensor_read_time_s = now;
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

  Serial.println("  Next scheduled sensor read time: " + String(next_scheduled_sensor_read_time));
  Serial.println("  Next scheduled SMS send time: " + String(next_scheduled_sms_send_time));

  // Figure out which of the two times is closer, and set the next wake up time to be that time.
  time_t next_wake_time = next_scheduled_sms_send_time;
  if (next_scheduled_sensor_read_time < next_scheduled_sms_send_time)
  {
    next_wake_time = next_scheduled_sensor_read_time;
  }

  Serial.println("  Next wake time: " + String(next_wake_time));

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
  esp_sleep_enable_ext0_wakeup(INPUT_PIN, triggerOnEdge);

  //  Configure the deep sleep timer
  esp_sleep_enable_timer_wakeup(seconds_until_wakeup * 1000000ull);

  // Log some information for debugging purposes:
  Serial.println("  Total water usage time: " + String(total_water_usage_time_s) + " seconds");

  // Go to sleep
  Serial.println("  Going to sleep now for " + String(seconds_until_wakeup) + " seconds until next scheduled wake up");
  esp_deep_sleep_start();
}

void loop()
{
  // Our code shouldn't ever get here, but if we do, then go immediately into deep sleep.
  Serial.println("loop() -- SHOULD NOT BE HERE");
  doTimeChecks();
}
