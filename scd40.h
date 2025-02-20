#ifndef SCD40_H
#define SCD40_H

#include "pico/stdlib.h"
#include "stdint.h"

void    verify_checksum_calculation();
int32_t scd40_get_serial_number(uint16_t *serial_number);

#endif  // SCD40_H
