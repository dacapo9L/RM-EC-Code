#include "vision_decision.h"

#include "bsp_sbus.h"
#include "shoot_control.h"
#include "vision_uart.h"
#include "main.h"

#include <string.h>

/* ================= 参数 ================= */

/* 目标进入这个窗口才算瞄准到位 */
#define FIRE_WINDOW_X_PX          16u
#define FIRE_WINDOW_Y_PX          14u

/* 连续多少次视觉更新满足窗口，才允许开火 */
#define FIRE_STABLE_COUNT_TH      3u

/* 8 秒内最多 8 发 */
#define FIRE_WINDOW_TIME_MS       8000u
#define FIRE_WINDOW_MAX_SHOTS     8u

/* 图像中心，需和 vision_uart.c 保持一致 */
#define VISION_IMAGE_WIDTH        640u
#define VISION_IMAGE_HEIGHT       480u
#define VISION_CENTER_X           (VISION_IMAGE_WIDTH / 2u)
#define VISION_CENTER_Y           (VISION_IMAGE_HEIGHT / 2u)

/* ================= 内部状态 ================= */

typedef struct {
    uint8_t aim_ready;
    uint8_t stable_ready;
    uint8_t rate_limited;

    uint8_t stable_cnt;

    uint32_t shot_ticks[FIRE_WINDOW_MAX_SHOTS];
    uint8_t shot_count;
} vision_decision_ctx_t;

static vision_decision_ctx_t g_vdec;

/* ================= 工具函数 ================= */

static uint8_t Remote_IsOnline(void)
{
    const SBUS_Data_t *sbus = SBUS_Get_Data();
    if (sbus == NULL) {
        return 0u;
    }

    /* 如果你的字段名不是 connect_status，就改成你工程里的真实在线标志 */
    return sbus->connect_status ? 1u : 0u;
}

static void ShotWindow_Prune(uint32_t now)
{
    uint8_t write_idx = 0;

    for (uint8_t i = 0; i < g_vdec.shot_count; i++) {
        uint32_t dt = now - g_vdec.shot_ticks[i];
        if (dt < FIRE_WINDOW_TIME_MS) {
            g_vdec.shot_ticks[write_idx++] = g_vdec.shot_ticks[i];
        }
    }

    g_vdec.shot_count = write_idx;
}

static uint8_t ShotWindow_CanFire(uint32_t now)
{
    ShotWindow_Prune(now);
    return (g_vdec.shot_count < FIRE_WINDOW_MAX_SHOTS) ? 1u : 0u;
}

static void ShotWindow_RecordFire(uint32_t now)
{
    ShotWindow_Prune(now);

    if (g_vdec.shot_count < FIRE_WINDOW_MAX_SHOTS) {
        g_vdec.shot_ticks[g_vdec.shot_count++] = now;
    }
}

static uint8_t AimWindow_Check(const VisionPacket_t *pkt)
{
    uint16_t x_err = (pkt->x > VISION_CENTER_X) ? (pkt->x - VISION_CENTER_X) : (VISION_CENTER_X - pkt->x);
    uint16_t y_err = (pkt->y > VISION_CENTER_Y) ? (pkt->y - VISION_CENTER_Y) : (VISION_CENTER_Y - pkt->y);

    if (x_err <= FIRE_WINDOW_X_PX && y_err <= FIRE_WINDOW_Y_PX) {
        return 1u;
    }
    return 0u;
}

/* ================= 对外接口 ================= */

void Vision_Decision_Init(void)
{
    memset(&g_vdec, 0, sizeof(g_vdec));
}

void Vision_Decision_Update(void)
{
    VisionPacket_t pkt;
    uint32_t now = HAL_GetTick();

    g_vdec.aim_ready = 0u;
    g_vdec.stable_ready = 0u;

    /* 先刷新限频状态 */
    g_vdec.rate_limited = !ShotWindow_CanFire(now);

    /* 条件1：遥控在线即可授权自动开火 */
    if (!Remote_IsOnline()) {
        g_vdec.stable_cnt = 0u;
        return;
    }

    /* 条件2：视觉在线 */
    if (!Vision_UART_IsOnline()) {
        g_vdec.stable_cnt = 0u;
        return;
    }

    /* 条件3：本周期有新视觉包 */
    if (!Vision_UART_PeekLatest(&pkt)) {
        return;
    }

    /* 条件4：必须有目标 */
    if (((pkt.flags & VISION_FLAG_HAS_TARGET) == 0u) || pkt.digit == 0xFFu) {
        g_vdec.stable_cnt = 0u;
        return;
    }

    /* 条件5：摩擦轮必须就绪 */
    if (!Shoot_Is_Friction_Ready()) {
        g_vdec.stable_cnt = 0u;
        return;
    }

    /* 如果当前人为按下了手动连发，手动优先 */
    if (Shoot_Get_Mode() == SHOOT_MODE_BURST) {
        g_vdec.stable_cnt = 0u;
        return;
    }

    /* 窗口判定 */
    if (AimWindow_Check(&pkt)) {
        g_vdec.aim_ready = 1u;

        if (g_vdec.stable_cnt < 255u) {
            g_vdec.stable_cnt++;
        }
    } else {
        g_vdec.stable_cnt = 0u;
    }

    if (g_vdec.stable_cnt >= FIRE_STABLE_COUNT_TH) {
        g_vdec.stable_ready = 1u;
    }

    g_vdec.rate_limited = !ShotWindow_CanFire(now);

    /* 满足条件则自动触发一发 */
    if (g_vdec.aim_ready &&
        g_vdec.stable_ready &&
        !g_vdec.rate_limited) {

        Shoot_Set_Mode(SHOOT_MODE_SINGLE);
        Shoot_Trigger_Single();

        /* 记录本次射击时间戳，限制 8 秒 8 发 */
        ShotWindow_RecordFire(now);
    }
}

uint8_t Vision_Decision_AimReady(void)
{
    return g_vdec.aim_ready;
}

uint8_t Vision_Decision_StableReady(void)
{
    return g_vdec.stable_ready;
}

uint8_t Vision_Decision_RateLimited(void)
{
    return g_vdec.rate_limited;
}

