#include <stdio.h>
#include <rzlib_bitstream.h>

using namespace rzlib;

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

int main(int num_args, char* args[])
{
    const uint16_t test_data[] = { 0x1234, 0x5678 };

    bitstream<uint16_t, big_endian_byte_swap, bit_masking> test_stream(test_data, ARRAY_SIZE(test_data));;
    uint32_t result;
    while (test_stream.read_bits(16, &result))
    {
        printf("0x%x, ", result);
    }
    printf("\n");
    return 0;
}
