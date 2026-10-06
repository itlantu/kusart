#ifndef KUSART_RINGBUFFER_H
#define KUSART_RINGBUFFER_H

#include <atomic>
#include <bit>
#include <cstdint>

namespace kusart {

using kRingBufferSizeType = uint16_t;
constexpr kRingBufferSizeType kRingBufferMaxSize = UINT16_MAX;

/**
 * @brief 定长环形缓冲区, 实际可缓存BUFFER_SIZE - 1个字节
 * @tparam BUFFER_SIZE 缓冲区容量，必须是大于0且小于kRingBufferMaxSize的 2 的幂
 * @note 单生产者单消费者：push() 与 pop() 必须分别只由一个执行流调用，典型用法是中断里 push()、主循环里 pop()
 * @note 读写索引为 std::atomic 并使用 acquire/release 顺序；在 Cortex-M3/M4/M7/M33 上 lock-free，
 *       代价是每次 push/pop 各多两条 dmb。Cortex-M0/M0+ 的类型特征报告非 lock-free，实测仍生成内联代码
 */
template <kRingBufferSizeType BUFFER_SIZE>
struct RingBuffer {
    static_assert(BUFFER_SIZE > 0, "容量必须大于 0");
    static_assert(BUFFER_SIZE < kRingBufferMaxSize, "容量必须小于 kRingBufferMaxSize");
    static_assert(std::has_single_bit(BUFFER_SIZE), "容量必须是 2 的幂");

    uint8_t buffer[BUFFER_SIZE]{};
    std::atomic<uint16_t> read_index;
    std::atomic<uint16_t> write_index;

    /**
     * @brief 获取当前已缓存的字节数
     * @return 已缓存字节数，取值范围为闭区间的[0, BUFFER_SIZE - 1]
     */
    [[nodiscard]] uint16_t count() const;

    /**
     * @brief 获取当前仍可写入的字节数
     * @return 剩余可写字节数；它与count()之和应为BUFFER_SIZE - 1
     */
    [[nodiscard]] uint16_t free() const;

    /**
     * @brief 清空缓冲区，把读写索引一并复位到起点
     * @note 会同时写两个索引，只能在 push/pop 均已停止时调用
     */
    void clear();

    /**
     * @brief 向缓冲区尾部写入一个字节
     * @param ch 待写入的字节
     * @return 写入成功返回true；缓冲区已满返回false
     */
    bool push(uint8_t ch);

    /**
     * @brief 从缓冲区头部取出一个字节
     * @param ch 用于接收取出字节的引用，仅在返回true 时被修改
     * @return 取出成功返回true；缓冲区为空返回false
     */
    bool pop(uint8_t& ch);

    static constexpr uint16_t kModMark = BUFFER_SIZE - 1;

    /**
     * @brief 获取缓冲区数组长度
     * @return 缓冲区容量，等于模板参数BUFFER_SIZE
     */
    static constexpr uint16_t capacity();
};

}  // namespace kusart

#include "ringbuffer-inl.h"

#endif  // KUSART_RINGBUFFER_H
