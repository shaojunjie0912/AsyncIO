#pragma once

#include <cutecoro/concepts/future.hpp>
#include <cutecoro/event_loop.hpp>
#include <cutecoro/scheduled_task.hpp>

namespace cutecoro {

template <concepts::Future Fut>
decltype(auto) run(Fut&& main) {
    auto t = schedule_task(std::forward<Fut>(main));
    get_event_loop().run_until_complete();
    if constexpr (std::is_lvalue_reference_v<Fut&&>) {
        return t.get_result();
    } else {
        return std::move(t).get_result();
    }
}

}  // namespace cutecoro
