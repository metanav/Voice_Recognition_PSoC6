/*
 * eink.c
 *
 *  Created on: Jan 20, 2021
 *      Author: naveen
 */

#include "cyhal.h"
#include "cy8ckit_028_epd_pins.h"
#include "mtb_e2271cs021.h"
#include "GUI.h"
#include "LCDConf.h"
#include "eink.h"

#define AMBIENT_TEMPERATURE_C    (20)
#define SPI_BAUD_RATE_HZ         (20000000)

cyhal_spi_t spi;

const mtb_e2271cs021_pins_t pins =
{
    .spi_mosi = CY8CKIT_028_EPD_PIN_DISPLAY_SPI_MOSI,
    .spi_miso = CY8CKIT_028_EPD_PIN_DISPLAY_SPI_MISO,
    .spi_sclk = CY8CKIT_028_EPD_PIN_DISPLAY_SPI_SCLK,
    .spi_cs = CY8CKIT_028_EPD_PIN_DISPLAY_CS,
    .reset = CY8CKIT_028_EPD_PIN_DISPLAY_RST,
    .busy = CY8CKIT_028_EPD_PIN_DISPLAY_BUSY,
    .discharge = CY8CKIT_028_EPD_PIN_DISPLAY_DISCHARGE,
    .enable = CY8CKIT_028_EPD_PIN_DISPLAY_EN,
    .border = CY8CKIT_028_EPD_PIN_DISPLAY_BORDER,
    .io_enable = CY8CKIT_028_EPD_PIN_DISPLAY_IOEN,
};

/* Buffer to the previous frame written on the display */
uint8_t previous_frame[PV_EINK_IMAGE_SIZE] = {0};

/* Pointer to the new frame that need to be written */
uint8_t *current_frame;

void eink_init()
{
	cy_rslt_t result = CY_RSLT_SUCCESS;

    /* Initialize SPI and EINK display */
	result = cyhal_spi_init(&spi, CY8CKIT_028_EPD_PIN_DISPLAY_SPI_MOSI,
                      CY8CKIT_028_EPD_PIN_DISPLAY_SPI_MISO,
                      CY8CKIT_028_EPD_PIN_DISPLAY_SPI_SCLK, NC, NULL, 8,
                      CYHAL_SPI_MODE_00_MSB, false);

    if (CY_RSLT_SUCCESS == result)
    {
        result = cyhal_spi_set_frequency(&spi, SPI_BAUD_RATE_HZ);
    }

    result = mtb_e2271cs021_init(&pins, &spi);

    /* Set ambient temperature, in degree C, in order to perform temperature
     * compensation of E-INK parameters */
    mtb_e2271cs021_set_temp_factor(AMBIENT_TEMPERATURE_C);

    current_frame = (uint8_t*)LCD_GetDisplayBuffer();

    GUI_Init();
}

void eink_display(const char* message)
{
    GUI_SetTextMode(GUI_TM_NORMAL);
    GUI_SetFont(GUI_FONT_32B_1);
    GUI_SetBkColor(GUI_WHITE);
    GUI_SetColor(GUI_BLACK);
    GUI_SetTextAlign(GUI_ALIGN_HCENTER | GUI_ALIGN_HCENTER);
    GUI_Clear();

    GUI_DispStringAt(message, LCD_GetXSize() / 2, (LCD_GetYSize() / 2) - (GUI_GetFontSizeY() / 2));
}

void eink_update()
{
    /* update the display */
    mtb_e2271cs021_show_frame(previous_frame, current_frame, MTB_E2271CS021_FULL_4STAGE, true);
}



