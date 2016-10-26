/* -*- C++ -*- */
//
// Copyright 2016 WebAssembly Community Group participants
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Defines a circular vector.

#ifndef DECOMPRESSOR_SRC_CIRCULAR_VECTOR_H
#define DECOMPRESSOR_SRC_CIRCULAR_VECTOR_H

#include <cassert>
#include <iterator>
#include <vector>

namespace wasm {

namespace utils {

// WARNING: May not destroy popped elements during the pop. However,
// the elements will eventually get destroyed (no later than destruction).
template<class T, class Alloc = std::allocator<T>>
class circular_vector {
 public:
  typedef T value_type;
  typedef std::allocator<T> allocator_type;
  typedef value_type& reference;
  typedef const value_type& const_reference;
  typedef value_type* pointer;
  typedef const value_type* const_pointer;
  typedef size_t size_type;

  class circ_iterator : public std::bidirectional_iterator_tag {

    circ_iterator() : circ_vector(nullptr), index(0) {}

    explicit circ_iterator(circ_iterator& i)
        : circ_vector(i.circ_vector), index(i.index) {}

    explicit circ_iterator(circular_vector& v, size_t index)
        : circ_vector(&v), index(index) {}

    ~circ_iterator();

    circ_iterator& operator=(const circ_iterator& i) {
      circ_vector = i.circ_vector;
      index = i.index;
    }

    bool operator==(const circ_iterator& i) const {
      return circ_vector == i.circ_vector && index == i.index;
    }

    bool operator!=(const circ_iterator& i) const {
      return circ_vector != i.circ_vector || index != i.index;
    }

    reference operator*() {
      assert(circ_vector != nullptr);
      return (*circ_vector)[index];
    }

    const_reference operator*() const {
      assert(circ_vector != nullptr);
      return (*circ_vector)[index];
    }

    reference operator->() {
      assert(circ_vector != nullptr);
      return (*circ_vector)[index];
    }

    const_reference operator->() const {
      assert(circ_vector != nullptr);
      return (*circ_vector)[index];
    }

    value_type* operator++() {
      assert(circ_vector != nullptr);
      return &(*circ_vector)[++index];
    }

    value_type* operator++(int) {
      assert(circ_vector != nullptr);
      return *(*circ_vector)[index++];
    }

    value_type* operator--() {
      assert(circ_vector != nullptr);
      return &(*circ_vector)[--index];
    }

    value_type* operator--(int) {
      assert(circ_vector != nullptr);
      return &(*circ_vector)[index--];
    }

   private:
    circular_vector* circ_vector;
    size_t index;
  };

  typedef circ_iterator iterator;
  typedef const circ_iterator const_iterator;
  typedef circ_iterator reverse_iterator;
  typedef const circ_iterator const_reverse_iterator;


  explicit circular_vector(size_type vector_max_size,
                           const value_type& value = value_type(),
                           allocator_type alloc = allocator_type())
      : contents(value, alloc),
        vector_max_size(vector_max_size),
        start_index(0),
        vector_size(0) {
    assert(vector_max_size <= contents.max_size());
    contents.reserve(vector_max_size);
  }

  explicit circular_vector(const circular_vector& cv)
      : contents(cv.get_allocator()), vector_max_size(cv.vector_max_size) {
    for (const auto v : cv)
      contents.push_back(v);
  }

  ~circular_vector() {}

  size_type size() const { return vector_size; }

  size_type max_size() const { return vector_max_size; }

  size_type capacity() const { return contents.capacity(); }

  bool empty() const { return vector_size == 0; }

  bool full() const { return vector_size == vector_max_size; }

  iterator begin() {
    return circ_iterator(*this, 0);
  }

  const_iterator begin() const {
    return circ_iterator(*this, 0);
  }

  iterator end() {
    return circ_iterator(*this, size());
  }

  const_iterator end() const {
    return circ_iterator(*this, size());
  }

  reverse_iterator rbegin() {
    return circ_iterator(*this, size() - 1);
  }

  const_reverse_iterator rbegin() const {
    return circ_iterator(*this, size() - 1);
  }

  reverse_iterator rend() {
    return circ_iterator(*this, size_type(-1));
  }

  const_reverse_iterator rend() const {
    return circ_iterator(*this, size_type(-1));
  }

  reference operator[](size_type n) {
    return contents[get_index(n)];
  }

  const_reference operator[](size_type n) const {
    return contents[get_index(n)];
  }

  reference at(size_type n) { return contents.at(get_index(n)); }

  const_reference at(size_type n) const {
    return contents.at(get_index(n));
  }

  reference front() { return contents[get_index(0)]; }

  const_reference front() const {
    return contents[get_index(0)];
  }

  reference back() { return contents[get_index(vector_size - 1)]; }

  const_reference back() const {
    return contents[get_index(vector_size - 1)];
  }

  void push_front(const value_type& v) {
    if (vector_size < vector_max_size)
      ++vector_size;
    dec_start_index();
    contents[start_index] = v;
  }

  void push_back(const value_type& v) {
    if (vector_size == vector_max_size) {
      contents[start_index] = v;
      inc_start_index();
      return;
    }
    contents[get_index(vector_size++)] = v;
  }

  void pop_front() {
    assert(vector_size > 0);
    --vector_size;
    inc_start_index();
  }

  void pop_back() {
    assert(vector_size > 0);
    --vector_size;
  }

  void swap(circular_vector& cv) {
    contents.swap(cv.contents);
    std::swap(vector_max_size, cv.vector_max_size);
    std::swap(start_index, cv.start_index);
    std::swap(vector_size, cv.vector_size);
  }

  void clear() {
    contents.clear();
    start_index = 0;
    vector_size = 0;
  }

  void resize(size_type new_size) {
    assert(new_size <= contents.max_size());
    rotate_start_to_zero();
    if (new_size >= vector_max_size)
      vector_max_size = new_size;
    else
      for (size_t i = new_size, e = vector_max_size; i < e; ++i)
        pop_front();
  }

  allocator_type get_allocator() const { return contents.get_allocator(); }

 private:
  std::vector<T, Alloc> contents;
  size_type vector_max_size;
  size_type start_index;
  size_type vector_size;
  size_type get_index(size_type n) const {
    return (start_index + n) % vector_max_size;
  }
  void inc_start_index() {
    start_index = (start_index + 1) % vector_max_size;
  }
  void dec_start_index() {
    if (start_index == 0)
      start_index = vector_max_size - 1;
    else
      --start_index;
  }
  void rotate_start_to_zero() {
    circular_vector tmp(*this);
    clear();
    for (size_type i = 0, e = tmp.size(); i < e; ++i)
      push_back(tmp[i]);
  }
};

} // end of namespace utils

} // end of namespace wasm

#endif // DECOMPRESSOR_SRC_CIRCULAR_VECTOR_H
