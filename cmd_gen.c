#include "cmd_gen.h"

const uint8_t command_preamble[3]                = {0x01, 0x10, 0x00};
uint8_t       command_buffer[COMMAND_BYTE_COUNT] = {0};

uint8_t decomposed_buffer[COMMAND_BYTE_COUNT] = {0};

uint8_t *populate_command_buffer(enum AirconUpdateType update_type, enum AirconMode mode,
                                 enum AirconFanSpeed fan_speed, uint8_t temperature,
                                 uint16_t timer_on_duration, uint16_t timer_off_duration) {
  printf(
      "Update Type %u, Aircon Mode: %u, Fan Speed %u, Temperature %u, On Duration %u, Off Duration "
      "%u\n",
      update_type, mode, fan_speed, temperature, timer_on_duration, timer_off_duration);

  uint8_t raw_data_buffer[COMMAND_DATA_COUNT] = {0};

  raw_data_buffer[0]  = 0x40;                             // Constant
  raw_data_buffer[1]  = 0xFF;                             // Constant
  raw_data_buffer[2]  = 0xCC;                             // Constant
  raw_data_buffer[3]  = 0x92;                             // Constant
  raw_data_buffer[4]  = (uint8_t)update_type;             // Update Type
  raw_data_buffer[5]  = temperature << 2;                 // Temperature
  raw_data_buffer[6]  = 0x00;                             // Constant
  raw_data_buffer[7]  = (timer_off_duration & 0xF) << 4;  // Byte7[7:4] Timer Off Minutes low nibble
  raw_data_buffer[8]  = (timer_off_duration >> 4) & 0xFF;  // Byte8[7:0] Timer Off Minutes high byte
  raw_data_buffer[9]  = (timer_on_duration & 0xFF);        // Byte9[7:0] Timer On Minutes low byte
  raw_data_buffer[10] = (timer_on_duration >> 8) & 0xF;  // Byte10[3:0] Timer On Minutes high nibble
  raw_data_buffer[10] |= (timer_off_duration > 0) << 4;  // Timer Off Flag: Byte10[4]
  raw_data_buffer[10] |= (timer_on_duration > 0) << 5;   // Timer On Flag: Byte10[5]
  if (mode == AC_MODE_OFF) {                             // TODO: Do we need this?
    raw_data_buffer[11] = (fan_speed << 4) | AC_MODE_HEATING;  // Fan Speed and Aircon Mode
  } else {
    raw_data_buffer[11] = (fan_speed << 4) | mode;  // Fan Speed and Aircon Mode
  }
  raw_data_buffer[12] = mode == AC_MODE_OFF                                ? 0xE1 :
                        mode == AC_MODE_HEATING || mode == AC_MODE_COOLING ? 0xF1 :
                                                                             0xF0;
  raw_data_buffer[13] = 0x00;  // Constant
  raw_data_buffer[14] = 0x00;  // Constant
  raw_data_buffer[15] = 0x80;  // Constant
  raw_data_buffer[16] = 0x03;  // Constant
  raw_data_buffer[17] = 0x01;  // Constant
  raw_data_buffer[18] = 0x88;  // Constant
  raw_data_buffer[19] = 0x00;  // Constant
  raw_data_buffer[20] = 0x00;  // Constant
  raw_data_buffer[21] = 0xFF;  // Constant
  raw_data_buffer[22] = 0xFF;  // Constant
  raw_data_buffer[23] = 0xFF;  // Constant
  raw_data_buffer[24] = 0xFF;  // Constant

  // Convert to command buffer
  // Every second byte after the first three are the bitwise inverse of the first byte in a pair
  command_buffer[0] = command_preamble[0];
  command_buffer[1] = command_preamble[1];
  command_buffer[2] = command_preamble[2];
  for (uint8_t i = 0; i < COMMAND_DATA_COUNT; i++) {
    command_buffer[2 * i + 3] = raw_data_buffer[i];
    command_buffer[2 * i + 4] = ~raw_data_buffer[i];
  }

  return command_buffer;
}

