#pragma once

#include "rzlib_bitstream.h"
#include <vector>
#include <deque>
#include <map>
#include <algorithm>

namespace rzlib
{
    namespace details_
    {
        template <typename TData>
        struct huffman_node
        {
            union
            {
                // negative child index means child is leaf
                int32_t child[2];
                TData data;
            };
        };
    } // namespace details_

    template <typename TData>
    struct huffman_symbol_info
    {
        int32_t num_bits;
        uint32_t bits;
    };

    template <typename TData>
    class huffman_encoder
    {
    public:
        struct symbol
        {
            TData data;
            int32_t frequency;
        };

        huffman_encoder(const std::vector<symbol>& symbols)
        {
            std::deque<node*> list;

            for (auto& symbol : symbols)
            {
                list.push_back(new node { symbol.frequency, nullptr, false, symbol.data });
            }

            auto less_than = [](const node* a, const node* b)
            { return a->frequency < b->frequency; };

            std::sort(list.begin(), list.end(), less_than);

            while (list.size() > 1)
            {
                // Remove two lowest entries
                node* a = list.front();
                list.pop_front();
                node* b = list.front();
                list.pop_front();

                node* parent = new node;
                parent->frequency = a->frequency + b->frequency;
                parent->parent = nullptr;
                parent->is_node = true;
                parent->child[0] = a;
                parent->child[1] = b;
                a->parent = parent;
                b->parent = parent;

                list.emplace(std::lower_bound(list.begin(), list.end(), parent, less_than), parent);
            }

            store_node(list.front(), 0, 0);

            std::vector<node*> del;
            del.push_back(list.front());
            while (!del.empty())
            {
                node* n = del.back();
                del.pop_back();

                if (n->is_node)
                {
                    del.push_back(n->child[0]);
                    del.push_back(n->child[1]);
                }

                delete n;
            }
        }

        template <typename TBlock>
        bool encode(bitstream_writer<TBlock>& stream, const TData* data, size_t num_elements)
        {
            for (size_t i = 0; i < num_elements; ++i)
            {
                auto it = lookup.find(data[i]);
                if (it == lookup.end())
                {
                    return false;
                }
                stream.write_bits(it->second.num_bits, it->second.bits);
            }
            return true;
        }

    private:
        huffman_encoder(const huffman_encoder&) = delete;
        huffman_encoder& operator= (const huffman_encoder&) = delete;

    private:
        struct node
        {
            int32_t frequency;
            node* parent;
            bool is_node;
            union
            {
                TData data;
                node* child[2];
            };
        };

        int32_t store_node(node* n, int32_t num_bits, uint32_t bits)
        {
            // reserve our space
            int32_t i = (int32_t)nodes.size();

            if (n->is_node)
            {
                nodes.push_back({});

                int32_t l = store_node(n->child[0], num_bits + 1, (bits << 1) | 0);
                int32_t r = store_node(n->child[1], num_bits + 1, (bits << 1) | 1);
                nodes[i].child[0] = l;
                nodes[i].child[1] = r;

                return i;
            }
            else
            {
                nodes.push_back({ n->data });
                lookup[n->data].num_bits = num_bits;
                lookup[n->data].bits = bits;
                return -i;
            }
        }

        template <typename TData>
        friend class huffman_decoder;

        std::vector<details_::huffman_node<TData>> nodes;
        std::map<TData, huffman_symbol_info<TData>> lookup;
    };

    template <typename TData>
    class huffman_decoder
    {
    public:
        huffman_decoder(const huffman_encoder<TData>& encoder)
            : nodes(encoder.nodes)
        {
        }

        template <typename TBlock, typename byte_swap, typename masking>
        bool decode_next(bitstream_reader<TBlock, byte_swap, masking>& stream, TData* result)
        {
            uint32_t bits = 0;
            int32_t node = 0;
            do
            {
                if (!stream.read_bits(1, &bits))
                {
                    return false;
                }

                node = nodes[node].child[bits & 0x1];

                if (node == 0)
                {
                    // error, no node in this direction. bitstream
                    // being decoded contains a sequence not in the decode table.
                    assert(false);
                    return false;
                }

            } while (node > 0);

            assert(node < 0);
            *result = nodes[-node].data;
            return true;
        }

    private:
        huffman_decoder(const huffman_decoder&) = delete;
        huffman_decoder& operator= (const huffman_decoder&) = delete;

    private:
        std::vector<details_::huffman_node<TData>> nodes;
    };

} // namespace rzlib
