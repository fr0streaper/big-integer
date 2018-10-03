//
// Created by fr0streaper on 02/10/18.
//

#ifndef BIGINT_STORAGE_H
#define BIGINT_STORAGE_H

#include <memory>
#include <vector>

struct storage
{
    storage();
    storage(size_t size, int fill);

    ~storage();

    size_t size() const;
    void resize(size_t new_size);
    void resize(size_t new_size, int fill);

    uint32_t back() const;
    uint32_t& back();

    uint32_t operator[](size_t pos) const;
    uint32_t& operator[](size_t pos);

    void push_back(uint32_t val);
    void pop_back();

    storage& operator=(storage const& rhs);
    friend bool operator==(storage const& a, storage const& b);

private:

    static const size_t SMALL_STORAGE_SIZE = 4;
    size_t _size;

    //union {
        uint32_t small_storage[SMALL_STORAGE_SIZE];
        std::shared_ptr<std::vector<uint32_t> > large_storage;
    //};

    bool is_small() const;

    void fix_large_storage();
};

bool operator==(storage const& a, storage const& b);

#endif //BIGINT_STORAGE_H
