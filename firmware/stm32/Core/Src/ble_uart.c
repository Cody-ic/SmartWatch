/**
 * @file    ble_uart.c
 * @brief   BLE UART 驱动实现 — USART1 中断接收 + 环形缓冲 + 数据包解析
 *          [2026-07-10]: PCB 重做, 底层从 LPUART1 迁到 USART1(PA9/PA10, AF7)。
 */

#include "ble_uart.h"
#include "rtc_time.h"  /* [2026-07-03]: BLE 授时 → RTC 校正 */
#include "app.h"       /* [2026-07-04]: 用户参数下发 → 运动评分模型 */
#include "module_flags.h"  /* [2026-07-06]: 模块开关 0x80/0x02 */
#include "proto_model.h"   /* [2026-07-12]: 原型网络存储 */
#include <string.h>

/* USART1 句柄（CubeMX 在 usart.c 中定义） [2026-07-10]: LPUART1→USART1 */
extern UART_HandleTypeDef huart1;

/* ============================================================
 * 接收环形缓冲区
 * ============================================================ */
static uint8_t  rx_ring[BLE_RX_BUF_SIZE];
static volatile uint16_t rx_head = 0;   /* ISR 写入位置 */
/* [2026-07-04 Fix]: rx_tail 加 volatile — ISR (line 86) 读取此值判断缓冲是否满,
 *   但任务上下文 (ble_uart_process) 写入。非 volatile 时编译器可缓存 ISR 中的读取值。
 *   交叉验证: C11 §5.1.2.3 + GCC ARM 优化行为 + FreeRTOS ISR↔Task 共享变量规范 */
static volatile uint16_t rx_tail = 0;
static volatile uint8_t  rx_overflow = 0;
static uint8_t rx_byte;

/* 发送缓冲区 */
static uint8_t  tx_buf[BLE_TX_BUF_SIZE];

/* 连接状态 */
static volatile uint8_t ble_connected = 0;
static volatile uint32_t last_rx_tick = 0;  /* ISR 写入，任务读取，须 volatile */

/* [2026-07-03]: 帧解析状态机 — 替代原有的 (void)byte 丢弃逻辑 */
typedef enum {
    RX_WAIT_HEADER = 0,
    RX_WAIT_TYPE,
    RX_WAIT_LEN,
    RX_WAIT_PAYLOAD,
    RX_WAIT_CHECKSUM
} rx_state_t;

static rx_state_t rx_state = RX_WAIT_HEADER;
static uint8_t   rx_frame_type;
static uint8_t   rx_frame_len;
static uint8_t   rx_frame_payload[124];  /* BLE_TX_BUF_SIZE - 4 = 124 */
static uint8_t   rx_frame_idx;          /* 当前 payload 已读字节数 */

/* Restore v2: BEGIN(transaction ID, CRC32), then acknowledged fragments. */
#define PROTO_FRAG_DATA_MAX 122
#define PROTO_FULL_SIZE (2 + BLE_PROTO_N_CLASSES * BLE_PROTO_DIM * 4)
#define PROTO_FRAG_COUNT ((PROTO_FULL_SIZE + PROTO_FRAG_DATA_MAX - 1) / PROTO_FRAG_DATA_MAX)
#define PROTO_TIMEOUT_MS 5000U
static uint8_t proto_rx_buf[PROTO_FULL_SIZE];
static uint32_t proto_rx_frag_mask;
static uint32_t proto_rx_id, proto_rx_crc, proto_rx_tick;
static uint8_t proto_rx_session, proto_rx_complete;

static uint32_t proto_crc32(const uint8_t *data, uint16_t len)
{
    uint32_t crc = 0xFFFFFFFFU;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
    }
    return ~crc;
}

static void proto_ack(uint8_t index, uint8_t status)
{
    uint8_t ack[6];
    memcpy(ack, &proto_rx_id, 4);
    ack[4] = index;
    ack[5] = status; /* 0=accepted, 1=committed, 2=rejected */
    ble_uart_send(BLE_MSG_PROTO_ACK, ack, sizeof(ack));
}

