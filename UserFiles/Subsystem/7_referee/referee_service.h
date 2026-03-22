#ifndef __REFEREE_SERVICE_H
#define __REFEREE_SERVICE_H

#include "referee_parser.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void Referee_Init(void);
void Referee_OnUartRx(const uint8_t *buf, uint16_t len);
const RefereeData_t *Referee_GetData(void);
uint8_t Referee_IsOnline(void);

#ifdef __cplusplus
}
#endif

#endif // __REFEREE_SERVICE_H
