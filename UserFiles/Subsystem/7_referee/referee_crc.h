#ifndef __REFEREE_CRC_H
#define __REFEREE_CRC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint8_t Referee_CRC8_Calc(const uint8_t *data, uint16_t len, uint8_t init);
uint8_t Referee_CRC8_Verify(const uint8_t *data, uint16_t len_with_crc);

uint16_t Referee_CRC16_Calc(const uint8_t *data, uint32_t len, uint16_t init);
uint8_t Referee_CRC16_Verify(const uint8_t *data, uint32_t len_with_crc);

#ifdef __cplusplus
}
#endif

#endif // __REFEREE_CRC_H
