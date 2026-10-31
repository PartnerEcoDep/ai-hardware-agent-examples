# 公共代码归拢与平台入口迁移（2026-10-15）

本轮变更以操作前文件内容为基线，不把工作区此前未提交的整改计入本轮。移动文件内容逐一校验一致；业务接口、板型参数和产物名称保留。

## 目录与职责

- app → src/application/core：公共状态机与协调。
- services → src/application/services：公共云、媒体、网络能力。
- ui → src/application/ui：公共 LVGL 与手机网页。
- src/platforms/esp_idf/app → src/platforms/esp_idf/entry；src/platforms/goldieos/app → src/platforms/goldieos/entry：平台启动与组装。
- 保留 ai_core/main 等构建组件名，避免无必要的兼容性变化；boards 与平台组件不迁移。

## 逐文件修改

| 原路径 | 当前路径 | 本轮修改 |
|---|---|---|
| `README.md` | [README.md](../README.md) | 更新路径、职责说明或文档链接 |
| `docs/architecture-walkthrough.md` | [docs/architecture-walkthrough.md](architecture-walkthrough.md) | 更新路径、职责说明或文档链接 |
| `docs/architecture.md` | [docs/architecture.md](architecture.md) | 更新路径、职责说明或文档链接 |
| `docs/board-comparison.md` | [docs/board-comparison.md](board-comparison.md) | 更新路径、职责说明或文档链接 |
| `docs/changes-2026-10-15.md` | [docs/changes-2026-10-15.md](changes-2026-10-15.md) | 更新路径、职责说明或文档链接 |
| `docs/changes-batch4-2026-10-15.md` | [docs/changes-batch4-2026-10-15.md](changes-batch4-2026-10-15.md) | 更新路径、职责说明或文档链接 |
| `docs/changes-media-contract-2026-10-15.md` | [docs/changes-media-contract-2026-10-15.md](changes-media-contract-2026-10-15.md) | 更新路径、职责说明或文档链接 |
| `docs/changes-simplify-2026-10-15.md` | [docs/changes-simplify-2026-10-15.md](changes-simplify-2026-10-15.md) | 更新路径、职责说明或文档链接 |
| `docs/repository-guide.md` | [docs/repository-guide.md](repository-guide.md) | 更新路径、职责说明或文档链接 |
| `docs/validation.md` | [docs/validation.md](validation.md) | 更新路径、职责说明或文档链接 |
| `src/platforms/README.md` | [src/platforms/README.md](../src/platforms/README.md) | 更新路径、职责说明或文档链接 |
| `src/platforms/esp_idf/cmake/components/ai_cloud/CMakeLists.txt` | [src/platforms/esp_idf/cmake/components/ai_cloud/CMakeLists.txt](../src/platforms/esp_idf/cmake/components/ai_cloud/CMakeLists.txt) | 更新公共源文件/入口路径引用 |
| `src/platforms/esp_idf/cmake/components/ai_core/CMakeLists.txt` | [src/platforms/esp_idf/cmake/components/ai_core/CMakeLists.txt](../src/platforms/esp_idf/cmake/components/ai_core/CMakeLists.txt) | 更新公共源文件/入口路径引用 |
| `src/platforms/esp_idf/cmake/components/ai_media/CMakeLists.txt` | [src/platforms/esp_idf/cmake/components/ai_media/CMakeLists.txt](../src/platforms/esp_idf/cmake/components/ai_media/CMakeLists.txt) | 更新公共源文件/入口路径引用 |
| `src/platforms/esp_idf/cmake/components/ai_network/CMakeLists.txt` | [src/platforms/esp_idf/cmake/components/ai_network/CMakeLists.txt](../src/platforms/esp_idf/cmake/components/ai_network/CMakeLists.txt) | 更新公共源文件/入口路径引用 |
| `src/platforms/esp_idf/cmake/components/ai_ui/CMakeLists.txt` | [src/platforms/esp_idf/cmake/components/ai_ui/CMakeLists.txt](../src/platforms/esp_idf/cmake/components/ai_ui/CMakeLists.txt) | 更新公共源文件/入口路径引用 |
| `src/platforms/esp_idf/cmake/components/esp-wifi-connect/CMakeLists.txt` | [src/platforms/esp_idf/cmake/components/esp-wifi-connect/CMakeLists.txt](../src/platforms/esp_idf/cmake/components/esp-wifi-connect/CMakeLists.txt) | 更新公共源文件/入口路径引用 |
| `src/platforms/esp_idf/cmake/components/main/CMakeLists.txt` | [src/platforms/esp_idf/cmake/components/main/CMakeLists.txt](../src/platforms/esp_idf/cmake/components/main/CMakeLists.txt) | 更新公共源文件/入口路径引用 |
| `src/platforms/esp_idf/app/app_main.c` | [src/platforms/esp_idf/entry/app_main.c](../src/platforms/esp_idf/entry/app_main.c) | 仅移动，内容不变 |
| `src/platforms/goldieos/README.md` | [src/platforms/goldieos/README.md](../src/platforms/goldieos/README.md) | 更新路径、职责说明或文档链接 |
| `src/platforms/goldieos/cmake/common.cmake` | [src/platforms/goldieos/cmake/common.cmake](../src/platforms/goldieos/cmake/common.cmake) | 更新公共源文件/入口路径引用 |
| `src/platforms/goldieos/cmake/ws63-core/CMakeLists.txt` | [src/platforms/goldieos/cmake/ws63-core/CMakeLists.txt](../src/platforms/goldieos/cmake/ws63-core/CMakeLists.txt) | 更新公共源文件/入口路径引用 |
| `src/platforms/goldieos/components/network/component.cmake` | [src/platforms/goldieos/components/network/component.cmake](../src/platforms/goldieos/components/network/component.cmake) | 更新公共源文件/入口路径引用 |
| `src/platforms/goldieos/components/ui/component.cmake` | [src/platforms/goldieos/components/ui/component.cmake](../src/platforms/goldieos/components/ui/component.cmake) | 更新公共源文件/入口路径引用 |
| `src/platforms/goldieos/app/app.c` | [src/platforms/goldieos/entry/app.c](../src/platforms/goldieos/entry/app.c) | 仅移动，内容不变 |
| `app/ai_app.c` | [src/application/core/ai_app.c](../src/application/core/ai_app.c) | 仅移动，内容不变 |
| `app/ai_controller.c` | [src/application/core/ai_controller.c](../src/application/core/ai_controller.c) | 仅移动，内容不变 |
| `app/include/ai_app.h` | [src/application/core/include/ai_app.h](../src/application/core/include/ai_app.h) | 仅移动，内容不变 |
| `app/include/ai_controller.h` | [src/application/core/include/ai_controller.h](../src/application/core/include/ai_controller.h) | 仅移动，内容不变 |
| `services/cloud/ai_cloud.c` | [src/application/services/cloud/ai_cloud.c](../src/application/services/cloud/ai_cloud.c) | 仅移动，内容不变 |
| `services/cloud/include/ai_cloud.h` | [src/application/services/cloud/include/ai_cloud.h](../src/application/services/cloud/include/ai_cloud.h) | 仅移动，内容不变 |
| `services/media/ai_audio.c` | [src/application/services/media/ai_audio.c](../src/application/services/media/ai_audio.c) | 仅移动，内容不变 |
| `services/media/ai_playback.c` | [src/application/services/media/ai_playback.c](../src/application/services/media/ai_playback.c) | 仅移动，内容不变 |
| `services/media/include/ai_audio.h` | [src/application/services/media/include/ai_audio.h](../src/application/services/media/include/ai_audio.h) | 仅移动，内容不变 |
| `services/media/include/ai_media.h` | [src/application/services/media/include/ai_media.h](../src/application/services/media/include/ai_media.h) | 仅移动，内容不变 |
| `services/media/include/ai_playback.h` | [src/application/services/media/include/ai_playback.h](../src/application/services/media/include/ai_playback.h) | 仅移动，内容不变 |
| `services/network/ai_network_status.c` | [src/application/services/network/ai_network_status.c](../src/application/services/network/ai_network_status.c) | 仅移动，内容不变 |
| `services/network/ai_portal.c` | [src/application/services/network/ai_portal.c](../src/application/services/network/ai_portal.c) | 仅移动，内容不变 |
| `services/network/ai_provision.c` | [src/application/services/network/ai_provision.c](../src/application/services/network/ai_provision.c) | 仅移动，内容不变 |
| `services/network/ai_wifi_store.c` | [src/application/services/network/ai_wifi_store.c](../src/application/services/network/ai_wifi_store.c) | 仅移动，内容不变 |
| `services/network/include/ai_network_status.h` | [src/application/services/network/include/ai_network_status.h](../src/application/services/network/include/ai_network_status.h) | 仅移动，内容不变 |
| `services/network/include/ai_portal.h` | [src/application/services/network/include/ai_portal.h](../src/application/services/network/include/ai_portal.h) | 仅移动，内容不变 |
| `services/network/include/ai_provision.h` | [src/application/services/network/include/ai_provision.h](../src/application/services/network/include/ai_provision.h) | 仅移动，内容不变 |
| `services/network/include/ai_wifi_store.h` | [src/application/services/network/include/ai_wifi_store.h](../src/application/services/network/include/ai_wifi_store.h) | 仅移动，内容不变 |
| `ui/ai_lvgl.c` | [src/application/ui/ai_lvgl.c](../src/application/ui/ai_lvgl.c) | 仅移动，内容不变 |
| `ui/include/ai_lvgl.h` | [src/application/ui/include/ai_lvgl.h](../src/application/ui/include/ai_lvgl.h) | 仅移动，内容不变 |
| `ui/include/ai_screen.h` | [src/application/ui/include/ai_screen.h](../src/application/ui/include/ai_screen.h) | 仅移动，内容不变 |
| `ui/include/ai_ui.h` | [src/application/ui/include/ai_ui.h](../src/application/ui/include/ai_ui.h) | 仅移动，内容不变 |
| `tests/CMakeLists.txt` | [tests/CMakeLists.txt](../tests/CMakeLists.txt) | 更新公共源文件/入口路径引用 |
| `tests/test_wifi_web.cjs` | [tests/test_wifi_web.cjs](../tests/test_wifi_web.cjs) | 更新公共源文件/入口路径引用 |
| `tools/check_architecture.py` | [tools/check_architecture.py](../tools/check_architecture.py) | 架构检查扫描新公共目录，保持原依赖限制 |
| — | [docs/changes-shared-layout-2026-10-15.md](changes-shared-layout-2026-10-15.md) | 新增本轮逐文件记录 |

