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