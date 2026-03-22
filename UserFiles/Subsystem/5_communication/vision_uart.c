#include "vision_uart.h"

#include "gm6020_imu.h"
#include "main.h"

#include <math.h>
#include <string.h>

/* 按你的实际分辨率修改 */
#define VISION_IMAGE_WIDTH 720.0f
#define VISION_IMAGE_HEIGHT 540.0f
#define VISION_CENTER_X (VISION_IMAGE_WIDTH * 0.5f)
#define VISION_CENTER_Y (VISION_IMAGE_HEIGHT * 0.5f)

/* 死区，避免抖动 */
#define VISION_DEADBAND_X 6.0f
#define VISION_DEADBAND_Y 6.0f

/* 像素误差 -> 遥控叠加补偿量 */
#define VISION_YAW_K 0.008 * 0.13 * 0.8   //*0.8f //有一点偏慢0.7
#define VISION_PITCH_K 0.006 * 0.13 * 0.8 //*0.8f

/* 最终补偿量限幅 */
#define VISION_OUTPUT_MAX 0.80f

/* 离线超时 */
#define VISION_OFFLINE_TIMEOUT_MS 100u

/* 方向反了就改成 -1.0f */
#define VISION_YAW_DIR (-1.0f)
#define VISION_PITCH_DIR (-1.0f)

typedef struct {
  uint8_t rx_buf[VISION_UART_FRAME_LEN];
  uint8_t rx_index;

  VisionPacket_t packet_buf[2];
  uint8_t read_idx;
  uint8_t write_idx;

  uint8_t update_flag;
  uint32_t last_rx_tick_ms;
} vision_uart_ctx_t;

static vision_uart_ctx_t g_vision;

static inline float ClampF(float v, float min_v, float max_v) {
  if (v < min_v)
    return min_v;
  if (v > max_v)
    return max_v;
  return v;
}

static uint8_t Vision_CRC8(const uint8_t *data, uint16_t len) {
  uint8_t crc = 0x00;

  for (uint16_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x80) {
        crc = (uint8_t)((crc << 1) ^ 0x31);
      } else {
        crc <<= 1;
      }
    }
  }
  return crc;
}

static void Vision_ParseByte(uint8_t byte) {
  if (g_vision.rx_index == 0u) {
    if (byte == VISION_UART_HEADER) {
      g_vision.rx_buf[0] = byte;
      g_vision.rx_index = 1u;
    }
    return;
  }

  if (g_vision.rx_index == 1u) {
    if (byte == VISION_UART_PAYLOAD_LEN) {
      g_vision.rx_buf[1] = byte;
      g_vision.rx_index = 2u;
    } else if (byte == VISION_UART_HEADER) {
      g_vision.rx_buf[0] = byte;
      g_vision.rx_index = 1u;
    } else {
      g_vision.rx_index = 0u;
    }
    return;
  }

  g_vision.rx_buf[g_vision.rx_index++] = byte;

  if (g_vision.rx_index < VISION_UART_FRAME_LEN) {
    return;
  }

  if (g_vision.rx_buf[VISION_UART_FRAME_LEN - 1u] == VISION_UART_TAIL) {
    uint8_t crc_recv = g_vision.rx_buf[10];
    uint8_t crc_calc =
        Vision_CRC8(&g_vision.rx_buf[2], VISION_UART_PAYLOAD_LEN);

    if (crc_recv == crc_calc) {
      memcpy(&g_vision.packet_buf[g_vision.write_idx], &g_vision.rx_buf[2],
             sizeof(VisionPacket_t));

      g_vision.read_idx = g_vision.write_idx;
      g_vision.write_idx ^= 1u;
      g_vision.update_flag = 1u;
      g_vision.last_rx_tick_ms = HAL_GetTick();
    }
  }

  g_vision.rx_index = 0u;
}

void Vision_UART_Init(void) { memset(&g_vision, 0, sizeof(g_vision)); }

void Vision_UART_InputBytes(const uint8_t *buf, uint16_t size) {
  if (buf == NULL || size == 0u) {
    return;
  }

  for (uint16_t i = 0; i < size; i++) {
    Vision_ParseByte(buf[i]);
  }
}

uint8_t Vision_UART_IsOnline(void) {
  uint32_t now = HAL_GetTick();

  if (g_vision.last_rx_tick_ms == 0u) {
    return 0u;
  }

  if ((now - g_vision.last_rx_tick_ms) > VISION_OFFLINE_TIMEOUT_MS) {
    return 0u;
  }

  return 1u;
}

uint8_t Vision_UART_GetLatest(VisionPacket_t *out) {
  if (out == NULL) {
    return 0u;
  }

  if (g_vision.update_flag == 0u) {
    return 0u;
  }

  memcpy(out, &g_vision.packet_buf[g_vision.read_idx], sizeof(VisionPacket_t));
  g_vision.update_flag = 0u;
  return 1u;
}

void Vision_AutoAim_Apply(void) {
  VisionPacket_t pkt;

  /* 视觉离线：不补偿 */
  if (!Vision_UART_IsOnline()) {
    return;
  }

  /* 读取最近一次目标，但不消费 */
  if (!Vision_UART_PeekLatest(&pkt)) {
    return;
  }

  /* 没目标：不补偿 */
  if (((pkt.flags & VISION_FLAG_HAS_TARGET) == 0u) || pkt.digit == 0xFFu) {
    return;
  }

  float ex = (float)pkt.x - VISION_CENTER_X;
  float ey = (float)pkt.y - VISION_CENTER_Y;

  /* 死区 */
  if (fabsf(ex) < VISION_DEADBAND_X) {
    ex = 0.0f;
  }
  if (fabsf(ey) < VISION_DEADBAND_Y) {
    ey = 0.0f;
  }

  /* 像素误差 -> 虚拟摇杆输入 */
  float yaw_corr = VISION_YAW_DIR * ex * VISION_YAW_K;
  float pitch_corr = VISION_PITCH_DIR * ey * VISION_PITCH_K;

  /* 限幅到 [-1, 1] */
  yaw_corr = ClampF(yaw_corr, -VISION_OUTPUT_MAX, VISION_OUTPUT_MAX);
  pitch_corr = ClampF(pitch_corr, -VISION_OUTPUT_MAX, VISION_OUTPUT_MAX);

  /* 叠加到现有云台控制链 */
  GM6020_IMU_Yaw_Speed_Control_Mode(yaw_corr);
  // GM6020_IMU_Pitch_Speed_Control_Mode(pitch_corr);
}

uint8_t Vision_UART_PeekLatest(VisionPacket_t *out) {
  if (out == NULL) {
    return 0u;
  }

  if (g_vision.update_flag == 0u) {
    return 0u;
  }

  memcpy(out, &g_vision.packet_buf[g_vision.read_idx], sizeof(VisionPacket_t));
  return 1u;
}
