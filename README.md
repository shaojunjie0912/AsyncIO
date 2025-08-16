# AsyncIO

一个基于 C++20 协程的高性能异步编程库，提供简洁易用的协程任务调度、网络 I/O 和并发控制功能。

## ✨ 特性

- 🚀 **现代 C++20 协程支持** - 基于 std::coroutine 实现的高性能异步编程
- 🔄 **事件驱动架构** - 高效的事件循环和任务调度机制，基于 epoll I/O 多路复用
- 🌐 **异步网络编程** - 支持 TCP 客户端/服务端的异步操作，非阻塞 I/O
- ⏰ **时间控制** - 异步睡眠和超时等待功能
- 🎯 **并发执行** - Gather 模式支持多任务并发执行和结果收集
- 🔍 **调试支持** - 协程调用栈跟踪和错误处理
- 🛡️ **资源管理** - RAII 和 Finally 机制确保资源安全释放
- 📦 **轻量级设计** - 最小化依赖，易于集成
- 🔧 **类型安全** - C++20 概念约束，编译期类型检查
- ⚡ **零拷贝设计** - 移动语义和完美转发优化性能

## 🏗️ 架构概览

```
AsyncIO 架构
├── Task<T>              # 协程任务封装，支持返回值类型
├── EventLoop            # 事件循环和调度器 (单例模式)
│   ├── EpollSelector   # Linux epoll I/O 多路复用
│   ├── TimerHandle     # 定时器管理 (最小堆)
│   └── HandleQueue     # 就绪任务队列
├── Stream              # 异步网络流 (TCP)
├── Handle              # 协程句柄管理基类
│   ├── CoroHandle     # 协程特化句柄
│   └── PromiseType    # 协程 Promise 类型
├── Result<T>           # 结果封装 (值/异常)
├── Gather              # 并发任务收集器
├── WaitFor             # 超时等待机制
├── ScheduledTask       # 调度任务包装器
├── Finally             # 资源清理机制 (RAII)
└── Runner              # 任务运行器
```

## 🚀 快速开始

### 安装

```bash
# 克隆项目
git clone https://github.com/your-repo/AsyncIO.git
cd AsyncIO

# 安装依赖
xmake require

# 构建项目
xmake

# 运行示例
xmake run hello_world
```

### Hello World 示例

```cpp
#include <asyncio/asyncio.hpp>
#include <fmt/core.h>

using asyncio::Task;
using namespace std::chrono_literals;

Task<std::string> hello() {
    co_await asyncio::Sleep(1s);  // 异步等待 1 秒
    co_return "Hello";
}

Task<std::string> world() {
    co_await asyncio::Sleep(1s);  // 异步等待 1 秒
    co_return "World";
}

Task<> main_coro() {
    // 并发执行两个任务 (总耗时约 1 秒而非 2 秒)
    auto [h, w] = co_await asyncio::Gather(hello(), world());
    fmt::println("{} {}!", h, w);
}

int main() {
    asyncio::Run(main_coro());  // 运行协程直到完成
    return 0;
}
```

## 📚 核心 API 详解

### Task<T> - 协程任务

```cpp
// 创建返回值任务
Task<int> compute() {
    co_return 42;
}

// 创建无返回值任务
Task<> process() {
    co_await compute();
}

// 运行任务
int result = asyncio::Run(compute());

// 检查任务状态
Task<int> task = compute();
bool valid = task.IsValid();     // 任务是否有效
bool done = task.IsDone();       // 任务是否完成

// 获取结果 (左值/右值重载)
int result1 = task.GetResult();      // 拷贝结果
int result2 = std::move(task).GetResult();  // 移动结果
```

### PromiseType - 协程 Promise

```cpp
// 支持源码位置跟踪
Task<int> traced_function() {
    // Promise 自动捕获调用位置 (std::source_location)
    co_return 42;
}

// 支持立即执行 (不挂起)
Task<int> immediate_task() {
    co_return asyncio::no_wait_at_initial_suspend, 42;
}
```

### Sleep - 异步延时

