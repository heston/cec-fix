#include "cec-event-queue.hpp"
#include <cassert>
#include <thread>
#include <cstdio>

int main() {
    CECEventQueue queue;
    CECEvent event = {};
    unsigned int dropped = 99;
    assert(!queue.pop(event, dropped));
    assert(dropped == 0);
    // A stalled consumer must not prevent the callback from enqueueing or boundlessly grow memory.
    std::thread producer([&] {
        for (unsigned int i = 0; i < 300; ++i) queue.push({i, i + 1, 2, 3, 4});
    });
    producer.join();
    assert(queue.pop(event, dropped));
    assert(dropped == 44 && event.reason == 0 && event.param1 == 1);
    for (unsigned int i = 1; i < 256; ++i) {
        assert(queue.pop(event, dropped));
        assert(event.reason == i && event.param1 == i + 1 && dropped == 0);
    }
    assert(!queue.pop(event, dropped));
    queue.push({400, 0, 0, 0, 0});
    assert(queue.pop(event, dropped) && event.reason == 400);

    std::thread first([&] { for (unsigned int i = 0; i < 100; ++i) queue.push({i, 1, 0, 0, 0}); });
    std::thread second([&] { for (unsigned int i = 0; i < 100; ++i) queue.push({i, 2, 0, 0, 0}); });
    unsigned int next[3] = {};
    for (int i = 0; i < 200;) {
        if (queue.pop(event, dropped)) {
            assert(event.reason == next[event.param1]++ && dropped == 0);
            ++i;
        } else {
            std::this_thread::yield();
        }
    }
    first.join(); second.join();
    assert(next[1] == 100 && next[2] == 100);
    std::puts("CEC queue regression tests passed");
}
