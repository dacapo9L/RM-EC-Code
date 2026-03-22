#include "referee_parser.h"

#include "main.h"
#include "referee_crc.h"
#include <string.h>

#define REFEREE_RX_CACHE_SIZE 512u
#define REFEREE_OFFLINE_TIMEOUT_MS 300u

static RefereeData_t g_referee_data;
static uint8_t g_rx_cache[REFEREE_RX_CACHE_SIZE];
static uint16_t g_rx_cache_len = 0u;

static void RefereeParser_HandleFrame(uint16_t cmd_id, const uint8_t *payload,
                                      uint16_t payload_len) {
  uint16_t expected_len = Referee_GetExpectedPayloadLength(cmd_id);
  if (expected_len == 0u || payload_len != expected_len || payload == 0) {
    return;
  }

  g_referee_data.last_cmd_id = cmd_id;
  g_referee_data.last_rx_tick_ms = HAL_GetTick();
  g_referee_data.online = 1u;

  switch (cmd_id) {
  case REF_CMD_GAME_STATE:
    memcpy(&g_referee_data.game_state, payload, sizeof(RefGameState_t));
    g_referee_data.has_game_state = 1u;
    break;
  case REF_CMD_GAME_RESULT:
    memcpy(&g_referee_data.game_result, payload, sizeof(RefGameResult_t));
    g_referee_data.has_game_result = 1u;
    break;
  case REF_CMD_GAME_ROBOT_HP:
    memcpy(&g_referee_data.game_robot_hp, payload, sizeof(RefGameRobotHp_t));
    g_referee_data.has_game_robot_hp = 1u;
    break;
  case REF_CMD_EVENT_DATA:
    memcpy(&g_referee_data.event_data, payload, sizeof(RefEventData_t));
    g_referee_data.has_event_data = 1u;
    break;
  case REF_CMD_REFEREE_WARNING:
    memcpy(&g_referee_data.referee_warning, payload,
           sizeof(RefRefereeWarning_t));
    g_referee_data.has_referee_warning = 1u;
    break;
  case REF_CMD_DART_INFO:
    memcpy(&g_referee_data.dart_info, payload, sizeof(RefDartInfo_t));
    g_referee_data.has_dart_info = 1u;
    break;
  case REF_CMD_GAME_ROBOT_STATE:
    memcpy(&g_referee_data.game_robot_state, payload,
           sizeof(RefGameRobotState_t));
    g_referee_data.has_game_robot_state = 1u;
    if (g_referee_data.expected_robot_id != 0u) {
      g_referee_data.robot_id_matched =
          (uint8_t)(g_referee_data.game_robot_state.robot_id ==
                    g_referee_data.expected_robot_id);
    } else {
      g_referee_data.robot_id_matched = 1u;
    }
    break;
  case REF_CMD_POWER_HEAT_DATA:
    memcpy(&g_referee_data.power_heat_data, payload,
           sizeof(RefPowerHeatData_t));
    g_referee_data.has_power_heat_data = 1u;
    break;
  case REF_CMD_GAME_ROBOT_POS:
    memcpy(&g_referee_data.game_robot_pos, payload, sizeof(RefGameRobotPos_t));
    g_referee_data.has_game_robot_pos = 1u;
    break;
  case REF_CMD_BUFF:
    memcpy(&g_referee_data.buff, payload, sizeof(RefBuff_t));
    g_referee_data.has_buff = 1u;
    break;
  case REF_CMD_ROBOT_HURT:
    memcpy(&g_referee_data.robot_hurt, payload, sizeof(RefRobotHurt_t));
    g_referee_data.has_robot_hurt = 1u;
    break;
  case REF_CMD_SHOOT_DATA:
    memcpy(&g_referee_data.shoot_data, payload, sizeof(RefShootData_t));
    g_referee_data.has_shoot_data = 1u;
    break;
  case REF_CMD_PROJECTILE_ALLOWANCE:
    memcpy(&g_referee_data.projectile_allowance, payload,
           sizeof(RefProjectileAllowance_t));
    g_referee_data.has_projectile_allowance = 1u;
    break;
  case REF_CMD_RFID_STATUS:
    memcpy(&g_referee_data.rfid_status, payload, sizeof(RefRfidStatus_t));
    g_referee_data.has_rfid_status = 1u;
    break;
  case REF_CMD_DART_CLIENT_CMD:
    memcpy(&g_referee_data.dart_client_cmd, payload,
           sizeof(RefDartClientCmd_t));
    g_referee_data.has_dart_client_cmd = 1u;
    break;
  case REF_CMD_GROUND_ROBOT_POSITION:
    memcpy(&g_referee_data.ground_robot_position, payload,
           sizeof(RefGroundRobotPosition_t));
    g_referee_data.has_ground_robot_position = 1u;
    break;
  case REF_CMD_RADAR_MARK_DATA:
    memcpy(&g_referee_data.radar_mark_data, payload,
           sizeof(RefRadarMarkData_t));
    g_referee_data.has_radar_mark_data = 1u;
    break;
  case REF_CMD_SENTRY_INFO:
    memcpy(&g_referee_data.sentry_info, payload, sizeof(RefSentryInfo_t));
    g_referee_data.has_sentry_info = 1u;
    break;
  case REF_CMD_RADAR_INFO:
    memcpy(&g_referee_data.radar_info, payload, sizeof(RefRadarInfo_t));
    g_referee_data.has_radar_info = 1u;
    break;
  default:
    break;
  }
}

