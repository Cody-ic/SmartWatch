/* Runs the production driver against a banked I2C/register model.
 * This checks upload, routing and failure handling, not classification accuracy.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "LSM6DSOX.h"

static unsigned checks;
#define CHECK(condition) do { \
    checks++; \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(1); \
    } \
} while (0)

struct expected_block {
    uint16_t start;
    const uint8_t *bytes;
    size_t length;
};
#include "lsm6dsox_mlc_expected.inc"

struct page_write {
    uint16_t address;
    uint8_t value;
};

static I2C_HandleTypeDef bus = { (void *)1 };
static uint8_t user_regs[256], embedded_regs[256], hub_regs[256];
static uint8_t page_memory[4096];
static uint8_t bank;
static uint32_t ticks;
static struct page_write page_writes[1024];
static size_t page_write_count;
static unsigned embedded_output_reads, wake_source_reads;
static unsigned restore_attempts, page_write_attempts;

/* One persistent failure exhausts the driver's three retries. */
static struct {
    int enabled, is_read, bank, reg, value, accept_then_error;
    unsigned skip, remaining;
} fault;

static uint8_t *registers(void)
{
    if (bank & 0x80) return embedded_regs;
    if (bank & 0x40) return hub_regs;
    return user_regs;
}

static int fail_transaction(int is_read, uint16_t reg, uint8_t value)
{
    if (!fault.enabled || fault.remaining == 0 || fault.is_read != is_read ||
        fault.bank != bank || fault.reg != reg ||
        (fault.value >= 0 && fault.value != value)) return 0;
    if (fault.skip) {
        fault.skip--;
        return 0;
    }
    fault.remaining--;
    return 1;
}

static void inject_fault(int is_read, int in_bank, int reg, int value, unsigned skip)
{
    fault.enabled = 1;
    fault.is_read = is_read;
    fault.bank = in_bank;
    fault.reg = reg;
    fault.value = value;
    fault.accept_then_error = 0;
    fault.skip = skip;
    fault.remaining = 3;
}

static void reset_chip(void)
{
    memset(user_regs, 0, sizeof(user_regs));
    memset(embedded_regs, 0, sizeof(embedded_regs));
    memset(hub_regs, 0, sizeof(hub_regs));
    memset(page_memory, 0xA5, sizeof(page_memory));
    user_regs[0x0F] = 0x6C;
    bank = 0;
}

HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *handle,
    uint16_t device, uint16_t reg, uint16_t address_size,
    uint8_t *data, uint16_t length, uint32_t timeout)
{
    CHECK(handle == &bus);
    CHECK(device == 0xD4 && address_size == I2C_MEMADD_SIZE_8BIT);
    CHECK(data != NULL && length == 1 && reg < 256 && timeout > 0);
    if (reg == 0x01 && data[0] == 0) restore_attempts++;
    if (bank == 0x80 && reg == 0x09) page_write_attempts++;
    int failed = fail_transaction(0, reg, data[0]);
    if (failed && !fault.accept_then_error) return HAL_ERROR;

    if (reg == 0x01) {
        bank = data[0];
    } else if (bank == 0 && reg == 0x12 && (data[0] & 0x01)) {
        reset_chip();  /* Hardware clears SW_RESET when reset completes. */
    } else if (bank == 0x80 && reg == 0x09) {
        uint16_t address;
        CHECK((embedded_regs[0x17] & 0x40) != 0); /* PAGE_WRITE must be open. */
        address = (uint16_t)((embedded_regs[0x02] >> 4) * 256 + embedded_regs[0x08]);
        CHECK(page_write_count < sizeof(page_writes) / sizeof(page_writes[0]));
        page_writes[page_write_count].address = address;
        page_writes[page_write_count++].value = data[0];
        page_memory[address] = data[0];
        embedded_regs[0x08]++;
    } else {
        registers()[reg] = data[0];
    }
    return failed ? HAL_ERROR : HAL_OK;
}

HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *handle,
    uint16_t device, uint16_t reg, uint16_t address_size,
    uint8_t *data, uint16_t length, uint32_t timeout)
{
    CHECK(handle == &bus);
    CHECK(device == 0xD4 && address_size == I2C_MEMADD_SIZE_8BIT);
    CHECK(data != NULL && reg + length <= 256 && timeout > 0);
    if (bank == 0x80 && reg == 0x70) embedded_output_reads++;
    if (bank == 0 && reg == 0x1B) wake_source_reads++;
    if (fail_transaction(1, reg, 0)) return HAL_ERROR;
    if (reg == 0x01 && length == 1) data[0] = bank;
    else memcpy(data, registers() + reg, length);
    if (bank == 0 && reg == 0x1B && (user_regs[0x56] & 0x40))
        user_regs[0x1B] = 0; /* INT_CLR_ON_READ. */
    return HAL_OK;
}

