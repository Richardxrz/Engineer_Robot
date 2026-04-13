# CAN通信协议分析报告

## 一、项目CAN架构概述

该项目是一个多电机控制系统（标准机器人平台），支持多种电机类型和协议。系统采用两路CAN总线（CAN1和CAN2）实现通信。

---

## 二、核心文件清单

### 2.1 硬件抽象层 (HAL/BSP)

| 文件路径 | 功能描述 |
|---------|--------|
| **Src/can.c** | CAN1/CAN2初始化、GPIO配置、中断管理（STM32 HAL库） |
| **Inc/can.h** | CAN接口头文件定义 |
| **bsp/boards/bsp_can.c** | CAN滤波器初始化和消息发送实现 |
| **bsp/boards/bsp_can.h** | CAN控制数据结构和接口定义 |

### 2.2 通信协议层

| 文件路径 | 功能描述 |
|---------|--------|
| **application/robot_cmd/CAN_receive.h/c** | CAN接收中断处理和数据解码 |
| **application/robot_cmd/CAN_communication.h/c** | 板间通信实现（RC和云台数据） |
| **application/typedef/can_typedef.h** | CAN通信数据结构定义 |
| **application/referee/protocol.h** | 裁判系统协议定义（参赛队伍协议） |

### 2.3 电机控制协议

| 文件路径 | 电机类型 | 功能 |
|---------|--------|------|
| **CAN_cmd_dji.h/c** | DJI M3508/M2006/M6020 | 标准ID，电流/电压控制 |
| **CAN_cmd_cybergear.h/c** | 小米Cybergear | 扩展ID，多种控制模式 |
| **CAN_cmd_damiao.h/c** | 达妙DM8009 | 标准ID，MIT控制模式 |
| **CAN_cmd_lingkong.h/c** | 瓴控MF9025 | 标准ID，转矩/电流控制 |
| **CAN_cmd_SupCap.h/c** | 超级电容 | 标准ID，功率控制 |

### 2.4 数据结构定义

| 文件路径 | 定义内容 |
|---------|--------|
| **application/robot_cmd/motor.h** | 通用电机结构体、测量数据结构 |
| **application/robot_cmd/SupCap.h** | 超级电容结构体 |
| **components/algorithm/user_lib.h** | 数据转换函数库 |

---

## 三、协议格式定义详解

### 3.1 DJI电机协议 (标准ID)

#### 消息ID定义
```c
DJI_M1_ID  = 0x201  // 3508/2006电机M1
DJI_M2_ID  = 0x202  // 3508/2006电机M2
DJI_M3_ID  = 0x203  // 3508/2006电机M3
DJI_M4_ID  = 0x204  // 3508/2006电机M4
DJI_M5_ID  = 0x205  // 3508/2006电机M5 或 GM6020电机
DJI_M6_ID  = 0x206  // 3508/2006电机M6 或 GM6020电机
DJI_M7_ID  = 0x207  // 3508/2006电机M7 或 GM6020电机
DJI_M8_ID  = 0x208  // 3508/2006电机M8 或 GM6020电机
DJI_M9_ID  = 0x209  // GM6020电机M9
DJI_M10_ID = 0x20A  // GM6020电机M10
DJI_M11_ID = 0x20B  // GM6020电机M11
```

#### 发送格式 (8字节) - 控制4个电机
```
StdId选择: 0x200(M1-M4), 0x1FF(M5-M8), 0x2FF(M9-M11)
[0-1]: 电机1电流(int16_t)   字节0(高), 字节1(低)
[2-3]: 电机2电流(int16_t)   字节2(高), 字节3(低)
[4-5]: 电机3电流(int16_t)   字节4(高), 字节5(低)
[6-7]: 电机4电流(int16_t)   字节6(高), 字节7(低)
```

#### 接收格式 (8字节) - 每个电机独占一条消息
```
[0-1]: 编码器值(uint16_t) ecd
[2-3]: 速度rpm(uint16_t)
[4-5]: 给定电流(uint16_t)
[6]:   温度(uint8_t)
[7]:   保留
```

