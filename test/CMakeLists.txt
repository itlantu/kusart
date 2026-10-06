set(KUSART_TEST_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${KUSART_INCLUDE_DIRS})
set(KUSART_DOCTEST_DIR       ${PROJECT_SOURCE_DIR}/third_party/doctest)

if(NOT EXISTS "${KUSART_DOCTEST_DIR}/doctest/doctest.h")
    message(FATAL_ERROR
        "找不到 doctest 子模块：${KUSART_DOCTEST_DIR}\n"
        "请先执行  git submodule update --init --recursive")
endif()

include(${KUSART_DOCTEST_DIR}/scripts/cmake/doctest.cmake)

# 只取doctest的头文件目录, 不执行它自带的CMakeLists村儿避免注入install规则
add_library(doctest_interface INTERFACE)
target_include_directories(doctest_interface SYSTEM INTERFACE ${KUSART_DOCTEST_DIR})
target_compile_definitions(doctest_interface INTERFACE DOCTEST_CONFIG_USE_STD_HEADERS)
add_library(doctest::doctest ALIAS doctest_interface)

# 添加一个测试可执行文件，并把其中的 doctest 用例逐个注册到 CTest；prefix 提供 CTest 侧的用例名命名空间
function(kusart_add_test target prefix)
    add_executable(${target} ${ARGN})

    target_include_directories(${target} PRIVATE ${KUSART_TEST_INCLUDE_DIRS})
    target_link_libraries(${target} PRIVATE kusart doctest::doctest)

    doctest_discover_tests(${target} TEST_PREFIX "${prefix}: " PROPERTIES TIMEOUT 30)
endfunction()

kusart_add_test(test_ringbuffer "ringbuffer" "test_main.cc" "ringbuffer/test_ringbuffer.cc")

# 以无异常和无RTTI编译，验证嵌入式约束不被破坏
add_executable(test_embedded_build "embedded_check.cc")
target_include_directories(test_embedded_build PRIVATE ${KUSART_TEST_INCLUDE_DIRS})
target_link_libraries(test_embedded_build PRIVATE kusart)
if(MSVC)
    target_compile_options(test_embedded_build PRIVATE /EHs-c- /GR-)
else()
    target_compile_options(test_embedded_build PRIVATE -fno-exceptions -fno-rtti)
endif()

add_test(NAME test_embedded_build COMMAND $<TARGET_FILE:test_embedded_build>)
set_tests_properties(test_embedded_build PROPERTIES TIMEOUT 30)
