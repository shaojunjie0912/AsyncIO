#pragma once

#include <fmt/core.h>

#include <cstdint>
#include <source_location>

namespace cutecoro {

using HandleId = uint64_t;  // 句柄 ID 数据类型

// 句柄封装基类
// TODO: event_loop 类型擦除?
struct Handle {
    // 句柄状态
    enum State : uint8_t {
        UNSCHEDULED,  // 暂未调度
        SUSPEND,      // 挂起
        SCHEDULED,    // 调度中
    };

    Handle() noexcept : handle_id_(handle_id_generation_++) {}

    virtual ~Handle() = default;

    virtual void run() = 0;

    void set_state(State state) { state_ = state; }

    HandleId get_handle_id() { return handle_id_; }

private:
    HandleId handle_id_;  // 句柄 ID

    inline static HandleId handle_id_generation_ = 0;

protected:
    State state_{Handle::UNSCHEDULED};  // 句柄状态
};

// 句柄信息封装类
struct HandleInfo {
    // NOTE: 额外使用递增的句柄 ID 来跟踪句柄的生命周期
    // 而不是只用原始指针, 因为一个刚销毁的协程地址可能跟一个新创建好的协程地址相同
    HandleId id{};     // 句柄 ID
    Handle* handle{};  // 句柄指针(特殊标记当 handle == &handle 时, 指事件被触发且没有回调执行)
};

// 这就是为了给协程调用链追踪用的?
// 协程句柄封装类
struct CoroHandle : Handle {
    std::string frame_name() const {
        const auto& frame_info = get_frame_info();
        return fmt::format("{} at {}:{}", frame_info.function_name(), frame_info.file_name(),
                           frame_info.line());
    }

    virtual void dump_backtrace(size_t depth = 0) const;

    void schedule();

    // TODO: event_loop::cancel_handle()?
    void cancel();

private:
    virtual const std::source_location& get_frame_info() const;
};

}  // namespace cutecoro
