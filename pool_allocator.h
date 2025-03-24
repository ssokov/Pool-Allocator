#pragma once
#include <iostream>
#include <memory>
#include <cstdlib>
#include <new>
#include <type_traits>

template <typename T, size_t kBlockSize = 1024>
class PoolAllocator {
 private:
  union Chunk {
    Chunk* next;
    alignas(T) char data[sizeof(T)];
  };

  static constexpr size_t kChunkSize = sizeof(Chunk);
  static constexpr size_t kAlignment = alignof(T);
  static constexpr size_t kAlignedSize = ((kChunkSize + kAlignment - 1) / kAlignment) * kAlignment;

  Chunk* free_list_ = nullptr;
  void* memory_block_ = nullptr;

 public:
  using value_type = T;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using propagate_on_container_copy_assignment = std::true_type;
  using propagate_on_container_move_assignment = std::true_type;
  using propagate_on_container_swap = std::false_type;
  using is_always_equal = std::false_type;

  template <typename U>
  struct rebind {
    using other = PoolAllocator<U, kBlockSize>;
  };

  // Copy constructor: performs a deep copy of the allocator's state.
  PoolAllocator(const PoolAllocator& other) {
    try {
      memory_block_ = ::operator new(kBlockSize * kAlignedSize, std::align_val_t{kAlignment});
    } catch (const std::bad_alloc& e) {
      std::cerr << "Copy Constructor: Memory allocation failed: " << e.what() << "\n";
      throw;
    }
    for (size_t i = 0; i < kBlockSize; ++i) {
      Chunk* orig_chunk = reinterpret_cast<Chunk*>(
          reinterpret_cast<char*>(other.memory_block_) + i * kAlignedSize);
      Chunk* new_chunk = reinterpret_cast<Chunk*>(
          reinterpret_cast<char*>(memory_block_) + i * kAlignedSize);
      bool is_free = false;
      for (Chunk* free_ptr = other.free_list_; free_ptr != nullptr; free_ptr = free_ptr->next) {
        if (free_ptr == orig_chunk) {
          is_free = true;
          break;
        }
      }
      if (!is_free) {
        new (new_chunk->data) T(*reinterpret_cast<const T*>(orig_chunk->data));
      }
    }
    if (other.free_list_ != nullptr) {
      Chunk* old_ptr = other.free_list_;
      size_t chunk_index = (reinterpret_cast<char*>(old_ptr) -
                            reinterpret_cast<char*>(other.memory_block_)) /
                           kAlignedSize;
      free_list_ = reinterpret_cast<Chunk*>(
          reinterpret_cast<char*>(memory_block_) + chunk_index * kAlignedSize);
      Chunk* new_current = free_list_;

      while (old_ptr->next != nullptr) {
        old_ptr = old_ptr->next;
        chunk_index = (reinterpret_cast<char*>(old_ptr) -
                       reinterpret_cast<char*>(other.memory_block_)) / kAlignedSize;
        new_current->next = reinterpret_cast<Chunk*>(
            reinterpret_cast<char*>(memory_block_) + chunk_index * kAlignedSize);
        new_current = new_current->next;
      }
      new_current->next = nullptr;
    } else {
      free_list_ = nullptr;
    }
  }

  PoolAllocator& operator=(const PoolAllocator& other) {
    if (this != &other) {
      PoolAllocator temp(other);
      swap(temp);
    }
    return *this;
  }

  PoolAllocator(PoolAllocator&& other) noexcept
      : memory_block_(other.memory_block_), free_list_(other.free_list_) {
    other.memory_block_ = nullptr;
    other.free_list_ = nullptr;
  }

  PoolAllocator& operator=(PoolAllocator&& other) noexcept {
    if (this != &other) {
      if (memory_block_) {
        ::operator delete(memory_block_, std::align_val_t{kAlignment});
      }
      memory_block_ = other.memory_block_;
      free_list_ = other.free_list_;
      other.memory_block_ = nullptr;
      other.free_list_ = nullptr;
    }
    return *this;
  }

  PoolAllocator() {
    static_assert(kBlockSize > 0, "Block size must be positive");
    try {
      memory_block_ = ::operator new(kBlockSize * kAlignedSize, std::align_val_t{kAlignment});
    } catch (const std::bad_alloc& e) {
      std::cerr << "Default Constructor: Memory allocation failed: " << e.what() << "\n";
      throw;
    }
    free_list_ = static_cast<Chunk*>(memory_block_);
    Chunk* current = free_list_;
    for (size_t i = 0; i < kBlockSize - 1; ++i) {
      Chunk* next_chunk = reinterpret_cast<Chunk*>(
          reinterpret_cast<char*>(current) + kAlignedSize);
      current->next = next_chunk;
      current = next_chunk;
    }
    current->next = nullptr;
  }

  [[nodiscard]] T* allocate(size_t n = 1) {
    if (n != 1) {
      std::cerr << "PoolAllocator::allocate: Only single object allocations are supported\n";
      throw std::bad_alloc();
    }
    if (!free_list_) {
      std::cerr << "PoolAllocator::allocate: Memory pool exhausted\n";
      throw std::bad_alloc();
    }
    Chunk* chunk = free_list_;
    free_list_ = free_list_->next;
    return std::launder(reinterpret_cast<T*>(chunk->data));
  }

  void deallocate(T* p, size_t n = 1) noexcept {
    if (!p || n != 1) return;
    Chunk* chunk = std::launder(reinterpret_cast<Chunk*>(p));
    chunk->next = free_list_;
    free_list_ = chunk;
  }

  ~PoolAllocator() noexcept {
    if (memory_block_) {
      ::operator delete(memory_block_, std::align_val_t{kAlignment});
    }
  }

  [[nodiscard]] size_t max_size() const noexcept { return kBlockSize; }

  [[nodiscard]] bool is_valid() const noexcept { return memory_block_ != nullptr; }
  bool operator==(const PoolAllocator& other) const noexcept {
    return this == &other; 
  }

  bool operator!=(const PoolAllocator& other) const noexcept {
    return !(*this == other);
  }


 private:
  void swap(PoolAllocator& other) noexcept {
    std::swap(memory_block_, other.memory_block_);
    std::swap(free_list_, other.free_list_);
  }
};
