#include "referee_protocol.h"

uint16_t Referee_GetExpectedPayloadLength(uint16_t cmd_id) {
  switch (cmd_id) {
  case REF_CMD_GAME_STATE:
    return REF_LEN_GAME_STATE;
  case REF_CMD_GAME_RESULT:
    return REF_LEN_GAME_RESULT;
  case REF_CMD_GAME_ROBOT_HP:
    return REF_LEN_GAME_ROBOT_HP;
  case REF_CMD_EVENT_DATA:
    return REF_LEN_EVENT_DATA;
  case REF_CMD_REFEREE_WARNING:
    return REF_LEN_REFEREE_WARNING;
  case REF_CMD_DART_INFO:
    return REF_LEN_DART_INFO;
  case REF_CMD_GAME_ROBOT_STATE:
    return REF_LEN_GAME_ROBOT_STATE;
  case REF_CMD_POWER_HEAT_DATA:
    return REF_LEN_POWER_HEAT_DATA;
  case REF_CMD_GAME_ROBOT_POS:
    return REF_LEN_GAME_ROBOT_POS;
  case REF_CMD_BUFF:
    return REF_LEN_BUFF;
  case REF_CMD_ROBOT_HURT:
    return REF_LEN_ROBOT_HURT;
  case REF_CMD_SHOOT_DATA:
    return REF_LEN_SHOOT_DATA;
  case REF_CMD_PROJECTILE_ALLOWANCE:
    return REF_LEN_PROJECTILE_ALLOWANCE;
  case REF_CMD_RFID_STATUS:
    return REF_LEN_RFID_STATUS;
  case REF_CMD_DART_CLIENT_CMD:
    return REF_LEN_DART_CLIENT_CMD;
  case REF_CMD_GROUND_ROBOT_POSITION:
    return REF_LEN_GROUND_ROBOT_POSITION;
  case REF_CMD_RADAR_MARK_DATA:
    return REF_LEN_RADAR_MARK_DATA;
  case REF_CMD_SENTRY_INFO:
    return REF_LEN_SENTRY_INFO;
  case REF_CMD_RADAR_INFO:
    return REF_LEN_RADAR_INFO;
  default:
    return 0u;
  }
}
