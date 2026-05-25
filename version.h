#pragma once

// ============================================================
//  统一版本管理 — 只需修改这里
// ============================================================
#define VERSION_MAJOR      1
#define VERSION_MINOR      0
#define VERSION_PATCH      0
#define VERSION_BUILD      1

// 公司 / 产品信息
#define VER_COMPANY_NAME    "Qizhiwoniu"
#define VER_PRODUCT_NAME    "YGmax"
#define VER_FILE_DESC       "2D/3D"
#define VER_COPYRIGHT       "Copyright © 2026 Qizhiwoniu"
#define VER_ORIGINAL_NAME   "YGmax.exe"

// ============================================================
//  自动拼接，不要手动改下面这些
// ============================================================
#define _STRINGIZE(x)       #x
#define STRINGIZE(x)        _STRINGIZE(x)

// "1,0,0,0" 格式（给 RC 文件用）
#define VERSION_COMMA \
    VERSION_MAJOR,VERSION_MINOR,VERSION_PATCH,VERSION_BUILD

// "1.0.0.0" 字符串（给 UI / About 对话框用）
#define VERSION_STR \
    STRINGIZE(VERSION_MAJOR) "." \
    STRINGIZE(VERSION_MINOR) "." \
    STRINGIZE(VERSION_PATCH) "." \
    STRINGIZE(VERSION_BUILD)
