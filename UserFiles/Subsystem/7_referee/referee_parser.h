#ifndef __REFEREE_PARSER_H
#define __REFEREE_PARSER_H

#include "referee_protocol.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint8_t expected_robot_id;
  uint8_t robot_id_matched;
  uint8_t online;
  uint16_t last_cmd_id;
  uint32_t last_rx_tick_ms;

  uint8_t has_game_state;
  uint8_t has_game_result;
  uint8_t has_game_robot_hp;
  uint8_t has_event_data;
  uint8_t has_referee_warning;
  uint8_t has_dart_info;
  uint8_t has_game_robot_state;
  uint8_t has_power_heat_data;
  uint8_t has_game_robot_pos;
  uint8_t has_buff;
  uint8_t has_robot_hurt;
  uint8_t has_shoot_data;
  uint8_t has_projectile_allowance;
  uint8_t has_rfid_status;
  uint8_t has_dart_client_cmd;
  uint8_t has_ground_robot_position;
  uint8_t has_radar_mark_data;
  uint8_t has_sentry_info;
  uint8_t has_radar_info;

  RefGameState_t game_state;
  RefGameResult_t game_result;
  RefGameRobotHp_t game_robot_hp;
  RefEventData_t event_data;
  RefRefereeWarning_t referee_warning;
  RefDartInfo_t dart_info;
  RefGameRobotState_t game_robot_state;
  RefPowerHeatData_t power_heat_data;
  RefGameRobotPos_t game_robot_pos;
  RefBuff_t buff;
  RefRobotHurt_t robot_hurt;
  RefShootData_t shoot_data;
  RefProjectileAllowance_t projectile_allowance;
  RefRfidStatus_t rfid_status;
  RefDartClientCmd_t dart_client_cmd;
  RefGroundRobotPosition_t ground_robot_position;
  RefRadarMarkData_t radar_mark_data;
  RefSentryInfo_t sentry_info;
  RefRadarInfo_t radar_info;
} RefereeData_t;

void RefereeParser_Init(uint8_t expected_robot_id);
void RefereeParser_ParseBytes(const uint8_t *buf, uint16_t len);
const RefereeData_t *RefereeParser_GetData(void);
uint8_t RefereeParser_IsOnline(void);

#ifdef __cplusplus
}
#endif

#endif // __REFEREE_PARSER_H