static void proto_restore_fragment(const uint8_t *payload, uint8_t len)
{
    uint8_t idx = payload[0];
    if (!proto_rx_session || len < 2 || payload[1] != PROTO_FRAG_COUNT || idx >= PROTO_FRAG_COUNT) {
        proto_ack(idx, 2);
        return;
    }
    uint16_t off = idx * PROTO_FRAG_DATA_MAX;
    uint16_t count = PROTO_FULL_SIZE - off;
    if (count > PROTO_FRAG_DATA_MAX) count = PROTO_FRAG_DATA_MAX;
    if (len != count + 2) { proto_ack(idx, 2); return; }
    uint32_t bit = 1U << idx;
    if (proto_rx_frag_mask & bit) {
        /* Identical retransmissions are idempotent, including a lost final ACK. */
        if (memcmp(proto_rx_buf + off, payload + 2, count) != 0) {
            proto_rx_session = 0;
            proto_ack(idx, 2);
            return;
        }
    } else {
        memcpy(proto_rx_buf + off, payload + 2, count);
        proto_rx_frag_mask |= bit;
    }
    proto_rx_tick = HAL_GetTick();
    if (!proto_rx_complete && proto_rx_frag_mask == ((1U << PROTO_FRAG_COUNT) - 1U)) {
        if (proto_rx_buf[0] != BLE_PROTO_FULL ||
            proto_crc32(proto_rx_buf, sizeof(proto_rx_buf)) != proto_rx_crc ||
            !proto_model_set_raw(proto_rx_buf + 2, proto_rx_buf[1])) {
            proto_rx_session = 0;
            proto_ack(idx, 2);
            return;
        }
        proto_rx_complete = 1;
    }
    proto_ack(idx, proto_rx_complete ? 1 : 0);
}

/* 前向声明 */
static void ble_uart_dispatch(uint8_t type, const uint8_t *payload, uint8_t len);

/* ============================================================
 * 工具函数
 * ============================================================ */

static uint8_t checksum(const uint8_t *data, int len)
{
    uint32_t sum = 0;
    for (int i = 0; i < len; i++) sum += data[i];
    return (uint8_t)((~sum) + 1);
}

/* ============================================================
 * 初始化
 * ============================================================ */

void ble_uart_init(void)
{
    rx_head = 0;
    rx_tail = 0;
    rx_overflow = 0;
    ble_connected = 0;
    last_rx_tick = 0;
    rx_state = RX_WAIT_HEADER;
    proto_rx_session = 0;
    proto_rx_frag_mask = 0;

    /* 启动中断接收：每次收 1 字节，ISR 中继续 */
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
}

/* ============================================================
 * USART1 中断回调
 * ============================================================ */

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    /* [2026-07-10 Fix]: LPUART1→USART1。BLE 迁移 #3 遗漏此处 Instance 判断:
     *   句柄已改 huart1/Instance=USART1 (usart.c) 且中断走 USART1_IRQHandler→
     *   HAL_UART_IRQHandler(&huart1) (stm32u5xx_it.c), 但此回调仍比较 LPUART1,
     *   导致 huart->Instance(=USART1) != LPUART1 恒真 → 直接 return, BLE 接收全废
     *   (时间同步/模块开关/proto restore 全部收不到)。手改, 勿重生成。 */
    if (huart->Instance != USART1) return;

    last_rx_tick = HAL_GetTick();
    ble_connected = 1;   /* 收到任意数据即认为已连接 */

    /* 环形缓冲写入下一字节 */
    uint16_t next = (rx_head + 1) % BLE_RX_BUF_SIZE;
    if (next != rx_tail) {
        rx_ring[rx_head] = rx_byte;
        rx_head = next;
    } else {
        rx_overflow = 1;  /* 缓冲满，丢弃——主循环在 ble_uart_process 中可读取 */
        /* [2026-07-04 Fix]: 溢出时告警，但继续接收 (下次字节覆盖当前位置) */
    }

    /* 继续接收下一个字节 */
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
}

/* ============================================================
 * 接收处理（主循环中轮询调用）
 * ============================================================ */

