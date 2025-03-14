#include "scd40.h"

#include "hardware/i2c.h"
#include "pico/binary_info.h"
#include "stdio.h"

// https://d2air1d4eqhwg2.cloudfront.net/media/files/262fda6e-3a57-4326-b93d-a9d627defdc4.pdf

#define SCD40_ADDR      0x62
#define CRC8_POLYNOMIAL 0x31
#define CRC8_INIT       0xFF

#define MAX_READ_BYTES 9

enum SCD4xCommand {
  // Basic Commands
  SCD4x_CMD_START_PERIODIC_MEASUREMENT = 0x21B1,
  SCD4x_CMD_READ_MEASUREMENT           = 0xEC05,
  SCD4x_CMD_STOP_PERIODIC_MEASUREMENT  = 0x3F86,

  // On-chip Output Signal Compensation
  SCD4x_CMD_SET_TEMPERATURE_OFFSET = 0x241D,
  SCD4x_CMD_GET_TEMPERATURE_OFFSET = 0x2318,
  SCD4x_CMD_SET_SENSOR_ALTITUDE    = 0x2427,
  SCD4x_CMD_GET_SENSOR_ALTITUDE    = 0x2322,
  SCD4x_CMD_SET_AMBIENT_PRESSURE   = 0xE000,

  // Field Calibration
  SCD4x_CMD_PERFORM_FORCED_RECALIBRATION           = 0x362F,
  SCD4x_CMD_SET_AUTOMATIC_SELF_CALIBRATION_ENABLED = 0x2416,
  SCD4x_CMD_GET_AUTOMATIC_SELF_CALIBRATION_ENABLED = 0x2313,

  // Low Power
  SCD4x_CMD_START_LOW_POWER_PERIODIC_MEASUREMENT = 0x21AC,
  SCD4x_CMD_GET_DATA_READY_STATUS                = 0xE4B8,

  // Advanced Features
  SCD4x_CMD_PERSIST_SETTINGS      = 0x3615,
  SCD4x_CMD_GET_SERIAL_NUMBER     = 0x3682,
  SCD4x_CMD_PERFORM_SELF_TEST     = 0x3639,
  SCD4x_CMD_PERFORM_FACTORY_RESET = 0x3632,
  SCD4x_CMD_REINIT                = 0x3646,

  // Low Power Single Shot (SCD41 only)
  SCD4x_CMD_MEASURE_SINGLE_SHOT          = 0x219D,
  SCD4x_CMD_MEASURE_SINGLE_SHOT_RHT_ONLY = 0x2196,
};

static bool running_periodic_mode = false;

uint8_t scd40_checksum(const uint8_t *data, uint16_t count) {
  uint8_t crc = CRC8_INIT;

  for (uint16_t current_byte = 0; current_byte < count; current_byte++) {
    crc ^= data[current_byte];
    for (uint8_t crc_bit = 8; crc_bit > 0; crc_bit--) {
      if (crc & 0x80) {
        crc = (crc << 1) ^ CRC8_POLYNOMIAL;
      } else {
        crc = (crc << 1);
      }
    }
  }

  return crc;
}

void verify_checksum_calculation() {
  uint8_t       data[]   = {0xBE, 0xEF};
  const uint8_t expected = 0x92;
  uint8_t       result   = scd40_checksum(data, 2);

  printf("CRC for 0xBEEF: 0x%X. Expecting 0x92\n", result);
  if (result != expected) {
    printf("SCD40 CRC calculation failed!\n");
    while (1);
  }
}

int32_t scd40_write(uint16_t data, bool stop_bit) {
  uint8_t crc = scd40_checksum((uint8_t *)&data, 2);
  uint8_t transfer[3];

  transfer[0] = data >> 8;
  transfer[1] = data & 0xFF;
  transfer[2] = crc;
  i2c_write_blocking(i2c_default, SCD40_ADDR, transfer, 3, stop_bit);

  printf("scd40_write[0] 0x%X\n", transfer[0]);
  printf("scd40_write[1] 0x%X\n", transfer[1]);
  printf("scd40_write[2] 0x%X\n", transfer[2]);

  return PICO_ERROR_NONE;
}

