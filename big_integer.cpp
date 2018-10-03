#include "big_integer.h"

#include <cstring>
#include <utility>
#include <algorithm>
#include <stdexcept>

big_integer::big_integer()
    : big_integer(0)
{}

big_integer::big_integer(big_integer const& other)
    : data(other.data)
    , sign(other.sign)
{}

big_integer::big_integer(int a)
    : sign(a < 0)
{
    data.resize(1);

    if (a == INT32_MIN)
    {
        data[0] = static_cast<uint32_t>(std::abs(a + 1)) + 1;
    }
    else
    {
        data[0] = std::abs(a);
    }
}

big_integer::big_integer(uint32_t a)
    : data(1, a)
    , sign(false)
{}

big_integer::big_integer(std::string const& str)
{
    if (str.size() == 0) 
    {
        throw std::invalid_argument("Invalid argument: non-empty string expected");
    }

    size_t start = 0;
    sign = false;
    if (str[0] == '-' || str[0] == '+')
    {
        sign = (str[0] == '-');
        ++start;
    }

    big_integer number;
    for (size_t i = start; i < str.size(); ++i)
    {
        if (!isdigit(str[i]))
        {
            throw std::invalid_argument("Invalid argument: numerical string expected");
        }

        number *= 10;
        number += (str[i] - '0');
    }

    data = std::move(number.data);
}

big_integer& big_integer::operator+=(big_integer const& rhs)
{
    if (sign == rhs.sign)
    {
        uint32_t carry = 0;

        for (size_t i = 0; i < std::max(data.size(), rhs.data.size()) || carry; ++i)
        {
            if (i == data.size())
            {
                data.push_back(0);
            }

            uint64_t current_digit = data[i];
            uint64_t current_rhs_digit = 0;
            if (i < rhs.data.size())
            {
                current_rhs_digit = rhs.data[i];
            }

            current_digit += carry + current_rhs_digit;

            carry = (current_digit >> 32);
            data[i] = current_digit;
        }
    }
    else
    {
        bool initial_sign = sign;
        bool initial_rhs_sign = rhs.sign;

        big_integer positive_rhs(rhs);
        positive_rhs.sign = false;
        sign = false;

        if (*this < positive_rhs)
        {
            std::swap(data, positive_rhs.data);

            bool tmp = initial_sign;
            initial_sign = initial_rhs_sign;
            initial_rhs_sign = tmp;
        }

        int32_t carry = 0;

        for (size_t i = 0; i < positive_rhs.data.size() || carry; ++i)
        {
            int64_t current_digit = data[i];
            int64_t current_rhs_digit = 0;
            if (i < positive_rhs.data.size())
            {
                current_rhs_digit = positive_rhs.data[i];
            }

            current_digit -= carry + current_rhs_digit;

            carry = (current_digit < 0);
            if (carry)
            {
                current_digit += ((static_cast<int64_t>(1)) << 32);
            }

            data[i] = static_cast<uint32_t>(current_digit);
        }

        sign = initial_sign;
    }

    trim();
    if (is_zero())
    {
        sign = false;
    }

    return *this;
}

big_integer& big_integer::operator-=(big_integer const& rhs)
{
    sign = !sign;
    *this += rhs;
    sign = !sign;

    return *this;
}

big_integer& big_integer::operator*=(big_integer const& rhs)
{
    if (is_zero() || rhs.is_zero())
    {
        data = std::move(storage(1, 0));
        sign = false;
        return *this;
    }

    storage new_data(data.size() + rhs.data.size(), 0);

    for (size_t i = 0; i < data.size(); ++i)
    {
        uint32_t carry = 0;

        for (size_t j = 0; j < rhs.data.size() || carry; ++j)
        {
            uint64_t current_rhs_digit = 0;
            if (j < rhs.data.size())
            {
                current_rhs_digit = rhs.data[j];
            }

            uint64_t current_digit = new_data[i + j] + data[i] * current_rhs_digit + carry;
            new_data[i + j] = static_cast<uint32_t>(current_digit);
            carry = (current_digit >> 32);
        }
    }

    data = std::move(new_data);
    sign ^= rhs.sign;

    trim();

    return *this;
}

std::pair<big_integer, uint32_t> big_integer::single_digit_div(uint32_t rhs) const
{
    if (rhs == 0)
    {
        throw std::runtime_error("Runtime error: integer division by zero");
    }

    big_integer result;
    uint32_t carry = 0;
    result.data.resize(data.size());
    result.sign = sign;

    for (ptrdiff_t i = data.size() - 1; i >= 0; --i)
    {
        uint64_t current_digit = (static_cast<uint64_t>(carry) << 32) + data[i];
        result.data[i] = current_digit / rhs;
        carry = current_digit % rhs;
    }

    result.trim();

    return std::pair<big_integer, uint32_t>(result, carry);
}

