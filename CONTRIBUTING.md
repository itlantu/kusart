# 贡献指南

感谢你有兴趣为 kusart 做出贡献。本文档说明本项目的构建方式、代码约定与提交流程。

## 项目定位

kusart 是一个面向嵌入式的串口解析库，目标平台为 STM32 与 ESP32。

以下设计约束**不可违反**，任何改动都需要先保证它们成立：

| 约束 | 说明 |
| --- | --- |
| 零动态分配 | 不使用 `new` / `delete` / `malloc`，缓冲区由模板参数或调用方提供 |
| 无异常、无 RTTI | 必须能通过 `-fno-exceptions -fno-rtti` 编译 |
| 不依赖平台 | 不调用 HAL、不操作寄存器、不读时钟；超时与 tick 由调用方传入 |
| C++20 | 项目统一使用 C++20 |

`test_embedded_build` 目标专门用于守住第二条约束：改动库头文件后它必须仍然通过。

## 获取代码

本项目用 git submodule 引入 [doctest](https://github.com/doctest/doctest)，仅测试使用。

```bash
git clone --recursive <repo-url>
```

已经 clone 过、或看到 CMake 报「找不到 doctest 子模块」时：

```bash
git submodule update --init --recursive
```

## 构建与测试

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

运行部分用例：

```bash
ctest --test-dir build -R "Clear resets"            # 按名字过滤（正则）
build/test/test_ringbuffer.exe -ts=ringbuffer       # 按 test suite
build/test/test_ringbuffer.exe -tc="Push and pop*"  # 按用例名通配
```

构建产物：

| 目标 | 作用 |
| --- | --- |
| `test_ringbuffer` | `RingBuffer` 的行为测试（doctest） |
| `test_embedded_build` | 以无异常、无 RTTI 编译一遍库头文件，验证嵌入式约束 |

## 代码规范

### 格式

统一遵循仓库根目录的 [.clang-format](.clang-format)，提交前请执行：

```bash
clang-format -i <改动过的文件>
```

该配置已在 clang-format 19.1.5 与 22.0.0git 上验证结果一致。CI 建议锁定一个具体版本，避免不同 LLVM 版本产生格式差异。

### 命名

| 对象 | 规范 | 示例 |
| --- | --- | --- |
| 文件名 | `snake_case` | `ringbuffer-inl.h` |
| 类型 | `PascalCase` | `RingBuffer`、`ByteRange` |
| 函数 | `PascalCase`，动词开头 | `PushAll`、`RunSmokeCheck` |
| 变量、参数 | `snake_case` | `read_index`、`buffer_size` |
| 类成员变量 | `snake_case_` | `read_index_` |
| 结构体成员 | `snake_case` | `read_index` |
| 常量 | `k` + `PascalCase` | `kModMark`、`kUsableCapacity` |
| 宏 | `UPPER_SNAKE_CASE` | `KUSART_RINGBUFFER_H` |
| 命名空间 | 全小写 | `kusart` |
| 模板参数 | 类型 `PascalCase`，非类型 `snake_case` | `R`、`BUFFER_SIZE` |

### 注释

- Doxygen 注释写在**头文件**：每个类、函数、方法都要有；`@brief` 必填，有参数写 `@param`，有返回值写 `@return`
- 中文描述 + 英文术语
- 实现文件（`-inl.h`、`.cc`）不重复 Doxygen，只在需要说明实现细节时写普通注释
- 运行期输出（日志、断言消息）保持 **ASCII**：源码以 `/utf-8` 编译，中文字面量在 GBK 控制台下会显示为乱码

### 头文件组织

- 声明放在 `.h`，模板定义放在同目录的 `-inl.h`，由 `.h` 在 include guard 之内、文件末尾引入
- 模板定义必须对实例化点可见，因此不要放进 `.cc`
- `-inl.h` 只由对应的 `.h` 引入，不要被其他翻译单元直接包含

## 测试规范

测试框架是 doctest，测试文件放在 `test/<模块>/` 下，每个模块一个文件。

- **模块归属用 `TEST_SUITE`**，不要写进用例名：

  ```cpp
  TEST_SUITE("ringbuffer") {
      TEST_CASE("Initial state is empty") { ... }
  }
  ```

- **CTest 侧的命名空间由 CMake 提供**：`kusart_add_test(<target> <prefix> ...)` 会把 prefix 作为 `TEST_PREFIX` 传给 `doctest_discover_tests`，不要在用例名里手写前缀。最终 CTest 名字形如 `<prefix>:<用例名>` —— 冒号后**没有空格**，因为 CMake 的 `-D` 参数会剥掉结尾空白，所以不要指望 prefix 能以空格结尾
- **用例名不能含逗号**：doctest 的 `-tc` 与 `-ts` 过滤器都以逗号分隔参数
- 用例名写成描述性英文句子：首字母大写、缩写大写（如 `FIFO`）；**不要用斜杠与括号**——斜杠在 JUnit XML 及部分 CI 的「测试名 → 路径」映射中会出问题，括号是 `ctest -R` 的正则元字符
- 断言优先用 `CHECK_EQ` / `CHECK`；在循环里用 `CAPTURE()` 补充失败上下文
- `RingBuffer` 的并发契约是单生产者单消费者（典型为中断里 `push`、主循环里 `pop`），新增行为测试时不要假设其他调用模式

## 提交信息

格式为 `<前缀>: <中文描述>`：

| 前缀 | 用途 |
| --- | --- |
| `feat` | 新功能 |
| `fix` | 修复 bug |
| `test` | 测试相关 |
| `refactor` | 重构，不改变行为 |
| `perf` | 性能优化 |
| `docs` | 仅文档 |
| `style` | 格式调整，不影响逻辑 |
| `build` | 构建系统（`CMakeLists.txt` 等） |
| `ci` | CI 配置 |
| `chore` | 杂项、依赖更新 |
| `revert` | 回退 |
| `merge` | 合并分支 |

要求：描述用中文、动词开头、不超过 50 字符、不加句号、标点使用英文符号。一次提交尽量只做一件事。

示例：

```
fix: 修复 RingBuffer 写索引误用读端索引的问题
test: 增加环形缓冲区回绕场景的测试
```

## 环境注意事项

以下两条是本项目实际踩过的坑。

### 1. MSVC + Ninja 的增量构建在中文区域设置下失效

CMake 写入 `CMakeFiles/rules.ninja` 的 `msvc_deps_prefix` 会因编码问题与 `cl.exe` 实际输出的中文提示不匹配，结果是**改动头文件不会触发重新编译**——你会看到 `ninja: no work to do.`，然后对着旧产物调试。

应对方式：

- 改动 `include/` 下的头文件后强制整体重建（CLion：`Build → Rebuild Project`；命令行：删除 build 目录后重新配置）
- 根治：给 Visual Studio 安装英文语言包，让 `cl.exe` 输出 ASCII 提示

### 2. 运行期输出与终端编码

源码以 `/utf-8` 编译，而 `/utf-8` 会把执行字符集一并设为 UTF-8。因此中文字符串字面量在 GBK 控制台（中文 Windows 默认）下会显示为乱码。这也是测试用例名与断言消息统一使用英文的原因。
