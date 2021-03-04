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

namespace compile_time_bits
{
    template<typename _HashType, size_t N>
    class aligned_index;

    template<class T, size_t N>
    struct const_array;

    // compile time sort algorithm
    // uses insertion sort https://en.wikipedia.org/wiki/Insertion_sort
    template<typename _Traits, bool remove_duplicates>
    class TInsertSorter
    {
    public:
        using sort_traits = _Traits;
        using init_type  = typename _Traits::init_type;
        using value_type = typename _Traits::value_type;
        using hash_type  = typename _Traits::hash_type;

        template<size_t N>
        constexpr auto operator()(const init_type(&init)[N]) const noexcept
        {
            auto indices = make_indices(init);
            auto hashes = construct_hashes(init, indices, std::make_index_sequence<aligned_index<hash_type, N>::aligned_size > {});
            auto values = construct_values(init, indices, std::make_index_sequence<N>{});
            return std::make_pair(hashes, values);
        }

    protected:

        template<typename _Indices, typename _Value>
        static constexpr void insert_down(_Indices& indices, size_t head, size_t tail, _Value current)
        {
            auto saved = current;
            while (head != tail)
            {
                auto prev = tail--;
                indices[prev] = indices[tail];
            }
            indices[head] = saved;
        }
        template<typename _Indices, typename _Input>
        static constexpr size_t const_lower_bound(const _Indices& indices, const _Input& input, size_t last, size_t value)
        {
            typename _Traits::Pred pred{};
            size_t _UFirst = 0;
            size_t _Count = last;

            while (0 < _Count)
            {	// divide and conquer, find half that contains answer
                const size_t _Count2 = _Count >> 1; // TRANSITION, VSO#433486
                const size_t _UMid = _UFirst + _Count2;
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
        template<typename _Indices, typename _Input>
        static constexpr size_t const_upper_bound(const _Indices& indices, const _Input& input, size_t last, size_t value)
        {
            typename _Traits::Pred pred{};
            size_t _UFirst = 0;
            size_t _Count = last;

            while (0 < _Count)
            {	// divide and conquer, find half that contains answer
                const size_t _Count2 = _Count >> 1; // TRANSITION, VSO#433486
                const size_t _UMid = _UFirst + _Count2;
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
        static constexpr size_t insert_sort_indices(_Indices& result, const _Input& input)
        {
            typename _Traits::Pred pred{};
            auto size = result.size();
            if (size < 2)
                return size;

            // current is the first element of the unsorted part of the array
            size_t current = 0;
            // the last inserted element into sorted part of the array
            size_t last = current;
            result[0] = 0;
            current++;

            while (current != result.size())
            {
                if (pred(input, result[last], current))
                {// optimization for presorted arrays
                    result[++last] = current;
                }
                else {
                    // we may exclude last element since it's already known as smaller then current
                    // using const_upper_bound helps to preserve the order of rows with identical hash values
                    // using const_upper_bound will reverse that order
                    auto fit = const_upper_bound(result, input, last, current);
                    bool move_it = !remove_duplicates;
                    if (remove_duplicates)
                    {
                        move_it = (fit == 0) || pred(input, result[fit-1], current);
                    }
                    if (move_it)
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
                while (++current != result.size())
                {
                    result[current] = result[last];
                }
            }
            return 1 + last;
        }

        template<typename T, size_t N>
        static constexpr auto make_indices(const T(&input)[N]) noexcept
        {
            const_array<size_t, N> indices{};
            auto real_size = insert_sort_indices(indices, input);
            return std::make_pair(real_size, indices);
        }

        template<size_t I, typename _Input, typename _Indices>
        static constexpr hash_type select_hash(const _Input& init, const _Indices& indices) noexcept
        {
            size_t real_size = indices.first;
            if (I < real_size)
                return sort_traits::get_hash(init[indices.second[I]]);
            else
                return std::numeric_limits<hash_type>::max();
        }

        template<size_t N, typename _Indices, std::size_t... I>
        static constexpr auto construct_hashes(const init_type(&init)[N], const _Indices& indices, std::index_sequence<I...> /*is_unused*/) noexcept
            -> aligned_index<hash_type, N>
        {
            return { { select_hash<I>(init, indices) ...} };
        }

        template<typename _Input, typename _Indices, std::size_t... I>
        static constexpr auto construct_values(const _Input& input, const _Indices& indices, std::index_sequence<I...>) noexcept
            -> const_array<value_type, sizeof...(I)> 
        {
            auto real_size = indices.first;
            return { { sort_traits::construct(input, indices.second[I < real_size ? I : real_size - 1]) ...} };
        }
    };
}

#endif
