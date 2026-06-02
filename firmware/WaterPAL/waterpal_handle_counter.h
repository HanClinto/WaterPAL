// waterpal_handle_counter.h: Handle stroke counter functions and aggregate accounting

#ifndef WATERPAL_HANDLE_COUNTER_H
#define WATERPAL_HANDLE_COUNTER_H

#include <Arduino.h>
#include <Wire.h>
#include "waterpal_config.h"

#if WATERPAL_USE_HANDLE_COUNTER

volatile RTC_DATA_ATTR bool handle_counter_has_reading = false;
volatile RTC_DATA_ATTR uint32_t handle_counter_last_raw = 0;

volatile RTC_DATA_ATTR uint64_t handle_strokes_total = 0;
volatile RTC_DATA_ATTR uint64_t handle_strokes_flowing_total = 0;
volatile RTC_DATA_ATTR uint64_t dry_start_stroke_total = 0;
volatile RTC_DATA_ATTR uint32_t dry_start_stroke_max = 0;
volatile RTC_DATA_ATTR uint32_t dry_start_count = 0;

volatile RTC_DATA_ATTR uint64_t dry_start_candidate_strokes = 0;
volatile RTC_DATA_ATTR bool dry_start_candidate_valid = false;
volatile RTC_DATA_ATTR int64_t last_water_flow_end_time_s = 0;
volatile RTC_DATA_ATTR uint32_t handle_counter_read_fail_count = 0;

bool handle_counter_water_is_flowing(int water_sensor_value)
{
  return water_sensor_value != WATERPAL_FLOAT_SWITCH_INVERT;
}

void handle_counter_setup()
{
  Wire.begin(WATERPAL_COUNTER_SDA_PIN, WATERPAL_COUNTER_SCL_PIN);
  Wire.setClock(WATERPAL_COUNTER_I2C_SPEED_HZ);

#if WATERPAL_COUNTER_RST_PIN >= 0
  pinMode(WATERPAL_COUNTER_RST_PIN, OUTPUT);
  digitalWrite(WATERPAL_COUNTER_RST_PIN, HIGH);
#endif

#if WATERPAL_COUNTER_LOOP_PIN >= 0
  pinMode(WATERPAL_COUNTER_LOOP_PIN, INPUT);
#endif
}

bool handle_counter_read_raw(uint32_t *count)
{
  if (count == NULL)
  {
    return false;
  }

  uint8_t bytes_read = Wire.requestFrom((uint8_t)WATERPAL_COUNTER_I2C_ADDRESS, (uint8_t)3);
  if (bytes_read != 3)
  {
    handle_counter_read_fail_count++;
    return false;
  }

  uint32_t value = ((uint32_t)Wire.read() << 16);
  value |= ((uint32_t)Wire.read() << 8);
  value |= (uint32_t)Wire.read();
  *count = value & 0xFFFFFFUL;
  return true;
}

uint32_t handle_counter_delta_24bit(uint32_t previous_count, uint32_t current_count)
{
  // Handles a single 24-bit S-35770 wrap between ESP32 wakeups. Multiple wraps between reads require LOOP tracking.
  if (current_count >= previous_count)
  {
    return current_count - previous_count;
  }

  return (WATERPAL_COUNTER_MAX_COUNT - previous_count) + current_count;
}

bool handle_counter_update(bool previous_water_was_flowing)
{
  uint32_t current_raw = 0;
  if (!handle_counter_read_raw(&current_raw))
  {
    Serial.println("Handle counter read failed");
    return false;
  }

  if (!handle_counter_has_reading)
  {
    handle_counter_last_raw = current_raw;
    handle_counter_has_reading = true;
    dry_start_candidate_strokes = 0;
    dry_start_candidate_valid = true;
    Serial.println("Handle counter initialized at raw count " + String(current_raw));
    return true;
  }

  uint32_t delta = handle_counter_delta_24bit(handle_counter_last_raw, current_raw);
  handle_counter_last_raw = current_raw;

  handle_strokes_total += delta;
  if (previous_water_was_flowing)
  {
    handle_strokes_flowing_total += delta;
  }
  else if (dry_start_candidate_valid)
  {
    dry_start_candidate_strokes += delta;
  }

  Serial.println("Handle counter raw: " + String(current_raw) + " delta: " + String(delta) + " total: " + String((uint32_t)handle_strokes_total));
  return true;
}

