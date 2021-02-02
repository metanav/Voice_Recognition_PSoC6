
/*******************************************************************************
 * Include header files
 ******************************************************************************/
#include "cyhal.h"
#include "cybsp.h"
#include "cycfg_ble.h"
#include "cy_retarget_io.h"
#include "cy_pdl.h"
#include "ble_client.h"


/*******************************************************************************
* Macros
*******************************************************************************/
#define ENABLE                      (1u)
#define DISABLE                     (0u)
#define DEBUG_BLE_ENABLE            (DISABLE)
#define CY_ASSERT_FAILED            (0u)

#if DEBUG_BLE_ENABLE
#define DEBUG_BLE                   (printf)
#else
#define DEBUG_BLE(...)
#endif

#define BLESS_INTR_PRIORITY         (1u)
#define GATT_MTU_MAX                (512u)
#define CY_BLE_BD_ADDR_SIZE         (0x06u)
#define GATT_CCCD_LEN               (0x02u)
#define GATT_CCCD_NTF_ENABLE        (ENABLE)
#define AD_TYPE_COMPLETE_LOCAL_NAME (9u)


/* Length of notification packet received from GATT Server to initialize EZI2C bridge
 * Size of CapSense data structure:(2 bytes)
 * No. of notification packets to receive complete CapSense data: (1 byte) */
#define CAPSENSE_DS_SIZE_LSB_IDX    (0u)
#define CAPSENSE_DS_SIZE_MSB_IDX    (1u)
#define NOTIFICATION_COUNT_IDX      (2u)
#define TUNER_BRIDGE_INIT_NTF_SIZE  (3u)

#define CMD_CHAR_UUID           {0xC7u, 0x58u, 0xCFu, 0x70u, 0xB3u, 0xAFu,\
                                     0xE4u,0xADu, 0x65u, 0x44u, 0xA3u, 0x85u, 0x26u,\
                                     0x7Bu,0x70u, 0xD4u}
#define CMD_CHAR_UUID_SIZE      (16u)

#define SET_CHAR_UUID           {0xC8u, 0x58u, 0xCFu, 0x70u, 0xB3u, 0xAFu,\
                                     0xE4u,0xADu, 0x65u, 0x44u, 0xA3u, 0x85u, 0x26u,\
                                     0x7Bu,0x70u, 0xD4u}
#define SET_CHAR_UUID_SIZE      (16u)

#define SET_CCCD_UUID           {0x02u, 0x29u}
#define SET_CCCD_UUID_SIZE      (2u)
/*******************************************************************************
*        Function Prototypes
*******************************************************************************/
static void stack_event_handler(uint32_t event, void* eventParam);
static void ble_assert(void);
static void bless_interrupt_handler(void);
static uint8_t* adv_parser(uint16_t ad_type, cy_stc_ble_gapc_adv_report_param_t *scan_report, uint8_t *adv_type_length);
static void enable_notification(cy_ble_gatt_db_attr_handle_t  attrHandle);

/*******************************************************************************
 * Global variables
 ******************************************************************************/
static cy_ble_gatt_db_attr_handle_t gatt_notify_cccd_attrHandle;
/* Variable to store MTU size for active BLE connection */
static uint16_t att_mtu_size = CY_BLE_GATT_MTU;
/* Variable to keep track of the BLE API result */
static cy_en_ble_api_result_t ble_api_result;
/* Variable is set when a notification is received from GATT Server */
bool notify = false;

/* Variable to store no. of notification packets to receive complete CapSense data */
uint8_t max_notificaion_count = 1;

/* Variable to check if target gap peripheral device is found or not */
static bool target_found = false;

/* Variable to check if ezi2c bridge is initialized or not */
static bool bridge_initialised = false;

/* Variable to enable or disable notifications in GATT Server */
static uint8_t cccd_val[CY_BLE_CCCD_LEN] = {DISABLE, DISABLE};

/* Variable to store connection handle */
cy_stc_ble_conn_handle_t conn_handle;

/* Variable to store notification data received from server */
cy_stc_ble_gattc_handle_value_ntf_param_t notification_data;

