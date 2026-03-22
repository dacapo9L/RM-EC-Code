#ifndef __VISION_DECISION_H
#define __VISION_DECISION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void Vision_Decision_Init(void);
void Vision_Decision_Update(void);

uint8_t Vision_Decision_AimReady(void);
uint8_t Vision_Decision_StableReady(void);
uint8_t Vision_Decision_RateLimited(void);

#ifdef __cplusplus
}
#endif

#endif
