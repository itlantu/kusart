#include <doctest/doctest.h>

#include <array>
#include <concepts>
#include <cstdint>
#include <ranges>

#include "kusart/ringbuffer.h"

namespace {

using kusart::kRingBufferSizeType;
using kusart::RingBuffer;

constexpr kRingBufferSizeType kBufferSize = 8;
constexpr int kUsableCapacity = kBufferSize - 1;

using Buffer = RingBuffer<kBufferSize>;

// 编译期校验容量与取模掩码自洽
static_assert(Buffer::capacity() == kBufferSize);
static_assert(Buffer::kModMark == kBufferSize - 1);

/**
 * @brief 元素可转换为 uint8_t 的输入范围
 * @tparam R 范围类型
 */
template <typename R>
concept ByteRange = std::ranges::input_range<R> && std::convertible_to<std::ranges::range_value_t<R>, uint8_t>;

/**
 * @brief 依次把 expected 写入缓冲区，并要求每次写入都成功
 * @param buffer 目标缓冲区
 * @param expected 待写入的字节序列，可用 std::views::iota 等惰性范围表达
 * @tparam BUFFER_SIZE 缓冲区容量
 * @tparam R 字节序列类型
 */
template <kRingBufferSizeType BUFFER_SIZE, ByteRange R>
void PushAll(RingBuffer<BUFFER_SIZE>& buffer, R&& expected) {
    for (const auto byte : expected) {
        CAPTURE(byte);
        CHECK(buffer.push(static_cast<uint8_t>(byte)));
    }
}

/**
 * @brief 依次读出缓冲区数据并与 expected 逐字节比对
 * @param buffer 源缓冲区
 * @param expected 期望读出的字节序列
 * @tparam BUFFER_SIZE 缓冲区容量
 * @tparam R 字节序列类型
 */
template <kRingBufferSizeType BUFFER_SIZE, ByteRange R>
void PopAll(RingBuffer<BUFFER_SIZE>& buffer, R&& expected) {
    for (const auto byte : expected) {
        uint8_t actual = 0;
        CAPTURE(byte);
        REQUIRE(buffer.pop(actual));
        CHECK_EQ(actual, byte);
    }
}

TEST_SUITE("ringbuffer") {
    TEST_CASE("Initial state is empty") {
        Buffer buffer{};

        CHECK_EQ(buffer.count(), 0);
        CHECK_EQ(buffer.free(), kUsableCapacity);
        CHECK_EQ(buffer.read_index, 0);
        CHECK_EQ(buffer.write_index, 0);
    }

    TEST_CASE("Push and pop keep FIFO order and update count and free") {
        constexpr std::array<uint8_t, 3> kPayload{0x11, 0x22, 0x33};
        Buffer buffer{};

        PushAll(buffer, kPayload);
        CHECK_EQ(buffer.count(), kPayload.size());
        CHECK_EQ(buffer.free(), kUsableCapacity - static_cast<int>(kPayload.size()));

        PopAll(buffer, kPayload);
        CHECK_EQ(buffer.count(), 0);
        CHECK_EQ(buffer.free(), kUsableCapacity);
    }

    TEST_CASE("Push until full then reject without losing data") {
        Buffer buffer{};

        PushAll(buffer, std::views::iota(0, kUsableCapacity));
        CHECK_EQ(buffer.count(), kUsableCapacity);
        CHECK_EQ(buffer.free(), 0);

        // 已满时继续写入必须失败，且不改变已缓存字节数
        CHECK_FALSE(buffer.push(0xFF));
        CHECK_EQ(buffer.count(), kUsableCapacity);

        PopAll(buffer, std::views::iota(0, kUsableCapacity));
        CHECK_EQ(buffer.free(), kUsableCapacity);
    }

    TEST_CASE("Pop on empty fails and leaves out param untouched") {
        Buffer buffer{};
        uint8_t actual = 0xAA;

        CHECK_FALSE(buffer.pop(actual));
        CHECK_EQ(actual, 0xAA);
        CHECK_EQ(buffer.count(), 0);
    }

    TEST_CASE("Write index wraps around and order stays FIFO") {
        Buffer buffer{};

        PushAll(buffer, std::views::iota(0, 5));
        PopAll(buffer, std::views::iota(0, 3));
        CHECK_EQ(buffer.count(), 2);

        // 写入5~8，其中8的落点越过缓冲区末尾
        PushAll(buffer, std::views::iota(5, 9));
        PopAll(buffer, std::views::iota(3, 9));
        CHECK_EQ(buffer.count(), 0);
    }

    TEST_CASE("Repeated fill and drain rounds wrap correctly") {
        Buffer buffer{};

        for (const int round : std::views::iota(0, 3)) {
            const int base = round * kUsableCapacity;

            PushAll(buffer, std::views::iota(base, base + kUsableCapacity));
            CHECK_EQ(buffer.count(), kUsableCapacity);
            CHECK_FALSE(buffer.push(0xFF));

            PopAll(buffer, std::views::iota(base, base + kUsableCapacity));
            CHECK_EQ(buffer.count(), 0);
        }
    }

    TEST_CASE("Clear resets state and allows refilling") {
        Buffer buffer{};

        PushAll(buffer, std::views::iota(0, 4));
        CHECK_EQ(buffer.count(), 4);

        buffer.clear();
        CHECK_EQ(buffer.count(), 0);
        CHECK_EQ(buffer.free(), kUsableCapacity);

        uint8_t actual = 0;
        CHECK_FALSE(buffer.pop(actual));

        PushAll(buffer, std::views::iota(0, kUsableCapacity));
        CHECK_EQ(buffer.count(), kUsableCapacity);
        CHECK_EQ(buffer.free(), 0);
    }

    TEST_CASE("Extreme byte values are not mistaken for empty or full") {
        constexpr std::array<uint8_t, 2> kExtremes{0x00, 0xFF};
        Buffer buffer{};

        PushAll(buffer, kExtremes);
        CHECK_EQ(buffer.count(), kExtremes.size());
        PopAll(buffer, kExtremes);
    }

    TEST_CASE("Count plus free always equals usable capacity") {
        Buffer buffer{};
        uint8_t actual = 0;

        for (const int step : std::views::iota(0, 32)) {
            CHECK_EQ(buffer.count() + buffer.free(), kUsableCapacity);

            if (step % 2 == 0)
                buffer.push(static_cast<uint8_t>(step));
            else
                buffer.pop(actual);
        }
    }

    TEST_CASE("Template works at another capacity") {
        constexpr kRingBufferSizeType kOtherSize = 32;
        constexpr int kOtherCapacity = kOtherSize - 1;
        RingBuffer<kOtherSize> buffer{};

        PushAll(buffer, std::views::iota(0, kOtherCapacity));
        CHECK_EQ(buffer.count(), kOtherCapacity);
        CHECK_EQ(buffer.free(), 0);
        CHECK_FALSE(buffer.push(0xAB));

        PopAll(buffer, std::views::iota(0, kOtherCapacity));
        CHECK_EQ(buffer.count(), 0);
    }
}

}  // namespace
