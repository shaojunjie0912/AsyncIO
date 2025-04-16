#include <cutecoro/event_loop.hpp>
#include <cutecoro/handle.hpp>

namespace cutecoro {

const std::source_location& CoroHandle::get_frame_info() const {
    static const std::source_location frame_info = std::source_location::current();
    return frame_info;
}

void CoroHandle::schedule() {
    if (state_ == Handle::UNSCHEDULED) {
        get_event_loop().call_soon(*this);  // *this 是继承自 CoroHandle 的 promise_type
    }
}

void CoroHandle::cancel() {
    if (state_ != Handle::UNSCHEDULED) {
        get_event_loop().cancel_handle(*this);
    }
}

}  // namespace cutecoro