void ble_uart_process(void)
{
    /* 超时检测：5 秒无数据认为断开 */
    if (ble_connected && (HAL_GetTick() - last_rx_tick > 5000)) {
        ble_connected = 0;
    }

    if (proto_rx_session && (uint32_t)(HAL_GetTick() - proto_rx_tick) > PROTO_TIMEOUT_MS)
        proto_rx_session = 0;
    if (rx_overflow) {
        /* Discard damaged traffic atomically; the sender must start a new transfer. */
        uint32_t irq = __get_PRIMASK();
        __disable_irq();
        rx_tail = rx_head;
        rx_overflow = 0;
        __set_PRIMASK(irq);
        rx_state = RX_WAIT_HEADER;
        proto_rx_session = 0;
        return;
    }
    if (rx_state != RX_WAIT_HEADER && (uint32_t)(HAL_GetTick() - last_rx_tick) > 100)
        rx_state = RX_WAIT_HEADER;

    /* 环形缓冲无数据则返回 */
    if (rx_head == rx_tail) return;

    /* [2026-07-03]: 0xAA 帧协议解析 — 替代原有的 (void)byte 丢弃 */
    while (rx_tail != rx_head) {
        uint8_t byte = rx_ring[rx_tail];
        rx_tail = (rx_tail + 1) % BLE_RX_BUF_SIZE;

        switch (rx_state) {
        case RX_WAIT_HEADER:
            if (byte == BLE_PACKET_HEADER) {
                rx_state = RX_WAIT_TYPE;
            }
            /* else: 跳过非 header 字节 (未对齐/噪声) */
            break;

        case RX_WAIT_TYPE:
            rx_frame_type = byte;
            rx_state = RX_WAIT_LEN;
            break;

        case RX_WAIT_LEN:
            rx_frame_len = byte;
            if (rx_frame_len > sizeof(rx_frame_payload)) {
                /* payload 超长 → 丢弃此帧, 回到同步头搜索 */
                rx_state = RX_WAIT_HEADER;
            } else if (rx_frame_len == 0) {
                /* 空 payload → 直接进入校验 */
                rx_state = RX_WAIT_CHECKSUM;
            } else {
                rx_frame_idx = 0;
                rx_state = RX_WAIT_PAYLOAD;
            }
            break;

        case RX_WAIT_PAYLOAD:
            rx_frame_payload[rx_frame_idx++] = byte;
            if (rx_frame_idx >= rx_frame_len) {
                rx_state = RX_WAIT_CHECKSUM;
            }
            break;

        case RX_WAIT_CHECKSUM: {
            /* [2026-07-04 Fix]: 原为 VLA verify[3+rx_frame_len] (C99), 最长 127B。
             *   改用静态最大尺寸缓冲区 — MISRA C:2012 Rule 18.8 禁止 VLA。
             *   rx_frame_len 有效范围已在 LEN case 中限制为 ≤124。 */
            static uint8_t verify[127];  /* max: header(1)+type(1)+len(1)+payload(124) */
            verify[0] = BLE_PACKET_HEADER;
            verify[1] = rx_frame_type;
            verify[2] = rx_frame_len;
            if (rx_frame_len > 0) {
                memcpy(&verify[3], rx_frame_payload, rx_frame_len);
            }
            /* [2026-07-04 Fix]: VLA→static后 sizeof(verify)=127 恒等, 须用实际长度
             *   3+rx_frame_len 匹配发送端 (ble_uart_send:220 checksum(tx_buf,3+len)) */
            uint8_t expected = checksum(verify, 3 + rx_frame_len);
            if (byte == expected) {
                /* 校验通过 → 分发处理 */
                ble_uart_dispatch(rx_frame_type, rx_frame_payload, rx_frame_len);
            }
            /* 校验失败也回到 header 搜索，防止死锁在错误帧 */
            rx_state = RX_WAIT_HEADER;
            break;
        }
        } /* end switch */
    }
}

/* ============================================================
 * 帧分发实现
 * ============================================================ */

