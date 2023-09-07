#include <string.h>
#include "AppConfig.h"

/* Include DMM module */
#include "hal_types.h"
#include "ti_dmm_application_policy.h"
#include <bcomdef.h>
#include <devinfoservice.h>
#include <dmm/apps/common/freertos/util.h>
#include <dmm/dmm_policy.h>
#include <dmm/dmm_priority_ble_thread.h>
#include <dmm/dmm_scheduler.h>
#include <icall.h>
#include <icall_ble_api.h>
#include <util.h>

#include "ti_ble_config.h"
#include "ti_drivers_config.h"
#ifndef ICALL_FEATURE_SEPARATE_IMGINFO
#include <icall_addrs.h>
#endif /* ICALL_FEATURE_SEPARATE_IMGINFO */


#define SERIALGATT_SERV_UUID            0xFFFF
#define ADV_DATA_LENGTH                 0x0E
#define SCAN_RESP_DATA_LENGTH           0x14
#define USER_BLE_NAME                   "Resideo"
#define SERIALGATT_SERVICE              0x00000001

const uint8 SerialGATTServUUID[ATT_BT_UUID_SIZE] = { LO_UINT16(SERIALGATT_SERV_UUID), HI_UINT16(SERIALGATT_SERV_UUID) };

static bool userAdvState = 0;

/*********************************************************************
 * Profile Attributes - variables
 */

// Simple Profile Service attribute
static const gattAttrType_t SerialGATTService = { ATT_BT_UUID_SIZE, SerialGATTServUUID };

// Simple Profile Characteristic 1 Properties
static uint8_t SerialGATTChar1Props = GATT_PROP_READ | GATT_PROP_NOTIFY | GATT_PROP_WRITE_NO_RSP;

#define USERPROFILE_CHAR1_LEN 20 

// Characteristic 1 Value
static uint8_t SerialGATTChar1[20] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

CONST uint8 SerialGATTchar1UUID[ATT_BT_UUID_SIZE] = { LO_UINT16(SERIALGATT_SERV_UUID), HI_UINT16(SERIALGATT_SERV_UUID) };
// Simple Profile Characteristic 1 Configuration Each client has its own
// instantiation of the Client Characteristic Configuration. Reads of the
// Client Characteristic Configuration only shows the configuration for
// that client and writes only affect the configuration of that client.
static gattCharCfg_t * SerialGATTChar1Config;

// Simple Profile Characteristic 1 User Description
static uint8_t SerialGATTChar1UserDesp[17] = "Silence";

static bStatus_t UserBLE_ReadAttrCB(uint16 connHandle, gattAttribute_t *pAttr,
                                          uint8 *pValue, uint16 *pLen, uint16 offset,
                                          uint16 maxLen, uint8 method );

static bStatus_t UserBLE_WriteAttrCB(uint16 connHandle, gattAttribute_t *pAttr,
                                           uint8 *pValue, uint16 len, uint16 offset,
                                           uint8 method );
/*********************************************************************
 * PROFILE CALLBACKS
 */
// Simple Profile Service Callbacks
CONST gattServiceCBs_t SerialGATTCBs = {
    UserBLE_ReadAttrCB, //SerialGATT_ReadAttrCB,      // Read callback function pointer
    UserBLE_WriteAttrCB, //SerialGATT_WriteAttrCB,     // Write callback function pointer
    NULL                                // Authorization callback function pointer
};
/*********************************************************************
 * Profile Attributes - Table
 */

static gattAttribute_t SerialGATTAttrTbl[70] = {
    // Simple Profile Service
    {
        { ATT_BT_UUID_SIZE, primaryServiceUUID },       /* type */
        GATT_PERMIT_READ,                               /* permissions */
        0,                                              /* handle */
        (uint8 *) &SerialGATTService                    /* pValue */
    },

    // Characteristic 1 Declaration
    { { ATT_BT_UUID_SIZE, characterUUID }, 
        GATT_PERMIT_READ | GATT_PERMIT_WRITE, 
        0, &SerialGATTChar1Props },

    // Characteristic Value 1
    { { ATT_BT_UUID_SIZE, SerialGATTchar1UUID }, 
        GATT_PERMIT_READ | GATT_PERMIT_WRITE, 
        0, SerialGATTChar1 },

    // Characteristic 1 configuration
    { { ATT_BT_UUID_SIZE, clientCharCfgUUID }, 
        GATT_PERMIT_READ | GATT_PERMIT_WRITE, 
        0, (uint8 *) &SerialGATTChar1Config },

    // Characteristic 1 User Description
    { { ATT_BT_UUID_SIZE, charUserDescUUID }, GATT_PERMIT_READ, 
        0, SerialGATTChar1UserDesp }
};


static bStatus_t UserBLE_ReadAttrCB(uint16 connHandle, gattAttribute_t *pAttr,
                                          uint8 *pValue, uint16 *pLen, uint16 offset,
                                          uint16 maxLen, uint8 method )
{
    return SUCCESS;
}

static bStatus_t UserBLE_WriteAttrCB(uint16 connHandle, gattAttribute_t *pAttr,
                                           uint8 *pValue, uint16 len, uint16 offset,
                                           uint8 method )
{
    PLAT_LOG(" Silence feature (%d)", *pValue);
    
    return SUCCESS; 
}

