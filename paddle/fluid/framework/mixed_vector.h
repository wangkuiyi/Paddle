/* Copyright (c) 2016 PaddlePaddle Authors. All Rights Reserved.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License. */

#pragma once

#include <initializer_list>
#include <vector>

#include "paddle/fluid/framework/tensor.h"
#include "paddle/fluid/framework/tensor_util.h"

#include "glog/logging.h"

namespace paddle {
namespace framework {

// Vector is for replacing thrust::host_vector.  We want this
// replacement for two reasons:
//
// 1. thrust::host_vector has performance issue:
//    - https://github.com/PaddlePaddle/Paddle/issues/7713
//    - https://github.com/PaddlePaddle/Paddle/issues/7714
//
// 2. thrust::host_vector doesn't use paddle/fluid/memory for memory
//    management.
//
// Vector implements the thrust::host_vector interface
// (https://thrust.github.io/doc/classthrust_1_1host__vector.html),
// which is consistent with std::vector
// (http://en.cppreference.com/w/cpp/container/vector).
//
// Feel free to call Vector::Data and Vector::MutableData with any
// place
// (https://github.com/PaddlePaddle/Paddle/blob/develop/paddle/fluid/platform/place.h)
// -- Vector would sync the data.
//
// To ease the implementation, Vector uses Tensor as the underlying
// storage.  Please be aware that our Tensor could contain only
// POD-typed elements.
//
template <typename T>
class Vector {
 public:
  typedef T value_type;
  typedef T& reference;
  typedef const T& const_reference;
  typedef T* iterator;
  typedef T* const const_iterator;
  typedef size_t size_type;

  Vector() { InitEmpty(); }
  Vector(size_t count, const T& value);
  Vector(std::initializer_list<T> init);
  explicit Vector(const std::vector<T>& dat);

  Vector(const Vector<T>& other) { Copy(other); }
  Vector(Vector<T>&& other) { Move(other); }

  Vector<T>& operator=(const Vector<T>& other);

  reference operator[](size_t i);
  const_reference operator[](size_t i) const;

  size_type size() const { return size_; }

  iterator begin() {
    return capacity() == 0 ? &invalid_reference_placeholder_
                           : &this->operator[](0);
  }
  iterator end() {
    return capacity() == 0 ? &invalid_reference_placeholder_
                           : &this->operator[](size());
  }

  T& front() { return *begin(); }

  T& back() {
    auto it = end();
    --it;
    return *it;
  }

  const T* begin() const {
    return capacity() == 0 ? &invalid_reference_placeholder_
                           : &this->operator[](0);
  }

  const T* end() const {
    return capacity() == 0 ? &invalid_reference_placeholder_
                           : &this->operator[](size());
  }

  const T* cbegin() const { return begin(); }

  const T* cend() const { return end(); }

  const T& back() const {
    auto it = end();
    --it;
    return *it;
  }

  T* data() { return begin(); }

  const T* data() const { return begin(); }

  const T& front() const { return *begin(); }
  // end of std::vector iterator methods

  // assign this from iterator.
  // NOTE: the iterator must support `end-begin`
  template <typename Iter>
  void assign(Iter begin, Iter end) {
    InitByIter(end - begin, begin, end);
  }

  // push_back. If the previous capacity is not enough, the memory will
  // double.
  void push_back(T elem) {
    if (size_ + 1 > capacity()) {
      reserve((size_ + 1) << 1);
    }
    *end() = elem;
    ++size_;
  }

  // extend a vector by iterator.
  // NOTE: the iterator must support end-begin
  template <typename It>
  void Extend(It begin, It end) {
    size_t pre_size = size_;
    resize(pre_size + (end - begin));
    T* ptr = this->begin() + pre_size;
    for (; begin < end; ++begin, ++ptr) {
      *ptr = *begin;
    }
  }

  // resize the vector
  void resize(size_t size) {
    if (size + 1 < capacity()) {
      size_ = size;
    } else {
      MutableCPU();
      Tensor cpu_tensor;
      platform::Place cpu = platform::CPUPlace();
      T* ptr = cpu_tensor.mutable_data<T>(
          framework::make_ddim({static_cast<int64_t>(size)}), cpu);
      const T* old_ptr =
          cpu_vec_.memory_size() == 0 ? nullptr : cpu_vec_.data<T>();
      if (old_ptr != nullptr) {
        std::copy(old_ptr, old_ptr + size_, ptr);
      }
      size_ = size;
      cpu_vec_.ShareDataWith(cpu_tensor);
    }
  }

