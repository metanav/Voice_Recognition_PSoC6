/*
 * eink.h
 *
 *  Created on: Jan 20, 2021
 *      Author: naveen
 */

#ifndef EINK_H_
#define EINK_H_

#ifdef __cplusplus
extern "C" {
#endif


void eink_init();
void eink_display(const char* message);
void eink_update();


#ifdef __cplusplus
}
#endif

#endif /* EINK_H_ */
