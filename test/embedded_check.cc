#include <cstdint>
#include <cstdio>

#include "kusart/ringbuffer.h"

// 检查编译开关
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
#error "本目标必须禁用异常（MSVC: /EHs-c-，GCC/Clang: -fno-exceptions）"
#endif

#if defined(__GXX_RTTI) || defined(_CPPRTTI)
#error "本目标必须禁用 RTTI（MSVC: /GR-，GCC/Clang: -fno-rtti）"
#endif

namespace {

/**
 * @brief 在禁用异常与 RTTI 的前提下跑一遍最小收发流程
 * @return 行为全部符合预期时返回 true
 */
bool RunSmokeCheck() {
    kusart::RingBuffer<16> buffer{};
    uint8_t byte = 0;

    if (buffer.count() != 0 || buffer.free() != 15)
        return false;
    if (buffer.pop(byte))
        return false;
    if (!buffer.push(0x5A))
        return false;
    if (!buffer.pop(byte) || byte != 0x5A)
        return false;

    buffer.clear();
    return buffer.count() == 0;
}

}  // namespace

/**
 * @brief 检查目标入口
 * @return 检查通过返回 0，否则返回 1
 */
int main() {
    if (RunSmokeCheck())
        return 0;

    std::puts("embedded build check FAILED");
    return 1;
}
