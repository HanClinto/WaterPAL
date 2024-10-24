# WaterPAL

**Water Pump Activity Logger**

Low cost SMS-based water pump activity logger, built on the ESP32 platform.

# Hardware Platform

Lilygo SIM7000G

# Program Description

A water pump monitoring system that sends daily water usage statistics via SMS.

The system is designed to be powered by a solar panel and a battery, and to be able to operate in a low-power mode for extended periods of time.

The system is designed to be able to send an SMS message with the previous day's water usage statistics to a configured phone number.

The system is designed to be able to detect the rising and falling edges of a water sensor input pin, and to be able to calculate the time span between the rising and falling edges.

The system is designed to be able to accumulate the time spans between the rising and falling edges of the water sensor input pin, and to be able to send an SMS message with the accumulated time spans to a configured phone number.

The system is designed to be able to operate in a low-power mode for extended periods of time, and to be able to wake up from low-power mode to send an SMS message with the accumulated time spans to a configured phone number.

# Program Outline

On startup, check the wakeup reason.

* If it's powering on for the first time, then do all of our initialization
  * Ensure that we can communicate with the cell modem, and that it can connect with the tower
  * If it can connect with the tower, then set the internal system clock according to the cell tower's information.
  * Send an SMS with a "powered on" message including our MAC address, GPS, battery voltage, etc.
  * DoDeepSleep()

* If it's waking up from deep sleep due to our SMS send timer going off, then send a text message (to a configured number) with the the previous day's cumulative water usage time. If the message is sent successfully, then clear the cumulative amount and go back to sleep for another 24 hrs.

* If it's waking up from deep sleep due to a rising edge on the water sensor input pin, then log the current time of the rising edge, and go back into deep sleep.std

* If it's waking up from deep sleep due to a falling edge on the water sensor input pin, then log the current time of the falling edge, calculate the time span between the rising and falling edges, and add that time span to our day's total accumulation. Then go back into deep sleep.

* DoDeepSleep()
  * Calculate the amount of time remaining until our target SMS send time (10pm)
  * Set wake conditions of the device to be either the target SMS send time or a rising edge on the water sensor input pin -- whichever comes first.

## Ongoing Tasks / Problems

Problem: How to configure the devices at runtime for their configuration (update schedule, or even which cell phone number to text to, since we may not want to do international texting depending on the cell plan of the SIM cards that we purchase).
Possible solution: These devices have an SD Card slot on them. We could compile the application with a default configuration, but we could also support plugging in an SD card that has a small text file with configuration parameters (local SMS target number, GPS on/off, temp/humidity on/off, send frequency, etc) and on first-time boot, the device can check to see if an SD card is present, and read the text file from the SD card (if it's present) and override the default parameters if needed.

[ ] Task: Need to add support for logging temperature / humidity data on regular intervals

[ ] Task: Need to add support for waking up at regular intervals to log extra data (temp/humidity) as well as send SMS on regular intervals (at least daily, but possibly hourly or even faster for debug purposes).

[ ] Task: Need to build the IoT message receiver to collect SMS messages and collate data.

[ ] Task: Integrate with WhatsApp for business if possible.

[ ] Task: Would be good to have a unique QR code printed on each device that will let people scan it and view a webpage with usage data for this particular pump, as well as send feedback / report problems / ask questions about it.

[ ] Task: Need to add support for GPS data logging and sending.

[ ] Task: Need to add support for battery voltage monitoring and sending.

[ ] Task: Need to add support for solar panel voltage monitoring and sending.

[ ] Task: Need to add support for solar panel current monitoring and sending.

[ ] Task: Audit power consumption of the device and see if we can reduce it further.

[ ] Task: Audit time / profile functions and ensure that we don't have unnecessary sleeps or delays in the code.

[ ] Cleanup: Clean up debug prints and ensure that we have a good logging system in place.

[ ] Task: Save detailed log information to SD card (if present) -- part of above task to improve the logging system overall.

[ ] Task: Obtain a unique ID for each device without having to individually program it. Can we get the MAC address of a device without knowing the SIM network information, or possibly some other unique ID?

[ ] Task: How to tie a unique QR code to each device?

[ ] Task: Device should be resilient to losing power and going through first-time initialization many times.

[ ] Idea: Perhaps use an SD card as the trigger for whether or not a device is in debug-mode.

[ ] Task: Add support for multiple phone numbers

[ ] Task: Possibly read phone number from file saved to SDCard?