/* Constant to store server name and use during scanning */
//static const char target_name[] = "CapSense_GATT_Server";
static const char target_name[] = "REMOTE";

/* Variable to hold the address of the peer device */
static cy_stc_ble_gap_bd_addr_t peer_addr;

/* Variable to hold advertisement report parameter */
static cy_stc_ble_gapc_adv_report_param_t adv_report;


/*******************************************************************************
* Function Name: bless_interrupt_handler
********************************************************************************
* Summary:
*  Wrapper function for handling interrupts from BLESS.
*
*******************************************************************************/
static void bless_interrupt_handler(void)
{
    Cy_BLE_BlessIsrHandler();
}


/*******************************************************************************
 * Function Name: ble_init()
 *******************************************************************************
 *
 * Summary:
 *   This function initializes the BLE for use in the code example.
 *
 ******************************************************************************/
void ble_init(void)
{
    /* Variable to store common error codes received as API result */
    cy_en_ble_api_result_t api_result = CY_BLE_SUCCESS;

    /* BLESS interrupt configuration structure */
    const cy_stc_sysint_t  blessIsrCfg =
    {
        /* The BLESS interrupt */
        .intrSrc       = bless_interrupt_IRQn,

        /* The interrupt priority number */
        .intrPriority  = BLESS_INTR_PRIORITY
    };

    /* Initialize the BLESS interrupt */
    cy_ble_config.hw->blessIsrConfig = &blessIsrCfg;

    /* Hook interrupt service routines for BLESS */
    Cy_SysInt_Init(cy_ble_config.hw->blessIsrConfig, bless_interrupt_handler);

    /* Register the generic event handler */
    Cy_BLE_RegisterEventCallback(stack_event_handler);

    /* Initialize the BLE host */
    api_result = Cy_BLE_Init(&cy_ble_config);

    if (api_result != CY_BLE_SUCCESS)
    {
        /* BLE stack initialization failed, check configuration,
         * notify error and halt CPU in debug mode */
        DEBUG_BLE("Cy_BLE_Init API Error: %x \r\n", api_result);
        ble_assert();
    }
    else
    {
        DEBUG_BLE("Cy_BLE_Init API Success: %x \r\n", api_result);
    }

    /* Enable BLE */
    api_result = Cy_BLE_Enable();

    if (api_result != CY_BLE_SUCCESS)
    {
        /* BLE stack initialization failed, check configuration,
         * notify error and halt CPU in debug mode */
        DEBUG_BLE("Cy_BLE_Enable API Error: %x \r\n", api_result);
        ble_assert();
    }
    else
    {
        DEBUG_BLE("Cy_BLE_Enable API Success: %x \r\n", api_result);
    }
}


