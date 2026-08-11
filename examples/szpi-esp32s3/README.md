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

## SDK selection and source archive

The SDK provider is selected automatically at CMake configure time:

1. If `../../src/CMakeLists.txt` exists, it is built for ESP32-S3 and linked
   into the firmware. The same static target is copied to
   `build/artifacts/esp32-s3/libconvai_sdk.a` for SDK archiving.
2. Otherwise, `../../libs/esp32-s3/libconvai_sdk.a` is linked into the
   firmware. No new SDK archive is exported in this mode.
3. Configuration fails when neither provider is available.

The source package must be usable through `add_subdirectory()` and define a
local static-library target named `convai_sdk`. It can select ESP32-S3 through
`ESP_PLATFORM`, `IDF_TARGET`, or `CONVAI_PLATFORM`. It must also declare the
`idf::` component targets it directly uses. Paths inside the source package
should be based on `CMAKE_CURRENT_LIST_DIR`, not `CMAKE_SOURCE_DIR`, because the
latter points to this example when the SDK is embedded in ESP-IDF.

The component wrapper provides the source package with both `PROJECT_VERSION`
and `CONVAI_PROJECT_VERSION`. The shared `../../cmake/ConvaiVersion.cmake`
combines the numeric release with the Git commit count and short hash. Both the
repository build and this standalone ESP-IDF project therefore provide a value
such as `26.8.0.52-1f7afd5` to SDK source. A source archive without Git metadata
falls back to the numeric release version.

Components using the SDK should add `REQUIRES convai_sdk` to their
`idf_component_register` call and include headers as
`#include "convai/convai_api.h"`.
