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

    // 对于 PromiseType: 恢复协程执行
    virtual void Run() = 0;  // 派生类(例如 PromiseType)必须重载 Run() 方法

    // 设置句柄状态: (UNSCHEDULED SUSPEND SCHEDULED)
    void SetState(State state) { state_ = state; }

    HandleId GetHandleId() { return handle_id_; }

private:
    HandleId handle_id_;  // 句柄 ID

    inline static HandleId handle_id_generation_ = 0;

protected:
    State state_{Handle::UNSCHEDULED};  // 句柄状态 (默认: UNSCHEDULED 未调度)
};

// 句柄信息
struct HandleInfo {
    HandleId id{};     // 句柄 ID
    Handle* handle{};  // 句柄指针(特殊标记 handle == &handle, 指没有对应回调, 协程不需要挂起)
};

// 协程句柄
struct CoroHandle : Handle {
    virtual ~CoroHandle() = default;

    std::string FrameName() const {
        const auto& frame_info = GetFrameInfo();
        return fmt::format("{} at {}:{}", frame_info.function_name(), frame_info.file_name(),
                           frame_info.line());
    }

    // 立即调度
    void Schedule();

    // 取消调度
    void Cancel();

    // TODO: CoroHandle 的 DumpBacktrace() 啥也不做?
    virtual void DumpBacktrace(size_t depth = 0) const {};

private:
    virtual const std::source_location& GetFrameInfo() const;
};

}  // namespace cutecoro
