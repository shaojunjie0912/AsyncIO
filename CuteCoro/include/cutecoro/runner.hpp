#pragma once

#include <cutecoro/concepts/future.hpp>
#include <cutecoro/event_loop.hpp>
#include <cutecoro/scheduled_task.hpp>

namespace cutecoro {

template <concepts::Future Fut>
decltype(auto) Run(Fut&& main) {
    // ScheduledTask 构造函数会调用句柄的 Schedule()
    auto t = schedule_task(std::forward<Fut>(main));
    GetEventLoop().RunUntilComplete();
    if constexpr (std::is_lvalue_reference_v<Fut&&>) {
        return t.GetResult();
    } else {
        return std::move(t).GetResult();
    }
}

}  // namespace cutecoro
