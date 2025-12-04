/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#pragma once

#include <cstdlib>
#include <cstring>
#include <memory>

/**
 * @brief 简单的数据包缓冲区，避免 vector 的 memset 初始化开销
 * @details 只分配内存，不初始化，适合作为临时缓冲区使用
 */
class PacketBuffer
{
public:
  explicit PacketBuffer(size_t capacity = 4 * 1024 * 1024) : capacity_(capacity), data_(nullptr)
  {
    if (capacity_ > 0) {
      data_ = static_cast<char *>(malloc(capacity_));
      if (!data_) {
        capacity_ = 0;
      }
    }
  }

  ~PacketBuffer()
  {
    if (data_) {
      free(data_);
      data_ = nullptr;
    }
  }

  // 禁止拷贝
  PacketBuffer(const PacketBuffer &) = delete;
  PacketBuffer &operator=(const PacketBuffer &) = delete;

  // 允许移动
  PacketBuffer(PacketBuffer &&other) noexcept : capacity_(other.capacity_), data_(other.data_)
  {
    other.capacity_ = 0;
    other.data_      = nullptr;
  }

  PacketBuffer &operator=(PacketBuffer &&other) noexcept
  {
    if (this != &other) {
      if (data_) {
        free(data_);
      }
      capacity_ = other.capacity_;
      data_     = other.data_;
      other.capacity_ = 0;
      other.data_      = nullptr;
    }
    return *this;
  }

  /**
   * @brief 获取缓冲区指针
   * @return 缓冲区指针，如果容量为 0 则返回 nullptr
   */
  char *data() { return data_; }

  const char *data() const { return data_; }

  /**
   * @brief 获取缓冲区容量
   */
  size_t capacity() const { return capacity_; }

  /**
   * @brief 确保有足够的容量，如果不够则重新分配
   * @return 是否成功
   */
  bool ensure_capacity(size_t new_capacity)
  {
    if (new_capacity <= capacity_) {
      return true;
    }

    char *new_data = static_cast<char *>(realloc(data_, new_capacity));
    if (!new_data) {
      return false;
    }

    data_     = new_data;
    capacity_ = new_capacity;
    return true;
  }

private:
  size_t capacity_ = 0;
  char  *data_     = nullptr;
};