int32_t scd40_write_header(uint16_t command, bool stop_bit) {
  uint8_t transfer[2];

  transfer[0] = command >> 8;
  transfer[1] = command & 0xFF;
  i2c_write_blocking(i2c_default, SCD40_ADDR, transfer, 2, stop_bit);

  // printf("transfer[0] 0x%X\n", transfer[0]);
  // printf("transfer[1] 0x%X\n", transfer[1]);

  return PICO_ERROR_NONE;
}

int32_t scd40_read(uint8_t *data, uint8_t len) {
  // We need to wait for a bit between the request and response stages of this command
  uint8_t raw_data[MAX_READ_BYTES] = {0};
  uint8_t raw_len = len + (len >> 1);  // Half the length is added again for CRC bytes

  i2c_read_blocking(i2c_default, SCD40_ADDR, raw_data, raw_len, true);

  uint8_t output_data_index = 0;
  for (int i = 0; i < raw_len; i += 3) {
    // Check that the received data is valid
    uint8_t expected_checksum = scd40_checksum(&(raw_data[i]), 2);
    if (expected_checksum != raw_data[i + 2]) {
      printf("Bad checksum recieved for block %u\n", i);
      return PICO_ERROR_INVALID_DATA;
    }

    // Write out the received data
    data[output_data_index]     = raw_data[i];
    data[output_data_index + 1] = raw_data[i + 1];
    output_data_index += 2;
  }

  return PICO_ERROR_NONE;
}

void scd40_init(bool enable_internal_pullup) {
  i2c_init(i2c_default, 100 * 1000);
  gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
  gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
  if (enable_internal_pullup) {
    gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
    gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);
  }
  // Populate metadata in the binary for picotool
  bi_decl(bi_2pins_with_func(PICO_DEFAULT_I2C_SDA_PIN, PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C));

  sleep_ms(1000);  // Wait for powerup
}

//////////////////////////
// Header Only Commands //
//////////////////////////

// These commands are refered to as "send command" in the datasheet
int32_t scd40_header_only_command(uint16_t command, bool allowed_during_periodic,
                                  uint32_t delay_ms) {
  if (running_periodic_mode && allowed_during_periodic) {
    printf("Attempted running illegal command during measurement mode!\n");
    return PICO_ERROR_INVALID_STATE;
  }
  scd40_write_header(command, true);
  if (delay_ms > 0) {
    sleep_ms(delay_ms);
  }

  return PICO_ERROR_NONE;
}

int32_t scd40_start_periodic_measurement() {
  printf("Starting periodic measurements\n");
  return scd40_header_only_command(SCD4x_CMD_START_PERIODIC_MEASUREMENT, false, 0);
}

int32_t scd40_start_low_power_periodic_measurement() {
  printf("Starting low power periodic measurements\n");
  return scd40_header_only_command(SCD4x_CMD_START_LOW_POWER_PERIODIC_MEASUREMENT, false, 0);
}

int32_t scd40_stop_periodic_measurement() {
  printf("Stopping periodic measurements\n");
  return scd40_header_only_command(SCD4x_CMD_STOP_PERIODIC_MEASUREMENT, true, 500);
}

int32_t scd40_perform_factory_reset() {
  printf("Performing factory reset\n");
  return scd40_header_only_command(SCD4x_CMD_PERFORM_FACTORY_RESET, false, 1200);
}

int32_t scd40_reinit() {
  printf("Stopping periodic measurements\n");
  return scd40_header_only_command(SCD4x_CMD_REINIT, false, 20);
}

int32_t scd40_measure_single_shot() {
  return PICO_ERROR_VERSION_MISMATCH;  // Not permitted for the SCD40
  printf("Performing single shot measurement\n");
  return scd40_header_only_command(SCD4x_CMD_MEASURE_SINGLE_SHOT, false, 5000);
}

int32_t scd40_measure_single_shot_rht_only() {
  return PICO_ERROR_VERSION_MISMATCH;  // Not permitted for the SCD40
  printf("Performing single shot measurement (humidity and temperature only)\n");
  return scd40_header_only_command(SCD4x_CMD_MEASURE_SINGLE_SHOT_RHT_ONLY, false, 50);
}

