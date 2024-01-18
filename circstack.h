//
// Created by ariadne lewis-towbes on 6/21/23.
//

#ifndef WALDORF_WAVE_CONTROL_CIRCSTACK_H
#define WALDORF_WAVE_CONTROL_CIRCSTACK_H

namespace ParameterMapper
{

/**
 * a stack which works within a circular buffer of memory, implementing the following operations:
 *
 *
 * @tparam T type to store in the stack
 * @tparam N size of circular buffer (number of items that can be stored in the stack without overwriting previous items)
 */
template <typename T, size_t N = 128>
struct circstack
{
    circstack(T def) : write(1)
    {
        buffer.fill(def);
    }

    std::array<T, N> buffer;
    size_t write;

    void push(T t)
    {
        buffer[write++] = t;

        if (write == N)
        {
            write = 0;
        }
    }

    //! pushes an array of Ts
    template <size_t n>
    void push(std::array<T, n> ts)
    {
        push(ts.data(), ts.size());
    }

    void push(std::vector<T> ts)
    {
        push(ts.data(), ts.size());
    }

    //! pushes from the end of the array to the beginning, which makes more sense imho
    void push(T* tps, size_t num)
    {
        for (size_t i = num; i > 0; --i)
        {
            push(tps[i]);
        }
    }

    T pop()
    {
        if (write == 0)
        {
            write = N - 1;
        }

        return buffer[--write];
    }

    T top()
    {
        auto tmp = write == 0 ? N - 1 : write - 1;
        return buffer[tmp];
    }

};

} // namespace ParameterMapper

#endif //WALDORF_WAVE_CONTROL_CIRCSTACK_H