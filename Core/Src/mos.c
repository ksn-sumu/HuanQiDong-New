/*
 * @Author: ksn
 * @Date: 2025-12-18 22:06:06
 * @LastEditors: ksn
 * @LastEditTime: 2025-12-28 15:36:35
 */
//
// Created by 12076 on 2025/12/20.
//

#include "mos.h"
#include <string.h>

config_t config = {{0, 50}, {3, 3, 3, 3, 3}};
state_t state = {0, {0, 0, 0, 0, 0}, {MOS_CLOSE, MOS_CLOSE, MOS_CLOSE, MOS_CLOSE, MOS_CLOSE}};

void ctrl_one_mos(const uint8_t mos, const GPIO_PinState mos_state)
{
    switch (mos)
    {
    case 0:
        HAL_GPIO_WritePin(CTRL0_GPIO_Port, CTRL0_Pin, mos_state);
        break;
    case 1:
        HAL_GPIO_WritePin(CTRL1_GPIO_Port, CTRL1_Pin, mos_state);
        break;
    case 2:
        HAL_GPIO_WritePin(CTRL2_GPIO_Port, CTRL2_Pin, mos_state);
        break;
    case 3:
        HAL_GPIO_WritePin(CTRL3_GPIO_Port, CTRL3_Pin, mos_state);
        break;
    case 4:
        HAL_GPIO_WritePin(CTRL4_GPIO_Port, CTRL4_Pin, mos_state);
        break;
    default:
        break;
    }
    state.mos_state[mos] = mos_state;
}

void ctrl_all_mos(const GPIO_PinState mos_state)
{
    HAL_GPIO_WritePin(CTRL0_GPIO_Port, CTRL0_Pin, mos_state);
    HAL_GPIO_WritePin(CTRL1_GPIO_Port, CTRL1_Pin, mos_state);
    HAL_GPIO_WritePin(CTRL2_GPIO_Port, CTRL2_Pin, mos_state);
    HAL_GPIO_WritePin(CTRL3_GPIO_Port, CTRL3_Pin, mos_state);
    HAL_GPIO_WritePin(CTRL4_GPIO_Port, CTRL4_Pin, mos_state);
    for (int i = 0; i < 5; i++)
        state.mos_state[i] = mos_state;
}

void check_all(void)
{
    if (not_in(state.vin, config.vin_threshold))
        ctrl_all_mos(MOS_CLOSE);
    for (uint8_t j = 1; j < 5; j++)
    {
        if (state.i[j] > config.i_max[j])
            ctrl_one_mos(j, MOS_CLOSE);
    }
}

#define CONFIG_FLASH_ADDR 0x08007C00UL
#define CONFIG_MAGIC 0x43464731UL

typedef struct
{
    uint32_t magic;
    config_t cfg;
    uint32_t sum; /* 简单校验：所有 32bit 字相加 */
} config_blob_t;

static uint32_t sum32(const void *data)
{
    const uint32_t *p = data;
    const uint32_t words = sizeof(config_t) / 4U;
    uint32_t s = 0;
    for (uint32_t i = 0; i < words; i++)
        s += p[i];
    return s;
}

/* 从 Flash 加载到全局变量 config：成功返回 1，失败返回 0 */
uint8_t config_load(void)
{
    const config_blob_t *b = (const config_blob_t *)CONFIG_FLASH_ADDR;

    if (b->magic != CONFIG_MAGIC)
        return 0U;

    const uint32_t s = sum32(&b->cfg);
    if (s != b->sum)
        return 0U;

    memcpy(&config, &b->cfg, sizeof(config_t));
    return 1U;
}

/* 将全局变量 config 保存到 Flash：成功返回 1，失败返回 0 */
uint8_t config_save(void)
{
    /* 组织写入内容 */
    config_blob_t blob;
    blob.magic = CONFIG_MAGIC;
    blob.cfg = config;
    blob.sum = sum32(&blob.cfg);

    uint32_t write_len = sizeof(blob);
    if (write_len & 1U)
        write_len++; /* 半字对齐（通常本来就是偶数） */

    HAL_FLASH_Unlock();

    /* 1) 擦除最后一页（1KB） */
    FLASH_EraseInitTypeDef e = {0};
    uint32_t page_error = 0;

    e.TypeErase = FLASH_TYPEERASE_PAGES;
    e.PageAddress = CONFIG_FLASH_ADDR;
    e.NbPages = 1;

    if (HAL_FLASHEx_Erase(&e, &page_error) != HAL_OK)
    {
        HAL_FLASH_Lock();
        return 0U;
    }

    /* 2) 半字写入（STM32F1 只能 half-word 编程） */
    const uint8_t *b = (const uint8_t *)&blob;
    for (uint32_t i = 0; i < write_len; i += 2)
    {
        const uint16_t hw = (uint16_t)b[i] | ((uint16_t)b[i + 1] << 8);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, CONFIG_FLASH_ADDR + i, hw) != HAL_OK)
        {
            HAL_FLASH_Lock();
            return 0U;
        }
    }

    HAL_FLASH_Lock();
    return 1U;
}
