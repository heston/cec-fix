#pragma once

#include <cstdint>
#include "spdlog/spdlog.h"

// Broadcom vc_cec.h callback reasons. main.cpp checks these against the firmware
// headers; keeping this handler independent lets the regression run without a Pi.
namespace cec_notifications {
constexpr uint32_t logical_address = 1u << 6;
constexpr uint32_t topology = 1u << 7;
constexpr uint32_t logical_address_lost = 1u << 15;

inline bool handle(uint32_t reason, uint32_t param1, uint32_t param2) {
    switch (reason & 0xffff) {
    case logical_address:
    case logical_address_lost: {
        const auto physical = fmt::format("{:X}.{:X}.{:X}.{:X}",
            (param2 >> 12) & 0xf, (param2 >> 8) & 0xf,
            (param2 >> 4) & 0xf, param2 & 0xf);
        if ((reason & 0xffff) == logical_address_lost) {
            spdlog::warn("CEC logical address lost: logical={:X}, physical={}; TV control may be unavailable",
                         param1, physical);
        } else if (param1 == 0xf || param2 == 0xffff) {
            spdlog::warn("CEC address unavailable or released: logical={:X}, physical={}", param1, physical);
        } else {
            spdlog::info("CEC address allocated: logical={:X}, physical={}", param1, physical);
            if (param1 == 0 && param2 != 0) {
                spdlog::warn("CEC TV has physical address {}; root TV address is normally 0.0.0.0", physical);
            }
        }
        return true;
    }
    case topology:
        spdlog::debug("CEC topology notification: logical-address mask={:04X}", param1 & 0xffff);
        return true;
    default:
        return false;
    }
}
}