////////////////////////
// Read Only Commands //
////////////////////////

// These commands are refered to as "read" in the datasheet
int32_t scd40_read_command(uint16_t command, bool allowed_during_periodic, uint32_t delay_ms,
                           uint8_t *buffer, uint8_t bytes_to_read) {
  int32_t err = PICO_ERROR_NONE;
  if (running_periodic_mode && allowed_during_periodic) {
    err = PICO_ERROR_INVALID_STATE;
  }

  if (err == PICO_ERROR_NONE) {
    err = scd40_write_header(command, true);
  }

  if (delay_ms > 0 && err == PICO_ERROR_NONE) {
    sleep_ms(delay_ms);
  }

  if (err == PICO_ERROR_NONE) {
    err = scd40_read(buffer, bytes_to_read);
  }

  return err;
}

int32_t scd40_read_measurement(uint16_t *co2_ppm, uint16_t *temp_cel, uint16_t *rel_humidity) {
  printf("Retrieving SCD40x measurement...\n");
  uint8_t output[6] = {0};

  // TODO: Check if there's data ready to be read
  int32_t err = PICO_ERROR_NONE;
  err         = scd40_read_command(SCD4x_CMD_READ_MEASUREMENT, true, 1, output, 6);
  if (err) {
    printf("Non zero error code!\n");
  }

  *rel_humidity = (uint16_t)(output[4] << 8) | output[5];
  *rel_humidity = 100 * ((uint32_t)*rel_humidity) / (1 << 16);  // Apply post-processing
  *temp_cel     = (uint16_t)(output[2] << 8) | output[3];
  *temp_cel     = -45 + 175 * ((uint32_t)*temp_cel) / (1 << 16);  // Apply post-processing
  *co2_ppm      = (uint16_t)(output[0] << 8) | output[1];  // CO2 doesn't require post-processing

  return err;
}

int32_t scd40_get_temperature_offset(uint16_t *temp_offset) {
  printf("Checking SCD40x temperature offset...\n");
  uint8_t output[2] = {0};
  int32_t err       = PICO_ERROR_NONE;

  err = scd40_read_command(SCD4x_CMD_GET_TEMPERATURE_OFFSET, false, 1, output, 2);
  if (err) {
    printf("Non zero error code!\n");
  }

  *temp_offset = (uint16_t)(output[0] << 8) | output[1];
  *temp_offset = 175 * ((uint32_t)*temp_offset) / (1 << 16);  // Apply post-processing

  return err;
}

int32_t scd40_get_sensor_altitude(uint16_t *altitude_meters) {
  printf("Checking SCD40x sensor altitude...\n");
  uint8_t output[2] = {0};
  int32_t err       = PICO_ERROR_NONE;

  err = scd40_read_command(SCD4x_CMD_GET_SENSOR_ALTITUDE, false, 1, output, 2);
  if (err) {
    printf("Non zero error code!\n");
  }

  *altitude_meters = (uint16_t)(output[0] << 8) | output[1];

  return err;
}

int32_t scd40_get_automatic_self_calibration_enabled(bool *self_calibration_enabled) {
  printf("Checking SCD40x automatic self-calibration enabled...\n");
  uint8_t output[2] = {0};
  int32_t err       = PICO_ERROR_NONE;

  err = scd40_read_command(SCD4x_CMD_GET_AUTOMATIC_SELF_CALIBRATION_ENABLED, false, 1, output, 2);
  if (err) {
    printf("Non zero error code!\n");
  }

  *self_calibration_enabled = ((uint16_t)(output[0] << 8) | output[1]) == 1;

  return err;
}

int32_t scd40_get_data_ready_status(bool *data_waiting) {
  printf("Checking for waiting SCD40x measurement...\n");
  uint8_t output[2] = {0};
  int32_t err       = PICO_ERROR_NONE;

  err = scd40_read_command(SCD4x_CMD_GET_DATA_READY_STATUS, true, 1, output, 2);
  if (err) {
    printf("Non zero error code!\n");
  }

  uint16_t result = (uint16_t)(output[0] << 8) | output[1];
  *data_waiting   = (result & 0xFFF) != 0;  // If bits 11:0 are non-zero, data is waiting
  printf("Data waiting result: 0x%04X (%u)\n", result, *data_waiting);

  return err;
}

