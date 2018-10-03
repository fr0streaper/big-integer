//
// Created by fr0streaper on 02/10/18.
//

#include <memory>
#include <vector>
#include "storage.h"

storage::storage()
        : _size(0)
{}

storage::storage(size_t size, int fill)
        : _size(size)
{
    if (size > SMALL_STORAGE_SIZE)
    {
        large_storage = std::make_shared<std::vector<uint32_t> >();
        large_storage->resize(size, fill);
    }
    else
    {
        for (size_t i = 0; i < size; ++i)
        {
            small_storage[i] = fill;
        }
    }
}

storage::~storage()
{
    large_storage.reset();
}

size_t storage::size() const
{
    return _size;
}

void storage::resize(size_t new_size)
{
    if (is_small())
    {
        if (new_size > SMALL_STORAGE_SIZE)
        {
            large_storage = std::make_shared<std::vector<uint32_t> >();
            large_storage->resize(new_size);

            for (size_t i = 0; i < SMALL_STORAGE_SIZE; ++i)
            {
                large_storage->operator[](i) = small_storage[i];
            }
        }
    }
    else
    {
        if (new_size > SMALL_STORAGE_SIZE)
        {
            fix_large_storage();
            large_storage->resize(new_size);
        }
        else
        {
            for (size_t i = 0; i < new_size; ++i)
            {
                small_storage[i] = operator[](i);
            }
            large_storage.reset();
        }
    }
    _size = new_size;
}

void storage::resize(size_t new_size, int fill)
{
    size_t old_size = _size;
    resize(new_size);

    if (old_size < new_size)
    {
        for (size_t i = old_size; i < new_size; ++i)
        {
            operator[](i) = fill;
        }
    }
}

uint32_t storage::back() const
{
    return operator[](_size - 1);
}

uint32_t& storage::back()
{
    return operator[](_size - 1);
}

uint32_t storage::operator[](size_t pos) const
{
    if (is_small())
    {
        return small_storage[pos];
    }
    return large_storage->operator[](pos);
}

uint32_t& storage::operator[](size_t pos)
{
    if (is_small())
    {
        return small_storage[pos];
    }
    fix_large_storage();
    return large_storage->operator[](pos);
}

void storage::push_back(uint32_t val)
{
    resize(_size + 1);
    back() = val;
}

void storage::pop_back()
{
    resize(_size - 1);
}

storage& storage::operator=(storage const &rhs)
{
    if (!is_small())
    {
        fix_large_storage();
    }

    if (rhs.is_small())
    {
        for (size_t i = 0; i < rhs.size(); ++i)
        {
            small_storage[i] = rhs.small_storage[i];
        }
    }
    else
    {
        large_storage = std::shared_ptr<std::vector<uint32_t> >(rhs.large_storage);
    }

    _size = rhs.size();
    return *this;
}

bool storage::is_small() const
{
    return _size <= SMALL_STORAGE_SIZE;
}

void storage::fix_large_storage()
{
    if (!large_storage.unique())
    {
        large_storage = std::make_shared<std::vector<uint32_t> >(*large_storage);
    }
}

bool operator==(storage const &a, storage const &b)
{
    if (a.size() != b.size())
    {
        return false;
    }
    if (a.is_small())
    {
        for (size_t i = 0; i < a.size(); ++i)
        {
            if (a[i] != b[i])
            {
                return false;
            }
        }

        return true;
    }
    return (*a.large_storage) == (*b.large_storage);
}