#include "cec-notifications.hpp"
#include "spdlog/sinks/ostream_sink.h"
#include <cassert>
#include <sstream>
#include <cstdio>

int main() {
    std::ostringstream output;
    auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(output);
    auto logger = std::make_shared<spdlog::logger>("test", sink);
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::debug);

    // Exact startup event reported on hardware; this must bypass frame parsing.
    assert(cec_notifications::handle(0x60040, 0, 0x1200));
    assert(output.str().find("CEC address allocated: logical=0, physical=1.2.0.0") != std::string::npos);
    assert(output.str().find("root TV address") != std::string::npos);
    output.str("");

    assert(cec_notifications::handle(0x60040, 0, 0));
    assert(output.str().find("physical=0.0.0.0") != std::string::npos);
    assert(output.str().find("root TV address") == std::string::npos);
    output.str("");

    assert(cec_notifications::handle(0x60040, 0xf, 0xffff));
    assert(output.str().find("unavailable or released") != std::string::npos);
    assert(output.str().find("allocated") == std::string::npos);
    output.str("");
    assert(cec_notifications::handle(0x60040, 0xf, 0x1200));
    assert(output.str().find("unavailable or released") != std::string::npos);
    output.str("");

    assert(cec_notifications::handle(0x68000, 0, 0x1200));
    assert(output.str().find("logical address lost") != std::string::npos);
    assert(cec_notifications::handle(0x20080, 0x31, 0));
    assert(output.str().find("logical-address mask=0031") != std::string::npos);
    output.str("");

    // Ordinary TX/RX and button events still reach the existing frame parser.
    for (uint32_t reason : {0x20001u, 0x50002u, 0x30004u, 0x20008u, 0x30010u, 0x20020u}) {
        assert(!cec_notifications::handle(reason, 0, 0));
    }
    assert(output.str().empty());
    std::puts("CEC notification regression tests passed");
}
