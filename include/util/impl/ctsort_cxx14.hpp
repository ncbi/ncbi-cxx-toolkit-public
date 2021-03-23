#ifndef __CTSORT_CXX14_HPP_INCLUDED__
#define __CTSORT_CXX14_HPP_INCLUDED__

/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Authors:  Sergiy Gotvyanskyy
 *
 * File Description:
 *
 *  compile time sorting algorithm
 *
 *
 */

#include <type_traits>
#include <utility>
#include <tuple>
#include <limits>

namespace compile_time_bits
{

    using tag_SortByHashes = std::integral_constant<bool, true>;
    using tag_SortByValues = std::integral_constant<bool, false>;

    // compile time sort algorithm
    // uses insertion sort https://en.wikipedia.org/wiki/Insertion_sort
    template<typename _Traits, bool remove_duplicates>
    class TInsertSorter
    {
    public:
        using size_type   = std::size_t;
        using sort_traits = _Traits;
        using value_type  = typename sort_traits::value_type;
        using hash_type   = typename sort_traits::hash_type;
        static constexpr bool can_index = std::numeric_limits<hash_type>::is_integer;

        template<typename _Init>
        static constexpr auto sort(_Init&& init) noexcept
        {
            return x_sort(std::integral_constant<bool, can_index>{}, init);
        }

        template<bool _SortByValues, typename _Init>
        static constexpr auto sort(std::integral_constant<bool, _SortByValues> _Tag, _Init&& init) noexcept
        {
            return x_sort(_Tag, init);
        }

        template<typename _Init>
        static constexpr auto make_indices(_Init&& input) noexcept
        { // this will produce the index of elements sorted by rules provided in sort_traits
            const_array<size_type, array_size<_Init>::value> indices{};
            auto real_size = insert_sort_indices(indices, input);
            return std::make_pair(real_size, indices);
        }

    protected:

        struct Less
        {
            template<typename _Input>
            constexpr bool operator()(const _Input& input, size_t l, size_t r) const
            {
                return typename sort_traits::hashed_key_type::compare_hashes{}(
                    sort_traits::get_init_hash(input[l]),
                    sort_traits::get_init_hash(input[r])
                );
            }
        };

        template<typename _Init, size_type N = array_size<_Init>::value>
        static constexpr auto x_sort(tag_SortByHashes, const _Init& init) noexcept
        { // sort by hashes, return tuple( real_size, sorted_values, sorted_indices)
            auto indices = make_indices(init);
            auto hashes = construct_hashes(init, indices, std::make_index_sequence<N>{});
            auto values = construct_values(init, indices, std::make_index_sequence<N>{});
            return std::make_tuple(indices.first, values, hashes);
        }

        template<typename _Init, size_type N = array_size<_Init>::value>
        static constexpr auto x_sort(tag_SortByValues, const _Init& init) noexcept
        { // sort by values, return pair( real_size, sorted_values)
            auto indices = make_indices(init);
            auto values = construct_values(init, indices, std::make_index_sequence<N>{});
            return std::make_pair(indices.first, values);
        }

        template<typename _Indices>
        static constexpr void insert_down(_Indices& indices, size_type head, size_type tail, size_type current)
        {
            while (head != tail)
            {
                auto prev = tail--;
                indices[prev] = indices[tail];
            }
            indices[head] = current;
        }
        template<typename _Indices, typename _Input, typename _Pred>
        static constexpr size_type const_lower_bound(_Pred pred, const _Indices& indices, const _Input& input, size_type size, size_type value)
        {
            size_type _UFirst = 0;
            size_type _Count = size;

            while (0 < _Count)
            {	// divide and conquer, find half that contains answer
                const size_type _Count2 = _Count >> 1; // TRANSITION, VSO#433486
                const size_type _UMid = _UFirst + _Count2;
                if (pred(input, indices[_UMid], value))
                {	// try top half
                    _UFirst = (_UMid + 1); // _Next_iter(_UMid);
                    _Count -= _Count2 + 1;
                }
                else
                {
                    _Count = _Count2;
                }
            }

            return _UFirst;
        }
        template<typename _Indices, typename _Input, typename _Pred>
        static constexpr size_type const_upper_bound(_Pred pred, const _Indices& indices, const _Input& input, size_type size, size_type value)
        {
            size_type _UFirst = 0;
            size_type _Count = size;

            while (0 < _Count)
            {	// divide and conquer, find half that contains answer
                const size_type _Count2 = _Count >> 1; // TRANSITION, VSO#433486
                const size_type _UMid = _UFirst + _Count2;
                if (pred(input, value, indices[_UMid]))
                {
                    _Count = _Count2;
                }
                else
                {	// try top half
                    _UFirst = (_UMid + 1); // _Next_iter(_UMid);
                    _Count -= _Count2 + 1;
                }
            }

            return (_UFirst);
        }

        template<typename _Indices, typename _Input>
        static constexpr size_type insert_sort_indices(_Indices& result, const _Input& input)
        {
            Less pred{};
            const auto size = result.size();
            if (size < 2)
                return size;

            // current is the first element of the unsorted part of the array
            size_type current = 0;
            // the last inserted element into sorted part of the array
            size_type last = current;
            result[0] = 0;
            current++;

            while (current != size)
            {
                if (pred(input, result[last], current))
                {// optimization for presorted arrays
                    result[++last] = current;
                }
                else {
                    // we may exclude last element since it's already known as smaller then current
                    // using const_upper_bound helps to preserve the order of rows with identical values (or their hashes)
                    // using const_lower_bound will reverse that order
                    auto fit = const_upper_bound(pred, result, input, last, current);
                    bool need_to_move = !remove_duplicates;
                    if (remove_duplicates)
                    {
                        need_to_move = (fit == 0) || pred(input, result[fit-1], current);
                    }
                    if (need_to_move)
                    {
                        ++last;
                        insert_down(result, fit, last, current);
                    }
                }
                ++current;
            }
            if (remove_duplicates)
            {// fill the rest of the indices with maximum value
                current = last;
                while (++current != size)
                {
                    result[current] = result[last];
                }
            }
            return 1 + last;
        }

        template<typename _Input, typename _Indices, size_type... I>
        static constexpr auto construct_hashes(const _Input& input, const _Indices& indices, std::index_sequence<I...>) noexcept
            -> const_array<hash_type, sizeof...(I)>
        {
            size_type real_size = indices.first;
            return { { sort_traits::get_init_hash(input[indices.second[I < real_size ? I : real_size - 1]]) ...} };
        }

        template<typename _Input, typename _Indices, size_type... I>
        static constexpr auto construct_values(const _Input& input, const _Indices& indices, std::index_sequence<I...>) noexcept
            -> const_array<value_type, sizeof...(I)>
        {
            auto real_size = indices.first;
            return { { sort_traits::construct(input[indices.second[I < real_size ? I : real_size - 1]]) ...} };
        }
    };
}

#endif
