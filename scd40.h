#ifndef SCD40_H
#define SCD40_H

#include "pico/stdlib.h"
#include "stdint.h"

// Header Only Commands
int32_t scd40_start_periodic_measurement();
int32_t scd40_start_low_power_periodic_measurement();
int32_t scd40_stop_periodic_measurement();
int32_t scd40_perform_factory_reset();
int32_t scd40_reinit();
int32_t scd40_measure_single_shot();
int32_t scd40_measure_single_shot_rht_only();

// Read Only Commands
int32_t scd40_read_measurement(uint16_t *co2_ppm, uint16_t *temp_cel, uint16_t *rel_humidity);
int32_t scd40_get_temperature_offset(uint16_t *temp_offset);
int32_t scd40_get_sensor_altitude(uint16_t *altitude_meters);
int32_t scd40_get_automatic_self_calibration_enabled(bool *self_calibration_enabled);
int32_t scd40_get_data_ready_status(bool *data_waiting);
int32_t scd40_get_serial_number(uint16_t *serial_number);
int32_t scd40_perform_self_test();

#endif  // SCD40_H
