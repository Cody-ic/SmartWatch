/********************************** (C) COPYRIGHT *******************************
 * File Name          : peripheral.C
 * Author             : zhangxiyi @WCH
 * Version            : v0.1
 * Date               : 2020/11/26
 * Description        :
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *******************************************************************************/

/*********************************************************************
 * INCLUDES
 */
#include "CONFIG.h"
#include "devinfoservice.h"
#include "gattprofile.h"
#include "peripheral.h"

#include "ble_uart_service.h"
#include "app_drv_fifo.h"
#if defined(DEBUG) && DEBUG == Debug_UART1
#error "UART1 carries the watch protocol; disable DEBUG or use another port"
#endif

uint8 Peripheral_TaskID = INVALID_TASK_ID; // Task ID for internal task/event processing

//
static uint8_t to_test_buffer[BLE_BUFF_MAX_LEN - 4 - 3];

//The buffer length should be a power of 2
#define APP_UART_TX_BUFFER_LENGTH    512U
#define APP_UART_RX_BUFFER_LENGTH    4096U  /* Holds one complete prototype backup burst. */

//The tx buffer and rx buffer for app_drv_fifo
//length should be a power of 2
static uint8_t app_uart_tx_buffer[APP_UART_TX_BUFFER_LENGTH] = {0};
static uint8_t app_uart_rx_buffer[APP_UART_RX_BUFFER_LENGTH] = {0};

static app_drv_fifo_t app_uart_tx_fifo;
static app_drv_fifo_t app_uart_rx_fifo;

//interupt uart rx flag ,clear at main loop
bool uart_rx_flag = false;

//for interrupt rx blcak hole ,when uart rx fifo full
uint8_t for_uart_rx_black_hole = 0;

//fifo length less that MTU-3, retry times
uint32_t uart_to_ble_send_evt_cnt = 0;

void app_uart_process(void)
{
    UINT32 irq_status;
    SYS_DisableAllIrq(&irq_status);
    if(uart_rx_flag)
    {
        tmos_start_task(Peripheral_TaskID, UART_TO_BLE_SEND_EVT, 2);
        uart_rx_flag = false;
    }
    SYS_RecoverIrq(irq_status);

    //tx process
    if(R8_UART1_TFC < UART_FIFO_SIZE)
    {
        app_drv_fifo_read_to_same_addr(&app_uart_tx_fifo, (uint8_t *)&R8_UART1_THR, UART_FIFO_SIZE - R8_UART1_TFC);
    }
}

void app_uart_init()
{
    //tx fifo and tx fifo
    //The buffer length should be a power of 2
    app_drv_fifo_init(&app_uart_tx_fifo, app_uart_tx_buffer, APP_UART_TX_BUFFER_LENGTH);
    app_drv_fifo_init(&app_uart_rx_fifo, app_uart_rx_buffer, APP_UART_RX_BUFFER_LENGTH);

    /* [2026-07-11 Fix]: UART2→UART1(PA8/PA9)。原代码用 UART2(PA6/PA7) 与硬件不符。
     *   网表 Hardware Truth (Netlist_Schematic1_2026-07-09.net):
     *     L4689: U6-5 CH582M-PA8/RXD1 → LPUART1_TX → U1-42 STM32U575-PA9(BLE TX)
     *     L4694: U6-6 CH582M-PA9/TXD1 → LPUART1_RX → U1-43 STM32U575-PA10(BLE RX)
     *   CH582M PA8/PA9 是 UART1, 非 UART2; 且 PA6/PA7 在 U6 全引脚清单中完全不存在 → PCB 未布线。
     *   改: UART2_DefInit→UART1_DefInit, UART2_INTCfg→UART1_INTCfg, UART2_IRQHandler→UART1_IRQHandler,
     *   R8_UART2_*→R8_UART1_*, PFIC UART2_IRQn→UART1_IRQn, GPIO Pin_7/6→Pin_9/8,
     *   RB_PIN_UART2→RB_PIN_UART1。DEBUG TX(PA9)暂与UART1 TX共用, 禁用 DEBUG 后不冲突。
     *   交叉验证: 网表(权威真值) + STM32端 CLAUDE.md BLE引脚表 + peripheral.c 代码对照。手改勿重生成。 */
    GPIOPinRemap(DISABLE, RB_PIN_UART1);  /* RB_PIN_UART1=0 → PA8/PA9 */
    GPIOA_SetBits(GPIO_Pin_9);
    GPIOA_ModeCfg(GPIO_Pin_9, GPIO_ModeOut_PP_5mA);
    GPIOA_SetBits(GPIO_Pin_8);
    GPIOA_ModeCfg(GPIO_Pin_8, GPIO_ModeIN_PU);

    UART1_DefInit();
    UART1_BaudRateCfg(115200);
    UART1_INTCfg(ENABLE, RB_IER_RECV_RDY | RB_IER_LINE_STAT);
    PFIC_EnableIRQ(UART1_IRQn);
}

