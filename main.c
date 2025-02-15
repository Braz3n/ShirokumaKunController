/**
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "FreeRTOS.h"
#include "ir_recv.h"
#include "ir_send.h"
#include "lwip/ip4_addr.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "ping.h"
#include "scd40.h"
#include "task.h"
#include "tusb.h"

#ifndef PING_ADDR
#define PING_ADDR "10.0.1.11"
#endif
#ifndef RUN_FREERTOS_ON_CORE
#define RUN_FREERTOS_ON_CORE 0
#endif

#define TEST_TASK_PRIORITY (tskIDLE_PRIORITY + 1UL)

#define STRING(x)    #x
#define STRINGIZE(x) STRING(x)

void main_task(__unused void *params) {
  if (cyw43_arch_init()) {
    printf("failed to initialise\n");
    return;
  }
  cyw43_arch_enable_sta_mode();
  printf("Connecting to Wi-Fi...\n");
  if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA3_SAE_AES_PSK,
                                         30000)) {
    printf("failed to connect.\n");
    exit(1);
  } else {
    printf("Connected.\n");
  }

  ip_addr_t ping_addr;
  ipaddr_aton(PING_ADDR, &ping_addr);
  ping_init(&ping_addr);

  while (true) {
    // not much to do as LED is in another task, and we're using RAW (callback) lwIP API
    vTaskDelay(100);
  }

  cyw43_arch_deinit();
}

void vLaunch(void) {
  verify_checksum_calculation();

  while (1);

  TaskHandle_t task;
  // xTaskCreate(ir_recv_task, "IrRecvTask", configMINIMAL_STACK_SIZE, NULL, TEST_TASK_PRIORITY,
  //             &task);
  // xTaskCreate(ir_send_task, "IrSendTask", configMINIMAL_STACK_SIZE, NULL, TEST_TASK_PRIORITY,
  //             &task);
  // xTaskCreate(decompose_test_task, "IrTestTask", configMINIMAL_STACK_SIZE, NULL,
  // TEST_TASK_PRIORITY,
  //             &task);

#if NO_SYS && configUSE_CORE_AFFINITY && configNUMBER_OF_CORES > 1
  // we must bind the main task to one core (well at least while the init is called)
  // (note we only do this in NO_SYS mode, because cyw43_arch_freertos
  // takes care of it otherwise)
  vTaskCoreAffinitySet(task, 1);
#endif

  /* Start the tasks and timer running. */
  vTaskStartScheduler();
}

int main(void) {
  stdio_init_all();
  while (!tud_cdc_connected()) {
    sleep_ms(100);
  }  // Wait for USB serial connection
  printf("Starting the pico w!\n");

  /* Configure the hardware ready to run the demo. */
  const char *rtos_name;
#if (configNUMBER_OF_CORES > 1)
  rtos_name = "FreeRTOS SMP";
#else
  rtos_name = "FreeRTOS";
#endif

#if (configNUMBER_OF_CORES == 2)
  printf("Starting %s on both cores:\n", rtos_name);
  vLaunch();
#elif (RUN_FREERTOS_ON_CORE == 1)
  printf("Starting %s on core 1:\n", rtos_name);
  multicore_launch_core1(vLaunch);
  while (true);
#else
  printf("Starting %s on core 0:\n", rtos_name);
  vLaunch();
#endif
  return 0;
}