bool check_symbol(bool logic_level, uint32_t duration_us, bool expected_level, uint32_t min_us,
                  uint32_t max_us) {
  return logic_level == expected_level && duration_us >= min_us && duration_us <= max_us;
}

void decompose_input(bool logic_level, uint32_t duration_us) {
  // Decoding the preamble
  static uint8_t preamble_stage = 0;
  if (preamble_stage == 0 && logic_level == true) {
    // This is the time between boot at the start of a packet. We can ignore this.
    return;
  } else if (preamble_stage == 0 && check_symbol(logic_level, duration_us, false, 28000, 31000)) {
    preamble_stage++;
    return;
  } else if (preamble_stage == 1 && check_symbol(logic_level, duration_us, true, 48000, 51000)) {
    preamble_stage++;
    return;
  } else if (preamble_stage == 2 && check_symbol(logic_level, duration_us, false, 3300, 3500)) {
    preamble_stage++;
    return;
  } else if (preamble_stage == 3 && check_symbol(logic_level, duration_us, true, 1600, 1700)) {
    preamble_stage++;
    return;
  } else if (preamble_stage == 4) {
    // Continue to decoding the incoming data
  } else {
    printf("Failed preamble check at stage %u\n", preamble_stage);
    printf("Got %u for %u us\n", logic_level, duration_us);
    printf("Halting system\n");
    while (1);
  }

  // Decoding bytes
  static uint32_t byte_index          = 0;
  static uint8_t  incoming_byte       = 0;
  static uint8_t  incoming_byte_index = 0;

  static bool expected_logic_level = false;
  static bool parity_byte          = false;

  // First we need to read in a single byte
  // Then we need to read in the parity byte
  if (expected_logic_level == false) {
    // This is a 38kHz pulse on the LED
    // The duration is always the same
    if (check_symbol(logic_level, duration_us, false, 400, 470)) {
      // Not information
    } else if (logic_level == false) {
      // Malformed packet
      printf("Logic low symbol fell outside of expected duration: %uus for logic low\n",
             duration_us);
      printf("Halting system\n");
      while (1);
    }
    expected_logic_level = true;
  } else if (expected_logic_level == true) {
    // This is a break in the LED signal
    // The duration indicates either a zero or a one in the transmission
    if (check_symbol(logic_level, duration_us, true, 390, 450)) {
      // Logic zero
      incoming_byte |= 0 << incoming_byte_index;  // Does nothing
      printf("Received bit 0\n");
    } else if (check_symbol(logic_level, duration_us, true, 1230, 1290)) {
      // Logic one
      incoming_byte |= 1 << incoming_byte_index;  // Shift 1 into the register
      printf("Received bit 1\n");
    } else {
      printf("Logic high symbol fell outside of expected duration: %uus for logic high\n",
             duration_us);
      printf("Halting system\n");
      while (1);
    }
    expected_logic_level = false;
    if (++incoming_byte_index >= 8) {
      if (byte_index < 3) {
        // First three bytes aren't data/parity pairs
        printf("Received preamble byte %u (0x%X)\n", byte_index, incoming_byte);
        decomposed_buffer[byte_index] = incoming_byte;
        byte_index++;
        incoming_byte       = 0;
        incoming_byte_index = 0;
        return;  // Return to skip parity byte swapover
      } else if (!parity_byte) {
        // Save raw data byte
        decomposed_buffer[byte_index] = incoming_byte;
        printf("Received data byte %d (0x%X)\n", byte_index, decomposed_buffer[byte_index] & 0xFF);
        byte_index++;
      } else if (parity_byte && decomposed_buffer[byte_index - 1] != (~incoming_byte & 0xFF)) {
        // Parity byte failed check
        printf("Parity Byte Failure: data 0x%X, parity 0x%X\n", decomposed_buffer[byte_index],
               incoming_byte);
        printf("Parity Byte value: %u, inverted 0x%X\n", parity_byte, (~incoming_byte & 0xFF));
        printf("Halting system\n");
        while (1);
      } else {
        // Parity byte check passed
        printf("Parsed byte %u, with value 0x%X against parity byte (0x%X)\n", byte_index,
               decomposed_buffer[byte_index - 1], incoming_byte);
        decomposed_buffer[byte_index] = incoming_byte;
        byte_index++;
      }
      parity_byte         = !parity_byte;
      incoming_byte       = 0;
      incoming_byte_index = 0;
    }
  }

  // Reset the calculation
  if (byte_index == COMMAND_BYTE_COUNT) {  // Checked after byte_index++, so -1 isn't needed
    parse_command_buffer(decomposed_buffer);
    byte_index     = 0;
    preamble_stage = 0;
    parity_byte    = false;
    memset(decomposed_buffer, 0, COMMAND_BYTE_COUNT);
    printf("Full message received\n");
  }
}

