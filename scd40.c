#include "scd40.h"

#include "stdio.h"

// https://d2air1d4eqhwg2.cloudfront.net/media/files/262fda6e-3a57-4326-b93d-a9d627defdc4.pdf

#define SCD40_ADDR
#define CRC8_POLYNOMIAL 0x31
#define CRC8_INIT       0xFF

uint8_t scd40_checksum(const uint8_t* data, uint16_t count) {
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

void scd40_write() {}

void scd40_send() {}

void scd40_read() {
  // We need to wait for a bit between the request and response stages of this command
}

void scd40_fetch() {
  // We need to wait for a bit between the send and fetch stages of this command
}