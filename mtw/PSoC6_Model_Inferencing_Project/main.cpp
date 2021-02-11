#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include "cycfg_ble.h"
#include "cycfg.h"
#include "ble_client.h"
#include "pdm_pcm.h"
#include "eink.h"

/* Use CMSIS to make DSP work fast NOTE: define it before edge impulse header */
#define EIDSP_USE_CMSIS_DSP             1
#define EIDSP_LOAD_CMSIS_DSP_SOURCES    1

#include "ei_run_classifier.h"
#include "numpy.hpp"

/*******************************************************************************
* Macros
*******************************************************************************/
/* BLE macros */
#define BLE_WRITE_BUFFER_SIZE            (3u)
#define EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW 3

/*******************************************************************************
* Function Prototypes
********************************************************************************/
int  microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr);
void ble_write_characteristics(uint8_t command);
/*******************************************************************************
 * Global variables
 ******************************************************************************/
/* Buffer to hold characteristics value to be sent to GATT Server */
uint8_t ble_write_buffer[BLE_WRITE_BUFFER_SIZE];

/* Variable to hold the result of BLE apis */
cy_en_ble_api_result_t api_result;

/* Variable to hold the write request parameter to be sent to GATT Server */
cy_stc_ble_gattc_write_req_t write_req;


/*******************************************************************************
 * Extern variables
 ******************************************************************************/
/* Variable to store hold BLE connection handle */
extern cy_stc_ble_conn_handle_t conn_handle;


int main(void)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    /* Initialize the device and board peripherals */
    result = cybsp_init() ;

    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Initialize retarget-io to use the debug UART port */
    result = cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, \
                                    CY_RETARGET_IO_BAUDRATE);

    /* Initialize user LED */
    result |= cyhal_gpio_init((cyhal_gpio_t)CYBSP_USER_LED1, CYHAL_GPIO_DIR_OUTPUT,
                                    CYHAL_GPIO_DRIVE_STRONG, CYBSP_LED_STATE_OFF);

    /* retarget-io init or LED init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* GPIO initialization failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
    	CY_ASSERT(0);
    }

    /*Enable interrupts */
    __enable_irq();

    /* Initialize BLESS block */
    ble_init();

    /* Initialize EINK Display Shield */
    eink_init();

    printf("VR\n");

    eink_display("Jai Ho!");
    eink_update();

    /* Initialize sample data struct */
    signal_t signal;
    signal.total_length = EI_CLASSIFIER_SLICE_SIZE;
    signal.get_data = &microphone_audio_signal_get_data;
	int print_results = -(EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW);

    /* Initialize PDM/PCM block */
    pdm_pcm_init();

    for(;;)
    {
        /* Cy_BLE_ProcessEvents() allows BLE stack to process pending events */
        ble_process_events();

        if (data_ready_flag) {
        	data_ready_flag = false;

			ei_impulse_result_t result = { 0 };

			EI_IMPULSE_ERROR err = run_classifier_continuous(&signal, &result, /*debug*/ false);
			if (err != EI_IMPULSE_OK) {
				printf("ERR: Failed to run classifier (%d)\n", err);
				break;
			}


			if(++print_results >= (EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW >> 1)) {
				printf("Predictions (DSP: %d ms, Classification: %d ms: \n",
					result.timing.dsp, result.timing.classification);

				float max_prob = -0.1;
				int8_t argmax = -1;
				for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
					if (result.classification[ix].value > max_prob) {
                                            argmax = ix;
                                            max_prob = result.classification[ix].value;
					}
					printf("    %s: %.5f\n", result.classification[ix].label, result.classification[ix].value);
				}

				char noise[] = "noise";
				char unknown[] = "unknown";
				if((strncmp(result.classification[argmax].label, unknown, sizeof(unknown)) == 0)
						|| (strncmp(result.classification[argmax].label, noise, sizeof(noise)) == 0) ) {
					printf("    %s: %.5f\n", result.classification[argmax].label, result.classification[argmax].value);
				} else {
					if (result.classification[argmax].value >= 0.6 ) {
						//printf("    %s: %.5f\n", result.classification[argmax].label, result.classification[argmax].value);
						eink_display(result.classification[argmax].label);
						eink_update();
						// classes (in order): { "AC_OFF", "AC_ON", "TEMP_DOWN", "TEMP_UP", "noise", "unknown" };
						ble_write_characteristics(argmax);
					}
				}

				print_results = 0;
			}

        }
    }
}

int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr)
{
    numpy::int16_to_float(&full_rx_buffer[offset], out_ptr, length);
    return 0;
}

void ble_write_characteristics(uint8_t command)
{
	if (!cmd_char_uuid_found)
    {
         printf("cmd_char_uuid_found\n");
    }
	else
    {
        ble_write_buffer[0] = 0x00;
        ble_write_buffer[1] = 0x01;
        ble_write_buffer[2] = command;


        /* Send Write command to GATT Peripheral */
        write_req.handleValPair.attrHandle = gatt_cmd_attrHandle; // 0x2A;
        write_req.handleValPair.value.val = ble_write_buffer;
        write_req.handleValPair.value.len = BLE_WRITE_BUFFER_SIZE;
        write_req.connHandle = conn_handle;

        if(Cy_BLE_GATT_GetBusyStatus(conn_handle.attId) == CY_BLE_STACK_STATE_FREE)
        {
            Cy_BLE_GATTC_WriteWithoutResponse(&write_req);
            printf("Cy_BLE_GATTC_WriteWithoutResponse\n");
         }
    }
}

/* this is for checking the sample if it is recording correctly */
void send_recorded_sample_to_uart()
{
    printf("\nSTART\n");
    uint8_t *data = (uint8_t *) full_rx_buffer;
    for (uint32_t index = 0; index < PDM_PCM_BUFFER_SIZE * 2; index++)
    {
        cyhal_uart_putc(&cy_retarget_io_uart_obj, data[index]);
    }
}

