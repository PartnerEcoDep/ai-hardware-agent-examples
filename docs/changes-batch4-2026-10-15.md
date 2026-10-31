# 第四批整改文件变更清单（2026-10-15）

**状态：第四批代码已按用户要求整体回退（2026-10-15）。以下表格保留为实施历史，不是当前源码清单。**

本轮补记第三批润和验收，保留首次配网偶发断连为已知问题；第四批仅统一应用消息邮箱，不修改网络或媒体实现。以下为本轮实际文件修改，不包含前几批尚未提交的其他差异。

| 文件 | 操作 | 说明 |
|---|---|---|
| `src/application/core/include/ai_mailbox.h`（已删除） | 新增 | 公共有界邮箱、结果枚举与锁/上下文契约 |
| `src/application/core/ai_mailbox.c`（已删除） | 新增 | 拷贝消息、FIFO 环回、溢出整批丢弃和恢复 |
| [src/platforms/esp_idf/entry/app_main.c](../src/platforms/esp_idf/entry/app_main.c) | 修改 | 公共邮箱替换原队列，临界区保护、二值信号量唤醒 |
| [src/platforms/goldieos/entry/app.c](../src/platforms/goldieos/entry/app.c) | 修改 | 公共邮箱替换私有环形队列，保留原互斥锁和轮询 |
| [src/platforms/esp_idf/cmake/components/ai_core/CMakeLists.txt](../src/platforms/esp_idf/cmake/components/ai_core/CMakeLists.txt) | 修改 | 注册公共邮箱源码 |
| [src/platforms/goldieos/cmake/common.cmake](../src/platforms/goldieos/cmake/common.cmake) | 修改 | 将公共邮箱接入 GoldieOS 两个目标 |
| [src/platforms/goldieos/cmake/ws63-core/CMakeLists.txt](../src/platforms/goldieos/cmake/ws63-core/CMakeLists.txt) | 修改 | 可选公共静态库同步注册邮箱源码 |
| `tests/test_mailbox.c`（已删除） | 新增 | FIFO 环回、容量、拷贝隔离、溢出和恢复测试 |
| [tests/CMakeLists.txt](../tests/CMakeLists.txt) | 修改 | 注册 application_mailbox 测试 |
| [README.md](../README.md) | 修改 | 更新公共 app 职责 |
| [docs/architecture.md](architecture.md) | 修改 | 说明邮箱契约、两后端同步和未来扩展边界 |
| [docs/validation.md](validation.md) | 修改 | 第三批验收/已知问题及第四批构建测试证据 |
| [docs/repository-guide.md](repository-guide.md) | 修改 | 验收状态、文件用途、清单与行数 |
| [docs/changes-batch4-2026-10-15.md](changes-batch4-2026-10-15.md) | 新增 | 本轮逐文件变更说明 |

第三批润和镜像保存在 build-hoperun-app/accepted-network-media/unified_ai_hoperun.fwpkg；仅本地回退使用，保留其已知配网问题。四目标的新产物和日志保存在原 build-* 目录并被忽略；本轮不烧写、不启动模拟器验收、不提交代码。

完整上库清单及行数见 [上库清单](repository-guide.md)，产物哈希、构建与测试结果见 [验证记录](validation.md)。

## 回退轮文件变更

- 删除 src/application/core/ai_mailbox.c、src/application/core/include/ai_mailbox.h、tests/test_mailbox.c。
- 恢复 src/platforms/esp_idf/entry/app_main.c、src/platforms/goldieos/entry/app.c 的第三批队列实现。
- 撤回 src/platforms/esp_idf/cmake/components/ai_core/CMakeLists.txt、src/platforms/goldieos/cmake/common.cmake、src/platforms/goldieos/cmake/ws63-core/CMakeLists.txt 和 tests/CMakeLists.txt 中的邮箱源码/测试注册。
- 更新 README.md、docs/architecture.md、docs/validation.md、docs/repository-guide.md 和本文件，标明回退状态并保留第三批验收、已知问题与第四批历史。
- 本轮未烧写、未修改第三批实现；四目标回退构建结果见验证记录。
