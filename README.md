# PoolAllocator 

STL-compatible memory pool allocator

**Key Features**  
- 📦 O(1) allocations/deallocations  
- 🧩 STL-compatible
- 🚀 Single-header implementation (`#pragma once`)  
- 🔧 Customizable block size via template  
- 🚧 *Coming soon: Multithreading support, benchmarks, Google Tests*

## Usage

```cpp
#include "pool_allocator.h"
#include "list"
#include "iostream"
// STL container example
int main() {
    std::list<int,PoolAllocator<int>> ls;
    for(int i = 0; i < 10; ++i) {
        ls.push_back(i);
    }
    for(int i = 0; i < 10; ++i) {
        std::cout<<ls.back()<<std::endl;
        ls.pop_back();
    }
}