#### 关键结构体
```c
typedef struct _DjiMotorMeasure {
    uint16_t ecd;              // 编码器值 (0-8191)
    int16_t speed_rpm;         // 转速 (RPM)
    int16_t given_current;     // 反馈电流
    uint8_t temperate;         // 温度
    int16_t last_ecd;          // 上一帧编码值
    uint32_t last_fdb_time;    // 最后反馈时间戳
} DjiMotorMeasure_t;
```

---

### 3.2 小米Cybergear协议 (扩展ID)

#### 通信方式
使用CAN扩展ID (IDE = CAN_ID_EXT)

#### 扩展ID格式 (32位)
```
bit[0-7]:     motor_id (电机ID)
bit[8-23]:    data (16位有效数据)
bit[24-28]:   mode (通信类型，5位)
bit[29-31]:   保留 (3位)
```

#### 通信类型 (mode)
```
类型0: 读取参数反馈
类型1: 运控模式 (控制力矩、位置、速度、Kp、Kd)
类型2: 故障状态反馈
类型3: 电机使能 (Enable)
类型4: 电机停止 (Stop)
类型5: 设置ID
类型6: 设置机械零点
```

#### 发送数据 (8字节) - 运控模式示例
```
EXT_ID.mode = 1 (运控模式)
EXT_ID.data = FloatToUint(torque, -12.0, 12.0, 16bits)
[0-1]: 机械位置 Position: FloatToUint(P, -12.5, 12.5, 16bits)
[2-3]: 速度 Velocity: FloatToUint(V, -30.0, 30.0, 16bits)
[4-5]: Kp: FloatToUint(Kp, 0.0, 500.0, 16bits)
[6-7]: Kd: FloatToUint(Kd, 0.0, 5.0, 16bits)
```

#### 接收数据 (8字节) - 反馈格式
```
[0-1]: 位置 pos: ((uint16_t)-32767.5) / 32767.5 * 4π  (单位: rad)
[2-3]: 速度 vel: ((uint16_t)-32767.5) / 32767.5 * 30   (单位: rad/s)
[4-5]: 转矩 tor: ((uint16_t)-32767.5) / 32767.5 * 12   (单位: N·m)
[6-7]: 温度 temp: (uint16_t) / 10.0                    (单位: 摄氏度)
```

#### 关键结构体
```c
typedef struct {
    uint32_t info : 24;
    uint32_t communication_type : 5;
    uint32_t res : 3;
} __packed__ RxCanInfo_s;

typedef struct {
    RxCanInfo_s ext_id;
    uint8_t rx_data[8];
    uint32_t last_fdb_time;
} CybergearMeasure_s;
```

---

### 3.3 达妙电机协议 (标准ID)

#### 电机ID定义
```
DM_M1_ID = 0x51  // 主控ID = 从控ID + 0x50
DM_M2_ID = 0x52
DM_M3_ID = 0x53
DM_M4_ID = 0x54
DM_M5_ID = 0x55
DM_M6_ID = 0x56
```

#### 发送模式编码
```
MIT模式:       StdId = motor_id + 0x000
位置控制:      StdId = motor_id + 0x100
速度控制:      StdId = motor_id + 0x200
位置速度控制:  StdId = motor_id + 0x300
```

#### MIT模式发送格式 (8字节)
```
[0-1]:   位置 pos:       float_to_uint(pos, -12.5, 12.5, 16bits)
[2-3]:   速度 vel:       float_to_uint(vel, -30.0, 30.0, 12bits) [高8位]
[3]:     速度低4位 + Kp高4位
[4]:     Kp:            float_to_uint(kp, 0.0, 500.0, 12bits)
[5-6]:   Kd:            float_to_uint(kd, 0.0, 5.0, 12bits)
[7]:     转矩 torque:   float_to_uint(torq, -10.0, 10.0, 12bits)
```

