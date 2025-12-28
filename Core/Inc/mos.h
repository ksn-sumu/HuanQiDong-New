//
// Created by 12076 on 2025/12/20.
//

#ifndef HUANQIDONG_MOS_H
#define HUANQIDONG_MOS_H

#include "main.h"

#define MOS_CLOSE GPIO_PIN_RESET
#define MOS_OPEN GPIO_PIN_SET

#define not_in(num, threshold) (num < threshold[0] || num > threshold[1])

typedef struct {
	float vin_threshold[2];
    float i_max[5];
} config_t;

typedef struct {
	float vin;
	float i[5];
	GPIO_PinState mos_state[5];
} state_t;

extern config_t config;
extern state_t state;

void ctrl_one_mos(uint8_t mos, GPIO_PinState mos_state);
void ctrl_all_mos(GPIO_PinState mos_state);
void check_all(void);
uint8_t config_load(void);
uint8_t config_save(void);

#endif // HUANQIDONG_MOS_H
