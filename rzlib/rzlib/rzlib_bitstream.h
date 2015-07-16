#pragma once

#include "rzlib_common.h"
#include <type_traits>

namespace rzlib
{
    // Should be stripped out in release builds (inlined empty function)
    struct no_byte_swap
    {
        template <typename IntType>
        static void swap(IntType& input)
        {
            static_assert(
                std::is_unsigned<IntType>::value && std::is_integral<IntType>::value,
                "byte swap only supports unsigned integral types");
        }
    };

    struct big_endian_byte_swap
    {
        template <typename IntType>
        static void swap(IntType& input)
        {
            static_assert(
                std::is_unsigned<IntType>::value && std::is_integral<IntType>::value && sizeof(IntType) > 1,
                "byte swap only supports unsigned integral types greater than 1 byte");

            uint8_t* p = reinterpret_cast<uint8_t*>(&input);
            for (int i = 0; i < sizeof(IntType) / 2; ++i)
            {
                uint8_t temp = p[i];
                p[i] = p[sizeof(IntType) - 1 - i];
                p[sizeof(IntType) - 1 - i] = temp;
            }
        }
    };

    // Should be stripped out in release builds (inlined empty function)
    struct no_masking
    {
        template <typename IntType>
        static void mask(IntType& input, int num_bits)
        {
            static_assert(
                std::is_unsigned<IntType>::value && std::is_integral<IntType>::value,
                "masking only supports unsigned integral types");
        }
    };

    struct bit_masking
    {
        template <typename IntType>
        static void mask(IntType& input, int num_bits)
        {
            static_assert(
                std::is_unsigned<IntType>::value && std::is_integral<IntType>::value,
                "masking only supports unsigned integral types");
            input = input & ((1 << num_bits) - 1);
        }
    };

    template <typename TBlock, typename TByte_Swap = no_byte_swap, typename TMask = no_masking>
    class bitstream
    {
    public:
        bitstream(const TBlock* source, size_t num_blocks)
            : source(source), pCurrent(source), num_blocks(num_blocks), num_bits_per_block(sizeof(TBlock) * 8)
        {
            current = *pCurrent;
            TByte_Swap::swap(current);
            remaining_bits_this_block = num_bits_per_block;
        }

        template <typename TResult>
        bool read_bits(int num_bits, TResult* result)
        {
            assert(sizeof(TResult) * 8 >= num_bits);

            if (pCurrent - source == num_blocks)
            {
                // stream empty
                return false;
            }

            if (remaining_bits_this_block > num_bits)
            {
                *result = current >> (remaining_bits_this_block - num_bits);
                remaining_bits_this_block -= num_bits;
            }
            else
            {
                int num_bits_from_first_block = num_bits - remaining_bits_this_block;
                *result = current << num_bits_from_first_block;

                // Read in new block
                current = *(++pCurrent);
                TByte_Swap::swap(current);

                remaining_bits_this_block = num_bits_per_block - num_bits_from_first_block;
                *result |= current >> remaining_bits_this_block;
            }

            TMask::mask(*result, num_bits);

            return true;
        }

    private:
        bitstream(const bitstream&) = delete;
        bitstream& operator= (const bitstream&) = delete;

    private:
        const TBlock* source;
        const TBlock* pCurrent;
        TBlock current;
        size_t num_blocks;
        size_t num_bits_per_block;
        int remaining_bits_this_block;
    };

} // namespace rzlib
