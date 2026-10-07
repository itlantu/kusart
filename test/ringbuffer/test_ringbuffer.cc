#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "kusart/ringbuffer.h"

namespace {

using kusart::kRingBufferSizeType;
using kusart::OwnedRingBuffer;
using kusart::RingBuffer;

constexpr kRingBufferSizeType kCapacity = 8;
constexpr kRingBufferSizeType kUsableCapacity = kCapacity - 1;

/**
 * @brief 从 first 起依次写入 count 个字节，并要求每次都成功
 * @param buffer 目标缓冲区
 * @param first 首个字节的值
 * @param count 写入字节数
 */
void PushRange(RingBuffer& buffer, int first, int count) {
    for (int i = 0; i < count; ++i) {
        const auto value = static_cast<uint8_t>(first + i);
        CAPTURE(value);
        REQUIRE(buffer.push(value));
    }
}

/**
 * @brief 从 first 起依次读出 count 个字节并逐字节比对
 * @param buffer 源缓冲区
 * @param first 首个期望字节的值
 * @param count 读出字节数
 */
void PopRange(RingBuffer& buffer, int first, int count) {
    for (int i = 0; i < count; ++i) {
        const auto expected = static_cast<uint8_t>(first + i);
        uint8_t actual = 0;
        CAPTURE(expected);
        REQUIRE(buffer.pop(actual));
        CHECK_EQ(actual, expected);
    }
}

/**
 * @brief 与容量无关的消费端：只依赖 RingBuffer 本身，不需要知道自己拿到的缓冲区多大
 * @param buffer 数据源缓冲区
 * @param out 接收数据的缓冲区，长度必须不小于max
 * @param max 最多读出的字节数
 * @return 实际读出的字节数
 */
int DrainAll(RingBuffer& buffer, uint8_t* out, int max) {
    int count = 0;
    while (count < max && buffer.pop(out[count]))
        ++count;
    return count;
}

TEST_SUITE("ringbuffer") {
    TEST_CASE("Fresh buffer is empty") {
        uint8_t storage[kCapacity]{};
        RingBuffer buffer{storage};

        CHECK(buffer.is_config_valid());
        CHECK_EQ(buffer.capacity(), kCapacity);
        CHECK_EQ(buffer.usable_capacity(), kUsableCapacity);
        CHECK_EQ(buffer.count(), 0);
        CHECK_EQ(buffer.free(), kUsableCapacity);
    }

    TEST_CASE("Push and pop keep FIFO order and update count and free") {
        constexpr std::array<uint8_t, 3> kPayload{0x11, 0x22, 0x33};
        uint8_t storage[kCapacity]{};
        RingBuffer buffer{storage};

        for (const uint8_t byte : kPayload) {
            CAPTURE(byte);
            REQUIRE(buffer.push(byte));
        }
        CHECK_EQ(buffer.count(), kPayload.size());
        CHECK_EQ(buffer.free(), kUsableCapacity - static_cast<int>(kPayload.size()));

        for (const uint8_t expected : kPayload) {
            uint8_t byte = 0;
            CAPTURE(expected);
            REQUIRE(buffer.pop(byte));
            CHECK_EQ(byte, expected);
        }
        CHECK_EQ(buffer.count(), 0);
        CHECK_EQ(buffer.free(), kUsableCapacity);
    }

    TEST_CASE("Push until full then reject without losing data") {
        uint8_t storage[kCapacity]{};
        RingBuffer buffer{storage};

        PushRange(buffer, 0, kUsableCapacity);
        CHECK_EQ(buffer.count(), kUsableCapacity);
        CHECK_EQ(buffer.free(), 0);

        // 已满时继续写入必须失败，且不改变已缓存字节数
        CHECK_FALSE(buffer.push(0xFF));
        CHECK_EQ(buffer.count(), kUsableCapacity);

        PopRange(buffer, 0, kUsableCapacity);
        CHECK_EQ(buffer.free(), kUsableCapacity);
    }

    TEST_CASE("Pop on empty fails and leaves out param untouched") {
        uint8_t storage[kCapacity]{};
        RingBuffer buffer{storage};
        uint8_t byte = 0xAA;

        CHECK_FALSE(buffer.pop(byte));
        CHECK_EQ(byte, 0xAA);
        CHECK_EQ(buffer.count(), 0);
    }

    TEST_CASE("Write index wraps around and order stays FIFO") {
        uint8_t storage[kCapacity]{};
        RingBuffer buffer{storage};

        PushRange(buffer, 0, 5);
        PopRange(buffer, 0, 3);
        CHECK_EQ(buffer.count(), 2);

        // 写入5~8，其中8的落点越过存储末尾
        PushRange(buffer, 5, 4);
        PopRange(buffer, 3, 6);
        CHECK_EQ(buffer.count(), 0);
    }

    TEST_CASE("Repeated fill and drain rounds wrap correctly") {
        uint8_t storage[kCapacity]{};
        RingBuffer buffer{storage};

        for (int round = 0; round < 3; ++round) {
            const int base = round * kUsableCapacity;

            PushRange(buffer, base, kUsableCapacity);
            CHECK_EQ(buffer.count(), kUsableCapacity);
            CHECK_FALSE(buffer.push(0xFF));

            PopRange(buffer, base, kUsableCapacity);
            CHECK_EQ(buffer.count(), 0);
        }
    }

    TEST_CASE("Clear resets state and allows refilling") {
        uint8_t storage[kCapacity]{};
        RingBuffer buffer{storage};

        PushRange(buffer, 0, 4);
        CHECK_EQ(buffer.count(), 4);

        buffer.clear();
        CHECK_EQ(buffer.count(), 0);
        CHECK_EQ(buffer.free(), kUsableCapacity);

        uint8_t byte = 0;
        CHECK_FALSE(buffer.pop(byte));

        PushRange(buffer, 0, kUsableCapacity);
        CHECK_EQ(buffer.count(), kUsableCapacity);
        CHECK_EQ(buffer.free(), 0);
    }

    TEST_CASE("Capacity need not be a power of two") {
        constexpr kRingBufferSizeType kOddCapacity = 10;
        constexpr kRingBufferSizeType kOddUsable = kOddCapacity - 1;
        uint8_t storage[kOddCapacity]{};
        RingBuffer buffer{storage};

        CHECK(buffer.is_config_valid());
        CHECK_EQ(buffer.capacity(), kOddCapacity);
        CHECK_EQ(buffer.usable_capacity(), kOddUsable);

        PushRange(buffer, 0, kOddUsable);
        CHECK_EQ(buffer.count(), kOddUsable);
        CHECK_FALSE(buffer.push(0xFF));

        // 读走5个后写端绕过存储末尾，顺序仍应是 FIFO
        PopRange(buffer, 0, 5);
        PushRange(buffer, kOddUsable, 2);
        CHECK_EQ(buffer.count(), kOddUsable - 5 + 2);

        PopRange(buffer, 5, kOddUsable - 5 + 2);
        CHECK_EQ(buffer.count(), 0);
    }

    TEST_CASE("Minimum capacity holds exactly one byte") {
        uint8_t storage[2]{};
        RingBuffer buffer{storage};

        CHECK(buffer.is_config_valid());
        CHECK_EQ(buffer.capacity(), 2);
        CHECK_EQ(buffer.usable_capacity(), 1);

        REQUIRE(buffer.push(0xAA));
        CHECK_EQ(buffer.count(), 1);
        CHECK_EQ(buffer.free(), 0);
        CHECK_FALSE(buffer.push(0xBB));

        uint8_t byte = 0;
        REQUIRE(buffer.pop(byte));
        CHECK_EQ(byte, 0xAA);
        CHECK_FALSE(buffer.pop(byte));
    }

    TEST_CASE("Buffers over different storage stay independent") {
        uint8_t small_storage[4]{};
        uint8_t storage[kCapacity]{};
        RingBuffer small{small_storage};
        RingBuffer buffer{storage};

        CHECK_EQ(small.capacity(), 4);
        CHECK_EQ(buffer.capacity(), kCapacity);

        REQUIRE(small.push(0x01));
        CHECK_EQ(small.count(), 1);
        CHECK_EQ(buffer.count(), 0);

        // 数据直接落在各自的存储里
        CHECK_EQ(small_storage[0], 0x01);
        CHECK_EQ(storage[0], 0x00);
    }

    TEST_CASE("Non template consumer drains through a reference") {
        uint8_t storage[kCapacity]{};
        RingBuffer buffer{storage};

        PushRange(buffer, 0, kUsableCapacity);

        std::array<uint8_t, kCapacity> out{};
        const int drained = DrainAll(buffer, out.data(), static_cast<int>(out.size()));

        CHECK_EQ(drained, kUsableCapacity);
        for (int i = 0; i < drained; ++i)
            CHECK_EQ(out[static_cast<std::size_t>(i)], static_cast<uint8_t>(i));
        CHECK_EQ(buffer.count(), 0);
    }

    TEST_CASE("Owned wrapper carries its own storage") {
        OwnedRingBuffer<kCapacity> rx;
        OwnedRingBuffer<4> other;

        CHECK(rx.get().is_config_valid());
        CHECK_EQ(rx.get().capacity(), kCapacity);
        CHECK_EQ(rx.get().usable_capacity(), kUsableCapacity);
        CHECK_EQ(other.get().capacity(), 4);
        CHECK_EQ(other.get().count(), 0);

        REQUIRE(rx.get().push(0x11));
        CHECK_EQ(rx.get().count(), 1);
        CHECK_EQ(other.get().count(), 0);

        uint8_t byte = 0;
        REQUIRE(rx.get().pop(byte));
        CHECK_EQ(byte, 0x11);
        CHECK_EQ(rx.get().count(), 0);
    }

    TEST_CASE("Configuration is invalid below two bytes") {
        const std::span<uint8_t> nothing{};
        uint8_t single[1]{};
        RingBuffer empty{nothing};
        RingBuffer one{single};

        CHECK_FALSE(empty.is_config_valid());
        CHECK_EQ(empty.capacity(), 0);
        CHECK_FALSE(one.is_config_valid());
        CHECK_EQ(one.capacity(), 1);
    }
}

}  // namespace