```cpp
using namespace std::chrono_literals;

Task<> delayed_task() {
    co_await asyncio::Sleep(100ms);   // 毫秒
    co_await asyncio::Sleep(1s);      // 秒
    co_await asyncio::Sleep(1min);    // 分钟
    fmt::println("任务完成");
}
```

### Gather - 并发执行

```cpp
Task<> concurrent_tasks() {
    // 并发执行多个任务
    auto [result1, result2, result3] = co_await asyncio::Gather(
        task1(),     // Task<std::string>
        task2(),     // Task<int>
        task3()      // Task<>
    );
    
    // result1: std::string
    // result2: int  
    // result3: asyncio::detail::VoidValue (void 的占位符)
    
    // 处理结果...
}

// 异常处理：任一任务失败将终止所有任务
Task<> error_handling() {
    try {
        auto results = co_await asyncio::Gather(
            might_fail_task(),
            normal_task()
        );
    } catch (const std::exception& e) {
        fmt::println("某个任务失败: {}", e.what());
    }
}
```

### WaitFor - 超时等待

```cpp
Task<> timeout_example() {
    using namespace std::chrono_literals;
    
    try {
        // 最多等待 5 秒
        auto result = co_await asyncio::WaitFor(slow_task(), 5s);
        fmt::println("任务完成: {}", result);
    } catch (const asyncio::TimeoutError&) {
        fmt::println("任务超时!");
    }
}

// 内部实现：同时启动目标任务和超时任务
Task<> wait_for_implementation() {
    // WaitFor 内部创建两个并发任务：
    // 1. 目标任务
    // 2. 超时检测任务
    // 谁先完成就使用谁的结果
}
```

### Result<T> - 结果封装

```cpp
// Result 支持值和异常的统一处理
asyncio::Result<int> result;

// 设置值
result.SetValue(42);

// 设置异常
try {
    throw std::runtime_error("error");
} catch (...) {
    result.SetException(std::current_exception());
}

// 检查和获取结果
if (result.HasValue()) {
    int value = result.GetResult();  // 可能抛出异常
}
```

## 🌐 网络编程详解

### TCP 服务器

```cpp
#include <asyncio/asyncio.hpp>

using namespace asyncio;

// 客户端连接处理器
Task<> handle_client(Stream stream) {
    auto sockinfo = stream.GetSockInfo();
    char addr[INET6_ADDRSTRLEN]{};
    auto sa = reinterpret_cast<const sockaddr*>(&sockinfo);
    
    fmt::println("新连接来自: {}:{}", 
                inet_ntop(sockinfo.ss_family, GetInAddr(sa), addr, sizeof addr),
                GetInPort(sa));
    
    try {
        while (true) {
            // 异步读取数据
            auto data = co_await stream.Read(1024);
            if (data.empty()) {
                fmt::println("客户端断开连接");
                break;
            }
            
            fmt::println("收到数据: {}", data.data());
            
            // 异步发送响应
            co_await stream.Write(data);  // 回显
        }
    } catch (const std::exception& e) {
        fmt::println("处理客户端时出错: {}", e.what());
    }
    
    stream.Close();  // 关闭连接
}

Task<> echo_server() {
    // 启动服务器，绑定到指定地址和端口
    auto server = co_await StartServer(handle_client, "127.0.0.1", 8080);
    
    fmt::println("Echo 服务器启动在 127.0.0.1:8080");
    fmt::println("等待客户端连接...");
    
    // 永久运行服务器
    co_await server.ServeForever();
}

int main() {
    try {
        Run(echo_server());
    } catch (const std::exception& e) {
        fmt::println("服务器错误: {}", e.what());
    }
    return 0;
}
```

### TCP 客户端

```cpp
Task<> tcp_client() {
    try {
        // 异步连接到服务器
        auto stream = co_await OpenConnection("127.0.0.1", 8080);
        fmt::println("连接到服务器成功");
        
        // 发送数据
        std::string message = "Hello Server!";
        Stream::Buffer buffer(message.begin(), message.end());
        co_await stream.Write(buffer);
        fmt::println("发送数据: {}", message);
        
        // 接收响应 (带超时)
        auto response = co_await WaitFor(stream.Read(1024), 5s);
        fmt::println("收到响应: {}", response.data());
        
        // 优雅关闭连接
        stream.Close();
        fmt::println("连接已关闭");
        
    } catch (const asyncio::TimeoutError&) {
        fmt::println("操作超时");
    } catch (const std::exception& e) {
        fmt::println("客户端错误: {}", e.what());
    }
}
```

