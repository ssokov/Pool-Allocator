#pragma once
#include <iostream>
#include <memory>
#include <cstdlib>
#include <new>
#include <type_traits>

template <typename T, size_t BlockSize = 1024>
class PoolAllocator {
 private:
  union Chunk {
    Chunk* next;
    alignas(T) char data[sizeof(T)];
  };

  static constexpr size_t kChunkSize = sizeof(Chunk);
  static constexpr size_t kAlignment = alignof(T);
  static constexpr size_t kAlignedSize =
      ((kChunkSize + kAlignment - 1) / kAlignment) * kAlignment;

  Chunk* freeList_ = nullptr;
  void* memoryBlock_ = nullptr;
 public:
  using value_type = T;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using propagateOnContainerCopyAssignment = std::true_type;
  using propagateOnContainerMoveAssignment = std::true_type;
  using propagateOnContainerSwap = std::false_type;
  using isAlwaysEqual = std::false_type;

  template <typename U>
  struct rebind {
    using other = PoolAllocator<U, BlockSize>;
  };

  // Copy constructor: performs a deep copy of the allocator's state.
  PoolAllocator(const PoolAllocator& other) {
    memoryBlock_ = ::operator new(BlockSize * kAlignedSize, std::align_val_t{kAlignment});
    for (size_t i = 0; i < BlockSize; ++i) {
      Chunk* origChunk = reinterpret_cast<Chunk*>(
          reinterpret_cast<char*>(other.memoryBlock_) + i * kAlignedSize);
      Chunk* newChunk = reinterpret_cast<Chunk*>(
          reinterpret_cast<char*>(memoryBlock_) + i * kAlignedSize);
      bool isFree = false;
      for (Chunk* ptr = other.freeList_; ptr != nullptr; ptr = ptr->next) {
        if (ptr == origChunk) {
          isFree = true;
          break;
        }
      }
      if (!isFree) {
        new (newChunk->data)
            T(*reinterpret_cast<const T*>(origChunk->data));
      }
    }
    if (other.freeList_ != nullptr) {
      Chunk* oldPtr = other.freeList_;
      size_t index = (reinterpret_cast<char*>(oldPtr) -
                      reinterpret_cast<char*>(other.memoryBlock_)) /
                     kAlignedSize;
      freeList_ = reinterpret_cast<Chunk*>(
          reinterpret_cast<char*>(memoryBlock_) + index * kAlignedSize);
      Chunk* newCurrent = freeList_;
      while (oldPtr->next != nullptr) {
        oldPtr = oldPtr->next;
        index = (reinterpret_cast<char*>(oldPtr) -
                 reinterpret_cast<char*>(other.memoryBlock_)) / kAlignedSize;
        newCurrent->next = reinterpret_cast<Chunk*>(
            reinterpret_cast<char*>(memoryBlock_) + index * kAlignedSize);
        newCurrent = newCurrent->next;
      }
      newCurrent->next = nullptr;
    } else {
      freeList_ = nullptr;
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
      : memoryBlock_(other.memoryBlock_), freeList_(other.freeList_) {
    other.memoryBlock_ = nullptr;
    other.freeList_ = nullptr;
  }

  
  PoolAllocator& operator=(PoolAllocator&& other) noexcept {
    if (this != &other) {
      if (memoryBlock_) {
        ::operator delete(memoryBlock_, std::align_val_t{kAlignment});
      }
      memoryBlock_ = other.memoryBlock_;
      freeList_ = other.freeList_;
      other.memoryBlock_ = nullptr;
      other.freeList_ = nullptr;
    }
    return *this;
  }

  PoolAllocator() {
    static_assert(BlockSize > 0, "Block size must be positive");
    memoryBlock_ = ::operator new(BlockSize * kAlignedSize,
                                  std::align_val_t{kAlignment});
    freeList_ = static_cast<Chunk*>(memoryBlock_);
    Chunk* current = freeList_;
    for (size_t i = 0; i < BlockSize - 1; ++i) {
      Chunk* nextChunk = reinterpret_cast<Chunk*>(
          reinterpret_cast<char*>(current) + kAlignedSize);
      current->next = nextChunk;
      current = nextChunk;
    }
    current->next = nullptr;
  }

  
  [[nodiscard]] T* allocate(size_t n = 1) {
    if (n != 1 || !freeList_) {
      throw std::bad_alloc();
    }
    Chunk* chunk = freeList_;
    freeList_ = freeList_->next;
    return std::launder(reinterpret_cast<T*>(chunk->data));
  }

  
  void deallocate(T* p, size_t n = 1) noexcept {
    if (!p || n != 1) return;
    Chunk* chunk = std::launder(reinterpret_cast<Chunk*>(p));
    chunk->next = freeList_;
    freeList_ = chunk;
  }

  ~PoolAllocator() noexcept {
    if (memoryBlock_) {
      ::operator delete(memoryBlock_, std::align_val_t{kAlignment});
    }
  }


  [[nodiscard]] size_t max_size() const noexcept { return BlockSize; }

  [[nodiscard]] bool is_valid() const noexcept { return memoryBlock_ != nullptr; }

 private:
  void swap(PoolAllocator& other) noexcept {
    std::swap(memoryBlock_, other.memoryBlock_);
    std::swap(freeList_, other.freeList_);
  }
};