static void ble_uart_dispatch(uint8_t type, const uint8_t *payload, uint8_t len)
{
    (void)len;  /* 长度断言在各 case 中 */
    switch (type) {
    case 0x80:  /* BLE_MSG_CMD_RESP — 手机下发的指令 */
        if (len >= 1) {
            switch (payload[0]) {
            case 0x01:  /* [2026-07-04]: 用户参数配置 */
                if (len == 6) {
                    float weight_kg;
                    uint8_t age;
                    memcpy(&weight_kg, &payload[1], 4);
                    age = payload[5];
                    app_set_user_profile(weight_kg, age);
                    /* [2026-07-04]: ACK 回传确认值——手机可对比校验下发是否生效 */
                    ble_uart_send_user_config(weight_kg, age);
                }
                break;
            case 0x02:  /* [2026-07-06]: 功能模块按需开关 */
                if (len == 3) {
                    uint16_t flags;
                    memcpy(&flags, &payload[1], 2);  /* uint16 LE */
                    module_flags_set(flags);
                    /* ACK: 回传确认值 */
                    ble_uart_send(BLE_MSG_CMD_RESP, payload, len);
                }
                break;
            default:
                break;
            }
        }
        break;
    case 0x0C:  /* [2026-07-05]: BLE_MSG_SESSION_CTRL — 手机下发 session 控制
                 * [2026-07-14]: 实现 — 追踪单个活动类的 embedding, 结束时更新原型 */
        if (len >= 1) {
            switch (payload[0]) {
            case BLE_SESSION_START:
                if (len >= 2 && payload[1] < BLE_PROTO_N_CLASSES)
                    proto_session_start(payload[1]);
                break;
            case BLE_SESSION_END:
                proto_session_end();
                break;
            case BLE_SESSION_ABORT:
                proto_session_abort();
                break;
            }
        }
        break;
    case BLE_MSG_PROTO_RESTORE:
        if (len == 9 && payload[0] == BLE_PROTO_BEGIN) {
            uint32_t id, crc;
            memcpy(&id, payload + 1, 4);
            memcpy(&crc, payload + 5, 4);
            if (!proto_rx_session || id != proto_rx_id || crc != proto_rx_crc) {
                proto_rx_frag_mask = 0;
                proto_rx_complete = 0;
                memset(proto_rx_buf, 0, sizeof(proto_rx_buf));
            }
            proto_rx_id = id;
            proto_rx_crc = crc;
            proto_rx_tick = HAL_GetTick();
            proto_rx_session = 1;
            proto_ack(0xFF, 0);
        } else if (len == 1 && payload[0] == BLE_PROTO_FACTORY) {
            proto_rx_session = 0;
            proto_model_init();
        } else if (len == 5 && payload[0] == BLE_PROTO_REQUEST_BACKUP) {
            uint32_t id;
            memcpy(&id, payload + 1, 4);
            ble_uart_send_proto_backup(proto_model_get_version(), proto_model_get_ptr(), id);
        } else if (len >= 2) {
            proto_restore_fragment(payload, len);
        }
        break;
    case 0x91:  /* BLE_MSG_TIME_SYNC — [2026-07-03] 手机授时 */
        if (len == 4) {
            uint32_t unix = ((uint32_t)payload[0] << 24)
                          | ((uint32_t)payload[1] << 16)
                          | ((uint32_t)payload[2] << 8)
                          |  (uint32_t)payload[3];
            rtc_set_unix(unix);
        }
        break;
    default:
        /* 未知类型，忽略 */
        break;
    }
}

/* ============================================================
 * 连接状态
 * ============================================================ */

int ble_uart_is_connected(void)
{
    return ble_connected;
}

/* ============================================================
 * 发送
 * ============================================================ */

int ble_uart_send(ble_msg_type_t type, const uint8_t *payload, uint8_t len)
{
    /* 防溢出：header(1) + type(1) + len(1) + payload + checksum(1) */
    if (len > BLE_TX_BUF_SIZE - 4) return -1;

    tx_buf[0] = BLE_PACKET_HEADER;
    tx_buf[1] = (uint8_t)type;
    tx_buf[2] = len;
    if (len > 0 && payload != NULL) {
        memcpy(&tx_buf[3], payload, len);
    }
    tx_buf[3 + len] = checksum(tx_buf, 3 + len);

    HAL_StatusTypeDef ret = HAL_UART_Transmit(&huart1, tx_buf, 4 + len, 50);
    return (ret == HAL_OK) ? 0 : -1;
}

/* ============================================================
 * 便捷封装
 * ============================================================ */

