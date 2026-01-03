/*
 * @Author: ksn
 * @Date: 2025-12-19 13:16:22
 * @LastEditors: ksn
 * @LastEditTime: 2026-01-02 18:29:03
 */
//
// Created by 12076 on 2025/12/20.
//

#ifndef HUANQIDONG_MOS_H
#define HUANQIDONG_MOS_H

#include "main.h"

#define MOS_CLOSE GPIO_PIN_RESET
#define MOS_OPEN GPIO_PIN_SET

#define not_in(num, threshold) (num < threshold[0] || num > threshold[1])

#define error_vin_under 0x01
#define error_vin_over 0x02
#define error_i2_over 0x10
#define error_i3_over 0x20
#define error_i4_over 0x40
#define error_i5_over 0x80
#define error_all 0xF3

#define mos0 0x01
#define mos1 0x02
#define mos2 0x04
#define mos3 0x08
#define mos4 0x10

#define set1(x, y) (x |= y)
#define set0(x, y) (x &= ~y)
#define check(x, y) (x & y)

typedef struct
{
    float vin_threshold[2];
    float i_max[5];
} config_t;

typedef struct
{
    float vin;
    float i[5];
    uint8_t mos_state;
    uint8_t error;
    // bit0: vin 欠压
    // bit1: vin 过压
    // bit4: i2 过流
    // bit5: i3 过流
    // bit6: i4 过流
    // bit7: i5 过流
} state_t;

extern config_t config;
extern state_t state;

void ctrl_one_mos(uint8_t mos, GPIO_PinState mos_state);
void ctrl_all_mos(uint8_t mos_state);
void check_all(void);
uint8_t config_load(void);
uint8_t config_save(void);

#endif // HUANQIDONG_MOS_H
