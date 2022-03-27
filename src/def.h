#pragma once

#include <memory>
#include <cstdint>

using namespace std;

#define BLOCK_LINE_NUM (32768)

/**
 * @brief 整个文件里的行号、行数量
 */
using LogLineI = uint64_t;
/**
 * @brief Block里的行序号、行数量
 */
using BlockLineI = uint16_t;
/**
 * @brief 行里的字符位置
 */
using LineCharI = uint16_t;
/**
 * @brief Block里的字符位置
 * 
 * 一个块，最多有32768（固定行数） * 65536（每行最多字符数） = 0x10000000个字节
 */
using BlockCharI = uint32_t;
/**
 * @brief 文件里的字符位置
 */
using LogCharI = uint64_t;

using LogBlockI = uint32_t;

#define LastIndex(vec) ((vec).size() - 1)
#define LastItem(vec) ((vec)[LastIndex(vec)])