<!--
 * @Author: ksn
 * @Date: 2025-12-28 16:44:16
 * @LastEditors: ksn
 * @LastEditTime: 2025-12-28 17:24:55
-->
# HuanQiDong USB 协议

## 1. 总体约定

- **帧头**：1 字节，固定为 `0xAA`
- **单位**：
  - 电压：`10mV`（毫伏）
  - 电流：`mA`（毫安）
- **范围（u16 量化）**：
  - `u16` 最大 `65535` → `655.35 V `
  - `u16` 最大 `65535` → `65535 mA ≈ 65.535 A`


---

## 2. 帧格式

```
Byte0   Byte1   Byte2   Byte3..(3+LEN-1)
+------+-------+------+------------------+
| HDR  |  CMD  | LEN  |     PAYLOAD      |
+------+-------+------+------------------+
 0xAA   1 byte  1 byte     LEN bytes
```

- `HDR`：固定 `0xAA`
- `CMD`：命令码
- `LEN`：payload 长度（0~255）
- `PAYLOAD`：按命令定义

---

## 3. 命令列表

### 3.1 配置读取/写入

#### (1) GET_CFG
- **请求**：`CMD=0x01`，`LEN=0`
- **应答**：`CMD=0x81`，`LEN=12`，payload 为 CFG（见 4.1）

#### (2) SET_CFG
- **请求**：`CMD=0x02`，`LEN=12`，payload 为 CFG（见 4.1）
- **应答**：`CMD=0x82`，`LEN=1`，payload 为 `status`（见 5）

#### (3) SAVE_CFG
- **请求**：`CMD=0x03`，`LEN=0`
- **应答**：`CMD=0x83`，`LEN=1`，payload 为 `status`（见 5）

### 3.2 MOS 控制

#### (4) SET_MOS_BITS（多路 bitmask）
- **请求**：`CMD=0x04`，`LEN=1`
  - `payload[0] = bits`：bit0~bit4 对应 MOS1~MOS5（1=开，0=关）
- **应答**：`CMD=0x84`，`LEN=1`，payload 为 `status`

### 3.3 状态推送

#### (5) PUSH_STATE（下位机主动推送）
- **发送帧**：`CMD=0x85`，`LEN=11`，payload 同 STATE（见 4.2）
---

## 4. Payload 结构定义

### 4.1 CFG（LEN = 12 bytes）

| 字段 | 类型 | 单位 | 说明 |
|---|---|---|---|
| vin_min_mV | u16 | mV | VIN 下限阈值 |
| vin_max_mV | u16 | mV | VIN 上限阈值 |
| i1_max_mA  | u16 | mA | 通道1电流上限 |
| i2_max_mA  | u16 | mA | 通道2电流上限 |
| i3_max_mA  | u16 | mA | 通道3电流上限 |
| i4_max_mA  | u16 | mA | 通道4电流上限 |

字节布局：

```
offset: 0  1 | 2  3 | 4  5 | 6  7 | 8  9 | 10 11
       vin_min vin_max  i1    i2     i3      i4
```

### 4.2 STATE（LEN = 11 bytes）

| 字段 | 类型 | 单位 | 说明 |
|---|---|---|---|
| vin_mV | u16 | mV | 当前输入电压 |
| i1_mA  | u16 | mA | 通道1电流 |
| i2_mA  | u16 | mA | 通道2电流 |
| i3_mA  | u16 | mA | 通道3电流 |
| i4_mA  | u16 | mA | 通道4电流 |
| mos_bits | u8 | - | bit0~bit4 对应 MOS1~MOS5（1=开） |

字节布局：

```
offset: 0  1 | 2  3 | 4  5 | 6  7 | 8  9 | 10
       vin    i1     i2     i3     i4    mos_bits
```

---

## 5. status 码（LEN=1）

所有 `SET_*` 的应答 payload 都是 1 字节 `status`：

- `0x00`：OK
- `0x01`：长度错误（LEN 不匹配）
- `0x02`：参数错误（例如 idx 越界）
- `0x03`：参数错误（例如 on 不是 0/1）
- `0xFF`：未知命令 / 通用错误

---

## 6. PUSH_STATE 示例

假设：
- vin=50000mV → `0x1388`
- i1=1234mA → `0x04D2`
- i2=i3=i4=0
- mos_bits=0x03（MOS1、MOS2 开）

```
AA 83 0B  20 4E  D2 04  00 00  00 00  00 00  03
|  |  |    vin     i1     i2     i3     i4    bits
HDR CMD LEN
```
