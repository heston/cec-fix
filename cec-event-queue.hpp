#ifndef CEC_EVENT_QUEUE_HPP
#define CEC_EVENT_QUEUE_HPP

#include <cstdint>
#include <deque>
#include <mutex>

struct CECEvent {
    uint32_t reason, param1, param2, param3, param4;
};

// Firmware callbacks only enqueue. The main loop releases the lock before doing I/O.
class CECEventQueue {
public:
    void push(const CECEvent& event) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (events_.size() == CAPACITY) {
            ++dropped_;
            return;
        }
        events_.push_back(event);
    }

    bool pop(CECEvent& event, unsigned int& dropped) {
        std::lock_guard<std::mutex> lock(mutex_);
        dropped = dropped_;
        dropped_ = 0;
        if (events_.empty()) return false;
        event = events_.front();
        events_.pop_front();
        return true;
    }

private:
    static const unsigned int CAPACITY = 256;
    std::mutex mutex_;
    std::deque<CECEvent> events_;
    unsigned int dropped_ = 0;
};

#endif
