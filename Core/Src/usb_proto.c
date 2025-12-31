/*
 * @Author: ksn
 * @Date: 2025-12-28 14:09:01
 * @LastEditors: ksn
 * @LastEditTime: 2025-12-28 17:16:43
 */
#include "usb_proto.h"
#include <string.h>

#include "usbd_cdc_if.h"
#include "mos.h" // 需要：config, state, ctrl_one_mos(), ctrl_all_mos()

// ---------------- 帧参数 ----------------
// 帧头只保留 1 字节（无 CRC / 无 seq）
#define MB_HDR 0xAA

#define RX_BUF_SZ 512
#define TX_BUF_SZ 64 // 尽量 < 64，USB FS 单包64字节；超过也能发，但更容易分片

static uint8_t rx_buf[RX_BUF_SZ];
static uint16_t rx_len;

static uint8_t tx_buf[TX_BUF_SZ];
static uint16_t tx_len;
static uint8_t tx_pending;

// ---------------- 小工具：LE 读写（u16） ----------------
static void wr_u16_le(uint8_t *p, uint16_t v)
{
    p[1] = (uint8_t)(v);
    p[0] = (uint8_t)(v >> 8);
}

static uint16_t rd_u16_le(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[1] | ((uint16_t)p[0] << 8));
}

static uint16_t sat_u16_from_f(float v)
{
    // v < 0 -> 0; v > 65535 -> 65535
    if (v <= 0.0f)
        return 0;
    if (v >= 65535.0f)
        return 65535u;
    return (uint16_t)(v + 0.5f);
}

static void send_frame(uint8_t cmd, const uint8_t *payload, uint8_t plen)
{
    // 简化：一次只挂起一帧；BUSY就下次 poll 再发
    if (tx_pending)
        return;

    // 帧格式： [HDR][CMD][LEN][PAYLOAD...]
    uint16_t need = (uint16_t)(3 + plen);
    if (need > TX_BUF_SZ)
        return; // 超出则丢弃（或你可改成更大的 TX_BUF_SZ/分片发送）

    tx_buf[0] = MB_HDR;
    tx_buf[1] = cmd;
    tx_buf[2] = plen;

    if (plen && payload)
    {
        memcpy(&tx_buf[3], payload, plen);
    }

    tx_len = need;
    tx_pending = 1;
}

// status 应答（LEN=1）
static void reply_status(uint8_t cmd_resp, uint8_t status)
{
    uint8_t p[1] = {status};
    send_frame(cmd_resp, p, 1);
}

// ---------------- 业务打包 ----------------
static void pack_cfg(uint8_t *out12)
{
    // u16 量化：vin(mV)、i(mA)
    uint16_t vin_min_mV = sat_u16_from_f(config.vin_threshold[0] * 100.0f);
    uint16_t vin_max_mV = sat_u16_from_f(config.vin_threshold[1] * 100.0f);
    uint16_t i1_mA = sat_u16_from_f(config.i_max[1] * 1000.0f);
    uint16_t i2_mA = sat_u16_from_f(config.i_max[2] * 1000.0f);
    uint16_t i3_mA = sat_u16_from_f(config.i_max[3] * 1000.0f);
    uint16_t i4_mA = sat_u16_from_f(config.i_max[4] * 1000.0f);

    wr_u16_le(out12 + 0, vin_min_mV);
    wr_u16_le(out12 + 2, vin_max_mV);
    wr_u16_le(out12 + 4, i1_mA);
    wr_u16_le(out12 + 6, i2_mA);
    wr_u16_le(out12 + 8, i3_mA);
    wr_u16_le(out12 + 10, i4_mA);
}

static void apply_cfg(const uint8_t *in12)
{
    uint16_t vin_min_mV = rd_u16_le(in12 + 0);
    uint16_t vin_max_mV = rd_u16_le(in12 + 2);
    uint16_t i1_mA = rd_u16_le(in12 + 4);
    uint16_t i2_mA = rd_u16_le(in12 + 6);
    uint16_t i3_mA = rd_u16_le(in12 + 8);
    uint16_t i4_mA = rd_u16_le(in12 + 10);

    config.vin_threshold[0] = (float)vin_min_mV / 100.0f;
    config.vin_threshold[1] = (float)vin_max_mV / 100.0f;
    config.i_max[1] = (float)i1_mA / 1000.0f;
    config.i_max[2] = (float)i2_mA / 1000.0f;
    config.i_max[3] = (float)i3_mA / 1000.0f;
    config.i_max[4] = (float)i4_mA / 1000.0f;
}