int ble_uart_send_har(int activity, float confidence)
{
    uint8_t p[5];
    p[0] = (uint8_t)(activity & 0x07);
    uint16_t conf_scaled = (uint16_t)(confidence * 1000.0f);  /* 0.001 精度 */
    memcpy(&p[1], &conf_scaled, 2);
    p[3] = 0;  /* reserved */
    p[4] = 0;
    return ble_uart_send(BLE_MSG_HAR_RESULT, p, 5);
}

int ble_uart_send_hr(float bpm_smoothed, float bpm_raw, int valid)
{
    uint8_t p[5];
    uint16_t s = (uint16_t)(bpm_smoothed * 10.0f);  /* 0.1 精度 */
    uint16_t r = (uint16_t)(bpm_raw * 10.0f);
    memcpy(&p[0], &s, 2);
    memcpy(&p[2], &r, 2);
    p[4] = (uint8_t)(valid ? 1 : 0);
    return ble_uart_send(BLE_MSG_HR_DATA, p, 5);
}

int ble_uart_send_safety(uint8_t hr_anomaly, uint8_t acoustic_alert,
                          uint8_t imu_anomaly)
{
    uint8_t p[8];
    p[0] = hr_anomaly;
    p[1] = acoustic_alert;
    p[2] = imu_anomaly;
    uint32_t now = HAL_GetTick();
    memcpy(&p[3], &now, 4);
    p[7] = 0;  /* reserved */
    return ble_uart_send(BLE_MSG_SAFETY_ALERT, p, 8);
}

int ble_uart_send_steps(uint32_t steps, uint32_t goal, float distance_km, float kcal)
{
    uint8_t p[16];
    memcpy(&p[0], &steps, 4);
    memcpy(&p[4], &goal, 4);
    uint16_t dist = (uint16_t)(distance_km * 1000.0f);  /* 米 */
    uint16_t k    = (uint16_t)(kcal);
    memcpy(&p[8],  &dist, 2);
    memcpy(&p[10], &k,    2);
    p[12] = 0; p[13] = 0; p[14] = 0; p[15] = 0;
    return ble_uart_send(BLE_MSG_STEPS, p, 16);
}

int ble_uart_send_status(float calorie, float score, uint32_t exercise_time_s,
                           int current_act, float current_hr)
{
    uint8_t p[17];
    memcpy(&p[0],  &calorie,         4);
    memcpy(&p[4],  &score,           4);
    memcpy(&p[8],  &exercise_time_s, 4);
    p[12] = (uint8_t)current_act;
    memcpy(&p[13], &current_hr,      4);
    return ble_uart_send(BLE_MSG_STATUS, p, 17);
}

int ble_uart_send_battery(uint16_t mv, uint8_t pct)
{
    uint8_t p[3];
    memcpy(&p[0], &mv, 2);   /* uint16 LE: 电压 mV */
    p[2] = pct;               /* 电量 0-100% */
    return ble_uart_send(BLE_MSG_BATTERY, p, 3);
}

/* [2026-07-04]: 用户参数 ACK — 手机下发后回传确认值 */
int ble_uart_send_user_config(float weight_kg, uint8_t age)
{
    uint8_t p[6];
    p[0] = 0x01;  /* sub-cmd: SET_PROFILE */
    memcpy(&p[1], &weight_kg, 4);
    p[5] = age;
    return ble_uart_send(BLE_MSG_CMD_RESP, p, 6);
}

/* [2026-07-14]: Proto 推理结果 — class(1B) + conf(f32 LE) = 5B */
int ble_uart_send_proto_result(int proto_class, float confidence)
{
    uint8_t p[5];
    p[0] = (uint8_t)(proto_class & 0xFF);
    memcpy(&p[1], &confidence, 4);  /* f32 LE */
    return ble_uart_send(BLE_MSG_PROTO_RESULT, p, sizeof(p));
}

/* [2026-07-05]: 原型状态查询回复 — version(1B) + 7×sample_count(u16 LE) = 15B */
int ble_uart_send_proto_status(uint8_t version, const uint16_t counts[BLE_PROTO_N_CLASSES])
{
    uint8_t p[1 + BLE_PROTO_N_CLASSES * 2];  /* 1 + 14 = 15 */
    p[0] = version;
    for (int i = 0; i < BLE_PROTO_N_CLASSES; i++) {
        memcpy(&p[1 + i * 2], &counts[i], 2);  /* u16 LE */
    }
    return ble_uart_send(BLE_MSG_PROTO_STATUS, p, sizeof(p));
}