/*******************************************************************************
* Function Name: stack_event_handler
********************************************************************************
*
* Summary:
*   This is an event callback function to receive events from the BLE stack.
*
* Parameters:
*  uint32_t event:      event from the BLE stack
*  void* eventParam:  parameters related to the event
*
*******************************************************************************/
static void stack_event_handler(uint32_t event, void* eventParam)
{
    cy_en_ble_api_result_t api_result = CY_BLE_SUCCESS;

    switch(event)
    {
        /***********************************************************************
         *                       General Events
         **********************************************************************/
        case CY_BLE_EVT_STACK_ON:
        {
            /* Stack initialized; ready for scan */
            DEBUG_BLE("CY_BLE_EVT_STACK_ON \r\n");
            printf("Scanning for GAP Peripheral with device name %s\n\r",\
                                                               target_name);
            /* Start scan */
            Cy_BLE_GAPC_StartScan(CY_BLE_SCANNING_FAST,\
                                  CY_BLE_CENTRAL_CONFIGURATION_0_INDEX);
            break;
        }

        /* This event is received when there is a timeout. */
        case CY_BLE_EVT_TIMEOUT:
        {
            DEBUG_BLE("CY_BLE_EVT_TIMEOUT \r\n");
            break;
        }

        /***********************************************************************
         *                     GAP Central Events
         **********************************************************************/

        /* This event is triggered every time a device is discovered */
        case CY_BLE_EVT_GAPC_SCAN_PROGRESS_RESULT:
        {
            /* A new device listed in the scan report */
            adv_report = *(cy_stc_ble_gapc_adv_report_param_t *)eventParam;

            /* Pointer to store return value from advertisement parser */
            char* peer_name = NULL;
            uint8_t name_length = 0u;

            /* Process scan response packets */
            if(adv_report.eventType == CY_BLE_GAPC_SCAN_RSP)
            {
                peer_name = (char*) adv_parser(AD_TYPE_COMPLETE_LOCAL_NAME,\
                                                     &adv_report, &name_length);

                if(peer_name  == NULL)
                {
                    target_found = false;
                    break;
                }
                /* Compare peer name with "CapSense_GATT_Server" */
                else
                {
                    peer_name[name_length]= '\0';
                    target_found = ((strcmp(peer_name, target_name)) ? false : true);
                }
                if (target_found)
                {
                    /* Stop Scan */
                    Cy_BLE_GAPC_StopScan();
                }
            }
            break;
        }

        /* The central device has started/stopped scanning */
        case CY_BLE_EVT_GAPC_SCAN_START_STOP:
        {
            if(Cy_BLE_GetScanState() == CY_BLE_SCAN_STATE_STOPPED)
            {
                if(target_found == true)
                {
                    /* Scan stopped manually; do not restart scan */
                    target_found = false;

                    /* Get address and address type of the peer device to
                     * initiate connection */
                    for(uint8_t i = 0u; i < CY_BLE_BD_ADDR_SIZE; i++)
                    {
                        peer_addr.bdAddr[i] = adv_report.peerBdAddr[i];
                    }

                    printf("Found Peer Device with address:");

                    /* Print the peer bd address on UART terminal */
                    for(uint8_t i = (CY_BLE_BD_ADDR_SIZE); i > 0u; i--)
                    {
                        printf(" %X", peer_addr.bdAddr[i - 1u]);
                    }

                    /* Get the peer address type */
                    peer_addr.type = adv_report.peerAddrType;

                    /* Initiate connection with discovered peer device */
                    api_result = Cy_BLE_GAPC_ConnectDevice(&peer_addr,\
                                          CY_BLE_CENTRAL_CONFIGURATION_0_INDEX);
                    if(api_result == CY_BLE_SUCCESS)
                    {
                         DEBUG_BLE("Initiated GAP connection..\r\n");
                    }
                    else
                    {
                         printf("Failed to initiate connection %u \r\n",\
                                                                    api_result);
                         ble_assert();
                    }
                }
            }
            break;
        }

        case CY_BLE_EVT_GAP_DEVICE_CONNECTED:
        {
            /* BLE link is established */
            DEBUG_BLE("BLE Stack Event : CY_BLE_EVT_GAP_DEVICE_CONNECTED \r\n");

            /* Variable to store values to update PHY to 2M */
            cy_stc_ble_set_phy_info_t phy_param;
            phy_param.allPhyMask = CY_BLE_PHY_NO_PREF_MASK_NONE;
            phy_param.bdHandle = conn_handle.bdHandle;
            phy_param.rxPhyMask = CY_BLE_PHY_MASK_LE_2M;
            phy_param.txPhyMask = CY_BLE_PHY_MASK_LE_2M;

            /* Function call to set PHY to 2M */
            api_result = Cy_BLE_SetPhy(&phy_param);
            if(api_result == CY_BLE_SUCCESS)
            {
                DEBUG_BLE("Set PHY to 2M successfull");
                DEBUG_BLE("Request sent to switch PHY to 2M\r\n");
            }
            else
            {
                DEBUG_BLE("Set PHY to 2M API failure, errorcode = 0x%X", api_result);
            }
            break;
        }

        /* This event is triggered when there is a change to either the maximum
         * Payload length or the maximum transmission time of Data Channel PDUs
         * in either direction
         */
        case CY_BLE_EVT_DATA_LENGTH_CHANGE:
        {
            DEBUG_BLE("CY_BLE_EVT_DATA_LENGTH_CHANGE\r\n");
            break;
        }

        /* This event indicates that the controller has changed the transmitter
         * PHY or receiver PHY in use */
        case CY_BLE_EVT_PHY_UPDATE_COMPLETE:
        {
        	printf("CY_BLE_EVT_PHY_UPDATE_COMPLETE\n");
            /* Initiate an MTU exchange request */
            cy_stc_ble_gatt_xchg_mtu_param_t mtuParam = {conn_handle,\
                    GATT_MTU_MAX};

            /* To remove unused parameter warning when UART debug is disabled*/
#if (DEBUG_BLE_ENABLE == ENABLE)
            DEBUG_BLE("UPDATE PHY parameters\r\n");
            cy_stc_ble_events_param_generic_t *param =\
                                (cy_stc_ble_events_param_generic_t *)eventParam;
            cy_stc_ble_phy_param_t *phyparam = NULL;
            if(param->status == CY_BLE_SUCCESS)
            {
                phyparam = (cy_stc_ble_phy_param_t *)param->eventParams;
                DEBUG_BLE("RxPhy Mask : 0x%02X\r\nTxPhy Mask : 0x%02X\r\n",\
                        phyparam->rxPhyMask, phyparam->txPhyMask);
            }
#endif
            api_result = Cy_BLE_GATTC_ExchangeMtuReq(&mtuParam);

            if(api_result != CY_BLE_SUCCESS)
            {
                DEBUG_BLE("Cy_BLE_GATTC_ExchangeMtuReq API Error: %xd \r\n",\
                        api_result);
            }
            break;
        }

        /***********************************************************************
         *                       GATT Events
         **********************************************************************/

        /* This event is generated at the GAP Peripheral end after a connection
         * is completed with a peer Central device. For a GAP Central device,
         * this event is generated as in acknowledgment of receiving this event
         * successfully by the BLE Controller.
         */
        case CY_BLE_EVT_GATT_CONNECT_IND:
        {
            /* Connected as Central (master role) */
            conn_handle = *(cy_stc_ble_conn_handle_t *)eventParam;

            /* Turn the connected LED on */
            cyhal_gpio_write((cyhal_gpio_t)CYBSP_USER_LED1, CYBSP_LED_STATE_ON);

            DEBUG_BLE("CY_BLE_EVT_GATT_CONNECT_IND: attId=%X, bdHandle=%X\r\n",\
                       conn_handle.attId, conn_handle.bdHandle);
            printf("\n\rConnected to Device\r\n\n");

            /* Initiate an MTU exchange request */
            cy_stc_ble_gatt_xchg_mtu_param_t mtuParam = {conn_handle,\
                    GATT_MTU_MAX};
            api_result = Cy_BLE_GATTC_ExchangeMtuReq(&mtuParam);

            if(api_result != CY_BLE_SUCCESS)
            {
                DEBUG_BLE("Cy_BLE_GATTC_ExchangeMtuReq API Error: %xd \r\n",\
                        api_result);
            }

            break;
        }


        /* Disconnection event */
        case CY_BLE_EVT_GATT_DISCONNECT_IND:
        {
            DEBUG_BLE("CY_BLE_EVT_GATT_DISCONNECT_IND \r\n");
            if(conn_handle.bdHandle ==\
                             (*(cy_stc_ble_conn_handle_t *)eventParam).bdHandle)
            {
                printf("Disconnected.\r\n\n");
                conn_handle.bdHandle = 0xFF;
                conn_handle.attId    = 0xFF;
            }
            /* Free the tuner buffer memory */
           // clear_tuner_buffer();

            /* BLE connection disconnected. Turn OFF the LED */
            cyhal_gpio_write((cyhal_gpio_t)CYBSP_USER_LED1,\
                              CYBSP_LED_STATE_OFF);
            bridge_initialised = false;
            printf("Scanning for GAP Peripheral with device name %s\n\r",\
                                                               target_name);

            /* Start scan */
            Cy_BLE_GAPC_StartScan(CY_BLE_SCANNING_FAST,\
                                  CY_BLE_CENTRAL_CONFIGURATION_0_INDEX);
            break;
        }

        case CY_BLE_EVT_GATTC_WRITE_RSP:
        {
            /* Write response for the CCCD write; this means that the
             * notifications are now enabled. */
            printf("Notifications enabled successfully. Receiving data..\n");
            break;
        }

        case CY_BLE_EVT_GATTC_HANDLE_VALUE_NTF:
        {
            notification_data = *(cy_stc_ble_gattc_handle_value_ntf_param_t *)eventParam;
            //if(notification_data.handleValPair.attrHandle == 0x2E) { }
			printf("data: %u\n", notification_data.handleValPair.value.val[0]);

            break;
        }

        /* The event is received by the Client when the Server cannot perform
         * the requested operation and sends out an error response. */
        case CY_BLE_EVT_GATTC_ERROR_RSP:
        {
            DEBUG_BLE("CY_BLE_EVT_GATTC_ERROR_RSP \r\n");
            break;
        }

        /* This event is generated when response is received from GATT Server
         * for MTU exchange request */
        case CY_BLE_EVT_GATTC_XCHNG_MTU_RSP:
        {
            printf("BLE Stack Event: CY_BLE_EVT_GATTC_XCHNG_MTU_RSP\n");
            cy_stc_ble_gatt_xchg_mtu_param_t *mtu_xchg_resp =
                                  (cy_stc_ble_gatt_xchg_mtu_param_t*)eventParam;

            cy_stc_ble_gattc_find_info_req_t find_req_param;
            find_req_param.connHandle = conn_handle;
            /* 0x0001 - 0xFFFF is the range of attribute handles that can be present in a GATT database*/
            find_req_param.range.startHandle = 0x0001;
            find_req_param.range.endHandle = 0xFFFF;

            printf("Negotiated MTU size: %d\r\n", mtu_xchg_resp->mtu);
            att_mtu_size = mtu_xchg_resp->mtu;
            if(att_mtu_size > CY_BLE_GATT_MTU)
            {
                att_mtu_size = CY_BLE_GATT_MTU;
            }

            /* Discover the characteristics present in GATT Server database */
            ble_api_result = Cy_BLE_GATTC_DiscoverCharacteristicDescriptors(&find_req_param);
            if(ble_api_result == CY_BLE_SUCCESS)
            {
                printf("Discover Characteristic Descriptor API success");
            }
            else
            {
                printf("Discover Characteristic Descriptor API, errorcode = 0x%X", ble_api_result);
            }
            break;
        }

        /* This event indicates that the 'Find Information Response' is received
         * from GATT Server device */
        case CY_BLE_EVT_GATTC_FIND_INFO_RSP:
        {
            printf("\nBLE Stack Event: CY_BLE_EVT_GATTC_FIND_INFO_RSP\n");

            cy_stc_ble_gattc_find_info_rsp_param_t find_info_param;

            find_info_param = *((cy_stc_ble_gattc_find_info_rsp_param_t*)eventParam);

            /* Search for 'cmd' characteristic UUID and get the attribute handle */
            uint8_t cmd_char_uuid[CMD_CHAR_UUID_SIZE] = CMD_CHAR_UUID;


            if(!cmd_char_uuid_found)
			{
				if (0 == memcmp(find_info_param.handleValueList.list + 2 , cmd_char_uuid, CMD_CHAR_UUID_SIZE))
				{
					cmd_char_uuid_found = true;
					gatt_cmd_attrHandle = find_info_param.handleValueList.list[0]
								| (find_info_param.handleValueList.list[1] << 8 );
					printf("Att Handle of Custom cmd characteristic: 0x%X\n", gatt_cmd_attrHandle);

				}
			}


            /* Search for 'set' characteristic UUID and get the attribute handle */
            uint8_t set_char_uuid[SET_CHAR_UUID_SIZE] = SET_CHAR_UUID;
			if(!set_char_uuid_found)
			{
				if (0 == memcmp(find_info_param.handleValueList.list + 2 , set_char_uuid, SET_CHAR_UUID_SIZE))
				{
					set_char_uuid_found = true;
					gatt_set_attrHandle = find_info_param.handleValueList.list[0]
								| (find_info_param.handleValueList.list[1] << 8 );
					printf("Att Handle of Custom set characteristic: 0x%X\n", gatt_set_attrHandle);

				}
			}

            if (find_info_param.uuidFormat == 0x01) {
            	uint8_t set_cccd_uuid[SET_CCCD_UUID_SIZE] = SET_CCCD_UUID;
				if (0 == memcmp(find_info_param.handleValueList.list + 2 , set_cccd_uuid, SET_CCCD_UUID_SIZE))
				{
					gatt_notify_cccd_attrHandle = find_info_param.handleValueList.list[0]
								| (find_info_param.handleValueList.list[1] << 8 );
					printf("Att Handle of Custom set characteristic CCCD: 0x%X\n", gatt_notify_cccd_attrHandle);

					enable_notification(gatt_notify_cccd_attrHandle);
				}
            }
            break;
        }
        /***********************************************************************
         *                       Other Events
         **********************************************************************/
        default:
            DEBUG_BLE("Unhandled BLE event: %x\r\n", event);
            break;
        }
}


