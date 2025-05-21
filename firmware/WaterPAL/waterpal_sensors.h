// waterpal_sensors.h: Extra sensor functions for reading and storing data

#ifndef WATERPAL_SENSORS_H
#define WATERPAL_SENSORS_H

// Extra Sensors: DHT11 / DHT22
#include "DHT.h"
#include "waterpal_config.h"

// NOTE: Set DHT pin and type in waterpal_config.h
#define DHTPIN WATERPAL_DHTPIN
#define DHTTYPE WATERPAL_DHTTYPE

DHT dht(DHTPIN, DHTTYPE);

void sensors_setup()
{
    dht.begin();
}

float sensors_read_humidity()
{
    float humidity = dht.readHumidity();
    // Ensure it's not NaN
    if (isnan(humidity))
    {
        return 0;
    }
    return humidity;
}

float sensors_read_humidity_retry()
{
    float humidity = 0;
    for (int i = 0; i < 10; i++)
    {
        watchdog_pet();

        humidity = sensors_read_humidity();
        if (humidity > 0 && humidity <= 100)
        {
            return humidity;
        }
        Serial.println("Failed to get humidity, retrying...");
        delay(100);
    }
    return humidity;
}

float sensors_read_temp_c()
{
    float temp_c = dht.readTemperature();
    // Ensure it's not NaN
    if (isnan(temp_c))
    {
        return 0;
    }
    return temp_c;
}

float sensors_read_temp_c_retry()
{
    float temp_c = 0;
    for (int i = 0; i < 10; i++)
    {
        watchdog_pet();
        
        temp_c = sensors_read_temp_c();
        // Note that the sensor can return 0 if it fails to read the temperature, which is also a valid temperature, so we'll just re-read 10 times if it happens to be exactly freezing.
        if (temp_c != 0)
        {
            return temp_c;
        }
        Serial.println("Failed to get temperature, retrying...");
        delay(100);
    }
    return temp_c;
}

#endif // WATERPAL_SENSORS_H