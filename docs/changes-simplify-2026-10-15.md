# 第三批精简文件记录（2026-10-15）

本轮按用户确认范围精简，不改变 ESP 双快照格式、存储恢复策略、保存失败反馈、公共列表操作或配网流程。第四批邮箱保持已回退状态。

| 文件 | 操作 | 说明 |
|---|---|---|
| `src/application/services/media/include/ai_media_stats.h` | 删除 | 移除未使用的累计音频统计契约 |
| [src/application/services/media/include/ai_playback.h](../src/application/services/media/include/ai_playback.h) | 修改 | 移除队列统计类型及字段 |
| [src/application/services/media/ai_playback.c](../src/application/services/media/ai_playback.c) | 修改 | 移除统计累加，保留缓冲和边界处理 |
| [src/application/services/network/include/ai_wifi_store.h](../src/application/services/network/include/ai_wifi_store.h) | 修改 | 移除清空接口声明，保留保存/删除/默认操作 |
| [src/application/services/network/ai_wifi_store.c](../src/application/services/network/ai_wifi_store.c) | 修改 | 移除清空实现，保留公共列表及保存结果逻辑 |
| [src/platforms/esp_idf/components/media/include/esp_media.h](../src/platforms/esp_idf/components/media/include/esp_media.h) | 修改 | 移除音频统计公开接口 |
| [src/platforms/esp_idf/components/media/media.c](../src/platforms/esp_idf/components/media/media.c) | 修改 | 移除累计统计，保留故障处理和原有低频日志 |
| [src/platforms/goldieos/components/media/include/ws_media.h](../src/platforms/goldieos/components/media/include/ws_media.h) | 修改 | 移除音频统计公开接口 |
| [src/platforms/goldieos/components/media/media.c](../src/platforms/goldieos/components/media/media.c) | 修改 | 移除累计统计和专用辅助变量，保留录放音行为 |
| [src/platforms/esp_idf/components/network/include/esp_network.h](../src/platforms/esp_idf/components/network/include/esp_network.h) | 修改 | 移除内部网络状态类型和查询声明 |
| [src/platforms/goldieos/components/network/include/ws_network.h](../src/platforms/goldieos/components/network/include/ws_network.h) | 修改 | 移除内部网络状态类型和查询声明 |
| [src/platforms/esp_idf/components/network/network.cpp](../src/platforms/esp_idf/components/network/network.cpp) | 修改 | 状态查询限定为文件内部函数 |
| [src/platforms/goldieos/components/network/ws63/network.c](../src/platforms/goldieos/components/network/ws63/network.c) | 修改 | 状态查询限定为文件内部函数，配网逻辑不变 |
| [src/platforms/goldieos/components/network/windows/network.c](../src/platforms/goldieos/components/network/windows/network.c) | 修改 | 状态查询限定为文件内部函数 |
| [src/platforms/esp_idf/components/network/esp-wifi-connect/include/ssid_manager.h](../src/platforms/esp_idf/components/network/esp-wifi-connect/include/ssid_manager.h) | 修改 | 移除闲置 Clear 声明 |
| [src/platforms/esp_idf/components/network/esp-wifi-connect/ssid_manager.cc](../src/platforms/esp_idf/components/network/esp-wifi-connect/ssid_manager.cc) | 修改 | 移除闲置 Clear 包装，不改存储加载或快照 |
| [src/platforms/esp_idf/cmake/components/esp_media/CMakeLists.txt](../src/platforms/esp_idf/cmake/components/esp_media/CMakeLists.txt) | 修改 | ai_media 恢复为实现私有依赖 |
| [src/platforms/esp_idf/cmake/components/esp_network/CMakeLists.txt](../src/platforms/esp_idf/cmake/components/esp_network/CMakeLists.txt) | 修改 | ai_network 收为实现私有依赖 |
| [tests/test_audio.c](../tests/test_audio.c) | 修改 | 删除统计断言，保留音频算法/缓冲行为测试 |
| [tests/test_network_contract.c](../tests/test_network_contract.c) | 修改 | 改为逐个删除至空并验证空列表删除失败 |
| [README.md](../README.md) | 修改 | 更新公共媒体层职责 |
| [docs/architecture.md](architecture.md) | 修改 | 更新精简后的契约和扩展边界 |
| [docs/validation.md](validation.md) | 修改 | 记录四目标构建、测试和待实测范围 |
| [docs/repository-guide.md](repository-guide.md) | 修改 | 移除删除文件，更新用途及行数 |
| [docs/changes-2026-10-15.md](changes-2026-10-15.md) | 修改 | 原始第三批记录标注后续精简及已删除文件 |
| [docs/changes-simplify-2026-10-15.md](changes-simplify-2026-10-15.md) | 新增 | 本轮逐文件修改记录 |

本轮未烧写，未实现单板 LVGL 配网；手机配网与首次配网偶发断连已知问题保持原状。四目标产物、日志和辅助脚本位于忽略的 build-* 目录，不上库。
