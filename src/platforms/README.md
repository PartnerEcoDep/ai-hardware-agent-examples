# 平台适配目录

更新日期：2026-10-15。按软件接口体系组织平台，不按硬件伙伴复制平台实现。

| 职责 | ESP-IDF | 当前 GoldieOS 实现 |
|---|---|---|
| 应用组装 | esp_idf/entry | goldieos/entry |
| 音频设备与任务 | esp_idf/components/media | goldieos/components/media |
| 网络与网络存储 | esp_idf/components/network | goldieos/components/network/{ws63,windows} |
| 云配置 | esp_idf/components/config | goldieos/components/config |
| 显示、输入与 UI 任务 | esp_idf/components/ui | goldieos/components/ui |
| ConvAI HAL | esp_idf/components/convai | goldieos/components/convai/{ws63,windows} |
| 运行时 | 现有 entry 与 SDK 组装，暂无独立模块 | goldieos/components/runtime/{ws63,windows} |
| 构建适配 | esp_idf/cmake/components | goldieos/cmake、各组件的 component.cmake |
| 构建入口 | esp_idf/targets/esp32s3；根目录兼容分发 | goldieos/targets/{ws63,windows} |

components 表示职责模块，不要求各平台使用同一种构建系统。ESP-IDF 的 esp_services 聚合已拆为 esp_media、esp_network、esp_config、esp_sdk；main、ai_*、esp-wifi-connect 保留。各组件公开头文件位于自身 include，SDK 内部头不再向云配置模块暴露。GoldieOS 接口也分组件，由 common.cmake 对直接使用者设置源码级 include，应用事件入口仍位于 goldieos/include。运行包原有单目标及第三方头文件范围暂时保留。

GoldieOS 每个组件拥有 component.cmake，common.cmake 只组装公共应用、服务、板级源文件并调用组件。network、runtime、convai 在构建时选择 ws63/windows 后端；Windows 网络状态已从 runtime 中分离。UI 共用 src/application/ui/ai_lvgl.c 和 web 资源；平台 UI 只负责运行与设备接入。

## WS63 不等于 GoldieOS

当前 GoldieOS 适配来自润和示例，依赖其 AudioService、WifiService、OSAL 等接口，不代表所有 WS63 伙伴采用这些服务。海思官方提供 ws63-liteos-app 构建及应用开发路径，见 [WS63 SDK 指南](https://docs.hisilicon.com/repos/fbb_ws63/zh-CN/master/software/SDK开发环境搭建用户指南/WS63V100%20SDK开发环境搭建%20用户指南.html)。不能据此假定 GoldieOS 是统一伙伴平台。

新增兼容 GoldieOS 服务 ABI 的单板，优先扩展 boards；直接使用海思 SDK/LiteOS 或其他系统的伙伴，应按其实际接口新增后端适配，确认共性后再抽取共享实现。WS63 芯片名仍用于工具链、静态库、构建后端以及现有 ws_* 接口；本轮未批量重命名 ABI 或配置项。

未来 Linux/树莓派及原生应用也按 entry、components 与构建适配的职责划分；只为真实差异创建模块，不建立空平台或复制 GoldieOS 服务接口。硬件引脚及外设布局留在 boards，公共业务与协议留在 src/application/core 与 src/application/services。测试命令未来复用应用命令入口，本轮未实现测试协议、test/release 或阶段 0 的新状态契约。

构建与配置见 [GoldieOS 说明](goldieos/README.md)、[模拟器说明](goldieos/targets/windows/README.md)、[项目 README](../../README.md)。整体规划见 [架构设计](../../docs/architecture.md)。

公共协调由 src/application/core/ai_controller.c 提供；平台 entry 只组装回调、驱动本地队列和任务。UI/按键命令与云事件使用 ai_message_t，配置准备、时间查询和配网能力通过 ai_controller_ops_t 提供。Windows 不注册配网回调。
