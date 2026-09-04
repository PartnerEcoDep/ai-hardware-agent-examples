#!/usr/bin/env node
/* Generate custom CJK LVGL fonts for the ESP32 AI chat UI.
 * Reads the symbol set from chat_font_symbols.txt (UTF-8) and invokes
 * lv_font_conv to produce 14px and 16px lvgl .c font files into main/.
 *
 * Auto-installs tools/node_modules (via npm install) when lv_font_conv is
 * missing, so it can be invoked directly from the build (CMake) without a
 * manual `npm install` step.
 */
"use strict";
const fs = require("fs");
const path = require("path");
const { spawnSync } = require("child_process");

const ROOT = path.dirname(__filename);
const symbols = fs.readFileSync(
  path.join(ROOT, "chat_font_symbols.txt"), "utf8").trim();
const fontSrc = "C:\\Windows\\Fonts\\simhei.ttf";
const convJs = path.join(ROOT, "node_modules", "lv_font_conv",
                         "lv_font_conv.js");
const outDir = path.join(ROOT, "..", "main");

/* Auto-install lv_font_conv if node_modules is missing/outdated. */
if (!fs.existsSync(convJs)) {
  console.log("lv_font_conv not found, running `npm install` in tools/ ...");
  const npm = spawnSync(
    process.platform === "win32" ? "npm.cmd" : "npm", ["install"],
    { cwd: ROOT, encoding: "utf8", stdio: "inherit" });
  if (npm.status !== 0) {
    console.error("npm install failed. Please run `cd tools && npm install` manually.");
    process.exit(npm.status || 1);
  }
}

function gen(size, varName, outName) {
  const out = path.join(outDir, outName);
  const args = [
    convJs,
    "--no-compress", "--no-prefilter",
    "--bpp", "4",
    "--size", String(size),
    "--font", fontSrc,
    "-r", "0x20-0x7f",
    "--symbols", symbols,
    "--format", "lvgl",
    "--lv-include", "lvgl.h",
    "--lv-font-name", varName,
    "--force-fast-kern-format",
    "-o", out,
  ];
  const r = spawnSync(process.execPath, args, { encoding: "utf8" });
  if (r.status !== 0) {
    console.error("lv_font_conv failed (" + outName + "):");
    console.error("stdout:", r.stdout);
    console.error("stderr:", r.stderr);
    process.exit(r.status || 1);
  }
  const kb = (fs.statSync(out).size / 1024).toFixed(1);
  console.log("generated " + outName + "  (" + kb + " KB)");
}

gen(16, "lv_font_custom_cjk_16", "lv_font_custom_cjk_16.c");
gen(14, "lv_font_custom_cjk_14", "lv_font_custom_cjk_14.c");