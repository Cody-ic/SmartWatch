/* Execute production C with hardware-only stubs. No copied protocol/model logic. */
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include "../../Core/Src/ble_uart.c"
#include "../../AI/proto_model.c"
#include "../../AI/sleep_model.c"
#include "../../Core/Src/sleep_features.c"
#include "spo2.h"

UART_HandleTypeDef huart1 = {USART1};
static uint32_t now_ms = 1000;
static uint8_t *receive_ptr;
static uint8_t transmitted[8192];
static size_t transmitted_len;
uint32_t HAL_GetTick(void) { return now_ms; }
int HAL_UART_Receive_IT(UART_HandleTypeDef *h, uint8_t *p, uint16_t n) {
    (void)h; (void)n; receive_ptr = p; return HAL_OK;
}
int HAL_UART_Transmit(UART_HandleTypeDef *h, uint8_t *p, uint16_t n, uint32_t timeout) {
    (void)h; (void)timeout;
    assert(transmitted_len + n <= sizeof(transmitted));
    memcpy(transmitted + transmitted_len, p, n); transmitted_len += n;
    return HAL_OK;
}
void rtc_set_unix(uint32_t t) { (void)t; }
void rtc_get_hms(uint8_t *h, uint8_t *m, uint8_t *s) { *h=12; *m=0; *s=0; }
void app_set_user_profile(float w, uint8_t a) { (void)w; (void)a; }
void module_flags_set(uint16_t f) { (void)f; }

static void byte_in(uint8_t b) { *receive_ptr=b; HAL_UART_RxCpltCallback(&huart1); }
static void send_payload(const uint8_t *p, uint8_t len) {
    uint8_t frame[128]={0xAA, BLE_MSG_PROTO_RESTORE, len};
    memcpy(frame+3,p,len); frame[3+len]=checksum(frame,3+len);
    for(int i=0;i<len+4;i++) byte_in(frame[i]);
    ble_uart_process();
}
static uint8_t ack_status(void) { assert(transmitted_len>=10); return transmitted[transmitted_len-2]; }
static void begin(uint32_t id, const uint8_t full[PROTO_FULL_SIZE], int bad_crc) {
    uint8_t p[9]={BLE_PROTO_BEGIN};
    uint32_t crc=proto_crc32(full,PROTO_FULL_SIZE) ^ (bad_crc ? 1U : 0U);
    memcpy(p+1,&id,4); memcpy(p+5,&crc,4); send_payload(p,9);
    assert(ack_status()==0);
}
static void fragment(const uint8_t full[PROTO_FULL_SIZE], int idx) {
    int n=PROTO_FULL_SIZE-idx*122; if(n>122)n=122;
    uint8_t p[124]={(uint8_t)idx,PROTO_FRAG_COUNT};
    memcpy(p+2,full+idx*122,n); send_payload(p,(uint8_t)(n+2));
}

