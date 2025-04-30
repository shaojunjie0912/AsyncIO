#include <cutecoro/event_loop.hpp>
#include <cutecoro/handle.hpp>

namespace cutecoro {

const std::source_location& CoroHandle::GetFrameInfo() const {
    static const std::source_location frame_info = std::source_location::current();
    return frame_info;
}

void CoroHandle::Schedule() {
    if (state_ == Handle::UNSCHEDULED) {
        GetEventLoop().CallSoon(*this);  // *this 是继承自 CoroHandle 的 promise_type
    }
}

void CoroHandle::Cancel() {
    if (state_ != Handle::UNSCHEDULED) {
        GetEventLoop().CancelHandle(*this);
    }
}

}  // namespace cutecoro
