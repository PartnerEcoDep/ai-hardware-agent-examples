# src 源码归拢与平台目录迁移（2026-10-15）

以本轮操作前工作区为基线，不把此前未提交改动计入本轮。未提交或推送代码。

## 调整范围

- shared/application → src/application/core；shared/services、shared/ui → src/application/services、src/application/ui。
- ports → src/platforms；两平台 components/sdk → components/convai；保留 entry 与其他组件职责。
- boards → src/boards，内部板型分组不变。
- 根 CMakeLists.txt 与 third_party、config、tests、tools、skills、docs 保留；build-* 目录与产物名称不变。
- 构建使用 UNIFIED_APP_ROOT、UNIFIED_SOURCE_ROOT，保留 ai_core、esp_sdk、main 等组件标识及现有函数名。技能脚本、说明、测试网页路径与架构检查同步。
- 新入口重配置时使用已有工具链；未更改鉴权、关闭功能或修改业务实现。

## 验证

四目标构建与 ESP 合并镜像生成通过，12 项主机测试通过。117 个运行源码/头文件/网页除 wifi_store.c 一处注释路径更新外，与操作前快照逐字节一致，上库配置模板不变。共迁移 135 个文件，详细变更如下。

| 产物 | 字节数 | SHA-256 |
|---|---:|---|
| `build-lrd/unified_ai_lrd_merged.bin` | 1654048 | `4fe77fd6b2e9cc0a9cca5d24122de10ae8a52dc531cd653c651803ab91204244` |
| `build-szpi/unified_ai_szpi_merged.bin` | 1660704 | `45c9179a7adb3f430183ffb9ac7a0b8d7319b74eb8892facf37c1abc7f5e6a48` |
| `build-hoperun-app/out/unified_ai_hoperun.fwpkg` | 2235288 | `c294492fdc6e5ad569f3d398b24fcfe96910804da00e937b5c5976374347a7a2` |
| `build-hoperun-win/unified_ai_simulator.exe` | 3655658 | `ab5edea88183a5a78456afd3c2a86934cfe14780f91577ca78d05b9cf8c70875` |

未烧写、未运行模拟器；既有 Error -7、Thinking 停滞、首次配网断连问题保留。文档链接和上库清单覆盖/行数另行校验，不把编译成功描述为运行验收。历史文档链接指向当前路径，原验收结论仍按历史版本理解。

## 本轮逐文件修改

