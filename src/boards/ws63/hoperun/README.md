# Hoperun WS63 板级适配

构建选择 `-DWS63_BOARD=hoperun`（默认）。固件与 Windows 模拟器均由 `src/platforms/goldieos/cmake/board.cmake` 选择此目录，不要在同一构建目录切换板型。

- `board.cmake`：板级源文件列表；运行依赖由平台的固定归档提供，不再推导外部 BSP 路径。
- `board.c`：AW9523 电源保持引脚、键盘初始化、麦克风与参考通道约定、显示名称。
- `display.c`：屏幕 RGB565 字节序、2040 像素分块和 BSP 显示调用。
- 接口：`src/boards/ws63/include/ws_board.h`，不暴露引脚或具体厂商名。

新增同类 WS63 板：在 src/boards/ws63/<vendor_board>/ 实现相同接口及 board.cmake，使用独立 build 目录传入 WS63_BOARD。通常只需调整此层的 BSP 路径、硬件初始化、显示及音频布局。网络、TLS、SDK、NV、业务和共用界面继续复用。

当前 src/platforms/goldieos 的运行环境仍为 GoldieOS OSAL、AudioService、Wlan_Drv、WifiService 和应用管理 ABI。兼容厂商 BSP 需要提供相同服务 ABI，服务获取由 platforms 负责；若连 RTOS/网络栈接口也不同，需要新增平台后端，不能仅改板名。Windows 目标还要求 BSP 提供兼容的模拟器服务。厂商底层驱动位于工程内固定运行归档，不复制到本板级目录。

基础驱动等待位于平台 runtime；音频与网络服务由各平台组件获取。板级接口不再传递服务句柄。
