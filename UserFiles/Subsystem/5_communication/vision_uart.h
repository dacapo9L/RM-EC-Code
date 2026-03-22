#ifndef __VISION_UART_H
#define __VISION_UART_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VISION_UART_HEADER ((uint8_t)'s')
#define VISION_UART_TAIL ((uint8_t)'e')
#define VISION_UART_PAYLOAD_LEN 8u
#define VISION_UART_FRAME_LEN 12u

#define VISION_FLAG_HAS_TARGET (1u << 0)
#define VISION_FLAG_HAS_DISTANCE (1u << 1)

#pragma pack(push, 1)
typedef struct {
  uint8_t digit;        // 有目标时 1~4；无目标时 0xFF
  uint8_t flags;        // bit0=has_target, bit1=has_distance
  uint16_t x;           // little-endian
  uint16_t y;           // little-endian
  uint16_t distance_mm; // little-endian
} VisionPacket_t;
#pragma pack(pop)

void Vision_UART_Init(void);
void Vision_UART_InputBytes(const uint8_t *buf, uint16_t size);
uint8_t Vision_UART_IsOnline(void);
uint8_t Vision_UART_GetLatest(VisionPacket_t *out);
void Vision_AutoAim_Apply(void);
uint8_t Vision_UART_PeekLatest(VisionPacket_t *out);

#ifdef __cplusplus
}
#endif

#endif