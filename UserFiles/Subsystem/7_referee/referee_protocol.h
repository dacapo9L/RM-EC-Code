#ifndef __REFEREE_PROTOCOL_H
#define __REFEREE_PROTOCOL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define REFEREE_SOF 0xA5u
#define REFEREE_HEADER_LEN 5u
#define REFEREE_CMD_ID_LEN 2u
#define REFEREE_TAIL_LEN 2u
#define REFEREE_MIN_FRAME_LEN                                                  \
  (REFEREE_HEADER_LEN + REFEREE_CMD_ID_LEN + REFEREE_TAIL_LEN)
#define REFEREE_MAX_FRAME_LEN 128u

#pragma pack(push, 1)

typedef struct {
  uint8_t sof;
  uint16_t data_length;
  uint8_t seq;
  uint8_t crc8;
} RefereeFrameHeader_t;

typedef enum {
  REF_CMD_GAME_STATE = 0x0001,
  REF_CMD_GAME_RESULT = 0x0002,
  REF_CMD_GAME_ROBOT_HP = 0x0003,
  REF_CMD_EVENT_DATA = 0x0101,
  REF_CMD_REFEREE_WARNING = 0x0104,
  REF_CMD_DART_INFO = 0x0105,
  REF_CMD_GAME_ROBOT_STATE = 0x0201,
  REF_CMD_POWER_HEAT_DATA = 0x0202,
  REF_CMD_GAME_ROBOT_POS = 0x0203,
  REF_CMD_BUFF = 0x0204,
  REF_CMD_ROBOT_HURT = 0x0206,
  REF_CMD_SHOOT_DATA = 0x0207,
  REF_CMD_PROJECTILE_ALLOWANCE = 0x0208,
  REF_CMD_RFID_STATUS = 0x0209,
  REF_CMD_DART_CLIENT_CMD = 0x020A,
  REF_CMD_GROUND_ROBOT_POSITION = 0x020B,
  REF_CMD_RADAR_MARK_DATA = 0x020C,
  REF_CMD_SENTRY_INFO = 0x020D,
  REF_CMD_RADAR_INFO = 0x020E,
  REF_CMD_STUDENT_INTERACTIVE = 0x0301,
} RefereeCmdId_e;

enum {
  REF_LEN_GAME_STATE = 11,
  REF_LEN_GAME_RESULT = 1,
  REF_LEN_GAME_ROBOT_HP = 16,
  REF_LEN_EVENT_DATA = 4,
  REF_LEN_REFEREE_WARNING = 3,
  REF_LEN_DART_INFO = 3,
  REF_LEN_GAME_ROBOT_STATE = 13,
  REF_LEN_POWER_HEAT_DATA = 14,
  REF_LEN_GAME_ROBOT_POS = 12,
  REF_LEN_BUFF = 8,
  REF_LEN_ROBOT_HURT = 1,
  REF_LEN_SHOOT_DATA = 7,
  REF_LEN_PROJECTILE_ALLOWANCE = 8,
  REF_LEN_RFID_STATUS = 5,
  REF_LEN_DART_CLIENT_CMD = 6,
  REF_LEN_GROUND_ROBOT_POSITION = 40,
  REF_LEN_RADAR_MARK_DATA = 2,
  REF_LEN_SENTRY_INFO = 6,
  REF_LEN_RADAR_INFO = 1,
};

typedef struct {
  uint8_t game_type_progress;
  uint16_t stage_remain_time;
  uint64_t sync_time_stamp;
} RefGameState_t;

typedef struct {
  uint8_t winner;
} RefGameResult_t;

typedef struct {
  uint16_t ally_1_robot_hp;
  uint16_t ally_2_robot_hp;
  uint16_t ally_3_robot_hp;
  uint16_t ally_4_robot_hp;
  uint16_t reserved;
  uint16_t ally_7_robot_hp;
  uint16_t ally_outpost_hp;
  uint16_t ally_base_hp;
} RefGameRobotHp_t;

typedef struct {
  uint32_t event_data;
} RefEventData_t;

typedef struct {
  uint8_t level;
  uint8_t offending_robot_id;
  uint8_t count;
} RefRefereeWarning_t;

typedef struct {
  uint8_t dart_remaining_time;
  uint16_t dart_info;
} RefDartInfo_t;

typedef struct {
  uint8_t robot_id;
  uint8_t robot_level;
  uint16_t current_hp;
  uint16_t maximum_hp;
  uint16_t shooter_barrel_cooling_value;
  uint16_t shooter_barrel_heat_limit;
  uint16_t chassis_power_limit;
  uint8_t power_management_flags;
} RefGameRobotState_t;

typedef struct {
  uint16_t reserved_0;
  uint16_t reserved_1;
  float reserved_2;
  uint16_t buffer_energy;
  uint16_t shooter_17mm_barrel_heat;
  uint16_t shooter_42mm_barrel_heat;
} RefPowerHeatData_t;

typedef struct {
  float x;
  float y;
  float angle;
} RefGameRobotPos_t;

typedef struct {
  uint8_t recovery_buff;
  uint16_t cooling_buff;
  uint8_t defence_buff;
  uint8_t vulnerability_buff;
  uint16_t attack_buff;
  uint8_t remaining_energy;
} RefBuff_t;

typedef struct {
  uint8_t armor_id_hp_deduction_reason;
} RefRobotHurt_t;

typedef struct {
  uint8_t bullet_type;
  uint8_t shooter_number;
  uint8_t launching_frequency;
  float initial_speed;
} RefShootData_t;

typedef struct {
  uint16_t projectile_allowance_17mm;
  uint16_t projectile_allowance_42mm;
  uint16_t remaining_gold_coin;
  uint16_t projectile_allowance_fortress;
} RefProjectileAllowance_t;

typedef struct {
  uint32_t rfid_status;
  uint8_t rfid_status_2;
} RefRfidStatus_t;

typedef struct {
  uint8_t dart_launch_opening_status;
  uint8_t reserved;
  uint16_t target_change_time;
  uint16_t latest_launch_cmd_time;
} RefDartClientCmd_t;

typedef struct {
  float hero_x;
  float hero_y;
  float engineer_x;
  float engineer_y;
  float standard_3_x;
  float standard_3_y;
  float standard_4_x;
  float standard_4_y;
  float reserved_0;
  float reserved_1;
} RefGroundRobotPosition_t;

typedef struct {
  uint16_t mark_progress;
} RefRadarMarkData_t;

typedef struct {
  uint32_t sentry_info;
  uint16_t sentry_info_2;
} RefSentryInfo_t;

typedef struct {
  uint8_t radar_info;
} RefRadarInfo_t;

uint16_t Referee_GetExpectedPayloadLength(uint16_t cmd_id);

#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif // __REFEREE_PROTOCOL_2026_H
