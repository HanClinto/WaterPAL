IMEI (identifier of sending unit)
Total SMS send count (debug information used to detect unreceived messages)
Daily water usage time (s)
Detected clock time drift
Temperature C (low)
Temperature C (avg)
Temperature C (high)
Humidity (low)
Humidity (avg)
Humidity (high)
Signal strength %
Battery charge status
Battery charge %
Battery voltage (mV)
Boot Count
Handle strokes total
Handle strokes while water was flowing
Handle strokes per minute while water was flowing
Dry start count
Dry start stroke total
Dry start stroke avg
Dry start stroke max

When GPRS/HTTP reporting is available, handle-counter reporting also includes:

handle_strokes_total (all handle strokes during the report period)
handle_strokes_flowing_total (handle strokes while water was flowing)
handle_strokes_flowing_per_min (handle strokes per flowing-water minute)
dry_start_count (qualifying dry starts during the report period)
dry_start_stroke_total (dry-start strokes across qualifying dry starts)
dry_start_stroke_avg (derived average dry-start strokes)
dry_start_stroke_max (max dry-start strokes)

Additionally, there is a weekly message that includes the following information:

IMEI (identifier of sending unit)
Total SMS send count (debug information)
GPS Lat (6 decimal places)
GPS Long (6 decimal places)
CPSI information (long string of debug information)

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
             handle_strokes_total_report, // Handle strokes total
             handle_strokes_flowing_total_report, // Handle strokes while water was flowing
             handle_strokes_flowing_per_min_report, // Handle strokes per minute while water was flowing
             dry_start_count_report, // Dry start count
             dry_start_stroke_total_report, // Dry start stroke total
             dry_start_stroke_avg_report, // Dry start stroke avg
             dry_start_stroke_max_report); // Dry start stroke max