#### 接收格式 (8字节)
```
[0]: id(低4位) + state(高4位)
[1-2]: 位置 p_int (16bits) -> pos = uint_to_float(p_int, -12.5, 12.5, 16)
[3-4]: 速度 v_int (12bits) + 其他 -> vel = uint_to_float(v_int, -30.0, 30.0, 12)
[4-5]: 转矩 t_int (12bits) -> tor = uint_to_float(t_int, -10.0, 10.0, 12)
[6]: MOS温度 t_mos
[7]: 转子温度 t_rotor
```

#### 关键结构体
```c
typedef struct {
    int id, state, p_int, v_int, t_int;
    float pos, vel, tor;          // 解码后的浮点值
    float t_mos, t_rotor;         // 温度
    uint32_t last_fdb_time;
} DmMeasure_s;
```

---

### 3.4 瓴控电机协议 (标准ID)

#### 电机ID和基地址
```
STDID_OFFSET = 0x140
LK_M1_ID = 0x141  // StdId = motor_id + 0x140
LK_M2_ID = 0x142
LK_M3_ID = 0x143
LK_M4_ID = 0x144
```

#### 电机命令格式
```
禁用:      [0x80, 0x00, ...]
停止:      [0x81, 0x00, ...]
使能:      [0x88, 0x00, ...]
```

#### 反馈格式 (8字节)
```
[0]:     控制ID
[1]:     温度
[2-3]:   电流(Iq) (int16_t)
[4-5]:   速度 (int16_t)
[6-7]:   编码器值 (uint16_t)
```

#### 关键结构体
```c
typedef struct {
    int8_t ctrl_id, temprature;
    int16_t iq, speed;
    uint16_t encoder;
    uint32_t last_fdb_time;
} LkMeasure_s;
```

---

### 3.5 超级电容协议

#### 消息ID
```
StdId = 0x210  (发送)
StdId = 0x211  (接收)
```

#### 发送格式 (2字节)
```
[0-1]: 目标功率(int16_t) Power = (data[0] << 8) | data[1]
```

#### 接收格式 (8字节)
```
[0-1]: 输入电压 voltage_in (uint16_t)
[2-3]: 电容电压 voltage_cap (uint16_t)
[4-5]: 输入电流 current_in (uint16_t)
[6-7]: 目标功率 power_target (uint16_t)
```

#### 关键结构体
```c
typedef struct {
    int16_t voltage_in, voltage_cap, current_in, power_target;
    uint32_t last_fdb_time;
} SupCapMeasure_s;
```

---

### 3.6 板间通信协议 (Inter-Board Communication)

#### StdId构成 (11位)
```
bit[9-10]:   base_id (base_id = CAN_STD_ID_PACK_BASE = 0x0400)
bit[6-8]:    type_id:
             - 1: Test数据
             - 2: RC遥控器数据
             - 3: 云台数据
bit[3-5]:    target_id (目标板卡ID)
bit[0-2]:    index_id (数据索引)

完整公式: StdId = (data_type << 6) | (target_id << 3) | index
```

#### 数据类型定义
```c
#define CAN_STD_ID_PACK_BASE     ((uint16_t) 0x0400)  // 基础ID
#define CAN_STD_ID_Test          ((uint16_t) 1)
#define CAN_STD_ID_Rc            ((uint16_t) 2)       // RC遥控器
#define CAN_STD_ID_Gimbal        ((uint16_t) 3)       // 云台
#define CAN_STD_ID_ANY_BASE      ((uint16_t) 0x0601)  // 任意类型
```

#### RC遥控器数据 (index=0)
```c
typedef struct {
    uint16_t ch0 : 11;     // 通道0 (11bits)
    uint16_t ch1 : 11;     // 通道1
    uint16_t ch2 : 11;     // 通道2
    uint16_t ch3 : 11;     // 通道3
    uint16_t ch4 : 11;     // 通道4
    uint8_t s0 : 2;        // 拨杆0
    uint8_t s1 : 2;        // 拨杆1
    bool offline : 1;      // 离线标志
    uint8_t reserved : 4;  // 保留
} __packed__ rc_packed_t;  // 共8字节
```

#### 键鼠数据 (index=1)
```c
typedef struct {
    int16_t mouse_x : 15;
    uint16_t mouse_press_l : 1;
    int16_t mouse_y : 15;
    uint16_t mouse_press_r : 1;
    int16_t mouse_z;
    int16_t key;
} __packed__ km_packed_t;  // 共8字节
```