### 高级网络示例

```cpp
// 多客户端并发处理
Task<> multi_client_server() {
    auto server = co_await StartServer([](Stream stream) -> Task<> {
        // 每个客户端连接都在独立的协程中处理
        // 支持大量并发连接而不会阻塞其他连接
        co_await handle_client_async(std::move(stream));
    }, "0.0.0.0", 8080);
    
    co_await server.ServeForever();
}

// 客户端连接池
Task<> connection_pool_example() {
    std::vector<Task<Stream>> connections;
    
    // 并发建立多个连接
    for (int i = 0; i < 10; ++i) {
        connections.emplace_back(OpenConnection("127.0.0.1", 8080));
    }
    
    // 等待所有连接建立
    auto streams = co_await Gather(std::move(connections)...);
    
    // 使用连接池...
}
```

## 🔧 高级特性

### 调用栈跟踪

```cpp
Task<int> factorial(int n) {
    if (n <= 1) {
        // 输出当前协程调用栈
        co_await DumpCallstack();
        co_return 1;
    }
    co_return (co_await factorial(n - 1)) * n;
}

// 输出示例:
// [0] factorial at factorial.cpp:123
// [1] factorial at factorial.cpp:125  
// [2] factorial at factorial.cpp:125
// [3] main_coro at main.cpp:45
```

### 异常处理

```cpp
Task<> comprehensive_error_handling() {
    try {
        co_await risky_operation();
    } catch (const asyncio::TimeoutError& e) {
        fmt::println("操作超时: {}", e.what());
    } catch (const asyncio::InvalidFuture& e) {
        fmt::println("无效的 Future: {}", e.what());
    } catch (const asyncio::NoResultError& e) {
        fmt::println("结果未设置: {}", e.what());
    } catch (const std::system_error& e) {
        fmt::println("系统错误: {} ({})", e.what(), e.code().value());
    } catch (const std::exception& e) {
        fmt::println("未知错误: {}", e.what());
    }
}
```

### 资源管理 (Finally)

```cpp
#include <asyncio/finally.hpp>

Task<> resource_management_example() {
    int fd = open("file.txt", O_RDONLY);
    
    // 确保文件描述符被关闭
    finally {
        if (fd >= 0) {
            close(fd);
            fmt::println("文件已关闭");
        }
    };
    
    // 或者使用函数对象
    auto cleanup = []() { fmt::println("清理完成"); };
    finally2(cleanup);
    
    // 无论如何退出，finally 代码都会执行
    if (some_condition) {
        co_return;  // finally 代码仍会执行
    }
    
    co_await some_async_operation();  // 即使抛出异常，finally 代码也会执行
}
```

### 事件循环控制

```cpp
Task<> event_loop_control() {
    auto& loop = asyncio::GetEventLoop();
    
    // 获取当前时间 (相对于事件循环启动时间)
    auto current_time = loop.time();
    
    // 延迟调度任务
    asyncio::Handle custom_handle;
    loop.CallLater(std::chrono::seconds(5), custom_handle);
    
    // 立即调度任务
    loop.CallSoon(custom_handle);
    
    // 取消已调度的任务
    loop.CancelHandle(custom_handle);
}
```

### 自定义 Awaitable

```cpp
// 实现自定义可等待对象
struct CustomAwaiter {
    bool await_ready() const noexcept { 
        return false;  // 总是挂起
    }
    
    void await_suspend(std::coroutine_handle<> handle) const noexcept {
        // 自定义挂起逻辑
        // 例如：添加到自定义队列、设置定时器等
    }
    
    int await_resume() const noexcept {
        return 42;  // 返回结果
    }
};

Task<> use_custom_awaiter() {
    int result = co_await CustomAwaiter{};
    fmt::println("自定义 awaiter 结果: {}", result);
}
```