void parse_command_buffer(uint8_t *command_buffer) {
  printf("----------------------------------\n");
  printf("Beginning to parse command buffer!\n");
  printf("----------------------------------\n");
  // Convert command buffer into plain text description
  // Also check that other bits aren't unexpectedly high
  if (command_buffer[3] != 0x40) {
    printf("command_buffer[0] != 0x40! 0x%X\n", command_buffer[3]);  // Const
  }
  if (command_buffer[5] != 0xFF) {
    printf("command_buffer[1] != 0xFF! 0x%X\n", command_buffer[5]);  // Const
  }
  if (command_buffer[7] != 0xCC) {
    printf("command_buffer[2] != 0xCC! 0x%X\n", command_buffer[7]);  // Const
  }
  if (command_buffer[9] != 0x92) {
    printf("command_buffer[3] != 0x92! 0x%X\n", command_buffer[9]);  // Const
  }

  // Update Type
  switch (command_buffer[11]) {
    case AC_UPDATE_AIRCON_MODE:
      printf("    Update: Mode\n");
      break;
    case AC_UPDATE_TIMER_ON:
      printf("    Update: Timer On\n");
      break;
    case AC_UPDATE_TIMER_OFF:
      printf("    Update: Timer Off\n");
      break;
    case AC_UPDATE_FAN_SPEED:
      printf("    Update: Fan Speed\n");
      break;
    case AC_UPDATE_TEMP_DOWN:
      printf("    Update: Temperature Down\n");
      break;
    case AC_UPDATE_TEMP_UP:
      printf("    Update: Temperature Up\n");
      break;
    case AC_UPDATE_FIN_DIR:
      printf("    Update: Fin Direction\n");
      break;
    default:
      printf("    Update: UNKNOWN!!!! 0x%X\n", command_buffer[4]);
      break;
  }

  // Temperature
  printf("    Temperature 0x%X\n", command_buffer[13] >> 2);

  if (command_buffer[15] != 0x00) {
    printf("command_buffer[6] != 0x00!\n");  // Const
  }

  uint16_t timer_off_duration = ((uint16_t)command_buffer[19]) << 8 | command_buffer[17] >> 4;
  printf("    Timer Off Duration 0x%X\n", timer_off_duration);
  if (command_buffer[17] & 0xF != 0) {
    printf("command_buffer[7] contains additional data!\n");
  }

  uint16_t timer_on_duration = (((uint16_t)command_buffer[23]) << 8) & 0xF | command_buffer[21];
  printf("    Timer On Duration 0x%X\n", timer_on_duration);
  if (command_buffer[23] & 0xC0 != 0) {
    printf("command_buffer[10] contains additional data!\n");
  }

  printf("    Timer On Flag 0x%X\n", command_buffer[23] & 0x20);
  printf("    Timer Off Flag 0x%X\n", command_buffer[23] & 0x10);

  // Mode
  switch (command_buffer[25] & 0xF) {
    case AC_MODE_OFF:
      printf("    Mode: OFF\n");
      break;
    case AC_MODE_VENTILATION:
      printf("    Mode: VENTILATION\n");
      break;
    case AC_MODE_COOLING:
      printf("    Mode: COOLING\n");
      break;
    case AC_MODE_DEHUMIDIFY:
      printf("    Mode: DEHUMIDIFY\n");
      break;
    case AC_MODE_HEATING:
      printf("    Mode: HEATING\n");
      break;
    default:
      printf("    Mode: UNKNOWN!!!! 0x%X\n", command_buffer[25] & 0xF);
      break;
  }

  // Fan Speed
  switch (command_buffer[25] >> 4) {
    case AC_FAN_0:
      printf("    Fan Speed: 0\n");
      break;
    case AC_FAN_1:
      printf("    Fan Speed: 1\n");
      break;
    case AC_FAN_2:
      printf("    Fan Speed: 2\n");
      break;
    case AC_FAN_3:
      printf("    Fan Speed: 3\n");
      break;
    case AC_FAN_5:
      printf("    Fan Speed: 5\n");
      break;
    case AC_FAN_AUTO:
      printf("    Fan Speed: AUTO\n");
      break;
    default:
      printf("    Fan Speed: UNKNOWN!!!! 0x%x\n", command_buffer[25] >> 4);
      break;
  }

  // Mode thing?
  switch (command_buffer[27]) {
    case 0xE1:
      printf("    Mode Thing: OFF\n");
      break;
    case 0xF1:
      printf("    Mode Thing: Heating/Cooling\n");
      break;
    case 0xF0:
      printf("    Mode Thing: Everything Else\n");
      break;
    default:
      printf("    Mode Thing: UNKNOWN!!!! 0x%X\n", command_buffer[27]);
      break;
  }

  if (command_buffer[29] != 0x00) {
    printf("command_buffer[13] != 0x00! (0x%X)\n", command_buffer[29]);  // Const
  }
  if (command_buffer[31] != 0x00) {
    printf("command_buffer[14] != 0x00! (0x%X)\n", command_buffer[31]);  // Const
  }
  if (command_buffer[33] != 0x80) {
    printf("command_buffer[15] != 0x80! (0x%X)\n", command_buffer[33]);  // Const
  }
  if (command_buffer[35] != 0x03) {
    printf("command_buffer[16] != 0x03! (0x%X)\n", command_buffer[35]);  // Const
  }
  if (command_buffer[37] != 0x01) {
    printf("command_buffer[17] != 0x01! (0x%X)\n", command_buffer[37]);  // Const
  }
  if (command_buffer[39] != 0x88) {
    printf("command_buffer[18] != 0x88! (0x%X)\n", command_buffer[39]);  // Const
  }
  if (command_buffer[41] != 0x00) {
    printf("command_buffer[19] != 0x00! (0x%X)\n", command_buffer[41]);  // Const
  }
  if (command_buffer[43] != 0x00) {
    printf("command_buffer[20] != 0x00! (0x%X)\n", command_buffer[43]);  // Const
  }
  if (command_buffer[45] != 0xFF) {
    printf("command_buffer[21] != 0xFF! (0x%X)\n", command_buffer[45]);  // Const
  }
  if (command_buffer[47] != 0xFF) {
    printf("command_buffer[22] != 0xFF! (0x%X)\n", command_buffer[47]);  // Const
  }
  if (command_buffer[49] != 0xFF) {
    printf("command_buffer[23] != 0xFF! (0x%X)\n", command_buffer[49]);  // Const
  }
  if (command_buffer[51] != 0xFF) {
    printf("command_buffer[24] != 0xFF! (0x%X)\n", command_buffer[51]);  // Const
  }
}