# USB通信协议

## 1. 总体约定

- **帧头**：固定为 `0xAA`，占 1 字节。
- **帧结构**：每帧格式为 `[HDR][CMD][LEN][PAYLOAD...]`。其中 `LEN` 是 payload 长度（0–255）。
- **单位**：
  - 电压单位为 `10 mV`，即发送/接收的 u16 值除以 100 后得到伏特。
  - 电流单位为 `mA`，即 u16 数值为毫安数。
- **u16 量化范围**：u16 最大值 65535，对应 655.35 V 或 65535 mA。

## 2. 命令与应答

| 功能 | CMD | 请求 payload | 应答 CMD | 应答说明 |
|---|---|---|---|---|
| **读取配置** GET_CFG | `0x01` | `LEN=0` | `0x81` | 返回 12 字节 CFG 配置 |
| **写入配置** SET_CFG | `0x02` | `LEN=12`，payload 为 CFG | `0x82` | 应答仅 1 字节 status；0=成功，1=长度错误 |
| **保存配置** SAVE_CFG | `0x03` | `LEN=0` | `0x83` | 将当前配置写入 Flash；status=0 表示保存成功 |
| **设置 MOS 状态** SET_MOS_BITS | `0x04` | `LEN=1`，一个 8bit bitmask；bit0–bit4 分别对应 MOS0–MOS4 | `0x84` | status=0 成功；长度不为 1 返回 status=1 |
| **清除错误状态** CLEAR_ERROR | `0x05` | `LEN=0` | `0x85` | 将 `state.error` 清零；status=0 表示完成 |
| **状态推送** PUSH_STATE | `0x86` | 无请求（由下位机主动发送） | 无 | 下位机定期调用 `usb_mbproto_send_state` 时发送，帧格式为 `0x86` + 12 字节 STATE |
| **未知命令应答** | 其他 | – | `CMD|0x80` | status=0xFF 表示未知命令 |

状态字节 status 在除 GET_CFG、PUSH_STATE 之外的应答中使用，具体见第 5 节。

## 3. 数据结构定义

### 3.1 配置结构 CFG（12 字节）

配置由 6 个 u16 字段组成，使用小端序：

- `vin_min_mV`：VIN 下限阈值（10 mV/LSB）
- `vin_max_mV`：VIN 上限阈值
- `i1_max_mA`：MOS1 通道电流上限
- `i2_max_mA`：MOS2 通道电流上限
- `i3_max_mA`：MOS3 通道电流上限
- `i4_max_mA`：MOS4 通道电流上限

字节布局：

| 字节偏移 | 0–1 | 2–3 | 4–5 | 6–7 | 8–9 | 10–11 |
|---|---|---|---|---|---|---|
| 字段说明 | vin_min_mV | vin_max_mV | i1_max_mA | i2_max_mA | i3_max_mA | i4_max_mA |

### 3.2 状态结构 STATE（12 字节）

下位机推送的状态由 10 字节量化数据和 2 字节标志组成：

- `vin_mV` (u16) – 当前输入电压（10 mV/LSB）。
- `i1_mA` ~ `i4_mA` (u16×4) – 四路 MOS 通道电流（mA/LSB）。
- `mos_bits` (u8) – 当前 MOS 状态，bit0~bit4 分别表示 MOS0~MOS4 是否打开；1=开，0=关。
- `err_bits` (u8) – 错误标志位。

错误位含义按 bit 定义如下：

- bit0 (`0x01`): 输入电压欠压（vin < 下限阈值）
- bit1 (`0x02`): 输入电压过压（vin > 上限阈值）
- bit4 (`0x10`): MOS2 通道电流过流
- bit5 (`0x20`): MOS3 通道电流过流
- bit6 (`0x40`): MOS4 通道电流过流
- bit7 (`0x80`): MOS5 通道电流过流

位2、位3 留空（未定义）。

字节布局：

| 字节偏移 | 0–1 | 2–3 | 4–5 | 6–7 | 8–9 | 10 | 11 |
|---|---|---|---|---|---|---|---|
| 字段说明 | vin_mV | i1_mA | i2_mA | i3_mA | i4_mA | mos_bits | err_bits |

## 4. status 码（应答 payload）

- `0x00` – 成功。
- `0x01` – 长度错误。请求 LEN 与命令定义不符时返回。
- `0xFF` – 未知命令或通用错误。

## 5. 示例

假设：VIN=12 V（120 × 10 mV）、I1=1500 mA、I2=I3=I4=0，mos_state=0b00011（MOS0 和 MOS1 打开），err_bits=0x00。则下位机推送的帧为：

| 字节序列   | AA | 86 | 0C | 00 78 | 05 DC | 00 00 | 00 00 | 00 00 | 03 | 00 |
|---|---|---|---|---|---|---|---|---|---|---|
| 字段说明   | HDR | CMD | LEN | vin_mV | i1_mA | i2_mA | i3_mA | i4_mA | mos_bits | err_bits |

其中 `0x86` 为 PUSH_STATE 命令，`0C` 为 payload 长度 12。