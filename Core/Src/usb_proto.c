/*
 * @Author: ksn
 * @Date: 2025-12-28 14:09:01
 * @LastEditors: ksn
 * @LastEditTime: 2025-12-28 15:50:26
 */
#include "usb_proto.h"
#include <string.h>

#include "usbd_cdc_if.h"
#include "mos.h" // 需要：config, state, ctrl_one_mos(), ctrl_all_mos()

// ---------------- 帧参数 ----------------
#define MB_HDR1 0x55
#define MB_HDR2 0xAA

#define RX_BUF_SZ 512
#define TX_BUF_SZ 64 // 尽量 < 64，USB FS 单包64字节；超过也能发，但更容易分片

static uint8_t rx_buf[RX_BUF_SZ];
static uint16_t rx_len;

static uint8_t tx_buf[TX_BUF_SZ];
static uint16_t tx_len;
static uint8_t tx_pending;

// ---------------- 小工具：LE 读写 ----------------
static void wr_i32_le(uint8_t *p, int32_t v)
{
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static int32_t rd_i32_le(const uint8_t *p)
{
    return (int32_t)((uint32_t)p[0] |
                     ((uint32_t)p[1] << 8) |
                     ((uint32_t)p[2] << 16) |
                     ((uint32_t)p[3] << 24));
}

static void send_frame(uint8_t cmd, uint8_t seq, const uint8_t *payload, uint8_t plen)
{
    // 简化：一次只挂起一帧；BUSY就下次 poll 再发
    if (tx_pending)
        return;

    uint16_t need = (uint16_t)(5 + plen);
    if (need > TX_BUF_SZ)
        return; // 超出则丢弃（或你可改成更大的 TX_BUF_SZ/分片发送）

    tx_buf[0] = MB_HDR1;
    tx_buf[1] = MB_HDR2;
    tx_buf[2] = cmd;
    tx_buf[3] = seq;
    tx_buf[4] = plen;

    if (plen && payload)
    {
        memcpy(&tx_buf[5], payload, plen);
    }

    tx_len = need;
    tx_pending = 1;
}

// status 应答（LEN=1）
static void reply_status(uint8_t cmd_resp, uint8_t seq, uint8_t status)
{
    uint8_t p[1] = {status};
    send_frame(cmd_resp, seq, p, 1);
}

// ---------------- 业务打包 ----------------
static void pack_cfg(uint8_t *out24)
{
    int32_t vin_min_mV = (int32_t)(config.vin_threshold[0] * 1000.0f + 0.5f);
    int32_t vin_max_mV = (int32_t)(config.vin_threshold[1] * 1000.0f + 0.5f);
    int32_t i1_mA = (int32_t)(config.i_max[1] * 1000.0f + 0.5f);
    int32_t i2_mA = (int32_t)(config.i_max[2] * 1000.0f + 0.5f);
    int32_t i3_mA = (int32_t)(config.i_max[3] * 1000.0f + 0.5f);
    int32_t i4_mA = (int32_t)(config.i_max[4] * 1000.0f + 0.5f);

    wr_i32_le(out24 + 0, vin_min_mV);
    wr_i32_le(out24 + 4, vin_max_mV);
    wr_i32_le(out24 + 8, i1_mA);
    wr_i32_le(out24 + 12, i2_mA);
    wr_i32_le(out24 + 16, i3_mA);
    wr_i32_le(out24 + 20, i4_mA);
}

static void apply_cfg(const uint8_t *in24)
{
    int32_t vin_min_mV = rd_i32_le(in24 + 0);
    int32_t vin_max_mV = rd_i32_le(in24 + 4);
    int32_t i1_mA = rd_i32_le(in24 + 8);
    int32_t i2_mA = rd_i32_le(in24 + 12);
    int32_t i3_mA = rd_i32_le(in24 + 16);
    int32_t i4_mA = rd_i32_le(in24 + 20);

    config.vin_threshold[0] = (float)vin_min_mV / 1000.0f;
    config.vin_threshold[1] = (float)vin_max_mV / 1000.0f;
    config.i_max[1] = (float)i1_mA / 1000.0f;
    config.i_max[2] = (float)i2_mA / 1000.0f;
    config.i_max[3] = (float)i3_mA / 1000.0f;
    config.i_max[4] = (float)i4_mA / 1000.0f;
}

static void pack_state(uint8_t *out21)
{
    int32_t vin_mV = (int32_t)(state.vin * 1000.0f + 0.5f);
    int32_t i1_mA = (int32_t)(state.i[1] * 1000.0f + 0.5f);
    int32_t i2_mA = (int32_t)(state.i[2] * 1000.0f + 0.5f);
    int32_t i3_mA = (int32_t)(state.i[3] * 1000.0f + 0.5f);
    int32_t i4_mA = (int32_t)(state.i[4] * 1000.0f + 0.5f);

    wr_i32_le(out21 + 0, vin_mV);
    wr_i32_le(out21 + 4, i1_mA);
    wr_i32_le(out21 + 8, i2_mA);
    wr_i32_le(out21 + 12, i3_mA);
    wr_i32_le(out21 + 16, i4_mA);

    uint8_t bits = 0;
    for (int i = 0; i < 5; i++)
    {
        if (state.mos_state[i] == MOS_OPEN)
            bits |= (1u << i);
    }
    out21[20] = bits;
}

// ---------------- 命令处理 ----------------
static void handle_frame(uint8_t cmd, uint8_t seq, const uint8_t *pl, uint8_t plen)
{
    switch (cmd)
    {
    case 0x01:
    { // GET_CFG
        if (plen != 0)
        {
            reply_status(0x81, seq, 1);
            return;
        }
        uint8_t p[24];
        pack_cfg(p);
        send_frame(0x81, seq, p, 24);
    }
    break;

    case 0x02:
    { // SET_CFG
        if (plen != 24)
        {
            reply_status(0x82, seq, 1);
            return;
        }
        apply_cfg(pl);
        reply_status(0x82, seq, 0);
    }
    break;

    case 0x03:
    { // GET_STATE
        if (plen != 0)
        {
            reply_status(0x83, seq, 1);
            return;
        }
        uint8_t p[21];
        pack_state(p);
        send_frame(0x83, seq, p, 21);
    }
    break;

    case 0x04:
    { // SET_MOS
        if (plen != 2)
        {
            reply_status(0x84, seq, 1);
            return;
        }
        uint8_t idx = pl[0];
        uint8_t on = pl[1];
        if (idx < 1 || idx > 5)
        {
            reply_status(0x84, seq, 2);
            return;
        }
        if (on > 1)
        {
            reply_status(0x84, seq, 3);
            return;
        }

        ctrl_one_mos(idx, on ? MOS_OPEN : MOS_CLOSE);
        reply_status(0x84, seq, 0);
    }
    break;

    case 0x05:
    { // SET_MOS_BITS
        if (plen != 1)
        {
            reply_status(0x85, seq, 1);
            return;
        }
        uint8_t bits = pl[0];
        for (int i = 0; i < 5; i++)
        {
            uint8_t on = (bits >> i) & 0x01;
            ctrl_one_mos((uint8_t)(i + 1), on ? MOS_OPEN : MOS_CLOSE);
        }
        reply_status(0x85, seq, 0);
    }
    break;

    default:
        // 未知命令：给一个通用错误
        reply_status((uint8_t)(cmd | 0x80), seq, 0xFF);
        break;
    }
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
    while (rx_len >= 5)
    {
        // 找帧头
        if (!(rx_buf[0] == MB_HDR1 && rx_buf[1] == MB_HDR2))
        {
            // 不是帧头就丢 1 字节继续找
            drop_bytes(1);
            continue;
        }

        uint8_t cmd = rx_buf[2];
        uint8_t seq = rx_buf[3];
        uint8_t plen = rx_buf[4];

        uint16_t frame_len = (uint16_t)(5 + plen);
        if (frame_len > RX_BUF_SZ)
        {
            drop_bytes(2);
            continue;
        } // 异常长度，丢弃帧头重找
        if (rx_len < frame_len)
            break; // 数据还不够，等待下一次 RX

        // 有完整帧
        const uint8_t *pl = &rx_buf[5];
        handle_frame(cmd, seq, pl, plen);

        // 消费这一帧
        drop_bytes(frame_len);

        // 若生成了应答，会在下一轮优先发送
        if (tx_pending)
            break;
    }
}
