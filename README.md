DimmingGlass - 智能调光玻璃控制系统

## 一、工程主要框架

该工程是一个基于 STM32H723ZGT6 的 PDLC 调光玻璃控制系统，使用 FreeRTOS 管理多任务，通过触控膜输入控制 105 个调光区域（7行×15列）。系统包含以下核心模块：

1. **硬件抽象层（BSP）**  
   - `bsp_uart3.c/h`：USART3 + DMA + IDLE 接收触控数据。  
   - `bsp_tim1.c/h`：TIM1 硬件定时器，产生 PWM 中断。  
   - `bsp_gpio.c/h`：初始化 22 路 PWM 输出引脚及按键。

2. **通用工具（Utils）**  
   - `ring_buffer.c/h`：环形缓冲区，用于串口数据缓存。

3. **应用层（App）**  
   - `touch_parser.c/h`：触控帧解析器，自动识别 A/B 厂协议。  
   - `app_dimmer.c/h`：PWM 生成与相位管理（包含 `app_dimmer_tick_isr()`）。  
   - `app_touch.c/h`：触控解析任务，调用解析器并处理触点数据。  
   - `freertos.c`：FreeRTOS 任务创建与配置。


## 二、函数调用关系

### 1. 硬件定时器中断 → PWM 输出
```
TIM1_UP_TIM10_IRQHandler()
    → HAL_TIM_IRQHandler(&htim1)
        → HAL_TIM_PeriodElapsedCallback(&htim1)
            → app_dimmer_tick_isr()
```
`app_dimmer_tick_isr()` 内部根据当前 tick 查表，使用 `HAL_GPIO_WritePin()` 输出 22 路 GPIO 电平。

### 2. 串口接收 → 环形缓冲区
```
HAL_UARTEx_RxEventCallback()
    → 将 DMA 缓冲区数据写入 ring_buffer
    → 重新启动 HAL_UARTEx_ReceiveToIdle_DMA()
```

### 3. 触控解析任务
```
vTouchParseTask()
    → 从 ring_buffer 逐字节读取
    → touch_parser_feed(byte)
        → 状态机识别帧头，接收完整帧，校验
        → 调用 touch_parser_on_frame_A/B()
            → 解析触点坐标，存储到全局数组
            → 可触发 PWM 相位更新
```

### 4. PWM 相位更新
```
app_dimmer_set_phase(channel, phase)   // 应用层调用
    → 更新 phase 数组
    → 后台重新生成 PWM 表
    → 双缓冲切换
```

## 三、硬件外设使用

| 外设 | 功能 | 配置要点 |
|------|------|----------|
| **TIM1** | 产生软件 PWM 中断 | APB2 总线，更新中断，优先级高于 FreeRTOS 管理优先级 |
| **USART3** | 触控数据接收 | DMA + IDLE 中断，Normal 模式，波特率根据触控膜设定 |
| **DMA1/2** | USART3_RX 数据搬运 | Normal 模式，内存递增，外设固定 |
| **GPIOA~E** | 22 路 PWM 输出 + 按键输入 | 输出速度 Very High，无上下拉；按键上拉输入 |
| **SysTick** | FreeRTOS 系统节拍 | 标准配置，通常 1 kHz |
| **TIM7** | HAL 时基 | 避免与 FreeRTOS 冲突 |


## 四、关键设计特点

- **双缓冲PWM表**：后台生成新相位表，前台继续输出，避免波形抖动。  
- **状态机解析**：兼容 A/B 厂帧格式，帧头不匹配时自动恢复。  
- **环形缓冲区**：解耦接收与解析，防止数据丢失。  
- **中断优先级**：TIM1 中断优先级高于configMAX_SYSCALL_INTERRUPT_PRIORITY，保证 PWM 实时性。