std::pair<big_integer, big_integer> big_integer::div(big_integer const& rhs) const
{
    if (rhs.is_zero())
    {
        throw std::runtime_error("Runtime error: integer division by zero");
    }

    big_integer x(*this), y(rhs);
    bool final_sign = sign ^ rhs.sign;

    x.sign = y.sign = false;

    if (x.data.size() < y.data.size())
    {
        return std::pair<big_integer, big_integer>(big_integer(), *this);
    }

    if (y.data.size() == 1)
    {
        auto result = x.single_digit_div(y.data[0]);
        big_integer result_second(result.second);

        if (!result.first.is_zero())
        {
            result.first.sign = final_sign;
        }
        if (!result_second.is_zero())
        {
            result_second.sign = sign;
        }

        return std::pair<big_integer, big_integer>(result.first, result_second);
    }

    // general case

    uint32_t normalization_parameter = (static_cast<uint64_t>(1) << 32) / (static_cast<uint64_t>(y.data.back()) + 1);

    x *= normalization_parameter;
    y *= normalization_parameter;

    storage result(x.data.size() - y.data.size() + 1, 0);

    big_integer remainder;

    for (ptrdiff_t i = x.data.size() - 1; i > x.data.size() - y.data.size(); --i)
    {
        remainder <<= 32;
        remainder += x.data[i];
    }

    for (ptrdiff_t i = x.data.size() - y.data.size(); i >= 0; --i)
    {
        remainder <<= 32;
        remainder += x.data[i];

        uint64_t remainder_approx = remainder.data.back();
        if (remainder.data.size() > y.data.size())
        {
            remainder_approx <<= 32;
            remainder_approx += remainder.data[remainder.data.size() - 2];
        }

        uint64_t quot_digit_approx = std::min(remainder_approx / static_cast<uint64_t>(y.data.back()),
                static_cast<uint64_t>(UINT32_MAX));

        big_integer digit_approx = y * static_cast<uint32_t>(quot_digit_approx);
        while (remainder < digit_approx)
        {
            quot_digit_approx--;
            digit_approx -= y;
        }

        result[i] = static_cast<uint32_t>(quot_digit_approx);
        remainder -= digit_approx;
    }

    big_integer quotient;
    quotient.sign = final_sign;
    quotient.data = std::move(result);
    quotient.trim();

    remainder.sign = sign;
    remainder /= normalization_parameter;
    remainder.trim();

    return std::pair<big_integer, big_integer>(quotient, remainder);
}

big_integer& big_integer::operator/=(big_integer const& rhs)
{
    *this = div(rhs).first;
    return *this;
}

big_integer& big_integer::operator%=(big_integer const& rhs)
{
    *this = div(rhs).second;
    return *this;
}

storage twos_complement(big_integer const &x) {
    if (!x.sign)
    {
        return x.data;
    }

    big_integer result(x);
    result.sign = false;

    for (size_t i = 0; i < result.data.size(); ++i)
    {
        result.data[i] = ~result.data[i];
    }

    result++;
    return std::move(result.data);
}

big_integer undo_twos_complement(storage const &cdata) {
    big_integer result;
    result.data = cdata;
    bool actual_sign = (cdata.back() & (1 << 31));

    if (actual_sign)
    {
        result--;
        for (size_t i = 0; i < result.data.size(); ++i)
        {
            result.data[i] = ~result.data[i];
        }
    }

    result.sign = actual_sign;
    result.trim();

    return result;
}

template<class FunctionT>
big_integer& big_integer::apply_bitwise_operation(big_integer const& rhs, FunctionT function)
{
    storage
            lhs_data = twos_complement(*this),
            rhs_data = twos_complement(rhs);

    if (lhs_data.size() < rhs_data.size())
    {
        lhs_data.resize(rhs_data.size(), 0);
    }

    for (size_t i = 0; i < lhs_data.size(); ++i)
    {
        uint32_t current_rhs_digit = 0;
        if (i < rhs_data.size())
        {
            current_rhs_digit = rhs_data[i];
        }

        lhs_data[i] = function(lhs_data[i], current_rhs_digit);
    }

    *this = undo_twos_complement(lhs_data);

    if (is_zero())
    {
        sign = false;
    }

    return *this;
}

big_integer& big_integer::operator&=(big_integer const& rhs)
{
    return apply_bitwise_operation(rhs, std::bit_and<uint32_t>());
}

big_integer& big_integer::operator|=(big_integer const& rhs)
{
    return apply_bitwise_operation(rhs, std::bit_or<uint32_t>());
}

big_integer& big_integer::operator^=(big_integer const& rhs)
{
    return apply_bitwise_operation(rhs, std::bit_xor<uint32_t>());
}

big_integer& big_integer::operator<<=(int rhs)
{
    big_integer result(*this);
    result.data.resize(data.size() + (rhs + 31) / 32, 0);

    int inner_shift = rhs & 31;
    int outer_shift = (rhs >> 5);

    if (outer_shift)
    {
        for (ptrdiff_t i = data.size() + outer_shift - 1; i >= outer_shift; --i)
        {
            result.data[i] = result.data[i - outer_shift];
        }

        for (ptrdiff_t i = outer_shift - 1; i >= 0; --i)
        {
            result.data[i] = 0;
        }
    }

    if (inner_shift)
    {
        for (ptrdiff_t i = data.size() + outer_shift - 1; i >= 0; --i)
        {
            result.data[i + 1] |= (result.data[i] >> (32 - inner_shift));
            result.data[i] <<= inner_shift;
        }
    }

    data = std::move(result.data);
    trim();

    return *this;
}

