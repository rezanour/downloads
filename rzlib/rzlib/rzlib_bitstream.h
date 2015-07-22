#pragma once

#include "rzlib_common.h"
#include <type_traits>
#include <vector>

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

    template<typename TBlock>
    class bitstream_writer;

    template <typename TBlock, typename TByte_Swap = no_byte_swap, typename TMask = no_masking>
    class bitstream_reader
    {
    public:
        bitstream_reader(const TBlock* source, size_t num_blocks)
            : data(num_blocks), num_bits_per_block(sizeof(TBlock) * 8)
        {
            // Push in data backwards, so we can pop from the back
            // which is more efficient
            for (size_t i = 0; i < num_blocks; ++i)
            {
                data[num_blocks - i - 1] = source[i];
            }

            current = TByte_Swap::swap(data.back());
            data.pop_back();

            remaining_bits_this_block = num_bits_per_block;
        }

        bitstream_reader(const bitstream_writer<TBlock>& writer)
            : data(writer.data.size()), num_bits_per_block(writer.num_bits_per_block)
            , bits_used_in_last_block(writer.num_bits_per_block - writer.remaining_bits_this_block)
        {
            // Push in data backwards, so we can pop from the back
            // which is more efficient
            for (size_t i = 0; i < data.size(); ++i)
            {
                data[data.size() - i - 1] = writer.data[i];
            }

            current = TByte_Swap::swap(data.back());
            data.pop_back();

            if (data.empty())
            {
                remaining_bits_this_block = bits_used_in_last_block;
            }
            else
            {
                remaining_bits_this_block = num_bits_per_block;
            }
        }

        template <typename TResult>
        bool read_bits(int num_bits, TResult* result)
        {
            assert(sizeof(TResult) * 8 >= num_bits);

            if (remaining_bits_this_block > num_bits)
            {
                *result = current >> (remaining_bits_this_block - num_bits);
                remaining_bits_this_block -= num_bits;
            }
            else
            {
                // We're going to need to pull another block, do we have data?
                int num_bits_left_over = num_bits - remaining_bits_this_block;
                if (num_bits_left_over / num_bits_per_block + 1 > (int)data.size())
                {
                    // reading past end of stream
                    return false;
                }

                // TODO: doesn't handle case where we're requesting more bits than num_bits_per_block
                *result = current << num_bits_left_over;

                // Read in new block
                current = TByte_Swap::swap(data.back());
                data.pop_back();

                if (data.empty())
                {
                    remaining_bits_this_block = bits_used_in_last_block - num_bits_left_over;
                }
                else
                {
                    remaining_bits_this_block = num_bits_per_block - num_bits_left_over;
                }
                *result |= current >> remaining_bits_this_block;
            }

            *result = TMask::mask(*result, num_bits);

            return true;
        }

    private:
        bitstream_reader(const bitstream_reader&) = delete;
        bitstream_reader& operator= (const bitstream_reader&) = delete;

    private:
        std::vector<TBlock> data;
        TBlock current;
        size_t num_bits_per_block;
        int remaining_bits_this_block;
        int bits_used_in_last_block;
    };

    template <typename TBlock>
    class bitstream_writer
    {
    public:
        bitstream_writer()
            : num_bits_per_block(sizeof(TBlock) * 8)
        {
            data.push_back(TBlock(0));
            pCurrent = &data[0];
            remaining_bits_this_block = num_bits_per_block;
        }

        template <typename TData>
        void write_bits(int num_bits, TData bits)
        {
            assert(sizeof(TData) * 8 >= num_bits);

            if (remaining_bits_this_block > num_bits)
            {
                *pCurrent |= bit_masking::mask(bits, num_bits) << (remaining_bits_this_block - num_bits);
                remaining_bits_this_block -= num_bits;
            }
            else
            {
                int num_bits_left_over = num_bits - remaining_bits_this_block;
                *pCurrent |= bit_masking::mask(bits >> num_bits_left_over, remaining_bits_this_block);

                // Start a new block
                data.push_back(TBlock(0));
                pCurrent = &data[data.size() - 1];

                remaining_bits_this_block = num_bits_per_block - num_bits_left_over;
                *pCurrent |= bit_masking::mask(bits, num_bits_left_over);
            }
        }

    private:
        bitstream_writer(const bitstream_writer&) = delete;
        bitstream_writer& operator= (const bitstream_writer&) = delete;

    private:
        template <typename TData, typename TByteSwap, typename TMasking>
        friend class bitstream_reader;

        std::vector<TBlock> data;
        TBlock* pCurrent;
        size_t num_bits_per_block;
        int remaining_bits_this_block;
    };

} // namespace rzlib
