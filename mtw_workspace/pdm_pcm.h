/*
 * pdm_pcm.h
 *
 *  Created on: Jan 20, 2021
 *      Author: naveen
 */

#ifndef PDM_PCM_H_
#define PDM_PCM_H_
#ifdef __cplusplus
extern "C" {
#endif

/* Define how many samples in a frame */
#define PDM_PCM_BUFFER_SIZE  (16000u)


volatile bool data_ready_flag;

/* Double buffer pointers */
int16_t *active_rx_buffer;
int16_t *full_rx_buffer;

void pdm_pcm_init();

#ifdef __cplusplus
}
#endif
#endif /* PDM_PCM_H_ */