#### 云台数据
```c
typedef struct {
    float yaw;            // 4字节，云台偏转角
    bool offline;         // 离线标志
    bool init_judge;      // 初始化判断
    uint16_t reserved;    // 保留
} __packed__ gimbal_packed_t;  // 共8字节
```

---

## 四、数据打包和解包函数

### 4.1 浮点数转整数函数

**文件位置：** `components/algorithm/user_lib.h` (声明) 和 `user_lib.c` (实现)

```c
// 将浮点数转换为整数 (用于发送)
extern int float_to_uint(float x_float, float x_min, float x_max, int bits);

// 说明:
// x_float: 待转换的浮点数
// x_min:   浮点数的最小值
// x_max:   浮点数的最大值
// bits:    转换后整数的位数 (通常为8, 12, 16)
// 返回值:  转换后的整数
```

#### 应用例子
```c
// Cybergear数据打包
uint32_t FloatToUint(float x, float x_min, float x_max, int bits) {
    float span = x_max - x_min;
    float offset = x_min;
    if (x > x_max) x = x_max;
    else if (x < x_min) x = x_min;
    return (uint32_t)((x - offset) * ((float)((1 << bits) - 1)) / span);
}

// 达妙电机MIT模式数据打包
pos_tmp = float_to_uint(pos, DM_P_MIN=-12.5, DM_P_MAX=12.5, 16);
vel_tmp = float_to_uint(vel, DM_V_MIN=-30.0, DM_V_MAX=30.0, 12);
kp_tmp = float_to_uint(kp, DM_KP_MIN=0.0, DM_KP_MAX=500.0, 12);
```

### 4.2 整数转浮点数函数

```c
// 将整数转换为浮点数 (用于接收)
extern float uint_to_float(int x_int, float x_min, float x_max, int bits);

// 应用例子 - DM电机接收解码
dm_measure->pos = uint_to_float(dm_measure->p_int, DM_P_MIN, DM_P_MAX, 16);
dm_measure->vel = uint_to_float(dm_measure->v_int, DM_V_MIN, DM_V_MAX, 12);
dm_measure->tor = uint_to_float(dm_measure->t_int, DM_T_MIN, DM_T_MAX, 12);
```

---

## 五、CAN通信收发接口

### 5.1 发送接口

#### 底层发送函数 (bsp_can.c)
```c
void CAN_SendTxMessage(CanCtrlData_s * can_ctrl_data)
{
    // can_ctrl_data 包含:
    // - hcan:        CAN句柄 (&hcan1 或 &hcan2)
    // - tx_header:   CAN帧头 (StdId, ExtId, DLC等)
    // - tx_data[8]:  8字节数据

    uint32_t free_TxMailbox =
        HAL_CAN_GetTxMailboxesFreeLevel(can_ctrl_data->hcan);
    while (free_TxMailbox < 3) {
        // 等待至少3个空闲邮箱
    }
    HAL_CAN_AddTxMessage(...);  // 发送到CAN总线
}
```

#### 控制数据结构 (bsp_can.h)
```c
typedef struct __CanCtrlData {
    hcan_t * hcan;                    // CAN句柄
    CAN_TxHeaderTypeDef tx_header;    // CAN帧头
    uint8_t tx_data[8];               // CAN数据
} CanCtrlData_s;
```

### 5.2 接收接口

#### 中断回调函数 (CAN_receive.c)
```c
void HAL_CAN_RxFifo0MsgPendingCallback(hcan_t * hcan)
{
    CAN_RxHeaderTypeDef rx_header;
    uint8_t rx_data[8];

    // 从FIFO获取消息
    HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, rx_data);

    // 根据ID类型解码
    if (rx_header.IDE == CAN_ID_STD) {        // 标准ID
        DecodeStdIdData(hcan, &rx_header, rx_data);
    } else if (rx_header.IDE == CAN_ID_EXT) { // 扩展ID
        DecodeExtIdData(hcan, &rx_header, rx_data);
    }
}
```

