// 本文件由 kusart/ringbuffer.h 在末尾引入，请勿直接包含。
#ifndef KUSART_RINGBUFFER_INL_H
#define KUSART_RINGBUFFER_INL_H

#include <atomic>
#include <cstdint>

namespace kusart {

template <kRingBufferSizeType BUFFER_SIZE>
uint16_t RingBuffer<BUFFER_SIZE>::count() const {
    const uint16_t read = read_index.load(std::memory_order_acquire);
    const uint16_t write = write_index.load(std::memory_order_acquire);

    if (write >= read)
        return static_cast<uint16_t>(write - read);
    return static_cast<uint16_t>(BUFFER_SIZE - read + write);
}

template <kRingBufferSizeType BUFFER_SIZE>
uint16_t RingBuffer<BUFFER_SIZE>::free() const {
    const auto used = count();
    return static_cast<uint16_t>((BUFFER_SIZE - 1) - used);
}

template <kRingBufferSizeType BUFFER_SIZE>
void RingBuffer<BUFFER_SIZE>::clear() {
    read_index.store(0, std::memory_order_relaxed);
    write_index.store(0, std::memory_order_relaxed);
}

template <kRingBufferSizeType BUFFER_SIZE>
bool RingBuffer<BUFFER_SIZE>::push(uint8_t ch) {
    const uint16_t write = write_index.load(std::memory_order_relaxed);
    const auto next = static_cast<uint16_t>((write + 1) & kModMark);

    if (next == read_index.load(std::memory_order_acquire))
        return false;
    buffer[write] = ch;
    write_index.store(next, std::memory_order_release);
    return true;
}

template <kRingBufferSizeType BUFFER_SIZE>
bool RingBuffer<BUFFER_SIZE>::pop(uint8_t& ch) {
    const uint16_t read = read_index.load(std::memory_order_relaxed);

    if (write_index.load(std::memory_order_acquire) == read)
        return false;
    ch = buffer[read];
    read_index.store(static_cast<uint16_t>((read + 1) & kModMark), std::memory_order_release);
    return true;
}

template <kRingBufferSizeType BUFFER_SIZE>
constexpr uint16_t RingBuffer<BUFFER_SIZE>::capacity() {
    return BUFFER_SIZE;
}

}  // namespace kusart

#endif  // KUSART_RINGBUFFER_INL_H
