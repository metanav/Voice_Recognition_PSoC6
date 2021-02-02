/*
 * pdm_pcm.c
 *
 *  Created on: Jan 20, 2021
 *      Author: naveen
 */

#include "cyhal.h"
#include "pdm_pcm.h"

/*******************************************************************************
* Macros
********************************************************************************/
/* Desired sample rate. Typical values: 8/16/22.05/32/44.1/48kHz */
#define SAMPLE_RATE_HZ              16000u
/* Decimation Rate of the PDM/PCM block. Typical value is 64 */
#define DECIMATION_RATE             64u
/* Audio Subsystem Clock. Typical values depends on the desire sample rate:
- 8/16/48kHz    : 24.576 MHz
- 22.05/44.1kHz : 22.579 MHz */
#define AUDIO_SYS_CLOCK_HZ          24576000u
/* PDM/PCM Pins */
#define PDM_DATA                    P10_5
#define PDM_CLK                     P10_4

/*******************************************************************************
* Function Prototypes
********************************************************************************/
void clock_init(void);
void pdm_pcm_isr_handler(void *arg, cyhal_pdm_pcm_event_t event);

/*******************************************************************************
* Global Variables
********************************************************************************/
/* HAL Object */
cyhal_pdm_pcm_t pdm_pcm;
cyhal_clock_t   audio_clock;
cyhal_clock_t   pll_clock;

/* HAL Config */
const cyhal_pdm_pcm_cfg_t pdm_pcm_cfg =
{
    .sample_rate     = SAMPLE_RATE_HZ,
    .decimation_rate = DECIMATION_RATE,
    .mode            = CYHAL_PDM_PCM_MODE_LEFT,
    .word_length     = 16,  /* bits */
    .left_gain       = CY_PDM_PCM_GAIN_10_5_DB,   /* dB */
    .right_gain      = 0U,   /* dB */
};

int16_t rx_buffer0[PDM_PCM_BUFFER_SIZE];
int16_t rx_buffer1[PDM_PCM_BUFFER_SIZE];

void pdm_pcm_init()
{
	data_ready_flag = false;

    /* Init the clocks */
    clock_init();

    /* Initialize the PDM/PCM block */
    cyhal_pdm_pcm_init(&pdm_pcm, PDM_DATA, PDM_CLK, &audio_clock, &pdm_pcm_cfg);
    cyhal_pdm_pcm_register_callback(&pdm_pcm, &pdm_pcm_isr_handler, &pdm_pcm);
    cyhal_pdm_pcm_enable_event(&pdm_pcm, CYHAL_PDM_PCM_ASYNC_COMPLETE, CYHAL_ISR_PRIORITY_DEFAULT, true);

    /* Configure asynchronous transfers to use DMA to free up the CPU during transfers */
    //cy_rslt_t result = cyhal_pdm_pcm_set_async_mode(&pdm_pcm, CYHAL_ASYNC_DMA, CYHAL_DMA_PRIORITY_DEFAULT);
    //CY_ASSERT(CY_RSLT_SUCCESS == result);

    //Cy_SysLib_Delay(1000);
    cyhal_pdm_pcm_start(&pdm_pcm);
    active_rx_buffer = rx_buffer0;
    full_rx_buffer   = rx_buffer1;
    cyhal_pdm_pcm_read_async(&pdm_pcm, rx_buffer0, PDM_PCM_BUFFER_SIZE);
}

void pdm_pcm_isr_handler(void *arg, cyhal_pdm_pcm_event_t event)
{
    (void) event;
    cyhal_pdm_pcm_t *pdm_pcm_obj = (cyhal_pdm_pcm_t *)arg;

    /* Flip the active and the next rx buffers */
	int16_t *temp = active_rx_buffer;
	active_rx_buffer = full_rx_buffer;
	full_rx_buffer = temp;

    data_ready_flag = true;
    //printf("pdm_pcm_isr_handler\n");

	/* Start reading into the next buffer while the just-filled one is being processed */
	cyhal_pdm_pcm_read_async(pdm_pcm_obj, active_rx_buffer, PDM_PCM_BUFFER_SIZE);
}

void clock_init(void)
{
    /* Initialize the PLL */
    cyhal_clock_get(&pll_clock, &CYHAL_CLOCK_PLL[0]);
    cyhal_clock_init(&pll_clock);
    cyhal_clock_set_frequency(&pll_clock, AUDIO_SYS_CLOCK_HZ, NULL);
    cyhal_clock_set_enabled(&pll_clock, true, true);

    /* Initialize the audio subsystem clock (CLK_HF[1])
     * The CLK_HF[1] is the root clock for the I2S and PDM/PCM blocks */
    cyhal_clock_get(&audio_clock, &CYHAL_CLOCK_HF[1]);
    cyhal_clock_init(&audio_clock);

    /* Source the audio subsystem clock from PLL */
    cyhal_clock_set_source(&audio_clock, &pll_clock);
    cyhal_clock_set_enabled(&audio_clock, true, true);
}