#### 标准ID解码处理 (CAN_receive.c)
```c
static void DecodeStdIdData(hcan_t * CAN, CAN_RxHeaderTypeDef * rx_header,
                             uint8_t rx_data[8]) {
    switch (rx_header->StdId) {
        case DJI_M1_ID ... DJI_M11_ID:  // DJI电机
            DjiFdbData(&CAN1_DJI_MEASURE[i], rx_data);
            break;

        case DM_M1_ID ... DM_M6_ID:     // DM电机
            DmFdbData(&CAN1_DM_MEASURE[i], rx_data);
            break;

        case LK_M1_ID ... LK_M4_ID:     // LK电机
            LkFdbData(&CAN1_LK_MEASURE[i], rx_data);
            break;

        case 0x211:                      // 超级电容
            SupCapFdbData(&SUP_CAP_MEASURE, rx_data);
            break;
    }
}
```

#### 扩展ID解码处理 (CAN_receive.c)
```c
static void DecodeExtIdData(hcan_t * CAN, CAN_RxHeaderTypeDef * rx_header,
                             uint8_t rx_data[8]) {
    // Cybergear电机使用扩展ID
    uint8_t motor_id = ((RxCanInfoType_2_s *)(&rx_header->ExtId))->motor_id;

    memcpy(&CAN1_CYBERGEAR_MEASURE[motor_id].ext_id, &rx_header->ExtId, 4);
    memcpy(CAN1_CYBERGEAR_MEASURE[motor_id].rx_data, rx_data, 8);
}
```

---

## 六、具体实现示例

### 6.1 DJI电机控制示例

```c
// 发送电流控制命令给4个电机 (M1-M4 使用StdId=0x200)
void CanCmdDjiMotor(uint8_t can, uint16_t std_id,
                    int16_t curr_1, int16_t curr_2,
                    int16_t curr_3, int16_t curr_4)
{
    CanCtrlData_s CAN_CTRL_DATA = {...};
    CAN_CTRL_DATA.hcan = (can == 1) ? &hcan1 : &hcan2;
    CAN_CTRL_DATA.tx_header.StdId = std_id;
    CAN_CTRL_DATA.tx_header.IDE = CAN_ID_STD;
    CAN_CTRL_DATA.tx_header.RTR = CAN_RTR_DATA;
    CAN_CTRL_DATA.tx_header.DLC = 8;

    // 数据打包
    CAN_CTRL_DATA.tx_data[0] = (curr_1 >> 8);    // M1高字节
    CAN_CTRL_DATA.tx_data[1] = curr_1;           // M1低字节
    CAN_CTRL_DATA.tx_data[2] = (curr_2 >> 8);    // M2高字节
    CAN_CTRL_DATA.tx_data[3] = curr_2;           // M2低字节
    CAN_CTRL_DATA.tx_data[4] = (curr_3 >> 8);    // M3高字节
    CAN_CTRL_DATA.tx_data[5] = curr_3;           // M3低字节
    CAN_CTRL_DATA.tx_data[6] = (curr_4 >> 8);    // M4高字节
    CAN_CTRL_DATA.tx_data[7] = curr_4;           // M4低字节

    CAN_SendTxMessage(&CAN_CTRL_DATA);
}

// 接收和解码
void DjiFdbData(DjiMotorMeasure_t * dji_measure, uint8_t * rx_data)
{
    dji_measure->last_ecd = dji_measure->ecd;
    dji_measure->ecd = (uint16_t)((rx_data[0] << 8) | rx_data[1]);
    dji_measure->speed_rpm = (uint16_t)((rx_data[2] << 8) | rx_data[3]);
    dji_measure->given_current = (uint16_t)((rx_data[4] << 8) | rx_data[5]);
    dji_measure->temperate = rx_data[6];
    dji_measure->last_fdb_time = HAL_GetTick();
}
```

### 6.2 Cybergear电机控制示例

