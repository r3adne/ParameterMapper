//
// Created by ariadne lewis-towbes on 6/21/23.
//

#ifndef WALDORF_WAVE_CONTROL_CIRCSTACK_H
#define WALDORF_WAVE_CONTROL_CIRCSTACK_H

#endif //WALDORF_WAVE_CONTROL_CIRCSTACK_H


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
    circstack() = default;

    std::array<T, N> buffer;
    size_t write = 0;

    void push(T t)
    {
        buffer[write] = t;
        ++write;
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





//    T pop()


};