int32_t scd40_get_serial_number(uint16_t *serial_number) {
  printf("Retrieving SCD40x Serial Number...\n");
  uint8_t output[6] = {0};
  int32_t err       = PICO_ERROR_NONE;
  err               = scd40_read_command(SCD4x_CMD_GET_SERIAL_NUMBER, false, 1, output, 6);

  if (err) {
    printf("Non zero error code!\n");
  }

  serial_number[2] = (uint16_t)(output[4] << 8) | output[5];
  serial_number[1] = (uint16_t)(output[2] << 8) | output[3];
  serial_number[0] = (uint16_t)(output[0] << 8) | output[1];

  printf("Serial Number: 0x%04X %04X %04X\n", serial_number[0], serial_number[1], serial_number[2]);
  for (int i = 0; i < 6; i++) {
    printf("%u: 0x%02X\n", i, output[i]);
  }

  return err;
}

int32_t scd40_perform_self_test() {
  printf("Checking SCD40x self test result...\n");
  uint8_t output[2] = {0};
  int32_t err       = PICO_ERROR_NONE;

  err = scd40_read_command(SCD4x_CMD_PERFORM_SELF_TEST, false, 10000, output, 2);
  if (err) {
    printf("Non zero error code!\n");
  }

  uint16_t self_test_result = ((uint16_t)(output[0] << 8) | output[1]) == 1;

  if (self_test_result != 0) {
    printf("Self test failed!\n");
    err = PICO_ERROR_GENERIC;
  }

  return err;
}

/////////////////////////
// Write Only Commands //
/////////////////////////

// These commands are refered to as "write" in the datasheet
int32_t scd40_write_command(uint16_t command, bool allowed_during_periodic, uint32_t delay_ms,
                            uint16_t data) {
  int32_t err = PICO_ERROR_NONE;
  if (running_periodic_mode && allowed_during_periodic) {
    err = PICO_ERROR_INVALID_STATE;
  }

  if (err == PICO_ERROR_NONE) {
    err = scd40_write_header(command, true);
  }

  if (err == PICO_ERROR_NONE) {
    err = scd40_write(data, true);
  }

  if (delay_ms > 0 && err == PICO_ERROR_NONE) {
    sleep_ms(delay_ms);
  }

  return err;
}

int32_t scd40_set_temperature_offset(uint16_t offset) {
  printf("Setting SCD40x temperature offset...\n");
  int32_t err = PICO_ERROR_NONE;

  uint16_t post_processed_offset = offset * (1 << 16) / 175;
  err = scd40_write_command(SCD4x_CMD_SET_TEMPERATURE_OFFSET, false, 1, post_processed_offset);
  if (err) {
    printf("Non zero error code!\n");
  }

  return err;
}

int32_t scd40_set_sensor_altitude(uint16_t altitude) {
  printf("Setting SCD40x sensor altitude...\n");
  int32_t err = PICO_ERROR_NONE;

  err = scd40_write_command(SCD4x_CMD_SET_SENSOR_ALTITUDE, false, 1, altitude);
  if (err) {
    printf("Non zero error code!\n");
  }

  return err;
}

int32_t scd40_set_ambient_pressure(uint16_t pressure_pa) {
  printf("Setting SCD40x ambient pressure...\n");
  int32_t err = PICO_ERROR_NONE;

  uint16_t post_processed_pressure = pressure_pa / 100;
  err = scd40_write_command(SCD4x_CMD_SET_AMBIENT_PRESSURE, false, 1, post_processed_pressure);
  if (err) {
    printf("Non zero error code!\n");
  }

  return err;
}

int32_t scd40_set_automatic_self_calibration_enabled(bool enabled) {
  printf("Setting SCD40x sensor altitude...\n");
  int32_t err = PICO_ERROR_NONE;

  uint16_t self_calibration_status = enabled ? 1 : 0;
  err = scd40_write_command(SCD4x_CMD_SET_AUTOMATIC_SELF_CALIBRATION_ENABLED, false, 1,
                            self_calibration_status);
  if (err) {
    printf("Non zero error code!\n");
  }

  return err;
}