```c
// 运控模式发送 (Mode=1)
void CybergearControl(Motor_s * p_motor, float torque,
                      float position, float velocity, float kp, float kd)
{
    // 构造扩展ID
    CybergearSendData[p_motor->id].EXT_ID.motor_id = p_motor->id;
    CybergearSendData[p_motor->id].EXT_ID.mode = 1;  // 运控模式
    CybergearSendData[p_motor->id].EXT_ID.data =
        FloatToUint(torque, -12.0, 12.0, 16);

    // 打包8字节数据
    CybergearSendData[p_motor->id].txdata[0] =
        FloatToUint(position, -12.5, 12.5, 16) >> 8;
    CybergearSendData[p_motor->id].txdata[1] =
        FloatToUint(position, -12.5, 12.5, 16);
    CybergearSendData[p_motor->id].txdata[2] =
        FloatToUint(velocity, -30.0, 30.0, 16) >> 8;
    CybergearSendData[p_motor->id].txdata[3] =
        FloatToUint(velocity, -30.0, 30.0, 16);
    CybergearSendData[p_motor->id].txdata[4] =
        FloatToUint(kp, 0.0, 500.0, 16) >> 8;
    CybergearSendData[p_motor->id].txdata[5] =
        FloatToUint(kp, 0.0, 500.0, 16);
    CybergearSendData[p_motor->id].txdata[6] =
        FloatToUint(kd, 0.0, 5.0, 16) >> 8;
    CybergearSendData[p_motor->id].txdata[7] =
        FloatToUint(kd, 0.0, 5.0, 16);

    CybergearCanTx(p_motor->id);
}

// 接收和解码
static void CybergearRxDecode(Motor_s * p_motor, uint8_t rx_data[8])
{
    uint16_t decode_temp;

    // 解析位置 (Range: -4π to 4π)
    decode_temp = (rx_data[0] << 8 | rx_data[1]);
    p_motor->fdb.pos = ((float)decode_temp - 32767.5f) / 32767.5f * 4 * 3.1415926f;

    // 解析速度 (Range: -30 to 30 rad/s)
    decode_temp = (rx_data[2] << 8 | rx_data[3]);
    p_motor->fdb.vel = ((float)decode_temp - 32767.5f) / 32767.5f * 30.0f;

    // 解析转矩 (Range: -12 to 12 N·m)
    decode_temp = (rx_data[4] << 8 | rx_data[5]);
    p_motor->fdb.tor = ((float)decode_temp - 32767.5f) / 32767.5f * 12.0f;

    // 解析温度
    decode_temp = (rx_data[6] << 8 | rx_data[7]);
    p_motor->fdb.temp = (float)decode_temp / 10.0f;
}
```

### 6.3 板间通信示例

```c
// 发送RC遥控数据到其他板卡
void CanSendRcDataToBoard(uint8_t can, uint16_t target_id, uint16_t index)
{
    // 构造StdId: base(0x0400) + type(2<<6) + target + index
    uint16_t std_id = CAN_STD_ID_PACK_BASE |
                      (CAN_STD_ID_Rc << TYPE_ID_OFFSET) |
                      (target_id << TARGET_ID_OFFSET) |
                      index;

    const RC_ctrl_t * rc_ctrl = get_remote_control_point();

    if (index == 0) {  // RC模拟量数据
        SEND_CBC.rc_data.rc.packed.ch0 = rc_ctrl->rc.ch[0] + RC_CH_VALUE_OFFSET;
        SEND_CBC.rc_data.rc.packed.ch1 = rc_ctrl->rc.ch[1] + RC_CH_VALUE_OFFSET;
        SEND_CBC.rc_data.rc.packed.ch2 = rc_ctrl->rc.ch[2] + RC_CH_VALUE_OFFSET;
        SEND_CBC.rc_data.rc.packed.ch3 = rc_ctrl->rc.ch[3] + RC_CH_VALUE_OFFSET;
        SEND_CBC.rc_data.rc.packed.ch4 = rc_ctrl->rc.ch[4] + RC_CH_VALUE_OFFSET;
        SEND_CBC.rc_data.rc.packed.s0 = rc_ctrl->rc.s[0];
        SEND_CBC.rc_data.rc.packed.s1 = rc_ctrl->rc.s[1];
        SEND_CBC.rc_data.rc.packed.offline = GetRcOffline();

        SendData(can, std_id, SEND_CBC.rc_data.rc.raw.data);
    }
}

// 接收RC数据
case CAN_STD_ID_Rc: {
    switch (index_id) {
        case 0: {  // RC数据
            memcpy(&RECEIVE_CBC.rc_data.rc.raw.data, rx_data, 8);
            RECEIVE_CBC.rc_data.rc_unpacked.rc.ch[0] =
                RECEIVE_CBC.rc_data.rc.packed.ch0 - RC_CH_VALUE_OFFSET;
            // ... 其他通道解码
        } break;
        case 1: {  // 键鼠数据
            memcpy(&RECEIVE_CBC.rc_data.km.raw.data, rx_data, 8);
            RECEIVE_CBC.rc_data.rc_unpacked.mouse.x =
                RECEIVE_CBC.rc_data.km.packed.mouse_x << 1;
            // ...
        } break;
    }
}
```