历史变更文档中的文件链接随迁移更新，以便继续跳转到当前源码；不改变其历史验收结论。repository-guide 同步文件覆盖、行数与当前验收状态，validation 新增本轮构建和哈希记录。

## 验证与交付

四目标构建、ESP 镜像合并、12 项主机测试和架构检查通过。已移动的 25 个文件内容校验完全一致；未烧写、未运行模拟器、未提交代码。必要修复保持，已知问题不因目录迁移视为解决。

| 产物 | 字节数 | SHA-256 |
|---|---:|---|
| `build-lrd/unified_ai_lrd_merged.bin` | 1654016 | `5b4b29437f4869787dd2525b146a514df8b0d9eb10fab6e808f6f337bb5da2c7` |
| `build-szpi/unified_ai_szpi_merged.bin` | 1660672 | `c7eaf0a53f44e132a6009ce029ace51a8871b113c65fe91bb655558044bc0024` |
| `build-hoperun-app/out/unified_ai_hoperun.fwpkg` | 2235288 | `c294492fdc6e5ad569f3d398b24fcfe96910804da00e937b5c5976374347a7a2` |
| `build-hoperun-win/unified_ai_simulator.exe` | 3655658 | `674d9d86d9567901887c1a5d226ed236ed38284f57e0ef2aed096fc644247b03` |