static volatile uint32_t uart_tx_rejected_packets;

uint8_t app_uart_tx_data(const uint8_t *data, uint16_t length)
{
    if(app_drv_fifo_write_all(&app_uart_tx_fifo, data, length) != APP_DRV_FIFO_RESULT_SUCCESS) {
        uart_tx_rejected_packets++;
        return ATT_ERR_INSUFFICIENT_RESOURCES;
    }
    return SUCCESS;
}
//
//Not every uart reception will end with a UART_II_RECV_TOUT
//UART_II_RECV_TOUT can only be triggered when R8_UARTx_RFC is not 0
//Here we cannot rely UART_II_RECV_TOUT as the end of a uart reception

__INTERRUPT
__HIGH_CODE
void UART1_IRQHandler(void)
{
    uint16_t error;
    switch(UART1_GetITFlag())
    {
        case UART_II_LINE_STAT:
            UART1_GetLinSTA();
            break;

        case UART_II_RECV_RDY:
        case UART_II_RECV_TOUT:
            error = app_drv_fifo_write_from_same_addr(&app_uart_rx_fifo, (uint8_t *)&R8_UART1_RBR, R8_UART1_RFC);
            if(error != APP_DRV_FIFO_RESULT_SUCCESS)
            {
                for(uint8_t i = 0; i < R8_UART1_RFC; i++)
                {
                    //fifo full,put to fifo black hole
                    for_uart_rx_black_hole = R8_UART1_RBR;
                }
            }
            uart_rx_flag = true;
            break;

        case UART_II_THR_EMPTY:
            break;
        case UART_II_MODEM_CHG:
            break;
        default:
            break;
    }
}

//ble uart service callback handler
void on_bleuartServiceEvt(uint16_t connection_handle, ble_uart_evt_t *p_evt)
{
    switch(p_evt->type)
    {
        case BLE_UART_EVT_TX_NOTI_DISABLED:
            PRINT("%02x:bleuart_EVT_TX_NOTI_DISABLED\r\n", connection_handle);
            break;
        case BLE_UART_EVT_TX_NOTI_ENABLED:
            PRINT("%02x:bleuart_EVT_TX_NOTI_ENABLED\r\n", connection_handle);
            break;
        case BLE_UART_EVT_BLE_DATA_RECIEVED:
            /* 手机发来数据 → UART TX 发给 STM32（不回声） */
            p_evt->status = app_uart_tx_data(p_evt->data.p_data, p_evt->data.length);

            break;
        default:
            break;
    }
}

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * CONSTANTS
 */

// How often to perform periodic event
#define SBP_PERIODIC_EVT_PERIOD              1600

// How often to perform read rssi event
#define SBP_READ_RSSI_EVT_PERIOD             3200

// Parameter update delay
#define SBP_PARAM_UPDATE_DELAY               6400

// What is the advertising interval when device is discoverable (units of 625us, 80=50ms)
#define DEFAULT_ADVERTISING_INTERVAL         160

// Limited discoverable mode advertises for 30.72s, and then stops
// General discoverable mode advertises indefinitely
#define DEFAULT_DISCOVERABLE_MODE            GAP_ADTYPE_FLAGS_GENERAL

// Minimum connection interval (units of 1.25ms, 10=12.5ms)
#define DEFAULT_DESIRED_MIN_CONN_INTERVAL    8

// Maximum connection interval (units of 1.25ms, 100=125ms)
#define DEFAULT_DESIRED_MAX_CONN_INTERVAL    20

// Slave latency to use parameter update
#define DEFAULT_DESIRED_SLAVE_LATENCY        0

// Supervision timeout value (units of 10ms, 100=1s)
#define DEFAULT_DESIRED_CONN_TIMEOUT         100

