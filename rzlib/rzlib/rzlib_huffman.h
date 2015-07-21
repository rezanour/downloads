#pragma once

#include "rzlib_bitstream.h"
#include <vector>
#include <deque>
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
    struct huffman_symbol
    {
        TData data;
        int frequency;
    };

    template <typename TData>
    class huffman_encoder
    {
    public:
        huffman_encoder(){}
        ~huffman_encoder(){}

        bool encode(const std::vector<huffman_symbol<TData>>& symbols)
        {
            std::deque<node> list;
            std::vector<node> the_nodes;

            for (auto& symbol : symbols)
            {
                list.push_back({ symbol.frequency, false, symbol.data, { -1 , -1 } });
            }

            auto less_than = [](const node& a, const node& b)
            { return a.frequency < b.frequency; };

            std::sort(list.begin(), list.end(), less_than);

            while (list.size() > 1)
            {
                // Remove two lowest entries
                node a = list.front();
                list.pop_front();
                node b = list.front();
                list.pop_front();

                int child[2]{a.index, b.index};
                if (!a.is_node)
                {
                    child[0] = (int)the_nodes.size();
                    the_nodes.push_back(a);
                }
                if (!b.is_node)
                {
                    child[1] = (int)the_nodes.size();
                    the_nodes.push_back(b);
                }

                node parent{ a.frequency + b.frequency, true, (int)the_nodes.size(), { child[0], child[1] } };
                the_nodes.push_back(parent);
                list.emplace(std::lower_bound(list.begin(), list.end(), parent, less_than), parent);
            }

            nodes.clear();
            store_node(the_nodes, list.front().index);

            return true;
        }

    private:
        huffman_encoder(const huffman_encoder&) = delete;
        huffman_encoder& operator= (const huffman_encoder&) = delete;

    private:
        struct node
        {
            int frequency;
            bool is_node;
            union
            {
                TData data;
                int index;
            };
            int child[2];
        };

        int store_node(const std::vector<node>& the_nodes, int index)
        {
            auto& n = the_nodes[index];
            if (n.is_node)
            {
                nodes.push_back({ { store_node(the_nodes, n.child[0]), store_node(the_nodes, n.child[1]) } });
                return (int)nodes.size() - 1;
            }
            else
            {
                nodes.push_back({ n.data });
                return -((int)nodes.size() - 1);
            }
        }

        std::vector<details_::huffman_node<TData>> nodes;
    };

    template <typename TData>
    class huffman_decoder
    {
    public:
        huffman_decoder()
        {
        }

        ~huffman_decoder()
        {
        }

        //bool add_symbol(uint32_t num_bits, uint32_t bits, TData symbol)
        //{
        //    assert(num_bits < sizeof(bits) * 8);
        //    int32_t node = 0;
        //    while (num_bits--)
        //    {
        //        uint32_t nextBit = bits & 0x1;
        //        bits >>= 1;

        //        int32_t nextNode = nodes[node].child[nextBit];
        //        if (nextNode == 0)
        //        {
        //            // no node yet in this direction. create one now
        //            details_::huffman_node<TData> aNode;
        //            if (num_bits == 0)
        //            {
        //                // leaf
        //                aNode.data = symbol;
        //                nodes.push_back(aNode);
        //                nodes[node].child[nextBit] = nextNode = -(int32_t)(nodes.size() - 1);
        //            }
        //            else
        //            {
        //                // inner node
        //                aNode.child[0] = 0;
        //                aNode.child[1] = 0;
        //                nodes.push_back(aNode);
        //                nodes[node].child[nextBit] = nextNode = (int32_t)(nodes.size() - 1);
        //            }
        //        }

        //        // existing node, move on down
        //        node = nextNode;
        //    }
        //    return true;
        //}

        template <typename TBlock, typename byte_swap, typename masking>
        bool decode_next(bitstream<TBlock, byte_swap, masking>& stream, TData* result)
        {
            if (nodes.empty())
            {
                assert(false);
                return false;
            }

            uint32_t bits = 0;
            int32_t node = 0;
            do
            {
                if (!stream.read_bits(1, &bits))
                {
                    return false;
                }

                node = nodes[node].child[bits];

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