## 🏗️ 项目结构详解

```
AsyncIO/
├── AsyncIO/                    # 核心库
│   ├── include/asyncio/        # 公共头文件目录
│   │   ├── asyncio.hpp        # 主头文件 (包含所有功能)
│   │   ├── task.hpp            # Task 和 PromiseType 实现
│   │   ├── event_loop.hpp      # 事件循环和调度器
│   │   ├── stream.hpp          # 异步网络流实现
│   │   ├── gather.hpp          # 并发任务收集器
│   │   ├── sleep.hpp           # 异步延时实现
│   │   ├── wait_for.hpp        # 超时等待机制
│   │   ├── result.hpp          # 结果封装类
│   │   ├── handle.hpp          # 协程句柄基类
│   │   ├── scheduled_task.hpp  # 调度任务包装
│   │   ├── runner.hpp          # 任务运行器
│   │   ├── open_connection.hpp # TCP 连接建立
│   │   ├── start_server.hpp    # TCP 服务器启动
│   │   ├── callstack.hpp       # 调用栈跟踪
│   │   ├── finally.hpp         # 资源清理机制
│   │   ├── exception.hpp       # 异常类型定义
│   │   └── detail/             # 实现细节
│   │       ├── concepts/       # C++20 概念定义
│   │       │   ├── awaitable.hpp   # Awaitable 概念
│   │       │   ├── future.hpp      # Future 概念
│   │       │   └── promise.hpp     # Promise 概念
│   │       ├── selector/       # I/O 多路复用
│   │       │   ├── selector.hpp    # 选择器接口
│   │       │   ├── epoll_selector.hpp  # epoll 实现
│   │       │   └── event.hpp       # 事件定义
│   │       ├── noncopyable.hpp # 禁用拷贝工具类
│   │       └── void_value.hpp  # void 类型占位符
│   ├── src/                    # 源代码实现
│   │   ├── event_loop.cpp      # 事件循环实现
│   │   ├── stream.cpp          # 网络流实现
│   │   ├── handle.cpp          # 句柄管理实现
│   │   └── open_connection.cpp # 连接建立实现
│   └── xmake.lua              # 库构建配置
├── tests/                      # 测试目录
│   ├── ut/                     # 单元测试
│   │   ├── test_task.cpp       # Task 功能测试
│   │   ├── test_result.cpp     # Result 功能测试
│   │   ├── test_counted.cpp    # 计数器测试工具
│   │   ├── counted.hpp         # 测试用计数类
│   │   └── xmake.lua          # 测试构建配置
│   ├── st/                     # 示例测试
│   │   ├── hello_world.cpp     # 基础示例
│   │   ├── echo_server.cpp     # Echo 服务器示例
│   │   ├── echo_client.cpp     # Echo 客户端示例
│   │   ├── dump_callstack.cpp  # 调用栈示例
│   │   └── xmake.lua          # 示例构建配置
│   ├── misc/                   # 其他测试
│   │   └── test_catch2.cpp     # Catch2 框架测试
│   ├── pt/                     # 性能测试 (当前为空)
│   └── xmake.lua              # 测试总配置
├── build/                      # 构建输出目录
├── .xmake/                     # XMake 缓存目录
├── .cache/                     # 编译缓存
├── xmake.lua                  # 项目主构建配置
├── compile_commands.json      # 编译命令数据库 (LSP 支持)
├── .clang-format              # 代码格式配置
├── .gitignore                 # Git 忽略配置
├── LICENSE                    # MIT 许可证
└── README.md                  # 项目文档
```

## 🎯 使用场景详解

### 异步网络编程
- **高并发 Web 服务器** - 处理大量客户端连接
- **TCP/UDP 服务器和客户端** - 游戏服务器、聊天应用
- **网络代理和负载均衡器** - 中间件和基础设施
- **实时通信系统** - WebSocket、长连接服务

### 并发任务处理  
- **并行计算** - CPU 密集型任务的协程调度
- **批处理系统** - 大量任务的并发执行
- **工作流引擎** - 复杂业务流程的协程化
- **数据管道** - ETL 过程的异步处理