// Company Identifier: WCH
#define WCH_COMPANY_ID                       0x07D7

/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL FUNCTIONS
 */

/*********************************************************************
 * LOCAL VARIABLES
 */

//for send to ble
typedef enum
{
    SEND_TO_BLE_TO_SEND = 1,
    SEND_TO_BLE_ALLOC_FAILED,
    SEND_TO_BLE_SEND_FAILED,
} send_to_ble_state_t;
send_to_ble_state_t send_to_ble_state = SEND_TO_BLE_TO_SEND;

blePaControlConfig_t pa_lna_ctl;

//static uint8 Peripheral_TaskID = INVALID_TASK_ID;   // Task ID for internal task/event processing

// GAP - SCAN RSP data (max size = 31 bytes)
// [2026-07-03 FIX]: 广播名 "ch583_ble_uart" → "SmartWatch", 与 GATT 设备名一致
static uint8 scanRspData[] = {
    // complete name
    10, // length of this data ("SmartWatch" = 10 chars)
    GAP_ADTYPE_LOCAL_NAME_COMPLETE,
    'S', 'm', 'a', 'r', 't', 'W', 'a', 't', 'c', 'h',
    // connection interval range
    0x05, // length of this data
    GAP_ADTYPE_SLAVE_CONN_INTERVAL_RANGE,
    LO_UINT16(DEFAULT_DESIRED_MIN_CONN_INTERVAL), // 100ms
    HI_UINT16(DEFAULT_DESIRED_MIN_CONN_INTERVAL),
    LO_UINT16(DEFAULT_DESIRED_MAX_CONN_INTERVAL), // 1s
    HI_UINT16(DEFAULT_DESIRED_MAX_CONN_INTERVAL),

    // Tx power level
    0x02, // length of this data
    GAP_ADTYPE_POWER_LEVEL,
    0 // 0dBm
};

// GAP - Advertisement data (max size = 31 bytes, though this is
// best kept short to conserve power while advertisting)
static uint8 advertData[] = {
    // Flags; this sets the device to use limited discoverable
    // mode (advertises for 30 seconds at a time) instead of general
    // discoverable mode (advertises indefinitely)
    0x02, // length of this data
    GAP_ADTYPE_FLAGS,
    DEFAULT_DISCOVERABLE_MODE | GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED,

    // service UUID, to notify central devices what services are included
    // in this peripheral
    0x03,                  // length of this data
    GAP_ADTYPE_16BIT_MORE, // some of the UUID's, but not all
    LO_UINT16(SIMPLEPROFILE_SERV_UUID),
    HI_UINT16(SIMPLEPROFILE_SERV_UUID)};

// GAP GATT Attributes
static uint8 attDeviceName[GAP_DEVICE_NAME_LEN] = "SmartWatch";

// Connection item list
static peripheralConnItem_t peripheralConnList;

/*********************************************************************
 * LOCAL FUNCTIONS
 */
static void Peripheral_ProcessTMOSMsg(tmos_event_hdr_t *pMsg);
static void peripheralStateNotificationCB(gapRole_States_t newState, gapRoleEvent_t *pEvent);

static void peripheralParamUpdateCB(uint16 connHandle, uint16 connInterval,
                                    uint16 connSlaveLatency, uint16 connTimeout);
static void peripheralInitConnItem(peripheralConnItem_t *peripheralConnList);
static void peripheralRssiCB(uint16 connHandle, int8 rssi);

/*********************************************************************
 * PROFILE CALLBACKS
 */

// GAP Role Callbacks
static gapRolesCBs_t Peripheral_PeripheralCBs = {
    peripheralStateNotificationCB, // Profile State Change Callbacks
    peripheralRssiCB,              // When a valid RSSI is read from controller (not used by application)
    peripheralParamUpdateCB};

// Broadcast Callbacks
static gapRolesBroadcasterCBs_t Broadcaster_BroadcasterCBs = {
    NULL, // Not used in peripheral role
    NULL  // Receive scan request callback
};

// GAP Bond Manager Callbacks
static gapBondCBs_t Peripheral_BondMgrCBs = {
    NULL, // Passcode callback (not used by application)
    NULL  // Pairing / Bonding state Callback (not used by application)
};

/*********************************************************************
 * PUBLIC FUNCTIONS
 */

