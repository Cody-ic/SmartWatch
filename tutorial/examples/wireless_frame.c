/* 教学例程：外层格式与当前手表一致；在主循环中逐字节调用。 */
#include <stdint.h>
#include <stddef.h>

/* 调用 receive_byte() 返回 1 后，先使用这三个结果，再喂下一个字节。 */
uint8_t frame_type;
uint8_t frame_length;
uint8_t frame_payload[124];

/* 返回实际帧长；参数或空间不足时返回 0。长度是载荷字节数。 */
size_t pack(uint8_t type, const uint8_t *payload, uint8_t length,
            uint8_t *out, size_t capacity)
{
    if (out == NULL || length > 124 || capacity < (size_t)length + 4)
        return 0;
    if (length != 0 && payload == NULL) return 0;
    out[0] = 0xAA;
    out[1] = type;
    out[2] = length;
    for (uint8_t i = 0; i < length; i++) out[3 + i] = payload[i];
    uint8_t sum = 0;
    for (size_t i = 0; i < (size_t)length + 3; i++) sum += out[i];
    out[3 + length] = (uint8_t)(0U - sum);
    return (size_t)length + 4;
}

/* 状态只表示“下一个字节应该放在哪个字段”。 */
enum { WAIT_HEADER, WAIT_TYPE, WAIT_LENGTH, WAIT_PAYLOAD, WAIT_CHECKSUM };
static uint8_t state = WAIT_HEADER;
static uint8_t index_in_payload;
static uint8_t running_sum;

/* 丢字节、缓冲区溢出或半帧超时后调用，重新寻找帧头。 */
void reset_receiver(void)
{
    state = WAIT_HEADER;
    index_in_payload = 0;
    running_sum = 0;
}

/* 返回 1：刚得到一条校验正确的消息；返回 0：继续等后续字节。 */
int receive_byte(uint8_t byte)
{
    switch (state) {
    case WAIT_HEADER:
        if (byte == 0xAA) {
            running_sum = byte;
            state = WAIT_TYPE;
        }
        break;
    case WAIT_TYPE:
        frame_type = byte;
        running_sum += byte;
        state = WAIT_LENGTH;
        break;
    case WAIT_LENGTH:
        if (byte > sizeof(frame_payload)) {
            reset_receiver();
            break;
        }
        frame_length = byte;
        index_in_payload = 0;
        running_sum += byte;
        state = byte == 0 ? WAIT_CHECKSUM : WAIT_PAYLOAD;
        break;
    case WAIT_PAYLOAD:
        frame_payload[index_in_payload++] = byte;
        running_sum += byte;
        if (index_in_payload == frame_length) state = WAIT_CHECKSUM;
        break;
    case WAIT_CHECKSUM: {
        int valid = (uint8_t)(running_sum + byte) == 0;
        reset_receiver();
        return valid;
    }
    default:
        reset_receiver();
        break;
    }
    return 0;
}
