// Copyright (c) 2018 Maikel Nadolski
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

#include <algorithm>

namespace doko {

template <typename T, int N> class StaticVector {
public:
  StaticVector() = default;
  StaticVector(int size, const T& default_value) : size_{size} {
    std::fill_n(data(), size, default_value);
  }

  T* data() noexcept { return data_; }
  const T* data() const noexcept { return data_; }

  int size() const noexcept { return size_; }

  T& operator[](int i) noexcept { return data_[i]; }
  const T& operator[](int i) const noexcept { return data_[i]; }

  T* begin() noexcept { return data(); }
  const T* begin() const noexcept { return data(); }
  const T* cbegin() const noexcept { return data(); }

  T* end() noexcept { return data() + size_; }
  const T* end() const noexcept { return data() + size_; }
  const T* cend() const noexcept { return data() + size_; }

private:
  T data_[N];
  int size_;
};

} // namespace doko

#endif