void handle_counter_log_water_state_change(bool water_is_flowing, int64_t now_s)
{
  if (!handle_counter_has_reading)
  {
    return;
  }

  if (water_is_flowing)
  {
    bool dry_start_qualified = last_water_flow_end_time_s == 0 || now_s - last_water_flow_end_time_s >= WATERPAL_MIN_DRY_DRAIN_TIME_S;
    if (dry_start_candidate_valid && dry_start_qualified)
    {
      dry_start_stroke_total += dry_start_candidate_strokes;
      if (dry_start_candidate_strokes > dry_start_stroke_max)
      {
        dry_start_stroke_max = dry_start_candidate_strokes > 0xFFFFFFFFULL ? 0xFFFFFFFFUL : (uint32_t)dry_start_candidate_strokes;
      }
      dry_start_count++;
      Serial.println("Dry start counted with " + String((uint32_t)dry_start_candidate_strokes) + " strokes");
    }
    else
    {
      Serial.println("Water flow started before dry-drain threshold; dry start not counted");
    }
    dry_start_candidate_strokes = 0;
    dry_start_candidate_valid = false;
  }
  else
  {
    last_water_flow_end_time_s = now_s;
    dry_start_candidate_strokes = 0;
    dry_start_candidate_valid = true;
    Serial.println("Water flow ended; dry-start baseline updated");
  }
}

uint32_t handle_counter_to_report_value(uint64_t value)
{
  if (value > 0xFFFFFFFFULL)
  {
    return 0xFFFFFFFFUL;
  }
  return (uint32_t)value;
}

uint32_t handle_counter_get_handle_strokes_total()
{
  return handle_counter_to_report_value(handle_strokes_total);
}

uint32_t handle_counter_get_handle_strokes_flowing_total()
{
  return handle_counter_to_report_value(handle_strokes_flowing_total);
}

uint32_t handle_counter_get_dry_start_count()
{
  return dry_start_count;
}

uint32_t handle_counter_get_dry_start_stroke_total()
{
  return handle_counter_to_report_value(dry_start_stroke_total);
}

uint32_t handle_counter_get_dry_start_stroke_avg()
{
  uint32_t report_dry_start_count = handle_counter_get_dry_start_count();
  if (report_dry_start_count == 0)
  {
    return 0;
  }

  return handle_counter_get_dry_start_stroke_total() / report_dry_start_count;
}

uint32_t handle_counter_get_dry_start_stroke_max()
{
  return dry_start_stroke_max;
}

void handle_counter_mark_report_sent()
{
  handle_strokes_total = 0;
  handle_strokes_flowing_total = 0;
  dry_start_stroke_total = 0;
  dry_start_stroke_max = 0;
  dry_start_count = 0;
}

#else

bool handle_counter_water_is_flowing(int water_sensor_value) { return water_sensor_value != WATERPAL_FLOAT_SWITCH_INVERT; }
void handle_counter_setup() {}
bool handle_counter_update(bool previous_water_was_flowing) { return false; }
void handle_counter_log_water_state_change(bool water_is_flowing, int64_t now_s) {}
uint32_t handle_counter_get_handle_strokes_total() { return 0; }
uint32_t handle_counter_get_handle_strokes_flowing_total() { return 0; }
uint32_t handle_counter_get_dry_start_count() { return 0; }
uint32_t handle_counter_get_dry_start_stroke_total() { return 0; }
uint32_t handle_counter_get_dry_start_stroke_avg() { return 0; }
uint32_t handle_counter_get_dry_start_stroke_max() { return 0; }
void handle_counter_mark_report_sent() {}

#endif // WATERPAL_USE_HANDLE_COUNTER

#endif // WATERPAL_HANDLE_COUNTER_H