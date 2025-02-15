#include "ir_send.h"

#include "FreeRTOS.h"
#include "cmd_gen.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "task.h"

#define GPIO_IR_SEND_PIN 16

#define PWM_IR_SEND_WRAP  ((uint16_t)(125e9 / 38e6) - 1)
#define PWM_IR_SEND_LEVEL ((uint16_t)(PWM_IR_SEND_WRAP * 0.3))

static void ir_send_init() {
  gpio_init(GPIO_IR_SEND_PIN);
  gpio_set_dir(GPIO_IR_SEND_PIN, GPIO_OUT);
  gpio_put(GPIO_IR_SEND_PIN, false);
  gpio_pull_down(GPIO_IR_SEND_PIN);
  gpio_set_slew_rate(GPIO_IR_SEND_PIN, GPIO_SLEW_RATE_FAST);
}

inline void pwm_on(uint slice_num) {
  pwm_set_counter(slice_num, 0);
  gpio_set_function(GPIO_IR_SEND_PIN, GPIO_FUNC_PWM);
  pwm_set_enabled(slice_num, true);
}

inline void pwm_off(uint slice_num) {
  gpio_put(GPIO_IR_SEND_PIN, false);
  gpio_set_function(GPIO_IR_SEND_PIN, GPIO_FUNC_SIO);
  pwm_set_enabled(slice_num, false);
}

inline void send_ir_symbol(bool new_transmission, bool value) {
  const uint slice_num = pwm_gpio_to_slice_num(GPIO_IR_SEND_PIN);
  if (new_transmission) {
    printf("STARTING NEW TRANSMISSION\n");
    // Send the start of transmission symbols

    // First Pulse (3200us)
    pwm_on(slice_num);
    busy_wait_us_32(30000);

    // First Pause (49500us)
    pwm_off(slice_num);
    busy_wait_us_32(49500);

    // Second Pulse (3200us)
    pwm_on(slice_num);
    busy_wait_us_32(3380);

    // Second Pause (1700us)
    pwm_off(slice_num);
    busy_wait_us_32(1700);
  }

  // Set pulse (always 410us)
  pwm_on(slice_num);
  busy_wait_us_32(410);

  // Set pause (1260us for one, 410us for zero)
  pwm_off(slice_num);
  gpio_put(GPIO_IR_SEND_PIN, false);
  if (value) {
    busy_wait_us_32(1256);  // Aiming for 1260
  } else {
    busy_wait_us_32(422);  // Aiming for 425us
  }
}

void send_aircon_command(enum AirconUpdateType update_type, enum AirconMode mode,
                         enum AirconFanSpeed fan_speed, uint8_t temperature,
                         uint16_t timer_on_duration, uint16_t timer_off_duration) {
  uint8_t *command_buffer = populate_command_buffer(update_type, mode, fan_speed, temperature,
                                                    timer_on_duration, timer_off_duration);

  // Prep PWM unit
  uint slice_num = pwm_gpio_to_slice_num(GPIO_IR_SEND_PIN);
  pwm_set_wrap(slice_num, PWM_IR_SEND_WRAP);
  pwm_set_gpio_level(GPIO_IR_SEND_PIN, PWM_IR_SEND_LEVEL);  // Set PWM trigger
  pwm_set_enabled(slice_num, true);
  gpio_set_function(GPIO_IR_SEND_PIN, GPIO_FUNC_SIO);
  gpio_put(GPIO_IR_SEND_PIN, false);
  gpio_disable_pulls(GPIO_IR_SEND_PIN);
  gpio_set_slew_rate(GPIO_IR_SEND_PIN, GPIO_SLEW_RATE_FAST);

  for (int i = 0; i < COMMAND_BYTE_COUNT; i++) {
    uint8_t byte = command_buffer[i];
    for (int j = 0; j < 8; j++) {
      send_ir_symbol(i == 0 && j == 0, byte & 0x01);
      // printf("Sent bit %u\n", byte & 0x01);
      byte = byte >> 1;
    }
    // printf("---- Just sent byte %u (0x%X)\n", i, command_buffer[i]);
  }

  // Set pulse (always 410us) to finish off the transmission
  // This is needed because send_ir_symbol() finishes on a pause. We need a pulse to signal the end
  // of the last bit
  pwm_on(slice_num);
  busy_wait_us_32(410);
  // Disable PWM now that the transmission is completed
  pwm_off(slice_num);
}

void ir_send_task(void *params) {
  printf("Settings %u %u\n", PWM_IR_SEND_WRAP, PWM_IR_SEND_LEVEL);
  ir_send_init();

  while (1) {
    printf("Sending Command\n");
    send_aircon_command(AC_UPDATE_AIRCON_MODE, AC_MODE_OFF, AC_FAN_AUTO, 25, 0, 0);
    printf("Command Sent\n");
    vTaskDelay(pdMS_TO_TICKS(5000));
  }

  while (1);
}