#include "ir_recv.h"

#include "FreeRTOS.h"
#include "cmd_gen.h"
#include "hardware/gpio.h"
#include "task.h"

#define GPIO_IR_RECV_PIN 15

static uint32_t durations[1000] = {0};
static bool     value[1000]     = {0};

static void ir_recv_init() {
  gpio_init(GPIO_IR_RECV_PIN);
  gpio_pull_up(GPIO_IR_RECV_PIN);
}

void decompose_test_task(void *params) {
  uint8_t *command_buffer =
      populate_command_buffer(AC_UPDATE_AIRCON_MODE, AC_MODE_OFF, AC_FAN_AUTO, 25, 0, 0);
  parse_command_buffer(command_buffer);

  printf("Test complete. Halting system.\n");
  while (1);
}

void ir_recv_task(void *params) {
  ir_recv_init();

  bool     last_state = true;
  uint32_t ticks      = time_us_32();

  uint32_t index = 0;

  while (1) {
    bool     state = gpio_get_all() & (1 << GPIO_IR_RECV_PIN);
    uint32_t diff  = time_us_32() - ticks;
    if (state != last_state) {
      durations[index] = diff;
      value[index]     = last_state;
      ticks            = time_us_32();
      // printf("State update %u %u\n", state, diff);
      // printf("State update %u\n", state);
      last_state = state;
      index++;
    }

    if (diff > 5000 && index > 5) {
      for (uint32_t i = 0; i < index; i++) {
        decompose_input(value[i], durations[i]);
      }
      index = 0;
    }
  }
}