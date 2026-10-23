#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "app_drv_fifo.c"
int main(void) {
    app_drv_fifo_t fifo;
    uint8_t storage[512], data[128], output[512];
    memset(data,0xAA,sizeof(data));
    assert(app_drv_fifo_init(&fifo,storage,sizeof(storage))==APP_DRV_FIFO_RESULT_SUCCESS);
    for(int i=0;i<4;i++)assert(app_drv_fifo_write_all(&fifo,data,128)==APP_DRV_FIFO_RESULT_SUCCESS);
    assert(app_drv_fifo_write_all(&fifo,data,1)==APP_DRV_FIFO_RESULT_NOT_MEM);
    uint16_t n=20; app_drv_fifo_read(&fifo,output,&n);
    assert(app_drv_fifo_write_all(&fifo,data,128)==APP_DRV_FIFO_RESULT_NOT_MEM);
    assert(app_drv_fifo_length(&fifo)==492);
    n=492; app_drv_fifo_read(&fifo,output,&n);
    for(int i=0;i<n;i++)assert(output[i]==0xAA);
    for(int i=0;i<600;i++) {
        assert(app_drv_fifo_write_all(&fifo,data,128)==APP_DRV_FIFO_RESULT_SUCCESS);
        n=128; app_drv_fifo_read(&fifo,output,&n);
        assert(n==128 && memcmp(data,output,128)==0);
    }
    puts("PASS: CH582M FIFO full/partial-space rejection and uint16 wrap preserve packets");
    return 0;
}