/* [2026-07-05]: 原型备份 — sub(0x01)+ver(1B)+7×128 float LE = 3586B
 * [2026-07-06 Fix]: 3586B 远超 124B 单帧上限, 实现分包发送。
 *   每帧: [frag_idx(1B)][total(1B)][chunk(≤122B)]
 *   分帧数 = ceil(3586/122) = 30。接收端按 frag_idx 拼接还原。 */
int ble_uart_send_proto_backup(uint8_t version, const float prototypes[BLE_PROTO_N_CLASSES][BLE_PROTO_DIM], uint32_t transfer_id)
{
    /* [2026-07-12]: static—3586B移出Sensor_Task栈(6KB→剩余1.5KB崩边缘)→BSS(+3.5KB可忽略).
     *   单线程无重入, 与gc9a01.c FillScreen static buf[480]同模式. */
    static uint8_t full[1 + 1 + BLE_PROTO_N_CLASSES * BLE_PROTO_DIM * 4];
    full[0] = BLE_PROTO_FULL;
    full[1] = version;
    for (int c = 0; c < BLE_PROTO_N_CLASSES; c++) {
        memcpy(&full[2 + c * BLE_PROTO_DIM * 4], prototypes[c], BLE_PROTO_DIM * 4);
    }

    uint8_t begin[9] = {BLE_PROTO_BEGIN};
    uint32_t crc = proto_crc32(full, sizeof(full));
    memcpy(begin + 1, &transfer_id, 4);
    memcpy(begin + 5, &crc, 4);
    if (ble_uart_send(BLE_MSG_PROTO_BACKUP, begin, sizeof(begin)) != 0) return -1;

    #define FRAG_DATA_MAX  122   /* 124 - 2B 分帧头 */
    const uint16_t total = sizeof(full);
    const uint8_t  n_frags = (total + FRAG_DATA_MAX - 1) / FRAG_DATA_MAX;
    int ok = 0;

    for (uint8_t i = 0; i < n_frags; i++) {
        uint16_t off = (uint16_t)i * FRAG_DATA_MAX;
        uint8_t  chunk = (off + FRAG_DATA_MAX <= total) ? FRAG_DATA_MAX : (uint8_t)(total - off);
        uint8_t  frag[2 + FRAG_DATA_MAX];
        frag[0] = i;
        frag[1] = n_frags;
        memcpy(&frag[2], &full[off], chunk);
        if (ble_uart_send(BLE_MSG_PROTO_BACKUP, frag, 2 + chunk) == 0) ok++;
    }
    return (ok == n_frags) ? 0 : -1;
}

/* [2026-07-05]: 睡眠呼吸暂停报告 — severity(1B)+odi(4Bf32)+events(2Bu16)
 *   +lowest_spo2(1B)+avg_dur(2Bu16)+monitor_min(2Bu16)+is_asleep(1B) = 13B */
int ble_uart_send_sleep_apnea(uint8_t severity, float odi, uint16_t total_events,
                                uint8_t lowest_spo2, uint16_t avg_dur_s,
                                uint16_t monitor_min, uint8_t is_asleep)
{
    uint8_t p[13];
    p[0] = severity;
    memcpy(&p[1], &odi, 4);        /* float LE */
    memcpy(&p[5], &total_events, 2); /* uint16 LE */
    p[7] = lowest_spo2;
    memcpy(&p[8], &avg_dur_s, 2);  /* uint16 LE */
    memcpy(&p[10], &monitor_min, 2); /* uint16 LE */
    p[12] = is_asleep;
    return ble_uart_send(BLE_MSG_SLEEP_APNEA, p, 13);
}

/* [2026-07-05]: 睡眠分期 — P(sleep)(4B f32 LE) + is_asleep(1B) = 5B */
int ble_uart_send_sleep_stage(float p_sleep, uint8_t is_asleep)
{
    uint8_t p[5];
    memcpy(&p[0], &p_sleep, 4);  /* float LE */
    p[4] = is_asleep;
    return ble_uart_send(BLE_MSG_SLEEP_STAGE, p, 5);
}

