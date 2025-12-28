/*
 * @Author: ksn
 * @Date: 2025-12-28 14:10:02
 * @LastEditors: ksn
 * @LastEditTime: 2025-12-28 15:34:12
 */
//
// Created by 12076 on 2025/12/23.
//

#ifndef HUANQIDONG_COMMUNICATION_H
#define HUANQIDONG_COMMUNICATION_H

#include "main.h"
#include <stdint.h>

void usb_mbproto_init(void);
void usb_mbproto_on_rx(const uint8_t *data, uint32_t len);
void usb_mbproto_poll(void);

#endif // HUANQIDONG_COMMUNICATION_H