/*********************************************************************
 * @fn      Peripheral_Init
 *
 * @brief   Initialization function for the Peripheral App Task.
 *          This is called during initialization and should contain
 *          any application specific initialization (ie. hardware
 *          initialization/setup, table initialization, power up
 *          notificaiton ... ).
 *
 * @param   task_id - the ID assigned by TMOS.  This ID should be
 *                    used to send messages and set timers.
 *
 * @return  none
 */
void Peripheral_Init()
{
    Peripheral_TaskID = TMOS_ProcessEventRegister(Peripheral_ProcessEvent);

    // Setup the GAP Peripheral Role Profile
    {
        uint8  initial_advertising_enable = TRUE;
        uint16 desired_min_interval = 6;
        uint16 desired_max_interval = 1000;

        // Set the GAP Role Parameters
        GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8), &initial_advertising_enable);
        GAPRole_SetParameter(GAPROLE_SCAN_RSP_DATA, sizeof(scanRspData), scanRspData);
        GAPRole_SetParameter(GAPROLE_ADVERT_DATA, sizeof(advertData), advertData);
        GAPRole_SetParameter(GAPROLE_MIN_CONN_INTERVAL, sizeof(uint16), &desired_min_interval);
        GAPRole_SetParameter(GAPROLE_MAX_CONN_INTERVAL, sizeof(uint16), &desired_max_interval);
    }

    // Set the GAP Characteristics
    GGS_SetParameter(GGS_DEVICE_NAME_ATT, GAP_DEVICE_NAME_LEN, attDeviceName);

    // Set advertising interval
    {
        uint16 advInt = DEFAULT_ADVERTISING_INTERVAL;

        GAP_SetParamValue(TGAP_DISC_ADV_INT_MIN, advInt);
        GAP_SetParamValue(TGAP_DISC_ADV_INT_MAX, advInt);
    }

    // Setup the GAP Bond Manager
    {
        uint32 passkey = 0; // passkey "000000"
        uint8  pairMode = GAPBOND_PAIRING_MODE_WAIT_FOR_REQ;
        uint8  mitm = TRUE;
        uint8  bonding = TRUE;
        uint8  ioCap = GAPBOND_IO_CAP_DISPLAY_ONLY;
        GAPBondMgr_SetParameter(GAPBOND_PERI_DEFAULT_PASSCODE, sizeof(uint32), &passkey);
        GAPBondMgr_SetParameter(GAPBOND_PERI_PAIRING_MODE, sizeof(uint8), &pairMode);
        GAPBondMgr_SetParameter(GAPBOND_PERI_MITM_PROTECTION, sizeof(uint8), &mitm);
        GAPBondMgr_SetParameter(GAPBOND_PERI_IO_CAPABILITIES, sizeof(uint8), &ioCap);
        GAPBondMgr_SetParameter(GAPBOND_PERI_BONDING_ENABLED, sizeof(uint8), &bonding);
    }

    // Initialize GATT attributes
    GGS_AddService(GATT_ALL_SERVICES);         // GAP
    GATTServApp_AddService(GATT_ALL_SERVICES); // GATT attributes
    DevInfo_AddService();                      // Device Information Service
    ble_uart_add_service(on_bleuartServiceEvt);

    // Init Connection Item
    peripheralInitConnItem(&peripheralConnList);

    // Register receive scan request callback
    GAPRole_BroadcasterSetCB(&Broadcaster_BroadcasterCBs);

    // Setup a delayed profile startup
    tmos_set_event(Peripheral_TaskID, SBP_START_DEVICE_EVT);
}

/*********************************************************************
 * @fn      peripheralInitConnItem
 *
 * @brief   Init Connection Item
 *
 * @param   peripheralConnList -
 *
 * @return  NULL
 */
static void peripheralInitConnItem(peripheralConnItem_t *peripheralConnList)
{
    peripheralConnList->connHandle = GAP_CONNHANDLE_INIT;
    peripheralConnList->connInterval = 0;
    peripheralConnList->connSlaveLatency = 0;
    peripheralConnList->connTimeout = 0;
}

uint32_t get_fattime(void)
{
    return 0;
}

/*********************************************************************
 * @fn      Peripheral_ProcessEvent
 *
 * @brief   Peripheral Application Task event processor.  This function
 *          is called to process all events for the task.  Events
 *          include timers, messages and any other user defined events.
 *
 * @param   task_id - The TMOS assigned task ID.
 * @param   events - events to process.  This is a bit map and can
 *                   contain more than one event.
 *
 * @return  events not processed
 */