void HAL_Delay(uint32_t milliseconds) { ticks += milliseconds; }
uint32_t HAL_GetTick(void) { return ticks; }

static void sensor_init(void)
{
    memset(&fault, 0, sizeof(fault));
    reset_chip();
    page_write_count = 0;
    page_write_attempts = 0;
    embedded_output_reads = wake_source_reads = restore_attempts = 0;
    CHECK(LSM6DSOX_Init(&bus) == 0);
    CHECK(bank == 0);
}

static void check_sampling_registers(void)
{
    CHECK(bank == 0);
    CHECK(user_regs[0x07] == 52 && user_regs[0x08] == 0);
    CHECK(user_regs[0x09] == 0x44 && user_regs[0x0A] == 0x06);
    CHECK(user_regs[0x10] == 0x4C && user_regs[0x11] == 0x4C);
    CHECK(user_regs[0x12] == 0x44 && user_regs[0x0D] == 0x08);
    CHECK((user_regs[0x5E] & 0x02) == 0); /* No MLC on FIFO INT1. */
    CHECK(embedded_regs[0x0D] == 0);       /* No MLC tree INT1 route. */
}

static void check_model_image(void)
{
    size_t stream = 0;
    for (size_t block = 0; block < sizeof(expected_model) / sizeof(expected_model[0]); block++) {
        const struct expected_block *expected = &expected_model[block];
        for (size_t i = 0; i < expected->length; i++, stream++) {
            uint16_t address = (uint16_t)(expected->start + i);
            CHECK(stream < page_write_count);
            CHECK(page_writes[stream].address == address);
            CHECK(page_writes[stream].value == expected->bytes[i]);
            CHECK(page_memory[address] == expected->bytes[i]);
        }
    }
    CHECK(stream == 425 && page_write_count == stream);
    CHECK((embedded_regs[0x17] & 0x60) == 0); /* Page access closed. */
    CHECK((embedded_regs[0x05] & 0x10) != 0); /* MLC enabled. */
    CHECK(embedded_regs[0x60] == 0x15);      /* Official MLC settings. */
}

static void load_model(void)
{
    sensor_init();
    CHECK(LSM6DSOX_MLC_Init() == 0);
    check_model_image();
    check_sampling_registers();
}

static void test_result_lifecycle(void)
{
    static const uint8_t codes[] = { 0x01, 0x04, 0x08, 0x00, 0x02, 0xFF };
    CHECK(LSM6DSOX_MLC_GetResult() == MLC_UNKNOWN);
    CHECK(LSM6DSOX_MLC_Init() != 0);
    sensor_init();
    CHECK(LSM6DSOX_MLC_GetResult() == MLC_UNKNOWN);
    CHECK(embedded_output_reads == 0);

    load_model();
    user_regs[0x70] = 0xAA; /* Distinguish user and embedded addresses. */
    for (size_t i = 0; i < sizeof(codes); i++) {
        uint8_t code = codes[i];
        embedded_regs[0x70] = code;
        CHECK(LSM6DSOX_MLC_GetResult() == (i < 3 ? code : MLC_UNKNOWN));
        CHECK(bank == 0);
    }
    CHECK(embedded_output_reads == sizeof(codes));
    CHECK(wake_source_reads == 0); /* Classification never reads WAKE_UP_SRC. */
    check_sampling_registers();

    bank = 0x80; /* A prior failed transfer may leave the chip in this bank. */
    CHECK(LSM6DSOX_Init(&bus) == 0);
    CHECK(bank == 0);
    CHECK(LSM6DSOX_MLC_GetResult() == MLC_UNKNOWN);
    CHECK(embedded_output_reads == sizeof(codes));
    puts("PASS: complete official page image, FIFO coexistence, output mapping and reset");
}

static void test_output_failures(void)
{
    load_model();
    embedded_regs[0x70] = MLC_WALKING;
    inject_fault(1, 0x80, 0x70, -1, 0);
    CHECK(LSM6DSOX_MLC_GetResult() == MLC_UNKNOWN);
    CHECK(fault.remaining == 0 && bank == 0);
    CHECK(LSM6DSOX_MLC_GetResult() == MLC_WALKING);

    inject_fault(0, 0x00, 0x01, 0x80, 0);
    CHECK(LSM6DSOX_MLC_GetResult() == MLC_UNKNOWN);
    CHECK(fault.remaining == 0 && bank == 0);
    CHECK(LSM6DSOX_MLC_GetResult() == MLC_WALKING);

    unsigned previous_restores = restore_attempts;
    inject_fault(0, 0x80, 0x01, 0x00, 0);
    CHECK(LSM6DSOX_MLC_GetResult() == MLC_UNKNOWN);
    CHECK(fault.remaining == 0 && bank == 0);
    CHECK(restore_attempts >= previous_restores + 4); /* Retry then best effort. */
    CHECK(LSM6DSOX_MLC_GetResult() == MLC_WALKING);
    check_sampling_registers();
    puts("PASS: output read/select/restore errors return UNKNOWN and recover bank");
}

