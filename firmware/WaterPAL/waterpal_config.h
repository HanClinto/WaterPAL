// waterpal_config.h: This file contains the configuration settings for the WaterPAL device.

#ifndef WATERPAL_CONFIG_H
#define WATERPAL_CONFIG_H
#include <esp_attr.h>
#include "DHT.h"

// **********
// SMS Configuration
// **********

// WATERPAL_DEST_PHONE_NUMBERS: Which phone numbers should receive SMS messages from the WaterPAL device.
//  The phone numbers must be in international format, e.g. +1 for the US.
//  You can add multiple phone numbers by separating them with commas.
const char* WATERPAL_DEST_PHONE_NUMBERS[] = {
  // TODO: Replace with your real phone numbers
  "+1987654321",
  "+1987654322"
};

#define WATERPAL_USE_GPRS true // Whether or not to use GPRS to send updates
// TODO: Replace with your APN information if you want to send updates via HTTP
const char WATERPAL_APN[] = "wholesale";
const char WATERPAL_GPRS_USER[] = "";
const char WATERPAL_GPRS_PASS[] = "";

// The max time to wait for a response from the server
// NOTE: There may be a bug with this -- need to test more: https://github.com/arduino-libraries/ArduinoHttpClient/issues/154
const uint32_t WATERPAL_HTTP_TIMEOUT_MS = 60 * 1000;

// WATERPAL_USE_GPS: Whether or not to use the GPS module to get the device's location.
//  If set to true, the device will attempt to get the GPS location and send it in an SMS message.
//  If set to false, the device will skip the GPS location step.
//  If you don't have a GPS module connected, or want to conserve power, set this to false.
#define WATERPAL_USE_GPS false

// WATERPAL_NETWORK_MODE: The network mode to use for the modem. Only set on first boot.
// 2 Automatic
// 13 GSM only
// 38 LTE only
// 51 GSM and LTE only
#define WATERPAL_NETWORK_MODE 13 // GSM only

// WATERPAL_SMS_RETRY_CNT: How many times to retry sending an SMS message
#define WATERPAL_SMS_RETRY_CNT 10

// WATERPAL_SMS_SHORT_RETRY_CNT: How many times to retry sending a short SMS message
#define WATERPAL_SMS_SHORT_RETRY_CNT 10

// How frequently do we want to send an SMS message?
// 22 hours after midnight
//#define SMS_DAILY_SEND_INTERVAL (22 * (60l * 60l)) // 22 hours in seconds
// 1 hour after midnight, and every 1 hour following. (high frequency for testing purposes)
// #define SMS_DAILY_SEND_INTERVAL (1 * (60l * 60l)) // 1 hour in seconds
// 5 minutes after midnight, and every 5 minutes following. (high frequency for testing purposes)
#define SMS_DAILY_SEND_INTERVAL (5 * 60l) // 5 minutes in seconds

// **********
// Float Sensor and Extra Sensor Configuration
// **********

// Note: When choosing input pins, make sure they are RTC GPIOs, and also that they are not used for other purposes.
//  For example, GPIO 0, 2, 12, and 15 are all used for boot mode selection, so they are not good choices for wake-up sources.
//  https://docs.espressif.com/projects/esptool/en/latest/esp32/advanced-topics/boot-mode-selection.html
/*
Note: you can only use pins that are RTC GPIOs with this wake-up source. Here’s a list of the RTC GPIOs for different ESP32 chip models:
    ESP32-S3: 0-21;
    ESP32: 0, 2, 4, 12-15, 25-27, 32-39;
    ESP32-S2: 0-21;
*/
#define WATERPAL_FLOAT_SWITCH_INPUT_PIN GPIO_NUM_34
#define WATERPAL_FLOAT_SWITCH_INVERT false // Set to true to invert the input pin value
#define WATERPAL_DHTPIN 32

#define NUM_EXTRA_SENSORS 2 // The number of extra sensors to read. Here, we have a humidity and temperature sensor.
#define NUM_EXTRA_SENSOR_READS_PER_DAY 24 // How many readings do we want to log per day? Here, we log every hour.

// NOTE: Uncomment the correct line for the DHT sensor you are using.
#define WATERPAL_DHTTYPE DHT11    // DHT 11
//#define WATERPAL_DHTTYPE DHT22  // DHT 22 (AM2302), AM2321
//#define WATERPAL_DHTTYPE DHT21  // DHT 21 (AM2301)

// **********
// T-SIM 7000g Pin Allocation
// **********

// 0 - Serial Bootloader (Boot mode selection)
// 2 - MIS0, also used for boot mode selection
// 4 - Modem (SIM7000G) POWER
// 12 - MTDI (Boot mode selection)
// 13 - CS
// 14 - SCLK
// 15 - MOSI, also MTDO (Boot mode selection)

// 22 - Wire_SCL
// 21 - Wire_SDA

// 23 - VSPI_MOSI
// 19 - VSPI_MISO
// 18 - VSPI_SCK
// 05 - VSPI_SS

// 21 - Wire SDA
// 22 - Wire SCL
// 25 - DTR
// 26 - Modem (SIM7000G) TX
// 27 - Modem (SIM7000G) RX

// 32 -
// 33 -
// 34 - 
// 35 - 


#endif // WATERPAL_CONFIG_H