bStatus_t UserBLEProfile_AddService(uint32 services)
{
    PLAT_LOG("JJM -> UserBLEProfile_AddService");

    uint8_t status;

    // Allocate Client Characteristic Configuration table
    SerialGATTChar1Config = (gattCharCfg_t *) ICall_malloc(sizeof(gattCharCfg_t) * linkDBNumConns);
    if (SerialGATTChar1Config == NULL)
    {
        return (bleMemAllocError);
    }

    // Initialize Client Characteristic Configuration attributes
    GATTServApp_InitCharCfg(0xFFFF, SerialGATTChar1Config);

    // Hardcode to enable notification in GATT table
    SerialGATTChar1Config[0].connHandle = 0x0000;
    SerialGATTChar1Config[0].value      = 0x01;
    
    if (services & SERIALGATT_SERVICE)
    {
        // Register GATT attribute list and CBs with GATT Server App
        status = GATTServApp_RegisterService(SerialGATTAttrTbl,
                                             GATT_NUM_ATTRS(SerialGATTAttrTbl), 
                                             GATT_MAX_ENCRYPT_KEY_SIZE,
                                             &SerialGATTCBs);
    }
    else
    {
        status = SUCCESS;
    }
    return status;
}

bStatus_t UserBLEProfile_RegisterAppCBs()
{
    
    // buttons aren't initialized yet, so I think this is working. 
    return SUCCESS;
}

uint8_t UserBLEAdv_AddData(uint8_t * advData, uint8_t size)
{
    PLAT_LOG("UserBLEAdv_AddData");

    if (size < ADV_DATA_LENGTH)
        return 0;
    uint8_t advIndex    = 0;
    advData[advIndex++] = 0x0D;                    // Length of this data
    advData[advIndex++] = GAP_ADTYPE_SERVICE_DATA; // Data
    advData[advIndex++] = LO_UINT16(SERIALGATT_SERV_UUID);
    advData[advIndex++] = HI_UINT16(SERIALGATT_SERV_UUID);

    advData[advIndex++] = 0x00;
    advData[advIndex++] = 0x11; 
    advData[advIndex++] = 0x22; 
    advData[advIndex++] = 0x33;
    advData[advIndex++] = 0x44;
    advData[advIndex++] = 0x55;
    advData[advIndex++] = 0x66;
    advData[advIndex++] = 0x77;
    advData[advIndex++] = 0x88;
    advData[advIndex++] = 0x99;

    return advIndex;
}

uint8_t UserBLEScan_AddData(uint8_t * scanData, uint8_t size)
{
    PLAT_LOG("UserBLEScan_AddData()");
    
    if (size < SCAN_RESP_DATA_LENGTH)
        return 0;
    uint8_t scanIndex = 0;

    uint8_t nameLength = sizeof(USER_BLE_NAME) - 1;

    scanData[scanIndex++] = 1 + nameLength;
    scanData[scanIndex++] = GAP_ADTYPE_LOCAL_NAME_COMPLETE;
    memcpy(&scanData[scanIndex], USER_BLE_NAME, nameLength);
    scanIndex += nameLength;
    return scanIndex;
}

char * UserBLEGetName(uint8_t * length)
{
    PLAT_LOG("UserBLEGetName()");
    static char userBleName[sizeof(USER_BLE_NAME)];
    uint8_t nameLength = sizeof(USER_BLE_NAME) - 1; // Don't include string termination
    uint8_t nameIndex  = 0;

    memcpy(&userBleName[nameIndex], USER_BLE_NAME, nameLength);
    nameIndex += nameLength;

    *length = nameIndex;

    return userBleName;
}

void UserProcessGapMessage(gapEventHdr_t * pMsg)
{
    return;
    switch (pMsg->opcode)
    {
        case GAP_DEVICE_INIT_DONE_EVENT:
            PLAT_LOG("GAP_DEVICE_INIT_DONE_EVENT");
            break;

        case GAP_LINK_ESTABLISHED_EVENT:
            PLAT_LOG("GAP_LINK_ESTABLISHED_EVENT");
            break;

        case GAP_ADV_DATA_UPDATE_DONE_EVENT:
            PLAT_LOG("GAP_ADV_DATA_UPDATE_DONE_EVENT");
            break;

        case GAP_LINK_TERMINATED_EVENT:
            PLAT_LOG("GAP_LINK_TERMINATED_EVENT");
            // 
            // TODO: start advertising again?
            //
            break;

        case GAP_UPDATE_LINK_PARAM_REQ_EVENT:
            PLAT_LOG("GAP_UPDATE_LINK_PARAM_REQ_EVENT");
            break;

        case GAP_LINK_PARAM_UPDATE_EVENT:
            PLAT_LOG("GAP_LINK_PARAM_UPDATE_EVENT");
            break;

        default:
            break;
    }
}

void UserProcessAdvEvent(uint8_t event)
{
    return;
    switch (event)
    {
        case GAP_EVT_ADV_START_AFTER_ENABLE: 
            PLAT_LOG("GAP_EVT_ADV_START_AFTER_ENABLE");
            break;

        case GAP_EVT_ADV_END_AFTER_DISABLE: 
            PLAT_LOG("GAP_EVT_ADV_END_AFTER_DISABLE");
            break;

        case GAP_EVT_ADV_START:
            PLAT_LOG("GAP_EVT_ADV_START");
            break;

        case GAP_EVT_ADV_END:
            PLAT_LOG("GAP_EVT_ADV_END");
            break;

        case GAP_EVT_ADV_SET_TERMINATED:
            PLAT_LOG("GAP_EVT_ADV_SET_TERMINATED");
            break;

        case GAP_EVT_SCAN_REQ_RECEIVED:
            PLAT_LOG("GAP_EVT_SCAN_REQ_RECEIVED");
            break;

        default:
            break;
    }
}

uint8_t UserBLEGetAdvertisementState(void)
{
    return userAdvState;
}

void UserBLESetAdvState(uint8_t advState)
{
    userAdvState = advState;
}