uint16 Peripheral_ProcessEvent(uint8 task_id, uint16 events)
{
    static attHandleValueNoti_t noti;
    //  VOID task_id; // TMOS required parameter that isn't used in this function

    if(events & SYS_EVENT_MSG)
    {
        uint8 *pMsg;

        if((pMsg = tmos_msg_receive(Peripheral_TaskID)) != NULL)
        {
            Peripheral_ProcessTMOSMsg((tmos_event_hdr_t *)pMsg);
            // Release the TMOS message
            tmos_msg_deallocate(pMsg);
        }
        // return unprocessed events
        return (events ^ SYS_EVENT_MSG);
    }

    if(events & SBP_START_DEVICE_EVT)
    {
        // Start the Device
        GAPRole_PeripheralStartDevice(Peripheral_TaskID, &Peripheral_BondMgrCBs, &Peripheral_PeripheralCBs);
        return (events ^ SBP_START_DEVICE_EVT);
    }
    if(events & SBP_PARAM_UPDATE_EVT)
    {
        // Send connect param update request
        GAPRole_PeripheralConnParamUpdateReq(peripheralConnList.connHandle,
                                             DEFAULT_DESIRED_MIN_CONN_INTERVAL,
                                             DEFAULT_DESIRED_MAX_CONN_INTERVAL,
                                             DEFAULT_DESIRED_SLAVE_LATENCY,
                                             DEFAULT_DESIRED_CONN_TIMEOUT,
                                             Peripheral_TaskID);

        //        GAPRole_PeripheralConnParamUpdateReq( peripheralConnList.connHandle,
        //                                              10,
        //                                              20,
        //                                              0,
        //                                              400,
        //                                              Peripheral_TaskID);

        return (events ^ SBP_PARAM_UPDATE_EVT);
    }

    if(events & UART_TO_BLE_SEND_EVT)
    {
        static uint16_t read_length = 0;
        uint8_t result = 0xff;  /* [2026-07-04]: 移除多余分号 */
        switch(send_to_ble_state)
        {
            case SEND_TO_BLE_TO_SEND:

                //notify is not enabled
                if(!ble_uart_notify_is_ready(peripheralConnList.connHandle))
                {
                    if(peripheralConnList.connHandle == GAP_CONNHANDLE_INIT)
                    {
                        //connection lost, flush rx fifo here
                        app_drv_fifo_flush(&app_uart_rx_fifo);
                    }
                    break;
                }
                read_length = ATT_GetMTU(peripheralConnList.connHandle) - 3;

                if(app_drv_fifo_length(&app_uart_rx_fifo) >= read_length)
                {
                    PRINT("FIFO_LEN:%d\r\n", app_drv_fifo_length(&app_uart_rx_fifo));
                    result = app_drv_fifo_read(&app_uart_rx_fifo, to_test_buffer, &read_length);
                    uart_to_ble_send_evt_cnt = 0;
                }
                else
                {
                    if(uart_to_ble_send_evt_cnt > 10)
                    {
                        result = app_drv_fifo_read(&app_uart_rx_fifo, to_test_buffer, &read_length);
                        uart_to_ble_send_evt_cnt = 0;
                    }
                    else
                    {
                        tmos_start_task(Peripheral_TaskID, UART_TO_BLE_SEND_EVT, 4);
                        uart_to_ble_send_evt_cnt++;
                        PRINT("NO TIME OUT\r\n");
                    }
                }

                if(APP_DRV_FIFO_RESULT_SUCCESS == result)
                {
                    noti.len = read_length;
                    noti.pValue = GATT_bm_alloc(peripheralConnList.connHandle, ATT_HANDLE_VALUE_NOTI, noti.len, NULL, 0);
                    if(noti.pValue != NULL)
                    {
                        tmos_memcpy(noti.pValue, to_test_buffer, noti.len);
                        result = ble_uart_notify(peripheralConnList.connHandle, &noti, 0);
                        if(result != SUCCESS)
                        {
                            PRINT("R1:%02x\r\n", result);
                            send_to_ble_state = SEND_TO_BLE_SEND_FAILED;
                            GATT_bm_free((gattMsg_t *)&noti, ATT_HANDLE_VALUE_NOTI);
                            tmos_start_task(Peripheral_TaskID, UART_TO_BLE_SEND_EVT, 2);
                        }
                        else
                        {
                            send_to_ble_state = SEND_TO_BLE_TO_SEND;
                            //app_fifo_write(&app_uart_tx_fifo,to_test_buffer,&read_length);
                            //app_drv_fifo_write(&app_uart_tx_fifo,to_test_buffer,&read_length);
                            read_length = 0;
                            tmos_start_task(Peripheral_TaskID, UART_TO_BLE_SEND_EVT, 2);
                        }
                    }
                    else
                    {
                        send_to_ble_state = SEND_TO_BLE_ALLOC_FAILED;
                        tmos_start_task(Peripheral_TaskID, UART_TO_BLE_SEND_EVT, 2);
                    }
                }
                else
                {
                    //send_to_ble_state = SEND_TO_BLE_FIFO_EMPTY;
                }
                break;
            case SEND_TO_BLE_ALLOC_FAILED:
            case SEND_TO_BLE_SEND_FAILED:

                noti.len = read_length;
                noti.pValue = GATT_bm_alloc(peripheralConnList.connHandle, ATT_HANDLE_VALUE_NOTI, noti.len, NULL, 0);
                if(noti.pValue != NULL)
                {
                    tmos_memcpy(noti.pValue, to_test_buffer, noti.len);
                    result = ble_uart_notify(peripheralConnList.connHandle, &noti, 0);
                    if(result != SUCCESS)
                    {
                        PRINT("R2:%02x\r\n", result);
                        send_to_ble_state = SEND_TO_BLE_SEND_FAILED;
                        GATT_bm_free((gattMsg_t *)&noti, ATT_HANDLE_VALUE_NOTI);
                        tmos_start_task(Peripheral_TaskID, UART_TO_BLE_SEND_EVT, 2);
                    }
                    else
                    {
                        send_to_ble_state = SEND_TO_BLE_TO_SEND;
                        //app_drv_fifo_write(&app_uart_tx_fifo,to_test_buffer,&read_length);
                        read_length = 0;
                        tmos_start_task(Peripheral_TaskID, UART_TO_BLE_SEND_EVT, 2);
                    }
                }
                else
                {
                    send_to_ble_state = SEND_TO_BLE_ALLOC_FAILED;
                    tmos_start_task(Peripheral_TaskID, UART_TO_BLE_SEND_EVT, 2);
                }
                break;
            default:
                break;
        }
        return (events ^ UART_TO_BLE_SEND_EVT);
    }
    // Discard unknown events
    return 0;
}

