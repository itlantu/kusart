#ifndef KUSART_RINGBUFFER_H
#define KUSART_RINGBUFFER_H

#include <array>
#include <atomic>
#include <cstdint>
#include <span>

namespace kusart {

using kRingBufferSizeType = uint16_t;

static_assert(std::atomic<kRingBufferSizeType>::is_always_lock_free,
              "kRingBufferSizeType 必须支持无锁原子访问, 否则会引入运行期锁或依赖libatomic");

/**
 * @brief 环形缓冲区，实际可缓存usable_capacity()个字节
 * @note 读写索引是atomic成员，因此本类不可拷贝、不可移动
 */
class RingBuffer {
public:
    /**
     * @brief 在调用方提供的存储上构造
     * @param storage 存储视图，其长度即容量，必须不小于 2
     */
    explicit constexpr RingBuffer(std::span<uint8_t> storage) noexcept;

    /**
     * @brief 判断构造参数是否可用
     * @return 容量不小于 2 时返回 true
     */
    [[nodiscard]] constexpr bool is_config_valid() const noexcept;

    /**
     * @brief 获取缓冲区容量
     * @return 容量，等于构造时传入的存储长度
     */
    [[nodiscard]] constexpr kRingBufferSizeType capacity() const noexcept;

    /**
     * @brief 获取实际可缓存的字节数
     * @return 可用容量，等于 capacity() - 1
     */
    [[nodiscard]] constexpr kRingBufferSizeType usable_capacity() const noexcept;

    /**
     * @brief 获取当前已缓存的字节数
     * @return 已缓存字节数，取值范围为闭区间的[0, usable_capacity()]
     */
    [[nodiscard]] kRingBufferSizeType count() const noexcept;

    /**
     * @brief 获取当前仍可写入的字节数
     * @return 剩余可写字节数；它与count()之和应为usable_capacity()
     */
    [[nodiscard]] kRingBufferSizeType free() const noexcept;

    /**
     * @brief 清空缓冲区，把读写索引一并复位到起点
     * @note 会同时写两个索引，只能在 push/pop 均已停止时调用
     */
    void clear() noexcept;

    /**
     * @brief 向缓冲区尾部写入一个字节
     * @param ch 待写入的字节
     * @return 写入成功返回true；缓冲区已满返回false
     */
    bool push(uint8_t ch) noexcept;

    /**
     * @brief 从缓冲区头部取出一个字节
     * @param ch 用于接收取出字节的引用，仅在返回true 时被修改
     * @return 取出成功返回true；缓冲区为空返回false
     */
    bool pop(uint8_t& ch) noexcept;

private:
    /// 调用方提供的存储首地址
    uint8_t* storage_;
    /// 容量，等于存储长度
    kRingBufferSizeType capacity_;
    /// 读端索引
    std::atomic<kRingBufferSizeType> read_index_{0};
    /// 写端索引
    std::atomic<kRingBufferSizeType> write_index_{0};
};

/**
 * @brief 自带存储的便利包装，只负责提供存储
 * @tparam CAPACITY 存储长度，必须是大于1的常量
 * @note 不含任何算法，收发操作经get()交给内部RingBuffer
 */
template <kRingBufferSizeType CAPACITY>
class OwnedRingBuffer {
public:
    static_assert(CAPACITY > 1, "容量必须大于 1");

    /**
     * @brief 获取内部缓冲区
     * @return 指向自有存储的 RingBuffer 引用
     */
    [[nodiscard]] RingBuffer& get() noexcept { return buffer_; }

private:
    // 内容由读写索引界定，不做初始化：静态存储下同样落在.bss
    std::array<uint8_t, CAPACITY> storage_;
    // 依赖成员声明顺序：storage_先就绪，buffer_才有存储可用
    RingBuffer buffer_{storage_};
};

}  // namespace kusart

#include "ringbuffer-inl.h"

#endif  // KUSART_RINGBUFFER_H
