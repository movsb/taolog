#pragma once

#include <vector>

namespace taoetw {

class MemPool
{
public:
    MemPool() {}
    virtual ~MemPool() {}

    MemPool(const MemPool&) = delete;
    MemPool(MemPool&&) = delete;
    void operator=(const MemPool&) = delete;

    virtual void* alloc() = 0;
    virtual void destroy(void* p) = 0;

    virtual void clear() = 0;
};

template<typename T, size_t block_size>
class MemPoolT : public MemPool
{
    static_assert(block_size > 0, "block_size should > 0");

public:
    MemPoolT()
        : _root(nullptr)
    { }

    virtual ~MemPoolT()
    {
        clear();
    }

    MemPoolT(const MemPoolT&) = delete;
    MemPoolT(MemPoolT&&) = delete;
    void operator=(const MemPoolT&) = delete;

    virtual void* alloc() override;
    virtual void destroy(void* p) override;

    virtual void clear() override;

protected:
    union Chunk {
        Chunk* next;
        char memo[sizeof(T)];
    };

    struct Block {
        Block()
        {
            for(size_t i = 0; i < block_size - 1; ++i)
                chunks[i].next = &chunks[i+1];

            chunks[block_size - 1].next = nullptr;
        }

        Chunk chunks[block_size];
    };

protected:
    std::vector<Block*> _blocks;
    Chunk*              _root;
};

template<typename T, size_t block_size>
inline void* MemPoolT<T, block_size>::alloc()
{
    if(!_root) {
        auto block = new Block();
        _root = &block->chunks[0];
        _blocks.emplace_back(block);
    }

    auto ret = _root;
    _root = _root->next;
    return ret;
}

template<typename T, size_t block_size>
inline void MemPoolT<T, block_size>::destroy(void* p)
{
    if(p) {
        auto chunk = static_cast<Chunk*>(p);
        chunk->next = _root;
        _root = chunk;
    }
}

template<typename T, size_t block_size>
inline void MemPoolT<T, block_size>::clear()
{
    for(auto& blk : _blocks)
        delete blk;

    _blocks.resize(0);
    _root = nullptr;
}

}