/*********************************************************************
 * @fn      Peripheral_ProcessTMOSMsg
 *
 * @brief   Process an incoming task message.
 *
 * @param   pMsg - message to process
 *
 * @return  none
 */
static void Peripheral_ProcessTMOSMsg(tmos_event_hdr_t *pMsg)
{
    switch(pMsg->event)
    {
        default:
            break;
    }
}

/*********************************************************************
 * @fn      Peripheral_LinkEstablished
 *
 * @brief   Process link established.
 *
 * @param   pEvent - event to process
 *
 * @return  none
 */
static void Peripheral_LinkEstablished(gapRoleEvent_t *pEvent)
{
    gapEstLinkReqEvent_t *event = (gapEstLinkReqEvent_t *)pEvent;

    // See if already connected
    if(peripheralConnList.connHandle != GAP_CONNHANDLE_INIT)
    {
        GAPRole_TerminateLink(pEvent->linkCmpl.connectionHandle);
        PRINT("Connection max...\n");
    }
    else
    {
        peripheralConnList.connHandle = event->connectionHandle;
        peripheralConnList.connInterval = event->connInterval;
        peripheralConnList.connSlaveLatency = event->connLatency;
        peripheralConnList.connTimeout = event->connTimeout;

        // [2026-07-04]: MTU 交换 — 请求 ATT_MTU=247 (BLE 4.2 DLE max)
        //   默认 MTU=23 → 每包仅 20B payload; MTU=247 → 244B, 吞吐量提升 12×
        {
            attExchangeMTUReq_t mtuReq;
            mtuReq.clientRxMTU = 247;
            GATT_ExchangeMTU(event->connectionHandle, &mtuReq, Peripheral_TaskID);
        }

        // Set timer for param update event
        tmos_start_task(Peripheral_TaskID, SBP_PARAM_UPDATE_EVT, SBP_PARAM_UPDATE_DELAY);

        PRINT("Conn %x - Int %x \n", event->connectionHandle, event->connInterval);
    }
}

