#pragma once

struct CountedPolicy {
    bool move_constructable = true;  // 允许移动构造
    bool copy_constructable = true;  // 允许拷贝构造
    bool move_assignable = true;     // 允许移动赋值
    bool copy_assignable = true;     // 允许拷贝赋值
};

inline constexpr CountedPolicy default_counted_policy;

template <CountedPolicy policy = default_counted_policy>
struct Counted {
    /**
     * @brief 重置计数器
     *
     */
    static void reset_count() {
        move_construct_counts_ = 0;
        copy_construct_counts_ = 0;
        default_construct_counts_ = 0;
        copy_assign_counts_ = 0;
        move_assign_counts_ = 0;
        destruction_counts_ = 0;
    }

    Counted() { id_ = default_construct_counts_++; }

    ~Counted() { ++destruction_counts_; }

    Counted(const Counted&)
        requires(policy.copy_constructable)
    {
        ++copy_construct_counts_;
    }

    Counted(Counted&& other)
        requires(policy.move_constructable)
    {
        ++move_construct_counts_;
        other.id_ = -1;
    }

    Counted& operator=(const Counted&)
        requires(policy.copy_assignable)
    {
        ++copy_assign_counts_;
        return *this;
    }

    Counted& operator=(Counted&& other)
        requires(policy.move_assignable)
    {
        ++move_assign_counts_;
        other.id_ = -1;
        return *this;
    }

    static int construct_counts() {
        return move_construct_counts_ + copy_construct_counts_ + default_construct_counts_;
    }

    static int alive_counts() { return construct_counts() - destruction_counts_; }

    int id_;
    inline static int move_construct_counts_ = 0;     // 移动构造计数
    inline static int copy_construct_counts_ = 0;     // 拷贝构造计数
    inline static int copy_assign_counts_ = 0;        // 拷贝赋值计数
    inline static int move_assign_counts_ = 0;        // 移动赋值计数
    inline static int default_construct_counts_ = 0;  // 默认构造计数
    inline static int destruction_counts_ = 0;        // 析构计数
};
