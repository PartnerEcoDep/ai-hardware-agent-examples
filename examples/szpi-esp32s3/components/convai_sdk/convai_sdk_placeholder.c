/*
 * convai_sdk_placeholder.c
 *
 * ConvAI SDK 组件占位源文件。
 *
 * 真实 SDK 以预编译静态库 (libconvai_sdk.a) 形式通过 IMPORTED 库链接，
 * 本文件仅为让组件拥有一个源文件（避免 ${COMPONENT_LIB} 退化为 INTERFACE
 * 库而无法正确承载 IMPORTED 库的链接传播），不产生任何实际逻辑。
 */

#include "convai_types.h"

/* 对外暴露一个只读函数（非 static，避免 -Werror=unused 误伤），
 * 顺带验证 include 路径确实能解析到 SDK 头文件。 */
int convai_sdk_placeholder_probe(void) {
  return CONVAI_VERSION_MAJOR;
}
