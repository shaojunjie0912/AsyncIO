#pragma once

#include <fmt/core.h>

#include <cstdint>
#include <source_location>

namespace cutecoro {

using HandleId = uint64_t;  // 句柄 ID 数据类型

// 句柄基类
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

    virtual void run() = 0;  // 派生类(例如 PromiseType)必须重载 run() 方法

    void set_state(State state) { state_ = state; }

    HandleId get_handle_id() { return handle_id_; }

private:
    HandleId handle_id_;  // 句柄 ID

    inline static HandleId handle_id_generation_ = 0;

protected:
    State state_{Handle::UNSCHEDULED};  // 句柄状态
};

// 句柄信息
struct HandleInfo {
    HandleId id{};     // 句柄 ID
    Handle* handle{};  // 句柄指针(特殊标记 handle == &handle, 指没有对应回调, 协程不需要挂起)
};

// 协程句柄
struct CoroHandle : Handle {
    virtual ~CoroHandle() = default;

    std::string frame_name() const {
        const auto& frame_info = get_frame_info();
        return fmt::format("{} at {}:{}", frame_info.function_name(), frame_info.file_name(),
                           frame_info.line());
    }

    // 立即调度
    void schedule();

    // 取消调度
    void cancel();

    // TODO: CoroHandle 的 dump_backtrace() 啥也不做?
    virtual void dump_backtrace(size_t depth = 0) const {};

private:
    virtual const std::source_location& get_frame_info() const;
};

}  // namespace cutecoro
