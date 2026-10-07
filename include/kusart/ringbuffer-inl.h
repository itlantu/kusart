// 本文件由 kusart/ringbuffer.h 在末尾引入, 请勿直接包含
#ifndef KUSART_RINGBUFFER_INL_H
#define KUSART_RINGBUFFER_INL_H

#include <atomic>
#include <cstdint>
#include <span>

namespace kusart {

inline constexpr RingBuffer::RingBuffer(std::span<uint8_t> storage) noexcept
    : storage_(storage.data()), capacity_(static_cast<kRingBufferSizeType>(storage.size())) {}

inline constexpr bool RingBuffer::is_config_valid() const noexcept {
    return capacity_ > 1;
}

inline constexpr kRingBufferSizeType RingBuffer::capacity() const noexcept {
    return capacity_;
}

inline constexpr kRingBufferSizeType RingBuffer::usable_capacity() const noexcept {
    return static_cast<kRingBufferSizeType>(capacity_ - 1);
}

inline kRingBufferSizeType RingBuffer::count() const noexcept {
    const kRingBufferSizeType read = read_index_.load(std::memory_order_acquire);
    const kRingBufferSizeType write = write_index_.load(std::memory_order_acquire);

    // 两个索引都恒定落在[0, capacity_)内，因此可以直接比较而不必担心整型回绕
    if (write >= read)
        return static_cast<kRingBufferSizeType>(write - read);
    return static_cast<kRingBufferSizeType>(capacity_ - read + write);
}

inline kRingBufferSizeType RingBuffer::free() const noexcept {
    return static_cast<kRingBufferSizeType>(usable_capacity() - count());
}

inline void RingBuffer::clear() noexcept {
    read_index_.store(0, std::memory_order_relaxed);
    write_index_.store(0, std::memory_order_relaxed);
}

inline bool RingBuffer::push(uint8_t ch) noexcept {
    const kRingBufferSizeType write = write_index_.load(std::memory_order_relaxed);
    kRingBufferSizeType next = static_cast<kRingBufferSizeType>(write + 1);
    if (next == capacity_)
        next = 0;

    if (next == read_index_.load(std::memory_order_acquire))
        return false;
    storage_[write] = ch;
    write_index_.store(next, std::memory_order_release);
    return true;
}

inline bool RingBuffer::pop(uint8_t& ch) noexcept {
    const kRingBufferSizeType read = read_index_.load(std::memory_order_relaxed);

    if (write_index_.load(std::memory_order_acquire) == read)
        return false;
    ch = storage_[read];

    kRingBufferSizeType next = static_cast<kRingBufferSizeType>(read + 1);
    if (next == capacity_)
        next = 0;
    read_index_.store(next, std::memory_order_release);
    return true;
}

}  // namespace kusart

#endif  // KUSART_RINGBUFFER_INL_H
