# CJ40076 V2.0 固件（N32L403KBQ7）

多功能观测仪（V2.0 硬件）：激光测距（DYC-15A）+ 姿态罗盘（JY901B 垂直安装）+ GNSS（BV-220）+ CS1622 驱动的 COM×SEG 段码屏。**业务与 CJ40076_03（V3/N32G4FR）一致**，目录结构和驱动 API 已向 03 看齐；差异仅保留在板级引脚、芯片外设 API 和 LCD 渲染能力。

## 构建

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -S .
cmake --build build
```

产物：`build/CJ40076.hex / .bin / .elf`。日志：最小 SEGGER RTT 控制块，J-Link RTT Viewer 通道 0。

## 目录结构

```
src/
├── main.c                  入口（board_gpio_init 电源保持 -> app_run 任务）
├── board/                  板级：board(电源/按键/LCD GPIO) board_adc board_uart(RXDNE中断环形缓冲)
├── device/                 外设驱动：cs1622/lcd_segments、gnss、jy901b、ranger
├── app/                    业务层：app/key/measure/attitude/coord/display/calib/store/runtime_stats
└── common/                 rtt_log（与 03 相同最小 RTT 实现）
inc/                        头文件，子目录与 src 对应
vendor/                     N32L40x 标准库 2.2.0 + FreeRTOS-Kernel
```

分层依赖：`app -> device -> board -> vendor`。其中 `gnss.c`、`jy901b.c`、`ranger.c`、`app_attitude.c`、`app_coord.c`、`app_measure.c`、`rtt_log.c` 与 03 同逻辑/同 API。

## 引脚分配（CJ40076-V2.0，QFN32）

| 功能 | 引脚 | 说明 |
|---|---|---|
| 电池 ADC | PA0 (ADC_CH_1) | 分压 ×3/2 |
| 模式键 / 电源键 | PA6 / PA9 | 低有效 |
| 电源保持 | PA10 | 高有效 |
| 测距机 | UART4 PB0/PB1 + PA7 电源 | 115200，常供电 |
| JY901B | USART2 PA2/PA3 + PA8 电源 | 9600，按需供电 |
| GNSS | USART1 PA4/PA5 + PA12 电源 | 115200，按需供电 |
| 调试串口 | UART5 PB4/PB5 | 115200（预留，环形缓冲同一套驱动） |
| LCD | PA15=CS PB3=RD PB6=WR PB7=DATA PA11=IRQ | CS1622，常供电 |

## 与 03 的硬件差异

1. **无显示屏电源、无加热丝、无 NTC 温度**——供电矩阵只有：总电源保持（常）、LCD（常）、测距机（常）、IMU/GNSS（按需）
2. **LCD 不同**：01 用 CS1622 COM×SEG 玻璃；03 用四线移位玻璃。01 的距离/航向/俯仰/高程按玻璃能力整数显示；坐标仍为 DDD°MM′SS.ss″
3. **计数 5 位数码管（21~25）**，业务上限仍为 9999
4. **电池 4 格**：外框 T15 + T11/T12/T13/T14，T11 为最左格，格条数=档位（≥3800=4、≥3700=3、≥3600=2、以下=1，<2600mV 欠压保存计数后关机）

## 业务规则（与 03 一致）

- **按键**：电源短按测量/停止，长按 3s 关机保存计数；模式单击循环 单次→连续→多功能→测试；三击清零并立即写 Flash；四击 HEr；五/六击磁场校准开始/结束；七击 PIt（双键进 HIt，再双键保存退出）；八击加速度校准；九击角度参考；**十击 JY901B 恢复出厂并重新写入垂直安装/5Hz角度帧配置**
- **测距**：每轮先设多目标再发单次，末帧静默 200ms 聚合发布；单目标只显示 F；连续 8s 周期，测试 12s 周期；3s 超时发布横杠并计数 +1
- **姿态**：俯仰=−原始+PIt，航向=−原始+HIt+HEr，默认 PIt=0 / HIt=+90° / HEr=0
- **存储**：Flash 最后一页 0x0801F800，magic+CRC；三击清零/补偿保存/正常关机/欠压关机时写入