int main(int argc, char **argv) {
    ble_uart_init();
    assert(proto_crc32((const uint8_t*)"123456789",9)==0xCBF43926U);
    float initial[7][128]={{0}};
    for(int c=0;c<7;c++)initial[c][0]=1;
    proto_model_set(initial,7);
    uint8_t short_restore[]={0,1,1,1};
    send_payload(short_restore,sizeof(short_restore));
    assert(proto_model_get_version()==7 && proto_model_get_ptr()[0][0]==1);
    puts("PASS: short restore preserves model");

    uint8_t full[PROTO_FULL_SIZE]={1,8};
    for(int c=0;c<7;c++) { float v=2; memcpy(full+2+(c*128+c+1)*4,&v,4); }
    begin(42,full,0);
    fragment(full,0); /* 128 bytes arrive before any consumer runs. */
    assert(rx_overflow==0 && proto_rx_frag_mask==1 && ack_status()==0);
    fragment(full,0); /* Lost ACK: exact retry must not clear progress. */
    for(int i=29;i>=1;i--)fragment(full,i);
    assert(ack_status()==1 && proto_model_get_version()==8);
    for(int c=0;c<7;c++)assert(proto_model_get_ptr()[c][c+1]==1);
    fragment(full,29); assert(ack_status()==1);
    puts("PASS: maximum frame, out-of-order restore, duplicate and final ACK retry");

    full[1]=9;
    begin(43,full,1);
    for(int i=0;i<30;i++)fragment(full,i);
    assert(ack_status()==2 && proto_model_get_version()==8);
    puts("PASS: CRC failure preserves previous model");
    float invalid=NAN; memcpy(full+2,&invalid,4);
    begin(44,full,0); for(int i=0;i<30;i++)fragment(full,i);
    assert(ack_status()==2 && proto_model_get_version()==8);
    memset(full+2,0,4);
    puts("PASS: NaN payload with valid CRC is rejected");

    begin(45,full,0); fragment(full,0);
    full[3]^=1; fragment(full,0); full[3]^=1;
    assert(ack_status()==2 && !proto_rx_session);
    begin(46,full,0); fragment(full,0);
    now_ms+=5001; ble_uart_process(); fragment(full,1);
    assert(ack_status()==2 && proto_model_get_version()==8);
    begin(47,full,0); fragment(full,0);
    begin(48,full,0); fragment(full,1);
    assert(proto_rx_frag_mask==2 && proto_model_get_version()==8);
    puts("PASS: conflicting duplicates, expiry and new transactions cannot mix data");

    for(int i=0;i<BLE_RX_BUF_SIZE;i++)byte_in(0);
    assert(rx_overflow); ble_uart_process();
    assert(!rx_overflow && rx_head==rx_tail && rx_state==RX_WAIT_HEADER && !proto_rx_session);
    puts("PASS: overflow resets damaged receive state");

    spo2_state_t oxygen; spo2_init(&oxygen);
    oxygen.valid=1; oxygen.spo2=97; oxygen.last_valid_ms=1000;
    spo2_expire(&oxygen,6000); assert(spo2_is_valid(&oxygen));
    spo2_expire(&oxygen,6001); assert(!spo2_is_valid(&oxygen) && spo2_get(&oxygen)==0);
    oxygen.valid=1; oxygen.last_valid_ms=0xFFFFFF00U;
    spo2_expire(&oxygen,5000); assert(!spo2_is_valid(&oxygen));
    puts("PASS: SpO2 expires without new samples and across tick wrap");

    int16_t acc[26*3]={0}; uint32_t ppg[25];
    for(int i=0;i<25;i++)ppg[i]=1000;
    for(int i=0;i<120;i++)sleep_features_feed(acc,ppg,25,72,25);
    assert(epoch_idx==1 && epoch_cnt[0]==3120 && !sleep_features_ready());
    for(int i=0;i<480;i++)sleep_features_feed(acc,ppg,25,72,25);
    assert(sleep_features_ready());
    float features[SF_INPUT_DIM],probs[SLEEP_N_OUTPUTS];
    sleep_features_get(features); assert(!sleep_features_ready());
    for(int i=0;i<SF_INPUT_DIM;i++)assert(isfinite(features[i]));
    uint32_t short_ppg[1]={999};
    sleep_features_feed(acc,short_ppg,1,72,25);
    assert(ppg_buf[0][0]==999 && ppg_buf[0][1]==0 && !epoch_valid[0]);
    for(int i=1;i<120;i++)sleep_features_feed(acc,ppg,25,72,25);
    assert(!sleep_features_ready());
    memset(features,0,sizeof(features)); sleep_model_predict(features,probs);
    assert(probs[1]<0.5f && fabsf(probs[0]+probs[1]-1)<1e-6f);
    puts("PASS: 30-second raw-IMU epoch, consumed readiness and missing-PPG rejection");

    transmitted_len=0;
    assert(ble_uart_send_proto_backup(8,proto_model_get_ptr(),0x12345678)==0);
    assert(transmitted_len==3779);
    if(argc>1) {
        FILE *f=fopen(argv[1],"wb"); assert(f);
        assert(fwrite(transmitted,1,transmitted_len,f)==transmitted_len); fclose(f);
    }
    puts("PASS: production backup frames exported for Dart interoperability");
    if(argc>2) {
        transmitted_len=0; ble_uart_init(); proto_model_set(initial,7);
        FILE *f=fopen(argv[2],"rb"); assert(f);
        int ch;
        while((ch=fgetc(f))!=EOF) { byte_in((uint8_t)ch); ble_uart_process(); }
        fclose(f);
        assert(proto_model_get_version()==11 && ack_status()==1);
        for(int c=0;c<7;c++)assert(proto_model_get_ptr()[c][c+1]==1);
        puts("PASS: Dart-generated transaction accepted by production STM32 parser");
    }
    return 0;
}
