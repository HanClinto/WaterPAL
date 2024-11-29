// Extra sensor functions for reading and storing data

#ifndef WATERPAL_SENSORS_H
#define WATERPAL_SENSORS_H

// Extra Sensors: DHT11 / DHT22
#include "DHT.h"
#define DHTPIN 32      // Digital pin connected to the DHT sensor

// NOTE: Uncomment the correct line for the DHT sensor you are using.
#define DHTTYPE DHT11  // DHT 11
//#define DHTTYPE DHT22  // DHT 22  (AM2302), AM2321
//#define DHTTYPE DHT21  // DHT 21 (AM2301)

DHT dht(DHTPIN, DHTTYPE);

void sensors_setup()
{
    dht.begin();
}

float sensors_read_humidity()
{
    return dht.readHumidity();
}

float sensors_read_temp_c()
{
    return dht.readTemperature();
}

#endif // WATERPAL_SENSORS_H