---

## 七、硬件初始化流程

**文件位置：** `Src/can.c`

```c
// CAN1初始化 (STM32F4xx)
void MX_CAN1_Init(void) {
    hcan1.Instance = CAN1;
    hcan1.Init.Prescaler = 3;              // 波特率分频
    hcan1.Init.Mode = CAN_MODE_NORMAL;
    hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
    hcan1.Init.TimeSeg1 = CAN_BS1_10TQ;    // 时间位段1
    hcan1.Init.TimeSeg2 = CAN_BS2_3TQ;     // 时间位段2
    hcan1.Init.TimeTriggeredMode = DISABLE;
    hcan1.Init.AutoBusOff = DISABLE;
    hcan1.Init.AutoWakeUp = DISABLE;
    hcan1.Init.AutoRetransmission = DISABLE;
    hcan1.Init.ReceiveFifoLocked = DISABLE;
    hcan1.Init.TransmitFifoPriority = DISABLE;
    HAL_CAN_Init(&hcan1);
}

// CAN滤波器初始化
void can_filter_init(void) {
    CAN_FilterTypeDef can_filter_st;
    can_filter_st.FilterActivation = ENABLE;
    can_filter_st.FilterMode = CAN_FILTERMODE_IDMASK;  // ID/掩码模式
    can_filter_st.FilterScale = CAN_FILTERSCALE_32BIT;
    can_filter_st.FilterIdHigh = 0x0000;
    can_filter_st.FilterIdLow = 0x0000;
    can_filter_st.FilterMaskIdHigh = 0x0000;
    can_filter_st.FilterMaskIdLow = 0x0000;
    can_filter_st.FilterBank = 0;                      // CAN1使用bank 0-13
    can_filter_st.FilterFIFOAssignment = CAN_RX_FIFO0;

    HAL_CAN_ConfigFilter(&hcan1, &can_filter_st);
    HAL_CAN_Start(&hcan1);
    HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);

    can_filter_st.SlaveStartFilterBank = 14;           // CAN2使用bank 14-27
    can_filter_st.FilterBank = 14;
    HAL_CAN_ConfigFilter(&hcan2, &can_filter_st);
    HAL_CAN_Start(&hcan2);
    HAL_CAN_ActivateNotification(&hcan2, CAN_IT_RX_FIFO0_MSG_PENDING);
}
```

---

## 八、关键文件路径汇总

### 绝对路径列表

