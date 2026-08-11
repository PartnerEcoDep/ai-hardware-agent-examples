# ESP32-S3 minimal example

This is a standalone ESP-IDF project. It is deliberately separate from the
repository's GoldieOS CMake project, so building it does not change the WS63 or
Windows simulator builds.

It targets ESP32-S3 and prints a single message from `app_main`. The local
`convai_sdk` component automatically builds the SDK from `src/CMakeLists.txt`
when source is available, otherwise it falls back to
`libs/esp32-s3/libconvai_sdk.a`. The Hello World component does not call the SDK
yet: a working ConvAI application also needs an ESP-IDF implementation of the
SDK platform HAL.

## Build and flash

Run these commands after installing and exporting ESP-IDF:

```powershell
cd examples/szpi-esp32s3
idf.py build
idf.py -p COMx flash monitor
```

The expected startup log contains:

```text
I (...) convai_esp32s3: Hello world from the ESP32-S3 ConvAI example!
```

To use the SDK in a later component, add `REQUIRES convai_sdk` to that
component's `idf_component_register` call and include headers as
`#include "convai/convai_api.h"`.