static void test_upload_failures(void)
{
    static const unsigned positions[] = { 0, 210, 424 };
    for (size_t i = 0; i < sizeof(positions) / sizeof(positions[0]); i++) {
        sensor_init();
        inject_fault(0, 0x80, 0x09, -1, positions[i]);
        CHECK(LSM6DSOX_MLC_Init() != 0);
        CHECK(fault.remaining == 2 && bank == 0); /* Page write has no retry. */
        CHECK(page_write_attempts == positions[i] + 1);
        CHECK(page_write_count == positions[i]);
        CHECK((embedded_regs[0x17] & 0x60) == 0);
        CHECK((embedded_regs[0x05] & 0x10) == 0);
        CHECK(LSM6DSOX_MLC_GetResult() == MLC_UNKNOWN);
        CHECK(embedded_output_reads == 0);
        CHECK(LSM6DSOX_MLC_Init() != 0); /* Must reinitialize after partial upload. */
        load_model();
    }
    puts("PASS: first/middle/final page write errors close access and invalidate model");
}

static void test_ambiguous_page_write(void)
{
    sensor_init();
    inject_fault(0, 0x80, 0x09, -1, 210);
    fault.accept_then_error = 1;
    CHECK(LSM6DSOX_MLC_Init() != 0);
    CHECK(fault.remaining == 2 && page_write_attempts == 211);
    CHECK(page_write_count == 211); /* Chip accepted the byte before HAL failed. */
    CHECK(bank == 0 && (embedded_regs[0x17] & 0x60) == 0);
    CHECK((embedded_regs[0x05] & 0x10) == 0);
    CHECK(LSM6DSOX_MLC_GetResult() == MLC_UNKNOWN);
    CHECK(LSM6DSOX_MLC_Init() != 0);
    load_model();
    puts("PASS: accepted-then-error page write aborts upload without shifting the tree");
}

static void test_sampling_recovers_user_bank(void)
{
    int16_t acc[3] = { 123, 456, 789 }, gyro[3] = { 123, 456, 789 };
    load_model();
    embedded_regs[0x70] = MLC_WALKING;
    inject_fault(0, 0x80, 0x01, 0, 0);
    fault.remaining = 6;
    CHECK(LSM6DSOX_MLC_GetResult() == MLC_UNKNOWN);
    CHECK(fault.remaining == 0 && bank == 0x80); /* Both restore attempts failed. */
    user_regs[0x1B] = 0;
    embedded_regs[0x1B] = 0x08;
    CHECK(LSM6DSOX_WakeUp_GetResult() == 0);
    CHECK(bank == 0);

    inject_fault(0, 0x80, 0x01, 0, 0);
    fault.remaining = 6;
    CHECK(LSM6DSOX_MLC_GetResult() == MLC_UNKNOWN);
    CHECK(fault.remaining == 0 && bank == 0x80);
    embedded_regs[0x3A] = 0xFF; /* FIFO status must be read in the user bank. */
    CHECK(LSM6DSOX_GetFifoBatch(acc, gyro, NULL, 1) == 0);
    CHECK(bank == 0 && acc[0] == 123 && gyro[0] == 123);
    check_sampling_registers();
    puts("PASS: wake-up and FIFO recover user bank after all MLC restore retries fail");
}

static void test_wakeup_is_separate(void)
{
    load_model();
    CHECK(LSM6DSOX_WakeUp_Init() == 0);
    CHECK(user_regs[0x5B] == 0x02 && user_regs[0x5C] == 0);
    CHECK(user_regs[0x56] == 0x41 && user_regs[0x58] == 0x80);
    CHECK(user_regs[0x17] == 0); /* Raw/MLC accelerometer path unchanged. */
    CHECK((user_regs[0x5E] & 0x20) == 0 && (user_regs[0x5F] & 0x20) == 0);
    check_sampling_registers();
    user_regs[0x1B] = 0x08;
    CHECK(LSM6DSOX_WakeUp_GetResult() == 1);
    CHECK(LSM6DSOX_WakeUp_GetResult() == 0);
    inject_fault(1, 0, 0x1B, -1, 0);
    CHECK(LSM6DSOX_WakeUp_GetResult() == 0xFF);
    embedded_regs[0x70] = MLC_RUNNING;
    CHECK(LSM6DSOX_MLC_GetResult() == MLC_RUNNING);
    puts("PASS: wake-up read-clear stays separate from MLC and FIFO interrupt");
}

int main(void)
{
    test_result_lifecycle();
    test_output_failures();
    test_upload_failures();
    test_ambiguous_page_write();
    test_sampling_recovers_user_bank();
    test_wakeup_is_separate();
    printf("LSM6DSOX production-driver regression passed (%u checks).\n", checks);
    return 0;
}
