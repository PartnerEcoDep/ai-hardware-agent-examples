# ESP32-S3 SDK 库目录

请将内部编译好的 ESP32-S3 版 libconvai_sdk.a 放到此目录。

编译指南: docs/esp32_sdk_build_guide.md

编译命令摘要:
  1. 在内部环境安装 ESP32-S3 工具链
  2. 将 include/convai/*.h 和 SDK 源码放到同一目录
  3. 运行 build_esp32s3.sh → 产出 libconvai_sdk.a
  4. 将 libconvai_sdk.a 复制到此目录