big_integer& big_integer::operator>>=(int rhs)
{
    storage result(twos_complement(*this));

    int inner_shift = rhs & 31;
    int outer_shift = (rhs >> 5);

    if (outer_shift)
    {
        for (size_t i = 0; i < result.size() - outer_shift; ++i)
        {
            result[i] = result[i + outer_shift];
        }

        uint32_t value = 0;
        if (result.back() >> 31)
        {
            value = UINT32_MAX;
        }
        for (size_t i = result.size() - outer_shift; i < result.size(); ++i)
        {
            result[i] = value;
        }
    }

    if (inner_shift)
    {
        uint32_t saved_complement = ((result.back() >> (32 - inner_shift)) << (32 - inner_shift));

        for (size_t i = 0; i < result.size() - outer_shift; ++i)
        {
            if (i)
            {
                result[i - 1] |= (result[i] << (32 - inner_shift));
            }
            result[i] >>= inner_shift;
        }

        result[result.size() - outer_shift - 1] |= saved_complement;
    }

    *this = undo_twos_complement(result);
    trim();

    return *this;
}

big_integer big_integer::operator+() const
{
    return *this;
}

big_integer big_integer::operator-() const
{
    if (is_zero())
    {
        return *this;
    }

    big_integer tmp(*this);
    tmp.sign = !tmp.sign;
    return tmp;
}

big_integer big_integer::operator~() const
{
    return -(*this) - 1;
}

big_integer& big_integer::operator++()
{
    *this += 1;
    return *this;
}

big_integer big_integer::operator++(int)
{
    big_integer tmp(*this);
    ++(*this);
    return tmp;
}

big_integer& big_integer::operator--()
{
    *this -= 1;
    return *this;
}

big_integer big_integer::operator--(int)
{
    big_integer tmp(*this);
    --(*this);
    return tmp;
}

big_integer operator+(big_integer a, big_integer const& b)
{
    return a += b;
}

big_integer operator-(big_integer a, big_integer const& b)
{
    return a -= b;
}

big_integer operator*(big_integer a, big_integer const& b)
{
    return a *= b;
}

big_integer operator/(big_integer a, big_integer const& b)
{
    return a /= b;
}

big_integer operator%(big_integer a, big_integer const& b)
{
    return a %= b;
}

big_integer operator&(big_integer a, big_integer const& b)
{
    return a &= b;
}

big_integer operator|(big_integer a, big_integer const& b)
{
    return a |= b;
}

big_integer operator^(big_integer a, big_integer const& b)
{
    return a ^= b;
}

big_integer operator<<(big_integer a, int b)
{
    return a <<= b;
}

big_integer operator>>(big_integer a, int b)
{
    return a >>= b;
}

bool operator==(big_integer const& a, big_integer const& b)
{
    return a.sign == b.sign && a.data == b.data;
}

bool operator!=(big_integer const& a, big_integer const& b)
{
    return !(a == b);
}

bool operator<(big_integer const& a, big_integer const& b)
{
    if (a.sign != b.sign)
    {
        return a.sign;
    }
    if (a.data.size() != b.data.size())
    {
        if (a.sign)
        {
            return a.data.size() > b.data.size();
        }
        else
        {
            return a.data.size() < b.data.size();
        }
    }

    for (ptrdiff_t i = a.data.size() - 1; i >= 0; --i)
    {
        if (a.data[i] != b.data[i])
        {
            if (a.sign)
            {
                return a.data[i] > b.data[i];
            }
            else
            {
                return a.data[i] < b.data[i];
            }
        }
    }

    return false;
}

bool operator>(big_integer const& a, big_integer const& b)
{
    return b < a;
}

bool operator<=(big_integer const& a, big_integer const& b)
{
    return !(a > b);
}

bool operator>=(big_integer const& a, big_integer const& b)
{
    return !(a < b);
}

std::string to_string(big_integer const& a)
{
    if (a.is_zero())
    {
        return "0";
    }

    std::string result = "";
    big_integer tmp(a);

    while (!tmp.is_zero())
    {
        auto digit = tmp.single_digit_div(10);
        result += static_cast<char>(digit.second + '0');
        tmp = digit.first;
    }

    if (a.sign)
    {
        result += "-";
    }

    std::reverse(result.begin(), result.end());

    return result;
}

std::ostream& operator<<(std::ostream& s, big_integer const& a)
{
    return s << to_string(a);
}

void big_integer::trim()
{
    while (data.size() > 1 && data.back() == 0)
    {
        data.pop_back();
    }
}

bool big_integer::is_zero() const
{
    return (data.size() == 1 && data[0] == 0);
}

