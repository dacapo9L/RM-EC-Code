#include "referee_service.h"

#include "robot_config.h"

static uint8_t Referee_ResolveExpectedRobotId(void) {
  uint8_t index = (uint8_t)REF_INFANTRY_INDEX;
  if (index < 3u || index > 5u) {
    index = 3u;
  }
#if REF_TEAM_COLOR == REF_TEAM_COLOR_BLUE
  return (uint8_t)(100u + index);
#else
  return index;
#endif
}

void Referee_Init(void) {
  uint8_t expected_robot_id = Referee_ResolveExpectedRobotId();
  RefereeParser_Init(expected_robot_id);
}

void Referee_OnUartRx(const uint8_t *buf, uint16_t len) {
  RefereeParser_ParseBytes(buf, len);
}

const RefereeData_t *Referee_GetData(void) { return RefereeParser_GetData(); }

uint8_t Referee_IsOnline(void) { return RefereeParser_IsOnline(); }