  // get cuda ptr. immutable
  const T* CUDAData(platform::Place place) const {
    PADDLE_ENFORCE(platform::is_gpu_place(place),
                   "CUDA Data must on CUDA place");
    ImmutableCUDA(place);
    return cuda_vec_.data<T>();
  }

  // get cuda ptr. mutable
  T* CUDAMutableData(platform::Place place) {
    const T* ptr = CUDAData(place);
    data_status_ = kNeedSync | kOnCUDA;
    return const_cast<T*>(ptr);
  }

  // clear
  void clear() {
    size_ = 0;
    data_status_ = kNeedSync | kOnCPU;
  }

  size_t capacity() const {
    return cpu_vec_.memory_size() / SizeOfType(typeid(T));
  }

  // reserve data
  void reserve(size_t size) {
    size_t pre_size = size_;
    resize(size);
    resize(pre_size);
  }

  // the unify method to access CPU or CUDA data. immutable.
  const T* Data(platform::Place place) const {
    if (platform::is_gpu_place(place)) {
      return CUDAData(place);
    } else {
      return data();
    }
  }

  // the unify method to access CPU or CUDA data. mutable.
  T* MutableData(platform::Place place) {
    if (platform::is_gpu_place(place)) {
      return CUDAMutableData(place);
    } else {
      return data();
    }
  }

  // implicit cast operator. Vector can be cast to std::vector implicitly.
  operator std::vector<T>() const {
    std::vector<T> result;
    result.resize(size());
    std::copy(begin(), end(), result.begin());
    return result;
  }

  bool operator==(const Vector<T>& other) const {
    if (size() != other.size()) return false;
    auto it1 = cbegin();
    auto it2 = other.cbegin();
    for (; it1 < cend(); ++it1, ++it2) {
      if (*it1 != *it2) {
        return false;
      }
    }
    return true;
  }

 private:
  void InitEmpty() {
    size_ = 0;
    data_status_ = kOnCPU;
  }

  template <typename Iter>
  void InitByIter(size_t size, Iter begin, Iter end) {
    platform::Place cpu = platform::CPUPlace();
    T* ptr = this->cpu_vec_.template mutable_data<T>(
        framework::make_ddim({static_cast<int64_t>(size)}), cpu);
    for (size_t i = 0; i < size; ++i) {
      *ptr++ = *begin++;
    }
    data_status_ = kOnCPU | kNeedSync;
    size_ = size;
  }

  void Move(Vector<T>&& other);
  void Copy(const Vector<T>& other);

  void CopyToCPU() const {
    // COPY GPU Data To CPU
    TensorCopy(cuda_vec_, platform::CPUPlace(), &cpu_vec_);
    WaitPlace(cuda_vec_.place());
  }

  void MutableCPU() {
    if (IsInCUDA() && IsDirty()) {
      CopyToCPU();
    }
    data_status_ = kNeedSync | kOnCPU;
  }

  void ImmutableCUDA(platform::Place place) const {
    if (IsDirty()) {
      if (IsInCPU()) {
        TensorCopy(cpu_vec_, boost::get<platform::CUDAPlace>(place),
                   &cuda_vec_);
        WaitPlace(place);
        UnsetFlag(kNeedSync);
        SetFlag(kOnCUDA);
      } else if (IsInCUDA() && !(place == cuda_vec_.place())) {
        framework::Tensor tmp;
        TensorCopy(cuda_vec_, boost::get<platform::CUDAPlace>(place), &tmp);
        WaitPlace(cuda_vec_.place());
        cuda_vec_.ShareDataWith(tmp);
        // Still dirty
      } else {
        // Dirty && DataInCUDA && Device is same
        // Do nothing
      }
    } else {
      if (!IsInCUDA()) {
        // Even data is not dirty. However, data is not in CUDA. Copy data.
        TensorCopy(cpu_vec_, boost::get<platform::CUDAPlace>(place),
                   &cuda_vec_);
        WaitPlace(place);
        SetFlag(kOnCUDA);
      } else if (!(place == cuda_vec_.place())) {
        framework::Tensor tmp;
        WaitPlace(cuda_vec_.place());
        TensorCopy(cuda_vec_, boost::get<platform::CUDAPlace>(place), &tmp);
        WaitPlace(cuda_vec_.place());
        WaitPlace(place);
        cuda_vec_.ShareDataWith(tmp);
      } else {
        // Not Dirty && DataInCUDA && Device is same
        // Do nothing.
      }
    }
  }