/* [2026-07-05]: 实时 SpO2 — spo2(4B f32 LE) + valid(1B) = 5B */
int ble_uart_send_spo2(float spo2, uint8_t valid)
{
    uint8_t p[5];
    memcpy(&p[0], &spo2, 4);
    p[4] = valid;
    return ble_uart_send(BLE_MSG_SPO2_DATA, p, 5);
}

/* ============================================================
 * [2026-07-14]: Proto Session 追踪 — 单个活动类 embedding 累加
 *
 * 手机发 0x0C/0x01+class_id → 开始累积该类的 embedding
 * 每轮 HAR 推理 → proto_session_feed(embedding) 累加 sum[128]+count
 * 手机发 0x0C/0x02 → 算均值 → L2 归一化 → 更新该类原型
 * 手机发 0x0C/0x03 → 丢弃当前累加器
 *
 * RAM: sum[128]=512B + count=4B + class=1B + active=1B + counts[7]=14B = 532B BSS
 * ============================================================ */
static float    g_session_sum[BLE_PROTO_DIM];
static uint32_t g_session_count;
static uint8_t  g_session_class;
static uint8_t  g_session_active;
static uint16_t g_proto_counts[BLE_PROTO_N_CLASSES];

void proto_session_start(uint8_t class_id)
{
    memset(g_session_sum, 0, sizeof(g_session_sum));
    g_session_count = 0;
    g_session_class = class_id;
    g_session_active = 1;
}

void proto_session_feed(const float embedding[BLE_PROTO_DIM])
{
    if (!g_session_active) return;
    for (int i = 0; i < BLE_PROTO_DIM; i++)
        g_session_sum[i] += embedding[i];
    g_session_count++;
}

/* [2026-07-14]: 门控版本 — HAR 类别不匹配或置信度不足时拒绝, 防过渡态污染 */
void proto_session_feed_if(const float embedding[BLE_PROTO_DIM], int har_class, float har_conf)
{
    if (!g_session_active) return;
    if (har_class != (int)g_session_class) return;   /* HAR 不认为是目标类 */
    if (har_conf < 0.7f) return;                     /* 置信度不足 */
    proto_session_feed(embedding);
}

void proto_session_end(void)
{
    if (!g_session_active) { return; }
    g_session_active = 0;

    /* 最少需要 50 个有效样本 (~4.3 分钟连续一致活动), 太少则丢弃 */
    if (g_session_count < 50) {
        g_session_count = 0;
        memset(g_session_sum, 0, sizeof(g_session_sum));
        return;
    }

    /* 算均值 + L2 归一化 → 新原型 */
    float mean[BLE_PROTO_DIM];
    float inv = 1.0f / (float)g_session_count;
    for (int i = 0; i < BLE_PROTO_DIM; i++)
        mean[i] = g_session_sum[i] * inv;

    /* L2 归一化 */
    float norm = 0.0f;
    for (int i = 0; i < BLE_PROTO_DIM; i++) norm += mean[i] * mean[i];
    if (norm > 1e-8f) {
        norm = 1.0f / sqrtf(norm);
        for (int i = 0; i < BLE_PROTO_DIM; i++) mean[i] *= norm;
    }

    /* [2026-07-14]: static 移入 BSS — 3584B 栈数组会压爆 Sensor_Task 6KB 栈 */
    static float protos[BLE_PROTO_N_CLASSES][BLE_PROTO_DIM];
    uint8_t ver;
    proto_model_get(protos, &ver);
    memcpy(protos[g_session_class], mean, sizeof(mean));
    proto_model_set(protos, ver + 1);

    g_proto_counts[g_session_class] += (uint16_t)(g_session_count > 65535 ? 65535 : g_session_count);
    g_session_count = 0;
    memset(g_session_sum, 0, sizeof(g_session_sum));
}

void proto_session_abort(void)
{
    g_session_active = 0;
    /* 丢弃累加器, 不更新原型也不更新计数 */
}

int proto_session_is_active(void) { return g_session_active; }

const uint16_t *proto_session_get_counts(void) { return g_proto_counts; }