void RefereeParser_Init(uint8_t expected_robot_id) {
  memset(&g_referee_data, 0, sizeof(g_referee_data));
  g_referee_data.expected_robot_id = expected_robot_id;
  g_referee_data.robot_id_matched = 0u;
  g_rx_cache_len = 0u;
}

void RefereeParser_ParseBytes(const uint8_t *buf, uint16_t len) {
  uint16_t offset = 0u;

  if (buf == 0 || len == 0u) {
    return;
  }

  if (len > (uint16_t)(REFEREE_RX_CACHE_SIZE - g_rx_cache_len)) {
    g_rx_cache_len = 0u;
  }

  if (len > REFEREE_RX_CACHE_SIZE) {
    buf += (len - REFEREE_RX_CACHE_SIZE);
    len = REFEREE_RX_CACHE_SIZE;
  }

  memcpy(&g_rx_cache[g_rx_cache_len], buf, len);
  g_rx_cache_len = (uint16_t)(g_rx_cache_len + len);

  while ((uint16_t)(g_rx_cache_len - offset) >= REFEREE_MIN_FRAME_LEN) {
    uint16_t data_len;
    uint16_t frame_len;
    uint16_t cmd_id;
    const uint8_t *frame;
    const uint8_t *payload;

    while (offset < g_rx_cache_len && g_rx_cache[offset] != REFEREE_SOF) {
      ++offset;
    }
    if ((uint16_t)(g_rx_cache_len - offset) < REFEREE_MIN_FRAME_LEN) {
      break;
    }

    frame = &g_rx_cache[offset];
    if (!Referee_CRC8_Verify(frame, REFEREE_HEADER_LEN)) {
      ++offset;
      continue;
    }

    data_len = (uint16_t)frame[1] | (uint16_t)((uint16_t)frame[2] << 8);
    frame_len = (uint16_t)(REFEREE_HEADER_LEN + REFEREE_CMD_ID_LEN + data_len +
                           REFEREE_TAIL_LEN);
    if (frame_len < REFEREE_MIN_FRAME_LEN ||
        frame_len > REFEREE_MAX_FRAME_LEN) {
      ++offset;
      continue;
    }
    if ((uint16_t)(g_rx_cache_len - offset) < frame_len) {
      break;
    }
    if (!Referee_CRC16_Verify(frame, frame_len)) {
      ++offset;
      continue;
    }

    cmd_id = (uint16_t)frame[5] | (uint16_t)((uint16_t)frame[6] << 8);
    payload = &frame[7];
    RefereeParser_HandleFrame(cmd_id, payload, data_len);
    offset = (uint16_t)(offset + frame_len);
  }

  if (offset > 0u) {
    if (offset < g_rx_cache_len) {
      memmove(g_rx_cache, &g_rx_cache[offset],
              (size_t)(g_rx_cache_len - offset));
      g_rx_cache_len = (uint16_t)(g_rx_cache_len - offset);
    } else {
      g_rx_cache_len = 0u;
    }
  }
}

const RefereeData_t *RefereeParser_GetData(void) { return &g_referee_data; }

uint8_t RefereeParser_IsOnline(void) {
  if (g_referee_data.last_rx_tick_ms == 0u) {
    return 0u;
  }
  if ((uint32_t)(HAL_GetTick() - g_referee_data.last_rx_tick_ms) >
      REFEREE_OFFLINE_TIMEOUT_MS) {
    g_referee_data.online = 0u;
    return 0u;
  }
  return 1u;
}