  void ImmutableCPU() const {
    if (IsDirty() &&
        !IsInCPU()) {  // If data has been changed in CUDA, or CPU has no data.
      CopyToCPU();
      UnsetFlag(kNeedSync);
    }
    SetFlag(kOnCPU);
  }

  void UnsetFlag(int flag) const { data_status_ &= ~flag; }
  void SetFlag(int flag) const { data_status_ |= flag; }

  bool IsDirty() const { return data_status_ & kNeedSync; }

  bool IsInCUDA() const { return data_status_ & kOnCUDA; }

  bool IsInCPU() const { return data_status_ & kOnCPU; }

  static void WaitPlace(const platform::Place place) {
    if (platform::is_gpu_place(place)) {
      platform::DeviceContextPool::Instance()
          .Get(boost::get<platform::CUDAPlace>(place))
          ->Wait();
    }
  }

  enum DataStatus {
    kOnCPU = 0x01,
    kOnCUDA = 0x02,
    kNeedSync = 0x10  // data has been changed on the device.
  } data_status_;
  Tensor cpu_vec_;
  Tensor cuda_vec_;
  size_t size_;

  static T invalid_iterator_placeholder_;
};

template <typename T>
Vector<T>::Vector(size_t count, const T& value) {
  InitEmpty();
  if (count != 0) {
    resize(count);
    T* ptr = begin();
    for (size_t i = 0; i < count; ++i) {
      ptr[i] = value;
    }
  }
}

template <typename T>
Vector<T>::Vector(std::initializer_list<T> init) {
  if (init.size() == 0) {
    InitEmpty();
  } else {
    InitByIter(init.size(), init.begin(), init.end());
  }
}

template <typename T>
Vector<T>::Vector(const std::vector<T>& dat) {
  if (dat.size() == 0) {
    InitEmpty();
  } else {
    InitByIter(dat.size(), dat.begin(), dat.end());
  }
}

template <typename T>
Vector<T>& Vector<T>::operator=(const Vector<T>& other) {
  Copy(other);
  return *this;
}

template <typename T>
void Vector<T>::Move(Vector<T>&& other) {
  size_ = other.size_;
  data_status_ = other.data_status_;
  if (other.cuda_vec_.memory_size()) {
    cuda_vec_.ShareDataWith(other.cuda_vec_);
  }
  if (other.cpu_vec_.memory_size()) {
    cpu_vec_.ShareDataWith(other.cpu_vec_);
  }
}

template <typename T>
void Vector<T>::Copy(const Vector<T>& other) {
  if (other.size() != 0) {
    this->InitByIter(other.size(), other.begin(), other.end());
  } else {
    InitEmpty();
  }
}

template <typename T>
Vector<T>::reference Vector<T>::operator[](size_t i) {
  MutableCPU();
  return const_cast<T*>(cpu_vec_.data<T>())[i];
}

template <typename T>
Vector<T>::const_reference Vector<T>::operator[](size_t i) const {
  ImmutableCPU();
  return cpu_vec_.data<T>()[i];
}

template <typename T>
Vector<T>::iterator Vector<T>::begin() {
  return capacity() == 0 ? &invalid_reference_placeholder_
                         : &this->operator[](0);
}

template <typename T>
Vector<T>::iterator Vector<T>::end() {
  return capacity() == 0 ? &invalid_reference_placeholder_
                         : &this->operator[](size());
}

template <typename T>
const T* Vector<T>::begin() const {
  return capacity() == 0 ? &invalid_reference_placeholder_
                         : &this->operator[](0);
}

template <typename T>
const T* Vector<T>::end() const {
  return capacity() == 0 ? &invalid_reference_placeholder_
                         : &this->operator[](size());
}

template <typename T>
const T* Vector<T>::cbegin() const {
  return begin();
}

template <typename T>
const T* Vector<T>::cend() const {
  return end();
}

template <typename T>
T& Vector<T>::front() {
  return *begin();
}

template <typename T>
T& Vector<T>::back() {
  auto it = end();
  --it;
  return *it;
}

template <typename T>
const T& Vector<T>::back() const {
  auto it = end();
  --it;
  return *it;
}

}  // namespace framework
}  // namespace paddle