### 定时任务
- **定时器和调度器** - 延时执行和周期性任务
- **超时控制** - 网络请求、数据库操作的超时管理
- **限流和背压** - 流量控制和系统保护

### 微服务架构
- **异步 API 服务** - RESTful 和 gRPC 服务
- **消息队列处理** - RabbitMQ、Kafka 消费者
- **服务网格组件** - 代理、路由、监控

### 游戏开发
- **游戏服务器** - 低延迟网络通信
- **状态机** - 游戏逻辑的协程化实现
- **资源加载** - 异步资源管理

### 系统编程
- **异步 I/O 框架** - 文件系统、网络的异步操作
- **事件驱动系统** - GUI 应用、嵌入式系统

## 🔄 性能特点详解

### 内存效率
- **零拷贝设计** - 移动语义和完美转发
- **RAII 资源管理** - 自动内存管理，无内存泄漏
- **对象池复用** - 减少频繁分配和释放
- **紧凑内存布局** - 缓存友好的数据结构

### 执行效率
- **事件驱动** - epoll I/O 多路复用，O(1) 事件通知
- **轻量级协程** - 相比线程更低的上下文切换开销
- **编译期优化** - C++20 概念和模板元编程
- **分支预测优化** - `[[likely]]` 和 `[[unlikely]]` 属性

### 可扩展性
- **单线程异步模型** - 避免锁竞争和线程同步开销
- **支持大量并发连接** - 每个连接只需要少量内存
- **弹性调度** - 根据负载动态调整任务执行
- **组件化设计** - 模块间低耦合，易于扩展

### 性能基准 (理论值)
- **协程创建开销** - 纳秒级别，比线程快 1000 倍
- **内存占用** - 每个协程约 1KB，比线程少 1000 倍
- **并发连接数** - 支持 10K+ 并发连接
- **延迟** - 微秒级别的任务切换延迟

## 📖 API 参考手册

### 核心类型

#### Task<T>
```cpp
template<typename T = void>
class Task {
public:
    using promise_type = PromiseType<T>;
    
    // 构造和析构
    explicit Task(std::coroutine_handle<promise_type> h) noexcept;
    Task(Task&& other) noexcept;
    ~Task();
    
    // 结果获取
    decltype(auto) GetResult() &;
    decltype(auto) GetResult() &&;
    
    // 状态查询
    bool IsValid() const;
    bool IsDone() const;
    
    // 协程接口
    auto operator co_await() & noexcept;
    auto operator co_await() && noexcept;
};
```

#### EventLoop
```cpp
class EventLoop {
public:
    EventLoop();
    
    // 时间管理
    MSDuration time();
    
    // 任务调度
    void CallSoon(Handle& handle);
    template<typename Rep, typename Period>
    void CallLater(std::chrono::duration<Rep, Period> delay, Handle& callback);
    void CancelHandle(Handle& handle);
    
    // 事件等待
    template<typename Promise>
    auto WaitEvent(const Event& event);
    
    // 运行控制
    void RunUntilComplete();
    void RunOnce();
    
private:
    bool IsStop() const;
    void CleanupDelayedCall();
};

// 全局事件循环访问
EventLoop& GetEventLoop();
```

#### Stream
```cpp
class Stream {
public:
    using Buffer = std::vector<char>;
    
    // 构造函数
    Stream(int fd);
    Stream(int fd, const sockaddr_storage& sockinfo);
    Stream(Stream&& other);
    
    // 异步 I/O
    Task<Buffer> Read(size_t max_bytes);
    Task<> Write(const Buffer& data);
    
    // 连接管理
    void Close();
    const sockaddr_storage& GetSockInfo() const;
    
    // 状态查询
    bool IsValid() const;
};
```

