#ifndef CMD_GEN
#define CMD_GEN

#include "pico/stdlib.h"

#define COMMAND_BYTE_COUNT 53  // Data including preamble and parity bytes
#define COMMAND_DATA_COUNT 25  // Data only count

enum AirconUpdateType {
  AC_UPDATE_AIRCON_MODE = 0x13,
  AC_UPDATE_TIMER_ON    = 0x22,
  AC_UPDATE_TIMER_OFF   = 0x24,
  AC_UPDATE_FAN_SPEED   = 0x42,
  AC_UPDATE_TEMP_DOWN   = 0x43,
  AC_UPDATE_TEMP_UP     = 0x44,
  AC_UPDATE_FIN_DIR     = 0x81,
};

enum AirconMode {
  AC_MODE_OFF         = 0x0,
  AC_MODE_VENTILATION = 0x1,
  AC_MODE_COOLING     = 0x3,
  AC_MODE_DEHUMIDIFY  = 0x5,
  AC_MODE_HEATING     = 0x6,
};

enum AirconFanSpeed {
  AC_FAN_0    = 0x1,
  AC_FAN_1    = 0x2,
  AC_FAN_2    = 0x3,
  AC_FAN_3    = 0x4,
  AC_FAN_AUTO = 0x5,
  AC_FAN_5    = 0x6,
};

uint8_t* populate_command_buffer(enum AirconUpdateType update_type, enum AirconMode mode,
                                 enum AirconFanSpeed fan_speed, uint8_t temperature,
                                 uint16_t timer_on_duration, uint16_t timer_off_duration);

void decompose_input(bool logic_level, uint32_t duration_us);

void parse_command_buffer(uint8_t *command_buffer);

#endif  // CMD_GEN