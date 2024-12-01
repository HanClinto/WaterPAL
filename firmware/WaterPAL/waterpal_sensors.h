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
    return dht.readHumidity();
}

float sensors_read_temp_c()
{
    return dht.readTemperature();
}

#endif // WATERPAL_SENSORS_H