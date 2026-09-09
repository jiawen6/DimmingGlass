DimmingGlass - 智能调光玻璃控制系统

基于STM32H723 + FreeRTOS的玻璃透光率控制方案，支持触摸交互与串口命令，适用于智能隔断、车窗调光等场景。

主要特性：
- 无级调光：通过PWM（TIM1）精确控制透光率，占空比0~100%可调
- 多种控制方式：触控膜手势 + USART串口指令
- 实时系统：FreeRTOS多任务调度，响应延迟小于10ms
- 掉电记忆：亮度参数可保存至Flash/EEPROM
- 故障保护：驱动异常时自动关闭输出

硬件平台：
主控：STM32H723ZGT6（Cortex-M7，550MHz）
调光驱动：高压PWM驱动模块
触摸接口：I2C电容触摸屏/按键
调试接口：SWD

软件结构：
分层设计：App（应用任务）-> BSP（板级封装）-> Drivers（HAL驱动）-> Middlewares（FreeRTOS）。
使用STM32CubeMX初始化外设（DimmingGlass.ioc），使用Keil MDK-ARM编译（工程位于MDK-ARM/）。

串口指令（波特率115200，8N1）：
