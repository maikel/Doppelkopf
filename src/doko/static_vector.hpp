// Copyright (c) 2019 Maikel Nadolski
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef DOKO_STATIC_VECTOR_HPP
#define DOKO_STATIC_VECTOR_HPP

#include "fub/core/assert.hpp"

#include <algorithm>

namespace doko {

template <typename T, int N> class static_vector {
public:
  using value_type = T;

  static_vector() = default;
  static_vector(int size, const T& default_value) : size_{size} {
    std::fill_n(data(), size, default_value);
  }

  static_vector(std::initializer_list<T> list)
      : static_vector(list.begin(), list.end()) {}

  template <typename Iterator, typename Sentinel>
  static_vector(Iterator first, Sentinel last)
      : size_(std::distance(first, last)) {
    FUB_ASSERT(size_ <= N);
    std::copy_n(first, size_, &data_[0]);
  }

  T* data() noexcept { return data_; }
  const T* data() const noexcept { return data_; }

  int size() const noexcept { return size_; }

  T& operator[](int i) noexcept { return data_[i]; }
  const T& operator[](int i) const noexcept { return data_[i]; }

  T* begin() noexcept { return data(); }
  const T* begin() const noexcept { return data(); }
  const T* cbegin() const noexcept { return data(); }

  std::reverse_iterator<T*> rbegin() noexcept {
    return std::make_reverse_iterator(end());
  }
  std::reverse_iterator<const T*> rbegin() const noexcept {
    return std::make_reverse_iterator(end());
  }
  std::reverse_iterator<const T*> crbegin() const noexcept {
    return std::make_reverse_iterator(end());
  }

  T* end() noexcept { return data() + size_; }
  const T* end() const noexcept { return data() + size_; }
  const T* cend() const noexcept { return data() + size_; }

  std::reverse_iterator<T*> rend() noexcept {
    return std::make_reverse_iterator(begin());
  }
  std::reverse_iterator<const T*> rend() const noexcept {
    return std::make_reverse_iterator(begin());
  }
  std::reverse_iterator<const T*> crend() const noexcept {
    return std::make_reverse_iterator(begin());
  }

  template <typename S> bool emplace_back(const S& values) noexcept {
    if (size_ + values.size() <= N) {
      std::transform(values.begin(), values.end(), data_ + size_,
                     [](auto&& x) { return T(x); });
      size_ += values.size();
      return true;
    }
    return false;
  }

  bool push_back(const T& value) noexcept {
    if (size_ < N) {
      data_[size_] = value;
      ++size_;
      return true;
    }
    return false;
  }

  void pop_back() noexcept {
    if (size_ > 0) {
      size_ -= 1;
    }
  }

  bool resize(int n) noexcept {
    if (0 <= n && n <= N) {
      size_ = n;
      return true;
    }
    return false;
  }

  void clear() noexcept { size_ = 0; }

  bool empty() const noexcept { return size_ == 0; }

  const T& back() const noexcept {
    FUB_ASSERT(size_ > 0);
    return data_[size_ - 1];
  }

  const T& front() const noexcept {
    FUB_ASSERT(size_ > 0);
    return data_[0];
  }

private:
  T data_[N];
  int size_{};
};

template <typename T, int N, int M>
bool operator==(const static_vector<T, N>& v,
                const static_vector<T, M>& w) noexcept {
  return v.size() == w.size() && std::equal(v.begin(), v.end(), w.begin());
}

template <typename T, int N, int M>
bool operator!=(const static_vector<T, N>& v,
                const static_vector<T, M>& w) noexcept {
  return !(v == w);
}

} // namespace doko

#endif