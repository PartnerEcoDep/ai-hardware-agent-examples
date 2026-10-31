# 媒体契约归属整改（2026-10-15）

本轮只移动现有声明并收敛依赖，不新增媒体功能，不修改业务实现。不实施单板 LVGL 配网；首次配网偶发断连继续保留为已知问题。

| 文件 | 操作 | 说明 |
|---|---|---|
| [src/application/services/media/include/ai_media.h](../src/application/services/media/include/ai_media.h) | 新增 | 承接既有媒体操作及上行回调声明，无新增接口 |
| [src/application/services/cloud/include/ai_cloud.h](../src/application/services/cloud/include/ai_cloud.h) | 修改 | 引用媒体契约，删除重复归属的定义 |
| [src/platforms/esp_idf/components/media/include/esp_media.h](../src/platforms/esp_idf/components/media/include/esp_media.h) | 修改 | 显式引用应用事件及媒体契约，不再引用云头 |
| [src/platforms/goldieos/components/media/include/ws_media.h](../src/platforms/goldieos/components/media/include/ws_media.h) | 修改 | 仅引用媒体契约 |
| [src/platforms/esp_idf/cmake/components/ai_cloud/CMakeLists.txt](../src/platforms/esp_idf/cmake/components/ai_cloud/CMakeLists.txt) | 修改 | 云组件公开依赖媒体契约所在组件 |
| [src/platforms/esp_idf/cmake/components/esp_media/CMakeLists.txt](../src/platforms/esp_idf/cmake/components/esp_media/CMakeLists.txt) | 修改 | 移除云组件依赖，显式声明应用及媒体依赖 |
| [tests/CMakeLists.txt](../tests/CMakeLists.txt) | 修改 | 云/配置测试声明直接使用的媒体头目录 |
| [tools/check_architecture.py](../tools/check_architecture.py) | 修改 | 防止媒体模块直接引入云或 SDK 头 |
| [README.md](../README.md) | 修改 | 更新媒体契约归属 |
| [docs/architecture.md](architecture.md) | 修改 | 说明当前依赖方向及未改变范围 |
| [docs/validation.md](validation.md) | 修改 | 记录测试、四目标构建及产物哈希 |
| [docs/repository-guide.md](repository-guide.md) | 修改 | 同步文件用途及行数 |
| [docs/changes-media-contract-2026-10-15.md](changes-media-contract-2026-10-15.md) | 新增 | 本轮逐文件变更记录 |

构建输出和辅助脚本仅保存在忽略的 build-* 目录，不上库。本轮未烧写、未运行模拟器验收、未提交代码。构建证据见 [验证记录](validation.md)。
