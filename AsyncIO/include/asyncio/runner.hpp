#pragma once

#include <asyncio/detail/concepts/future.hpp>
#include <asyncio/event_loop.hpp>
#include <asyncio/scheduled_task.hpp>

namespace asyncio {

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

}  // namespace asyncio