/*******************************************************************************
 * Function Name: ble_assert()
 *******************************************************************************
 * Summary: ble initialization failed, halt CPU
 *
 ******************************************************************************/
static void ble_assert(void)
{
    CY_ASSERT(CY_ASSERT_FAILED);
}


/*******************************************************************************
* Function Name: ble_capsense_process
********************************************************************************
*
* Summary:
*   -  allows the BLE stack to process pending events
*
*******************************************************************************/
void ble_process_events(void)
{
    /* Cy_BLE_ProcessEvents() allows the BLE stack to process pending events */
    Cy_BLE_ProcessEvents();
}


/*******************************************************************************
* Function Name: uint8_t* adv_parser(uint16_t AD_type,
*       cy_stc_ble_gapc_adv_report_param_t* scan_report, uint8_t* adv_type_length)
********************************************************************************
*
* Summary: This function searches adv packets for the given type.
*
* Parameters:
*  uint16_t adv_type : the type of value to be discovered
*  cy_stc_ble_gapc_adv_report_param_t* scan_report : advertisement report
*                                                    parameter
*  uint8_t* adv_type_length : length of discovered value
*
* Return:
*  uint8_t* : Pointer to the value discovered in ADV packet
*
*******************************************************************************/
static uint8_t* adv_parser(uint16_t adv_type, cy_stc_ble_gapc_adv_report_param_t*
                         scan_report, uint8_t* adv_type_length)
{
    uint8_t length = 0u;
    uint8_t* pName = NULL;

    for(uint8_t i = 0u; i < scan_report->dataLen; i += (length+1))
    {
        length = scan_report->data[i];
        if(scan_report->data[i+1] == adv_type)
        {
            pName = &scan_report->data[i+2];
            *adv_type_length = length - 1;
            return(pName);
        }
    }
    return((uint8_t*)NULL);
}


static void enable_notification(cy_ble_gatt_db_attr_handle_t  attrHandle)
{
	cy_stc_ble_gattc_write_req_t write_req;

	cccd_val[0] = ENABLE;
	write_req.connHandle = conn_handle;
	write_req.handleValPair.attrHandle = attrHandle;
	write_req.handleValPair.value.val = cccd_val;
	write_req.handleValPair.value.len = GATT_CCCD_LEN;

	/* Enable notification to receive data */
	cy_en_ble_api_result_t api_result = Cy_BLE_GATTC_WriteCharacteristicDescriptors(&write_req);
	if(api_result != CY_BLE_SUCCESS)
	{
		DEBUG_BLE("Failed to enable notification"\
				  " [bdhandle 0x%02X]: \r\n", write_req.connHandle.bdHandle);
		ble_assert();
	}
	else
	{
		DEBUG_BLE("Enabling notification [bdhandle 0x%02X]\n", write_req.connHandle.bdHandle);
	}
}
