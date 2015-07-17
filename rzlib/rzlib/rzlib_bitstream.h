#pragma once

#include "rzlib_common.h"
#include <type_traits>

namespace rzlib
{
    struct no_byte_swap
    {
        template <typename IntType>
        static IntType swap(IntType input)
        {
            static_assert(
                std::is_unsigned<IntType>::value && std::is_integral<IntType>::value,
                "byte swap only supports unsigned integral types");

            return input;
        }
    };

    struct big_endian_byte_swap
    {
        template <typename IntType = uint64_t>
        static uint64_t swap(uint64_t input)
        {
            return _byteswap_uint64(input);
        }

        template <typename IntType = uint32_t>
        static uint32_t swap(uint32_t input)
        {
            return _byteswap_ulong(input);
        }

        template <typename IntType = uint16_t>
        static uint16_t swap(uint16_t input)
        {
            return _byteswap_ushort(input);
        }

        template <typename IntType = uint8_t>
        static uint8_t swap(uint8_t input)
        {
            return input;
        }

        template <typename IntType>
        static IntType swap(IntType input)
        {
            static_assert(false, "byte swap only supports unsigned integral types");
            return 0;
        }
    };

    // Should be stripped out in release builds (inlined empty function)
    struct no_masking
    {
        template <typename IntType>
        static IntType mask(IntType input, int num_bits)
        {
            static_assert(
                std::is_unsigned<IntType>::value && std::is_integral<IntType>::value,
                "masking only supports unsigned integral types");

            return input;
        }
    };

    struct bit_masking
    {
        template <typename IntType>
        static IntType mask(IntType input, int num_bits)
        {
            static_assert(
                std::is_unsigned<IntType>::value && std::is_integral<IntType>::value,
                "masking only supports unsigned integral types");

            return input & ((1 << num_bits) - 1);
        }
    };

    template <typename TBlock, typename TByte_Swap = no_byte_swap, typename TMask = no_masking>
    class bitstream
    {
    public:
        bitstream(const TBlock* source, size_t num_blocks)
            : source(source), pCurrent(source), num_blocks(num_blocks), num_bits_per_block(sizeof(TBlock) * 8)
        {
            current = TByte_Swap::swap(*pCurrent);
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
                current = TByte_Swap::swap(*(++pCurrent));
                if (pCurrent - source == num_blocks && num_bits > remaining_bits_this_block)
                {
                    // reading past end of stream
                    return false;
                }

                remaining_bits_this_block = num_bits_per_block - num_bits_from_first_block;
                *result |= current >> remaining_bits_this_block;
            }

            *result = TMask::mask(*result, num_bits);

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