#### Result<T>
```cpp
template<typename T>
class Result {
public:
    // 状态查询
    constexpr bool HasValue() const noexcept;
    
    // 值设置
    template<typename R>
    constexpr void SetValue(R&& value) noexcept;
    void SetException(std::exception_ptr exception) noexcept;
    
    // 值获取
    constexpr T GetResult() &;
    constexpr T GetResult() &&;
    
    // Promise 接口
    template<typename R>
    constexpr void return_value(R&& value) noexcept;
    void return_void() noexcept;  // void 特化
    void unhandled_exception() noexcept;
};
```

### 主要函数

#### 任务运行
```cpp
// 运行协程任务到完成
template<concepts::Future Fut>
decltype(auto) Run(Fut&& main);

// 调度任务 (不立即运行)
template<concepts::Future Fut>
ScheduledTask<Fut> schedule_task(Fut&& fut);
```

#### 时间控制
```cpp
// 异步延时等待
template<typename Rep, typename Period>
Task<> Sleep(std::chrono::duration<Rep, Period> delay);

// 超时等待
template<concepts::Awaitable Fut, typename Duration>
Task<AwaitResult<Fut>> WaitFor(Fut&& fut, Duration timeout);
```

#### 并发控制
```cpp
// 并发执行多个任务
template<concepts::Awaitable... Futs>
Task<std::tuple<AwaitResult<Futs>...>> Gather(Futs&&... futs);
```

#### 网络操作
```cpp
// 创建 TCP 连接
Task<Stream> OpenConnection(std::string_view ip, uint16_t port);

// 启动 TCP 服务器
template<concepts::ConnectCb CONNECT_CB>
Task<Server<CONNECT_CB>> StartServer(CONNECT_CB cb, 
                                   std::string_view ip, 
                                   uint16_t port);
```

#### 调试支持
```cpp
// 输出协程调用栈
auto DumpCallstack() -> detail::CallStackAwaiter;
```

#### 资源管理
```cpp
// RAII 资源清理
#define finally /* lambda 表达式 */
#define finally2(func) /* 已有清理函数 */

template<class F>
FinalAction<F> _finally(F&& f) noexcept;
```

### 异常类型

```cpp
namespace asyncio {
    // 操作超时异常
    struct TimeoutError : std::exception {
        const char* what() const noexcept override;
    };
    
    // 无效 Future 异常
    struct InvalidFuture : std::exception {
        const char* what() const noexcept override;
    };
    
    // 结果未设置异常
    struct NoResultError : std::exception {
        const char* what() const noexcept override;
    };
}
```

### C++20 概念约束

```cpp
namespace asyncio::concepts {
    // 可等待对象概念
    template<typename A>
    concept Awaitable = /* ... */;
    
    // Future 对象概念
    template<typename F>
    concept Future = /* ... */;
    
    // Promise 对象概念
    template<typename P>
    concept Promise = /* ... */;
    
    // 连接回调概念
    template<typename CONNECT_CB>
    concept ConnectCb = /* ... */;
}
```

## 🛠️ 设计模式和最佳实践

### RAII 资源管理
```cpp
Task<> file_operation() {
    int fd = open("file.txt", O_RDONLY);
    finally { close(fd); };  // 自动清理
    
    co_await process_file(fd);
    // fd 自动关闭，即使发生异常
}
```

### 异常安全
```cpp
Task<> exception_safe_operation() {
    try {
        co_await risky_operation();
    } catch (...) {
        // 确保资源被正确清理
        cleanup_resources();
        throw;  // 重新抛出异常
    }
}
```

### 性能优化
```cpp
// 使用移动语义避免拷贝
Task<std::string> optimized_operation() {
    std::string result = co_await expensive_computation();
    co_return std::move(result);  // 移动而非拷贝
}

// 使用右值重载
Task<> performance_critical() {
    auto task = create_task();
    auto result = std::move(task).GetResult();  // 调用右值重载
}
```

### 错误处理策略
```cpp
// 分层错误处理
Task<> layered_error_handling() {
    try {
        co_await business_logic();
    } catch (const BusinessError& e) {
        // 业务逻辑错误
        handle_business_error(e);
    } catch (const NetworkError& e) {
        // 网络错误
        handle_network_error(e);
    } catch (const SystemError& e) {
        // 系统错误
        handle_system_error(e);
    }
}
```