static void pack_state(uint8_t *out11)
{
    // u16 量化：vin(mV)、i(mA)
    uint16_t vin_mV = sat_u16_from_f(state.vin * 100.0f);
    uint16_t i1_mA = sat_u16_from_f(state.i[1] * 1000.0f);
    uint16_t i2_mA = sat_u16_from_f(state.i[2] * 1000.0f);
    uint16_t i3_mA = sat_u16_from_f(state.i[3] * 1000.0f);
    uint16_t i4_mA = sat_u16_from_f(state.i[4] * 1000.0f);

    wr_u16_le(out11 + 0, vin_mV);
    wr_u16_le(out11 + 2, i1_mA);
    wr_u16_le(out11 + 4, i2_mA);
    wr_u16_le(out11 + 6, i3_mA);
    wr_u16_le(out11 + 8, i4_mA);

    uint8_t bits = 0;
    for (int i = 0; i < 5; i++)
    {
        if (state.mos_state[i] == MOS_OPEN)
            bits |= (1u << i);
    }
    out11[10] = bits;
}

// ---------------- 命令处理 ----------------
static void handle_frame(uint8_t cmd, const uint8_t *pl, uint8_t plen)
{
    switch (cmd)
    {
    case 0x01:
    { // GET_CFG
        if (plen != 0)
        {
            reply_status(0x81, 1);
            return;
        }
        uint8_t p[12];
        pack_cfg(p);
        send_frame(0x81, p, 12);
    }
    break;

    case 0x02:
    { // SET_CFG
        if (plen != 12)
        {
            reply_status(0x82, 1);
            return;
        }
        apply_cfg(pl);
        reply_status(0x82, 0);
    }
    break;

    case 0x03:
    { // SAVE_STATE
        if (plen != 0)
        {
            reply_status(0x83, 1);
            return;
        }
        config_save();
        reply_status(0x83, 0);
    }
    break;

    case 0x04:
    { // SET_MOS_BITS
        if (plen != 1)
        {
            reply_status(0x84, 1);
            return;
        }
        uint8_t bits = pl[0];
        for (int i = 0; i < 5; i++)
        {
            uint8_t on = (bits >> i) & 0x01;
            ctrl_one_mos((uint8_t)(i), on ? MOS_OPEN : MOS_CLOSE);
        }
        reply_status(0x84, 0);
    }
    break;

    default:
        // 未知命令：给一个通用错误
        reply_status((uint8_t)(cmd | 0x80), 0xFF);
        break;
    }
}

uint8_t usb_mbproto_send_state(void)
{
    // 只排队，不在这里直接 CDC_Transmit_FS，避免在中断等场景误用
    if (tx_pending)
        return 0;

    uint8_t p[11];
    pack_state(p);
    send_frame(0x85, p, 11);
    return tx_pending ? 1 : 0;
}

// ---------------- 接收：缓存 + 解析 ----------------
void usb_mbproto_init(void)
{
    rx_len = 0;
    tx_pending = 0;
}

void usb_mbproto_on_rx(const uint8_t *data, uint32_t len)
{
    // 追加到 rx_buf，溢出则清空（无 CRC 情况下，简单粗暴更稳）
    if (len > (RX_BUF_SZ - rx_len))
    {
        rx_len = 0;
    }
    uint16_t cpy = (uint16_t)((len > (RX_BUF_SZ - rx_len)) ? (RX_BUF_SZ - rx_len) : len);
    memcpy(&rx_buf[rx_len], data, cpy);
    rx_len += cpy;
}

static void drop_bytes(uint16_t n)
{
    if (n >= rx_len)
    {
        rx_len = 0;
        return;
    }
    memmove(rx_buf, rx_buf + n, rx_len - n);
    rx_len -= n;
}

void usb_mbproto_poll(void)
{
    // 1) 先发 pending
    if (tx_pending)
    {
        if (CDC_Transmit_FS(tx_buf, tx_len) == USBD_OK)
        {
            tx_pending = 0;
        }
        return; // BUSY 下次再试
    }

    // 2) 解析帧：扫帧头 + 看 LEN 是否齐
    while (rx_len >= 3)
    {
        // 找帧头
        if (rx_buf[0] != MB_HDR)
        {
            // 不是帧头就丢 1 字节继续找
            drop_bytes(1);
            continue;
        }

        uint8_t cmd = rx_buf[1];
        uint8_t plen = rx_buf[2];

        uint16_t frame_len = (uint16_t)(3 + plen);
        if (frame_len > RX_BUF_SZ)
        {
            // 异常长度，丢弃帧头重找
            drop_bytes(1);
            continue;
        }
        if (rx_len < frame_len)
            break; // 数据还不够，等待下一次 RX

        // 有完整帧
        const uint8_t *pl = &rx_buf[3];
        handle_frame(cmd, pl, plen);

        // 消费这一帧
        drop_bytes(frame_len);

        // 若生成了应答，会在下一轮优先发送
        if (tx_pending)
            break;
    }
}