| 原路径 | 当前路径 | 修改 |
|---|---|---|
| `boards/esp32/common/esp32s3/audio_codec.h` | [src/boards/esp32/common/esp32s3/audio_codec.h](../src/boards/esp32/common/esp32s3/audio_codec.h) | 移动，内容不变 |
| `boards/esp32/common/esp32s3/audio_codec_lckfb.c` | [src/boards/esp32/common/esp32s3/audio_codec_lckfb.c](../src/boards/esp32/common/esp32s3/audio_codec_lckfb.c) | 移动，内容不变 |
| `boards/esp32/common/esp32s3/audio_codec_lckfb.h` | [src/boards/esp32/common/esp32s3/audio_codec_lckfb.h](../src/boards/esp32/common/esp32s3/audio_codec_lckfb.h) | 移动，内容不变 |
| `boards/esp32/common/esp32s3/board.c` | [src/boards/esp32/common/esp32s3/board.c](../src/boards/esp32/common/esp32s3/board.c) | 移动，内容不变 |
| `boards/esp32/common/esp32s3/include/board_display.h` | [src/boards/esp32/common/esp32s3/include/board_display.h](../src/boards/esp32/common/esp32s3/include/board_display.h) | 移动，内容不变 |
| `boards/esp32/lrd/board_init.c` | [src/boards/esp32/lrd/board_init.c](../src/boards/esp32/lrd/board_init.c) | 移动，内容不变 |
| `boards/esp32/lrd/board_init.h` | [src/boards/esp32/lrd/board_init.h](../src/boards/esp32/lrd/board_init.h) | 移动，内容不变 |
| `boards/esp32/lrd/board_pins.h` | [src/boards/esp32/lrd/board_pins.h](../src/boards/esp32/lrd/board_pins.h) | 移动，内容不变 |
| `boards/esp32/lrd/lcd_init.c` | [src/boards/esp32/lrd/lcd_init.c](../src/boards/esp32/lrd/lcd_init.c) | 移动，内容不变 |
| `boards/esp32/lrd/lcd_init.h` | [src/boards/esp32/lrd/lcd_init.h](../src/boards/esp32/lrd/lcd_init.h) | 移动，内容不变 |
| `boards/esp32/lrd/profile.c` | [src/boards/esp32/lrd/profile.c](../src/boards/esp32/lrd/profile.c) | 移动，内容不变 |
| `boards/esp32/lrd/tpt29555a.c` | [src/boards/esp32/lrd/tpt29555a.c](../src/boards/esp32/lrd/tpt29555a.c) | 移动，内容不变 |
| `boards/esp32/lrd/tpt29555a.h` | [src/boards/esp32/lrd/tpt29555a.h](../src/boards/esp32/lrd/tpt29555a.h) | 移动，内容不变 |
| `boards/esp32/szpi/board_init.c` | [src/boards/esp32/szpi/board_init.c](../src/boards/esp32/szpi/board_init.c) | 移动，内容不变 |
| `boards/esp32/szpi/board_init.h` | [src/boards/esp32/szpi/board_init.h](../src/boards/esp32/szpi/board_init.h) | 移动，内容不变 |
| `boards/esp32/szpi/board_pins.h` | [src/boards/esp32/szpi/board_pins.h](../src/boards/esp32/szpi/board_pins.h) | 移动，内容不变 |
| `boards/esp32/szpi/lcd_init.c` | [src/boards/esp32/szpi/lcd_init.c](../src/boards/esp32/szpi/lcd_init.c) | 移动，内容不变 |
| `boards/esp32/szpi/lcd_init.h` | [src/boards/esp32/szpi/lcd_init.h](../src/boards/esp32/szpi/lcd_init.h) | 移动，内容不变 |
| `boards/esp32/szpi/pca9557.c` | [src/boards/esp32/szpi/pca9557.c](../src/boards/esp32/szpi/pca9557.c) | 移动，内容不变 |
| `boards/esp32/szpi/pca9557.h` | [src/boards/esp32/szpi/pca9557.h](../src/boards/esp32/szpi/pca9557.h) | 移动，内容不变 |
| `boards/esp32/szpi/profile.c` | [src/boards/esp32/szpi/profile.c](../src/boards/esp32/szpi/profile.c) | 移动，内容不变 |
| `boards/ws63/hoperun/README.md` | [src/boards/ws63/hoperun/README.md](../src/boards/ws63/hoperun/README.md) | 移动并更新构建/文档路径 |
| `boards/ws63/hoperun/board.c` | [src/boards/ws63/hoperun/board.c](../src/boards/ws63/hoperun/board.c) | 移动，内容不变 |
| `boards/ws63/hoperun/board.cmake` | [src/boards/ws63/hoperun/board.cmake](../src/boards/ws63/hoperun/board.cmake) | 移动，内容不变 |
| `boards/ws63/hoperun/display.c` | [src/boards/ws63/hoperun/display.c](../src/boards/ws63/hoperun/display.c) | 移动，内容不变 |
| `docs/ai-hardware-ecosystem.md` | [docs/ai-hardware-ecosystem.md](../docs/ai-hardware-ecosystem.md) | 更新路径引用、职责说明或验证记录 |
| `docs/architecture-walkthrough.md` | [docs/architecture-walkthrough.md](../docs/architecture-walkthrough.md) | 更新路径引用、职责说明或验证记录 |
| `docs/architecture.md` | [docs/architecture.md](../docs/architecture.md) | 更新路径引用、职责说明或验证记录 |
| `docs/board-comparison.md` | [docs/board-comparison.md](../docs/board-comparison.md) | 更新路径引用、职责说明或验证记录 |
| `docs/changes-2026-10-15.md` | [docs/changes-2026-10-15.md](../docs/changes-2026-10-15.md) | 更新路径引用、职责说明或验证记录 |
| `docs/changes-batch4-2026-10-15.md` | [docs/changes-batch4-2026-10-15.md](../docs/changes-batch4-2026-10-15.md) | 更新路径引用、职责说明或验证记录 |
| `docs/changes-media-contract-2026-10-15.md` | [docs/changes-media-contract-2026-10-15.md](../docs/changes-media-contract-2026-10-15.md) | 更新路径引用、职责说明或验证记录 |
| `docs/changes-shared-layout-2026-10-15.md` | [docs/changes-shared-layout-2026-10-15.md](../docs/changes-shared-layout-2026-10-15.md) | 更新路径引用、职责说明或验证记录 |
| `docs/changes-simplify-2026-10-15.md` | [docs/changes-simplify-2026-10-15.md](../docs/changes-simplify-2026-10-15.md) | 更新路径引用、职责说明或验证记录 |
| `docs/repository-guide.md` | [docs/repository-guide.md](../docs/repository-guide.md) | 更新路径引用、职责说明或验证记录 |
| `docs/validation.md` | [docs/validation.md](../docs/validation.md) | 更新路径引用、职责说明或验证记录 |
| `ports/README.md` | [src/platforms/README.md](../src/platforms/README.md) | 移动并更新构建/文档路径 |
| `ports/esp_idf/cmake/components/ai_cloud/CMakeLists.txt` | [src/platforms/esp_idf/cmake/components/ai_cloud/CMakeLists.txt](../src/platforms/esp_idf/cmake/components/ai_cloud/CMakeLists.txt) | 移动并更新构建/文档路径 |
| `ports/esp_idf/cmake/components/ai_core/CMakeLists.txt` | [src/platforms/esp_idf/cmake/components/ai_core/CMakeLists.txt](../src/platforms/esp_idf/cmake/components/ai_core/CMakeLists.txt) | 移动并更新构建/文档路径 |
| `ports/esp_idf/cmake/components/ai_media/CMakeLists.txt` | [src/platforms/esp_idf/cmake/components/ai_media/CMakeLists.txt](../src/platforms/esp_idf/cmake/components/ai_media/CMakeLists.txt) | 移动并更新构建/文档路径 |
| `ports/esp_idf/cmake/components/ai_network/CMakeLists.txt` | [src/platforms/esp_idf/cmake/components/ai_network/CMakeLists.txt](../src/platforms/esp_idf/cmake/components/ai_network/CMakeLists.txt) | 移动并更新构建/文档路径 |
| `ports/esp_idf/cmake/components/ai_ui/CMakeLists.txt` | [src/platforms/esp_idf/cmake/components/ai_ui/CMakeLists.txt](../src/platforms/esp_idf/cmake/components/ai_ui/CMakeLists.txt) | 移动并更新构建/文档路径 |
| `ports/esp_idf/cmake/components/board/CMakeLists.txt` | [src/platforms/esp_idf/cmake/components/board/CMakeLists.txt](../src/platforms/esp_idf/cmake/components/board/CMakeLists.txt) | 移动并更新构建/文档路径 |
| `ports/esp_idf/cmake/components/convai_sdk/CMakeLists.txt` | [src/platforms/esp_idf/cmake/components/convai_sdk/CMakeLists.txt](../src/platforms/esp_idf/cmake/components/convai_sdk/CMakeLists.txt) | 移动，内容不变 |
| `ports/esp_idf/cmake/components/esp-wifi-connect/CMakeLists.txt` | [src/platforms/esp_idf/cmake/components/esp-wifi-connect/CMakeLists.txt](../src/platforms/esp_idf/cmake/components/esp-wifi-connect/CMakeLists.txt) | 移动并更新构建/文档路径 |
| `ports/esp_idf/cmake/components/esp-wifi-connect/idf_component.yml` | [src/platforms/esp_idf/cmake/components/esp-wifi-connect/idf_component.yml](../src/platforms/esp_idf/cmake/components/esp-wifi-connect/idf_component.yml) | 移动，内容不变 |
| `ports/esp_idf/cmake/components/esp_config/CMakeLists.txt` | [src/platforms/esp_idf/cmake/components/esp_config/CMakeLists.txt](../src/platforms/esp_idf/cmake/components/esp_config/CMakeLists.txt) | 移动并更新构建/文档路径 |
| `ports/esp_idf/cmake/components/esp_media/CMakeLists.txt` | [src/platforms/esp_idf/cmake/components/esp_media/CMakeLists.txt](../src/platforms/esp_idf/cmake/components/esp_media/CMakeLists.txt) | 移动并更新构建/文档路径 |
| `ports/esp_idf/cmake/components/esp_network/CMakeLists.txt` | [src/platforms/esp_idf/cmake/components/esp_network/CMakeLists.txt](../src/platforms/esp_idf/cmake/components/esp_network/CMakeLists.txt) | 移动并更新构建/文档路径 |
| `ports/esp_idf/cmake/components/esp_sdk/CMakeLists.txt` | [src/platforms/esp_idf/cmake/components/esp_sdk/CMakeLists.txt](../src/platforms/esp_idf/cmake/components/esp_sdk/CMakeLists.txt) | 移动并更新构建/文档路径 |
| `ports/esp_idf/cmake/components/main/CMakeLists.txt` | [src/platforms/esp_idf/cmake/components/main/CMakeLists.txt](../src/platforms/esp_idf/cmake/components/main/CMakeLists.txt) | 移动并更新构建/文档路径 |
| `ports/esp_idf/cmake/components/main/Kconfig.projbuild` | [src/platforms/esp_idf/cmake/components/main/Kconfig.projbuild](../src/platforms/esp_idf/cmake/components/main/Kconfig.projbuild) | 移动，内容不变 |
| `ports/esp_idf/cmake/components/main/idf_component.yml` | [src/platforms/esp_idf/cmake/components/main/idf_component.yml](../src/platforms/esp_idf/cmake/components/main/idf_component.yml) | 移动，内容不变 |
| `ports/esp_idf/components/config/config.c` | [src/platforms/esp_idf/components/config/config.c](../src/platforms/esp_idf/components/config/config.c) | 移动，内容不变 |
| `ports/esp_idf/components/config/include/esp_config.h` | [src/platforms/esp_idf/components/config/include/esp_config.h](../src/platforms/esp_idf/components/config/include/esp_config.h) | 移动，内容不变 |
| `ports/esp_idf/components/media/include/esp_media.h` | [src/platforms/esp_idf/components/media/include/esp_media.h](../src/platforms/esp_idf/components/media/include/esp_media.h) | 移动，内容不变 |
| `ports/esp_idf/components/media/media.c` | [src/platforms/esp_idf/components/media/media.c](../src/platforms/esp_idf/components/media/media.c) | 移动，内容不变 |
| `ports/esp_idf/components/network/esp-wifi-connect/dns_server.cc` | [src/platforms/esp_idf/components/network/esp-wifi-connect/dns_server.cc](../src/platforms/esp_idf/components/network/esp-wifi-connect/dns_server.cc) | 移动，内容不变 |
| `ports/esp_idf/components/network/esp-wifi-connect/esp_wifi_store.c` | [src/platforms/esp_idf/components/network/esp-wifi-connect/esp_wifi_store.c](../src/platforms/esp_idf/components/network/esp-wifi-connect/esp_wifi_store.c) | 移动，内容不变 |
| `ports/esp_idf/components/network/esp-wifi-connect/include/dns_server.h` | [src/platforms/esp_idf/components/network/esp-wifi-connect/include/dns_server.h](../src/platforms/esp_idf/components/network/esp-wifi-connect/include/dns_server.h) | 移动，内容不变 |
| `ports/esp_idf/components/network/esp-wifi-connect/include/esp_wifi_store.h` | [src/platforms/esp_idf/components/network/esp-wifi-connect/include/esp_wifi_store.h](../src/platforms/esp_idf/components/network/esp-wifi-connect/include/esp_wifi_store.h) | 移动，内容不变 |
| `ports/esp_idf/components/network/esp-wifi-connect/include/ssid_manager.h` | [src/platforms/esp_idf/components/network/esp-wifi-connect/include/ssid_manager.h](../src/platforms/esp_idf/components/network/esp-wifi-connect/include/ssid_manager.h) | 移动，内容不变 |
| `ports/esp_idf/components/network/esp-wifi-connect/include/wifi_configuration_ap.h` | [src/platforms/esp_idf/components/network/esp-wifi-connect/include/wifi_configuration_ap.h](../src/platforms/esp_idf/components/network/esp-wifi-connect/include/wifi_configuration_ap.h) | 移动，内容不变 |
| `ports/esp_idf/components/network/esp-wifi-connect/include/wifi_manager.h` | [src/platforms/esp_idf/components/network/esp-wifi-connect/include/wifi_manager.h](../src/platforms/esp_idf/components/network/esp-wifi-connect/include/wifi_manager.h) | 移动，内容不变 |
| `ports/esp_idf/components/network/esp-wifi-connect/include/wifi_station.h` | [src/platforms/esp_idf/components/network/esp-wifi-connect/include/wifi_station.h](../src/platforms/esp_idf/components/network/esp-wifi-connect/include/wifi_station.h) | 移动，内容不变 |
| `ports/esp_idf/components/network/esp-wifi-connect/ssid_manager.cc` | [src/platforms/esp_idf/components/network/esp-wifi-connect/ssid_manager.cc](../src/platforms/esp_idf/components/network/esp-wifi-connect/ssid_manager.cc) | 移动，内容不变 |
| `ports/esp_idf/components/network/esp-wifi-connect/wifi_configuration_ap.cc` | [src/platforms/esp_idf/components/network/esp-wifi-connect/wifi_configuration_ap.cc](../src/platforms/esp_idf/components/network/esp-wifi-connect/wifi_configuration_ap.cc) | 移动，内容不变 |
| `ports/esp_idf/components/network/esp-wifi-connect/wifi_manager.cc` | [src/platforms/esp_idf/components/network/esp-wifi-connect/wifi_manager.cc](../src/platforms/esp_idf/components/network/esp-wifi-connect/wifi_manager.cc) | 移动，内容不变 |
| `ports/esp_idf/components/network/esp-wifi-connect/wifi_station.cc` | [src/platforms/esp_idf/components/network/esp-wifi-connect/wifi_station.cc](../src/platforms/esp_idf/components/network/esp-wifi-connect/wifi_station.cc) | 移动，内容不变 |
| `ports/esp_idf/components/network/include/esp_network.h` | [src/platforms/esp_idf/components/network/include/esp_network.h](../src/platforms/esp_idf/components/network/include/esp_network.h) | 移动，内容不变 |
| `ports/esp_idf/components/network/network.cpp` | [src/platforms/esp_idf/components/network/network.cpp](../src/platforms/esp_idf/components/network/network.cpp) | 移动，内容不变 |
| `ports/esp_idf/components/sdk/convai_platform_esp32.c` | [src/platforms/esp_idf/components/convai/convai_platform_esp32.c](../src/platforms/esp_idf/components/convai/convai_platform_esp32.c) | 移动，内容不变 |
| `ports/esp_idf/components/sdk/convai_platform_esp32_internal.h` | [src/platforms/esp_idf/components/convai/convai_platform_esp32_internal.h](../src/platforms/esp_idf/components/convai/convai_platform_esp32_internal.h) | 移动，内容不变 |
| `ports/esp_idf/components/sdk/esp32_misc.c` | [src/platforms/esp_idf/components/convai/esp32_misc.c](../src/platforms/esp_idf/components/convai/esp32_misc.c) | 移动，内容不变 |
| `ports/esp_idf/components/sdk/esp32_netal.c` | [src/platforms/esp_idf/components/convai/esp32_netal.c](../src/platforms/esp_idf/components/convai/esp32_netal.c) | 移动，内容不变 |
| `ports/esp_idf/components/sdk/esp32_osal.c` | [src/platforms/esp_idf/components/convai/esp32_osal.c](../src/platforms/esp_idf/components/convai/esp32_osal.c) | 移动，内容不变 |
| `ports/esp_idf/components/sdk/esp32_tlsal.c` | [src/platforms/esp_idf/components/convai/esp32_tlsal.c](../src/platforms/esp_idf/components/convai/esp32_tlsal.c) | 移动，内容不变 |
| `ports/esp_idf/components/sdk/include/convai_platform_esp32.h` | [src/platforms/esp_idf/components/convai/include/convai_platform_esp32.h](../src/platforms/esp_idf/components/convai/include/convai_platform_esp32.h) | 移动，内容不变 |
| `ports/esp_idf/components/sdk/include/esp_device.h` | [src/platforms/esp_idf/components/convai/include/esp_device.h](../src/platforms/esp_idf/components/convai/include/esp_device.h) | 移动，内容不变 |
| `ports/esp_idf/components/ui/ai_ui.c` | [src/platforms/esp_idf/components/ui/ai_ui.c](../src/platforms/esp_idf/components/ui/ai_ui.c) | 移动，内容不变 |
| `ports/esp_idf/entry/app_main.c` | [src/platforms/esp_idf/entry/app_main.c](../src/platforms/esp_idf/entry/app_main.c) | 移动，内容不变 |
| `ports/esp_idf/targets/esp32s3/CMakeLists.txt` | [src/platforms/esp_idf/targets/esp32s3/CMakeLists.txt](../src/platforms/esp_idf/targets/esp32s3/CMakeLists.txt) | 移动并更新构建/文档路径 |
| `ports/esp_idf/targets/esp32s3/project.cmake` | [src/platforms/esp_idf/targets/esp32s3/project.cmake](../src/platforms/esp_idf/targets/esp32s3/project.cmake) | 移动并更新构建/文档路径 |
| `ports/goldieos/README.md` | [src/platforms/goldieos/README.md](../src/platforms/goldieos/README.md) | 移动并更新构建/文档路径 |
| `ports/goldieos/cmake/board.cmake` | [src/platforms/goldieos/cmake/board.cmake](../src/platforms/goldieos/cmake/board.cmake) | 移动并更新构建/文档路径 |
| `ports/goldieos/cmake/common.cmake` | [src/platforms/goldieos/cmake/common.cmake](../src/platforms/goldieos/cmake/common.cmake) | 移动并更新构建/文档路径 |
| `ports/goldieos/cmake/ws63-core/CMakeLists.txt` | [src/platforms/goldieos/cmake/ws63-core/CMakeLists.txt](../src/platforms/goldieos/cmake/ws63-core/CMakeLists.txt) | 移动并更新构建/文档路径 |
| `ports/goldieos/components/config/component.cmake` | [src/platforms/goldieos/components/config/component.cmake](../src/platforms/goldieos/components/config/component.cmake) | 移动，内容不变 |
| `ports/goldieos/components/config/config.c` | [src/platforms/goldieos/components/config/config.c](../src/platforms/goldieos/components/config/config.c) | 移动，内容不变 |
| `ports/goldieos/components/config/include/ws_config.h` | [src/platforms/goldieos/components/config/include/ws_config.h](../src/platforms/goldieos/components/config/include/ws_config.h) | 移动，内容不变 |
| `ports/goldieos/components/media/component.cmake` | [src/platforms/goldieos/components/media/component.cmake](../src/platforms/goldieos/components/media/component.cmake) | 移动，内容不变 |
| `ports/goldieos/components/media/include/ws_media.h` | [src/platforms/goldieos/components/media/include/ws_media.h](../src/platforms/goldieos/components/media/include/ws_media.h) | 移动，内容不变 |
| `ports/goldieos/components/media/media.c` | [src/platforms/goldieos/components/media/media.c](../src/platforms/goldieos/components/media/media.c) | 移动，内容不变 |
| `ports/goldieos/components/network/component.cmake` | [src/platforms/goldieos/components/network/component.cmake](../src/platforms/goldieos/components/network/component.cmake) | 移动并更新构建/文档路径 |
| `ports/goldieos/components/network/include/ws_network.h` | [src/platforms/goldieos/components/network/include/ws_network.h](../src/platforms/goldieos/components/network/include/ws_network.h) | 移动，内容不变 |
| `ports/goldieos/components/network/windows/network.c` | [src/platforms/goldieos/components/network/windows/network.c](../src/platforms/goldieos/components/network/windows/network.c) | 移动，内容不变 |
| `ports/goldieos/components/network/ws63/dhcp_gateway.c` | [src/platforms/goldieos/components/network/ws63/dhcp_gateway.c](../src/platforms/goldieos/components/network/ws63/dhcp_gateway.c) | 移动，内容不变 |
| `ports/goldieos/components/network/ws63/dhcp_gateway.h` | [src/platforms/goldieos/components/network/ws63/dhcp_gateway.h](../src/platforms/goldieos/components/network/ws63/dhcp_gateway.h) | 移动，内容不变 |
| `ports/goldieos/components/network/ws63/network.c` | [src/platforms/goldieos/components/network/ws63/network.c](../src/platforms/goldieos/components/network/ws63/network.c) | 移动，内容不变 |
| `ports/goldieos/components/network/ws63/portal_gateway.c` | [src/platforms/goldieos/components/network/ws63/portal_gateway.c](../src/platforms/goldieos/components/network/ws63/portal_gateway.c) | 移动，内容不变 |
| `ports/goldieos/components/network/ws63/wifi_store.c` | [src/platforms/goldieos/components/network/ws63/wifi_store.c](../src/platforms/goldieos/components/network/ws63/wifi_store.c) | 移动并更新构建/文档路径 |
| `ports/goldieos/components/network/ws63/wifi_store.h` | [src/platforms/goldieos/components/network/ws63/wifi_store.h](../src/platforms/goldieos/components/network/ws63/wifi_store.h) | 移动，内容不变 |
| `ports/goldieos/components/network/ws63/ws_wifi_abi.h` | [src/platforms/goldieos/components/network/ws63/ws_wifi_abi.h](../src/platforms/goldieos/components/network/ws63/ws_wifi_abi.h) | 移动，内容不变 |
| `ports/goldieos/components/runtime/component.cmake` | [src/platforms/goldieos/components/runtime/component.cmake](../src/platforms/goldieos/components/runtime/component.cmake) | 移动，内容不变 |
| `ports/goldieos/components/runtime/include/runtime.h` | [src/platforms/goldieos/components/runtime/include/runtime.h](../src/platforms/goldieos/components/runtime/include/runtime.h) | 移动，内容不变 |
| `ports/goldieos/components/runtime/windows/runtime.c` | [src/platforms/goldieos/components/runtime/windows/runtime.c](../src/platforms/goldieos/components/runtime/windows/runtime.c) | 移动，内容不变 |
| `ports/goldieos/components/runtime/ws63/runtime.c` | [src/platforms/goldieos/components/runtime/ws63/runtime.c](../src/platforms/goldieos/components/runtime/ws63/runtime.c) | 移动，内容不变 |
| `ports/goldieos/components/sdk/component.cmake` | [src/platforms/goldieos/components/convai/component.cmake](../src/platforms/goldieos/components/convai/component.cmake) | 移动，内容不变 |
| `ports/goldieos/components/sdk/windows/sdk_platform.c` | [src/platforms/goldieos/components/convai/windows/sdk_platform.c](../src/platforms/goldieos/components/convai/windows/sdk_platform.c) | 移动，内容不变 |
| `ports/goldieos/components/sdk/ws63/sdk_platform.c` | [src/platforms/goldieos/components/convai/ws63/sdk_platform.c](../src/platforms/goldieos/components/convai/ws63/sdk_platform.c) | 移动，内容不变 |
| `ports/goldieos/components/ui/component.cmake` | [src/platforms/goldieos/components/ui/component.cmake](../src/platforms/goldieos/components/ui/component.cmake) | 移动并更新构建/文档路径 |
| `ports/goldieos/components/ui/include/ws_ui.h` | [src/platforms/goldieos/components/ui/include/ws_ui.h](../src/platforms/goldieos/components/ui/include/ws_ui.h) | 移动，内容不变 |
| `ports/goldieos/components/ui/lv_conf.h` | [src/platforms/goldieos/components/ui/lv_conf.h](../src/platforms/goldieos/components/ui/lv_conf.h) | 移动，内容不变 |
| `ports/goldieos/components/ui/ui.c` | [src/platforms/goldieos/components/ui/ui.c](../src/platforms/goldieos/components/ui/ui.c) | 移动，内容不变 |
| `ports/goldieos/entry/app.c` | [src/platforms/goldieos/entry/app.c](../src/platforms/goldieos/entry/app.c) | 移动，内容不变 |
| `ports/goldieos/include/ws_app_events.h` | [src/platforms/goldieos/include/ws_app_events.h](../src/platforms/goldieos/include/ws_app_events.h) | 移动，内容不变 |
| `ports/goldieos/targets/windows/CMakeLists.txt` | [src/platforms/goldieos/targets/windows/CMakeLists.txt](../src/platforms/goldieos/targets/windows/CMakeLists.txt) | 移动并更新构建/文档路径 |
| `ports/goldieos/targets/windows/README.md` | [src/platforms/goldieos/targets/windows/README.md](../src/platforms/goldieos/targets/windows/README.md) | 移动并更新构建/文档路径 |
| `ports/goldieos/targets/ws63/CMakeLists.txt` | [src/platforms/goldieos/targets/ws63/CMakeLists.txt](../src/platforms/goldieos/targets/ws63/CMakeLists.txt) | 移动并更新构建/文档路径 |
| `shared/application/ai_app.c` | [src/application/core/ai_app.c](../src/application/core/ai_app.c) | 移动，内容不变 |
| `shared/application/ai_controller.c` | [src/application/core/ai_controller.c](../src/application/core/ai_controller.c) | 移动，内容不变 |
| `shared/application/include/ai_app.h` | [src/application/core/include/ai_app.h](../src/application/core/include/ai_app.h) | 移动，内容不变 |
| `shared/application/include/ai_controller.h` | [src/application/core/include/ai_controller.h](../src/application/core/include/ai_controller.h) | 移动，内容不变 |
| `shared/services/cloud/ai_cloud.c` | [src/application/services/cloud/ai_cloud.c](../src/application/services/cloud/ai_cloud.c) | 移动，内容不变 |
| `shared/services/cloud/include/ai_cloud.h` | [src/application/services/cloud/include/ai_cloud.h](../src/application/services/cloud/include/ai_cloud.h) | 移动，内容不变 |
| `shared/services/media/ai_audio.c` | [src/application/services/media/ai_audio.c](../src/application/services/media/ai_audio.c) | 移动，内容不变 |
| `shared/services/media/ai_playback.c` | [src/application/services/media/ai_playback.c](../src/application/services/media/ai_playback.c) | 移动，内容不变 |
| `shared/services/media/include/ai_audio.h` | [src/application/services/media/include/ai_audio.h](../src/application/services/media/include/ai_audio.h) | 移动，内容不变 |
| `shared/services/media/include/ai_media.h` | [src/application/services/media/include/ai_media.h](../src/application/services/media/include/ai_media.h) | 移动，内容不变 |
| `shared/services/media/include/ai_playback.h` | [src/application/services/media/include/ai_playback.h](../src/application/services/media/include/ai_playback.h) | 移动，内容不变 |
| `shared/services/network/ai_network_status.c` | [src/application/services/network/ai_network_status.c](../src/application/services/network/ai_network_status.c) | 移动，内容不变 |
| `shared/services/network/ai_portal.c` | [src/application/services/network/ai_portal.c](../src/application/services/network/ai_portal.c) | 移动，内容不变 |
| `shared/services/network/ai_provision.c` | [src/application/services/network/ai_provision.c](../src/application/services/network/ai_provision.c) | 移动，内容不变 |
| `shared/services/network/ai_wifi_store.c` | [src/application/services/network/ai_wifi_store.c](../src/application/services/network/ai_wifi_store.c) | 移动，内容不变 |
| `shared/services/network/include/ai_network_status.h` | [src/application/services/network/include/ai_network_status.h](../src/application/services/network/include/ai_network_status.h) | 移动，内容不变 |
| `shared/services/network/include/ai_portal.h` | [src/application/services/network/include/ai_portal.h](../src/application/services/network/include/ai_portal.h) | 移动，内容不变 |
| `shared/services/network/include/ai_provision.h` | [src/application/services/network/include/ai_provision.h](../src/application/services/network/include/ai_provision.h) | 移动，内容不变 |
| `shared/services/network/include/ai_wifi_store.h` | [src/application/services/network/include/ai_wifi_store.h](../src/application/services/network/include/ai_wifi_store.h) | 移动，内容不变 |
| `shared/ui/ai_lvgl.c` | [src/application/ui/ai_lvgl.c](../src/application/ui/ai_lvgl.c) | 移动，内容不变 |
| `shared/ui/include/ai_lvgl.h` | [src/application/ui/include/ai_lvgl.h](../src/application/ui/include/ai_lvgl.h) | 移动，内容不变 |
| `shared/ui/include/ai_screen.h` | [src/application/ui/include/ai_screen.h](../src/application/ui/include/ai_screen.h) | 移动，内容不变 |
| `shared/ui/include/ai_ui.h` | [src/application/ui/include/ai_ui.h](../src/application/ui/include/ai_ui.h) | 移动，内容不变 |
| `shared/ui/web/wifi_configuration.html` | [src/application/ui/web/wifi_configuration.html](../src/application/ui/web/wifi_configuration.html) | 移动，内容不变 |
| `shared/ui/web/wifi_configuration_done.html` | [src/application/ui/web/wifi_configuration_done.html](../src/application/ui/web/wifi_configuration_done.html) | 移动，内容不变 |
| `skills/_shared/references/esp-idf.md` | [skills/_shared/references/esp-idf.md](../skills/_shared/references/esp-idf.md) | 更新路径引用、职责说明或验证记录 |
| `skills/_shared/references/hoperun.md` | [skills/_shared/references/hoperun.md](../skills/_shared/references/hoperun.md) | 更新路径引用、职责说明或验证记录 |
| `skills/_shared/scripts/build.ps1` | [skills/_shared/scripts/build.ps1](../skills/_shared/scripts/build.ps1) | 更新路径引用、职责说明或验证记录 |
| `skills/build-hoperun-simulator/SKILL.md` | [skills/build-hoperun-simulator/SKILL.md](../skills/build-hoperun-simulator/SKILL.md) | 更新路径引用、职责说明或验证记录 |
| `skills/build-hoperun-ws63/SKILL.md` | [skills/build-hoperun-ws63/SKILL.md](../skills/build-hoperun-ws63/SKILL.md) | 更新路径引用、职责说明或验证记录 |
| `skills/build-lrd/SKILL.md` | [skills/build-lrd/SKILL.md](../skills/build-lrd/SKILL.md) | 更新路径引用、职责说明或验证记录 |
| `skills/build-szpi/SKILL.md` | [skills/build-szpi/SKILL.md](../skills/build-szpi/SKILL.md) | 更新路径引用、职责说明或验证记录 |
| `CMakeLists.txt` | [CMakeLists.txt](../CMakeLists.txt) | 更新路径引用、职责说明或验证记录 |
| `README.md` | [README.md](../README.md) | 更新路径引用、职责说明或验证记录 |
| `boards/include/board.h` | [src/boards/esp32/include/board.h](../src/boards/esp32/include/board.h) | src 迁移后，2026-10-20 再移入 ESP 板级范围，内容不变 |
| `boards/include/ws_board.h` | [src/boards/ws63/include/ws_board.h](../src/boards/ws63/include/ws_board.h) | src 迁移后，2026-10-20 再移入 WS63 板级范围，内容不变 |
| `tests/CMakeLists.txt` | [tests/CMakeLists.txt](../tests/CMakeLists.txt) | 更新路径引用、职责说明或验证记录 |
| `tests/test_wifi_web.cjs` | [tests/test_wifi_web.cjs](../tests/test_wifi_web.cjs) | 更新路径引用、职责说明或验证记录 |
| `third_party/README.md` | [third_party/README.md](../third_party/README.md) | 更新路径引用、职责说明或验证记录 |
| `tools/check_architecture.py` | [tools/check_architecture.py](../tools/check_architecture.py) | 更新路径引用、职责说明或验证记录 |
| — | [src/platforms/esp_idf/cmake/paths.cmake](../src/platforms/esp_idf/cmake/paths.cmake) | 新增统一构建路径定义 |
| — | [docs/changes-src-layout-2026-10-15.md](changes-src-layout-2026-10-15.md) | 新增本轮记录 |