1. `Src/can.c` - CAN硬件初始化
2. `Inc/can.h` - CAN接口头文件
3. `bsp/boards/bsp_can.c` - CAN底层发送
4. `bsp/boards/bsp_can.h` - CAN控制结构
5. `application/robot_cmd/CAN_receive.h` - CAN接收头文件
6. `application/robot_cmd/CAN_receive.c` - CAN接收实现
7. `application/robot_cmd/CAN_communication.h` - 板间通信头文件
8. `application/robot_cmd/CAN_communication.c` - 板间通信实现
9. `application/robot_cmd/CAN_cmd_dji.h` - DJI电机协议头文件
10. `application/robot_cmd/CAN_cmd_dji.c` - DJI电机协议实现
11. `application/robot_cmd/CAN_cmd_cybergear.h` - Cybergear协议头文件
12. `application/robot_cmd/CAN_cmd_cybergear.c` - Cybergear协议实现
13. `application/robot_cmd/CAN_cmd_damiao.h` - 达妙电机协议头文件
14. `application/robot_cmd/CAN_cmd_damiao.c` - 达妙电机协议实现
15. `application/robot_cmd/CAN_cmd_lingkong.h` - 瓴控电机协议头文件
16. `application/robot_cmd/CAN_cmd_lingkong.c` - 瓴控电机协议实现
17. `application/robot_cmd/CAN_cmd_SupCap.h` - 超级电容协议头文件
18. `application/robot_cmd/CAN_cmd_SupCap.c` - 超级电容协议实现
19. `application/robot_cmd/motor.h` - 电机结构体定义
20. `application/robot_cmd/SupCap.h` - 超级电容结构体
21. `application/typedef/can_typedef.h` - CAN类型定义
22. `application/referee/protocol.h` - 裁判系统协议
23. `components/algorithm/user_lib.h` - 数据转换工具

---

## 九、系统架构总结

### 分层架构

```
应用层 (Application Layer)
├── 机器人任务控制
└── 业务逻辑处理

通信协议层 (Protocol Layer)
├── DJI电机协议
├── Cybergear电机协议
├── 达妙电机协议
├── 瓴控电机协议
├── 超级电容协议
└── 板间通信协议

驱动层 (Driver Layer)
├── CAN接收处理 (中断回调)
├── CAN发送接口
└── 数据编解码

硬件抽象层 (HAL/BSP)
├── CAN外设初始化
├── CAN滤波器配置
└── GPIO/中断配置
```

### 协议特点对比

| 协议类型 | ID类型 | 控制模式 | 数据编码 | 应用场景 |
|---------|--------|---------|---------|---------|
| DJI | 标准ID | 电流控制 | 直接int16 | 通用电机控制 |
| Cybergear | 扩展ID | 多模式 | 浮点压缩 | 高精度位置控制 |
| 达妙DM | 标准ID | MIT模式 | 浮点压缩 | 力控/阻抗控制 |
| 瓴控LK | 标准ID | 转矩/电流 | 直接int16 | 力矩控制 |
| 超级电容 | 标准ID | 功率控制 | 直接int16 | 电源管理 |
| 板间通信 | 标准ID | 数据传输 | 位域压缩 | 多板协同 |

### 关键技术要点

1. **浮点数压缩编码**：通过`float_to_uint`和`uint_to_float`实现高效数据传输
2. **ID复用设计**：DJI协议一条消息控制4个电机，提高总线利用率
3. **扩展ID应用**：Cybergear利用扩展ID实现协议扩展
4. **板间通信**：通过ID位段编码实现多板数据透传
5. **中断驱动接收**：FIFO中断确保数据实时性

---

## 十、开发建议

### 新增电机协议步骤

1. 在`application/robot_cmd/`目录创建`CAN_cmd_xxx.h/c`文件
2. 定义电机ID、控制模式和数据结构
3. 实现数据打包函数（发送）
4. 实现数据解包函数（接收）
5. 在`CAN_receive.c`的`DecodeStdIdData`或`DecodeExtIdData`中添加解码分支
6. 在对应的任务中调用控制接口

### 调试建议

1. 使用CAN总线分析仪监控报文
2. 检查ID配置是否冲突
3. 验证数据编解码的正确性
4. 注意大小端字节序
5. 监控总线负载率

### 注意事项

1. **ID冲突**：确保所有设备的CAN ID唯一
2. **波特率匹配**：所有设备波特率必须一致（通常1Mbps）
3. **滤波器配置**：合理配置滤波器减少CPU负担
4. **数据范围检查**：发送前检查数据是否在有效范围内
5. **超时检测**：通过`last_fdb_time`检测设备离线

---

**文档生成时间：** 2025-12-22
**项目名称：** StandardRobotpp
**作者：** Claude Code Analysis
