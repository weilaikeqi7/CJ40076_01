# CJ40076 V2.0 固件（N32L403KBQ7）

多功能观测仪（V2.0 硬件）：激光测距（DYC-15A）+ 姿态罗盘（JY901B 垂直安装）+ GNSS（BV-220）+ CS1622 驱动的 COM×SEG 段码屏。**业务与 CJ40076_03（V3/N32G4FR）完全一致**，差异仅在硬件驱动层与显示屏能力。

## 构建

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -S .
cmake --build build
```

产物：`build/CJ40076.hex / .bin / .elf`。日志：SEGGER RTT 通道 0（含 CmBacktrace 死机转储）。

## 目录结构

```
src/
├── main.c                  入口（Board_Init 电源保持 -> 应用任务）
├── bsp/                    板级：board(电源/按键) bsp_gpio bsp_adc bsp_uart(DMA四环口)
├── drivers/                cs1622(LCD驱动芯片) lcd_segments(段码映射,27位+33符号)
├── modules/                协议驱动：bv220(GNGGA) jy901b(0x55帧+配置校准) rangefinder(DYC-15A)
└── app/                    业务层（与 03 相同架构）
    ├── app.c               主状态机（启动自检/供电调度/关机/欠压）
    ├── app_key.c           按键（40ms 消抖/多击/长按/双键/连发）
    ├── app_measure.c       测距轮次（200ms 聚合、3s 超时、8s/12s 周期）
    ├── app_attitude.c      姿态换算（0.01° 整数）
    ├── app_coord.c         目标坐标/高程解算
    ├── app_display.c       LCD 渲染（本玻璃无小数段，整数显示）
    ├── app_calib.c         补偿设置页 + JY901B 内部校准
    ├── app_store.c         Flash 参数区（末页 0x0801F800）
    ├── app_log.c/app_debug.c  SEGGER RTT 日志 + CmBacktrace
    └── app_runtime_stats.c FreeRTOS 运行统计时基（TIM5）
```

## 引脚分配（CJ40076-V2.0，QFN32）

| 功能 | 引脚 | 说明 |
|---|---|---|
| 电池 ADC | PA0 (ADC_CH_1) | 分压 ×3/2 |
| 模式键 / 电源键 | PA6 / PA9 | 低有效 |
| 电源保持 | PA10 | 高有效 |
| 测距机 | UART4 PB0/PB1 + PA7 电源 | 115200，常供电 |
| JY901B | USART2 PA2/PA3 + PA8 电源 | 9600，按需供电 |
| GNSS | USART1 PA4/PA5 + PA12 电源 | 115200，按需供电 |
| 调试串口 | UART5 PB4/PB5 | 115200（预留） |
| LCD | PA15=CS PB3=RD PB6=WR PB7=DATA PA11=IRQ | CS1622，常供电 |

## 与 03（V3）的业务差异（仅硬件引起）

1. **无显示屏电源、无加热丝、无 NTC 温度**——供电矩阵只有：总电源保持（常）、测距机（常）、IMU/GNSS（按需）
2. **显示格式**：本玻璃小数点段仅在坐标行 → 距离/航向/俯仰/高程按**整数**显示；坐标 DDD°MM′SS.ss″ 与 03 相同；补偿设置页补偿值以 **0.1° 整数**显示在高程区（如 123 = 12.3°）
3. **计数 5 位数码管（21~25）**，上限沿用 9999（如需 99999 改 `APP_COUNT_MAX` 与 store 即可）
4. **电池 4 段**：框 + 4 格条，格条数 = 档位（≥3800=4、≥3700=3、≥3600=2、以下=1，<2600mV 欠压关机保存计数）
5. 俯仰中行仅 2 位数码管（±99° 内），带专用负号/正号段

## 业务规则（与 03 一致，详见 03 项目 README）

按键（电源短按测量/3s 关机保存计数；模式单击循环、三击清零立即写 Flash、四击 HEr、五/六击磁场校准、七击 PIt、八击加计、九击角度参考、双键 1s 切页保存）、四模式（单次/连续 8s/多功能/测试 12s）、200ms 静默聚合、单目标只出 F、姿态换算（俯仰=−原始+PIt、航向=−原始+HIt+HEr、默认 HIt=+90°）、目标坐标（GNGGA fix>0 有效）、Flash（末页 magic+CRC，三击/双键/关机/欠压时写）。
