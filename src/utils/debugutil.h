/*---------------------------------------------------------*\
||| debugutil.h                                             |
|||                                                         |
|||   Debug utility for conditional logging                |
|||   Checks QSettings to determine if debug mode is on    |
|||                                                         |
|||   This file is part of the LL-Connect 3 project        |
|||   SPDX-License-Identifier: GPL-2.0-or-later            |
\*---------------------------------------------------------*/

#pragma once

#include <cstdio>
#include <cstdarg>

namespace DebugUtil {

// Check if debug mode is enabled (implemented in debugutil.cpp)
bool isDebugEnabled();

// Check if a specific debug category is enabled
bool isDebugCategoryEnabled(const char* category);

// Debug printf (only prints if debug mode is enabled)
template<typename... Args>
inline void debugPrintf(const char* format, Args... args) {
    if (isDebugEnabled()) {
        if constexpr (sizeof...(args) > 0) {
            std::printf(format, args...);
        } else {
            std::fputs(format, stdout);
        }
    }
}

// Category-specific debug printf (for printf-style format strings)
inline void debugPrintfCategory(const char* category, const char* format, ...) {
    if (isDebugCategoryEnabled(category)) {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }
}

} // namespace DebugUtil

// Convenience macros
#define DEBUG_PRINTF(...) DebugUtil::debugPrintf(__VA_ARGS__)
#define DEBUG_PRINTF_CATEGORY(category, ...) DebugUtil::debugPrintfCategory(category, __VA_ARGS__)
