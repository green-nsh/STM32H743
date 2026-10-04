# STM32H743 串口命令终端

使用 Keil 打开 `mcu/Projects/MDK-ARM/STM32H743.uvprojx`，编译并烧录。
本工程实现裸机串口 Shell，保留 PB0 呼吸灯；支持下列内置命令，可继续添加业务命令。

## 连接

使用 **3.3V TTL USB 转串口**：适配器 TX 接 PA10（USART1 RX），RX 接 PA9（USART1 TX），GND 共地。不要接 RS-232 电平。已有板载 USB 串口时，请先核对原理图是否接到 PA9/PA10。

终端设置：**115200、8 数据位、无校验、1 停止位、无流控**，关闭本地回显。
推荐使用支持 ANSI 和逐字符发送的终端。连接后按回车，看到 `stm32> ` 即可输入命令。
支持 CR、LF、CRLF 三种回车格式；普通串口助手也可以整行发送命令。

```text
stm32> help
stm32> info
stm32> uptime
stm32> led on
OK
stm32> led off
OK
stm32> led breathe
OK
stm32> echo hello stm32
hello stm32
stm32> clear
```

## 编辑与边界

- Backspace/Delete：删除最后一个字符。
- 上箭头：从新到旧浏览最近 16 条命令；下箭头：向较新命令切换，最后恢复输入草稿。连续重复命令只保存一条，空白输入不保存；复位后历史清空。
- Tab：补全命令名；有多个候选时显示列表。
- Ctrl+C / Ctrl+U：取消当前输入；Ctrl+L：清屏并保留输入。
- 单行最多 127 个 ASCII 字符，最多 8 个参数（含命令名）。参数以空白分隔，不支持引号、管道或 Linux 文件系统命令。
- 超长输入、接收溢出或串口错误会取消命令；按回车恢复，避免执行残缺指令。
- clear、方向键、Tab 等交互功能需要相应终端支持；左右箭头和光标中间编辑暂未实现。
- uptime 使用 HAL 的 32 位毫秒计数，约 49.7 天回绕。

## 扩展命令

在 `mcu/User/main.c` 添加 `void handler(int argc, char **argv)`，再向 `commands[]` 注册名称、帮助和函数指针。命令处理在主循环执行，耗时操作应改成分步任务，避免阻塞串口和呼吸灯。

`shell.c` 负责输入编辑、解析和分发；USART1 中断将字符放入 512 字节环形队列（有效容量 511），`shell_poll()` 每次最多处理 64 个字符。原驱动的 `g_usart_rx_buf/g_usart_rx_sta` 保留符号兼容，但不再累积 CRLF 数据；读取改用 `usart_read_char()`。

## 内部温度命令

输入 `temp`，输出整数摄氏度和估算的 VREF+ 电压，例如：

```text
stm32> temp
MCU temperature: 42 C | VREF+: 3301 mV
```

ADC3 在第一次执行命令时初始化并执行偏移/线性校准，使用内部 VREFINT 和温度通道，每个通道取 16 次采样平均。库函数根据芯片修订版自动选择 110℃或130℃校准点，读数是芯片内部温度，并非环境温度。采集失败时输出错误，不输出虚假温度。

模块占用 ADC3，使用当前系统已配置的 PLL2 P 输出，ADC 异步 16 分频；当前时钟下约 13.75 MHz。采样时间为 810.5 个周期。后续增加其他 ADC 功能或修改 PLL2 时钟时，需要统一协调配置。`temp` 是短时阻塞采集，首读包含校准，期间呼吸灯硬件 PWM 继续输出，但亮度更新可能短暂停顿。

Keil 工程已加入 `mcu_temp.c`、`stm32h7xx_hal_adc.c`、`stm32h7xx_hal_adc_ex.c`。请自行 Rebuild、烧录，在串口输入 `help` 和 `temp` 验证。当前修改未执行完整固件编译或板上测试。