/*********************************************************************
 * @fn      Peripheral_LinkTerminated
 *
 * @brief   Process link terminated.
 *
 * @param   pEvent - event to process
 *
 * @return  none
 */
static void Peripheral_LinkTerminated(gapRoleEvent_t *pEvent)
{
    gapTerminateLinkEvent_t *event = (gapTerminateLinkEvent_t *)pEvent;

    if(event->connectionHandle == peripheralConnList.connHandle)
    {
        peripheralConnList.connHandle = GAP_CONNHANDLE_INIT;
        peripheralConnList.connInterval = 0;
        peripheralConnList.connSlaveLatency = 0;
        peripheralConnList.connTimeout = 0;

        // Restart advertising
        {
            uint8 advertising_enable = TRUE;
            GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8), &advertising_enable);
        }
    }
    else
    {
        PRINT("ERR..\n");
    }
}

/*********************************************************************
 * @fn      peripheralRssiCB
 *
 * @brief   RSSI callback.
 *
 * @param   connHandle - connection handle
 * @param   rssi - RSSI
 *
 * @return  none
 */
static void peripheralRssiCB(uint16 connHandle, int8 rssi)
{
    PRINT("RSSI -%d dB Conn  %x \n", -rssi, connHandle);
}

/*********************************************************************
 * @fn      peripheralParamUpdateCB
 *
 * @brief   Parameter update complete callback
 *
 * @param   connHandle - connect handle
 *          connInterval - connect interval
 *          connSlaveLatency - connect slave latency
 *          connTimeout - connect timeout
 *
 * @return  none
 */
static void peripheralParamUpdateCB(uint16 connHandle, uint16 connInterval,
                                    uint16 connSlaveLatency, uint16 connTimeout)
{
    if(connHandle == peripheralConnList.connHandle)
    {
        peripheralConnList.connInterval = connInterval;
        peripheralConnList.connSlaveLatency = connSlaveLatency;
        peripheralConnList.connTimeout = connTimeout;

        PRINT("Update %x - Int %x \n", connHandle, connInterval);
    }
    else
    {
        PRINT("peripheralParamUpdateCB err..\n");
    }
}

/*********************************************************************
 * @fn      peripheralStateNotificationCB
 *
 * @brief   Notification from the profile of a state change.
 *
 * @param   newState - new state
 *
 * @return  none
 */
static void peripheralStateNotificationCB(gapRole_States_t newState, gapRoleEvent_t *pEvent)
{
    switch(newState & GAPROLE_STATE_ADV_MASK)
    {
        case GAPROLE_STARTED:
            PRINT("Initialized..\n");
            break;

        case GAPROLE_ADVERTISING:
            if(pEvent->gap.opcode == GAP_LINK_TERMINATED_EVENT)
            {
                Peripheral_LinkTerminated(pEvent);
            }
            PRINT("Advertising..\n");
            break;

        case GAPROLE_CONNECTED:
            if(pEvent->gap.opcode == GAP_LINK_ESTABLISHED_EVENT)
            {
                Peripheral_LinkEstablished(pEvent);
                PRINT("Connected..\n");
            }
            break;

        case GAPROLE_CONNECTED_ADV:
            PRINT("Connected Advertising..\n");
            break;

        case GAPROLE_WAITING:
            if(pEvent->gap.opcode == GAP_END_DISCOVERABLE_DONE_EVENT)
            {
                PRINT("Waiting for advertising..\n");
            }
            else if(pEvent->gap.opcode == GAP_LINK_TERMINATED_EVENT)
            {
                Peripheral_LinkTerminated(pEvent);
                PRINT("Disconnected.. Reason:%x\n", pEvent->linkTerminate.reason);
            }
            else if(pEvent->gap.opcode == GAP_LINK_ESTABLISHED_EVENT)
            {
                if(pEvent->gap.hdr.status != SUCCESS)
                {
                    PRINT("Waiting for advertising..\n");
                }
                else
                {
                    PRINT("Error..\n");
                }
            }
            else
            {
                PRINT("Error..%x\n", pEvent->gap.opcode);
            }
            break;

        case GAPROLE_ERROR:
            PRINT("Error..\n");
            break;

        default:
            break;
    }
}

/*********************************************************************
*********************************************************************/
