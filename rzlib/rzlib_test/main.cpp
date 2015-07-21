#include <stdio.h>
#include <rzlib_bitstream.h>
#include <rzlib_huffman.h>

using namespace rzlib;

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

int main(int num_args, char* args[])
{
    const uint16_t test_data[] = { 0x1234, 0x5678 };

    bitstream_reader<uint16_t, big_endian_byte_swap, bit_masking> test_stream(test_data, ARRAY_SIZE(test_data));;
    uint32_t result;
    while (test_stream.read_bits(16, &result))
    {
        printf("0x%x, ", result);
    }
    printf("\n");

    char message[] = "Hello, World";
    std::vector<huffman_encoder<char>::symbol> symbols;
    symbols.push_back({ 'H', 1 });
    symbols.push_back({ 'e', 1 });
    symbols.push_back({ 'l', 3 });
    symbols.push_back({ 'o', 2 });
    symbols.push_back({ ',', 1 });
    symbols.push_back({ ' ', 1 });
    symbols.push_back({ 'W', 1 });
    symbols.push_back({ 'r', 1 });
    symbols.push_back({ 'd', 1 });
    huffman_encoder<char> encoder(symbols);

    bitstream_writer<uint8_t> encoded_stream;
    if (!encoder.encode(encoded_stream, "Hello, World", 12))
    {
        printf("Encoding failed.\n");
    }

    bitstream_reader<uint8_t> stream_reader(encoded_stream);

    huffman_decoder<char> decoder;

    //bitstream<uint32_t, no_byte_swap, no_masking> test_stream2(encoded_data, ARRAY_SIZE(encoded_data));;
    //char nextChar = 0;
    //while (decoder.decode_next(test_stream2, &nextChar))
    //{
    //    printf("%c", nextChar);
    //}
    //printf("\n");

    return 0;
}
