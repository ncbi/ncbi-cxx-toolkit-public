/*
Copyright(c) 2002-2005 Anatoliy Kuznetsov(anatoliy_kuznetsov at yahoo.com)

Permission is hereby granted, free of charge, to any person 
obtaining a copy of this software and associated documentation 
files (the "Software"), to deal in the Software without restriction, 
including without limitation the rights to use, copy, modify, merge, 
publish, distribute, sublicense, and/or sell copies of the Software, 
and to permit persons to whom the Software is furnished to do so, 
subject to the following conditions:

The above copyright notice and this permission notice shall be included 
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES 
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, 
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, 
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR 
OTHER DEALINGS IN THE SOFTWARE.

For more information please visit:  http://bmagic.sourceforge.net

*/

#ifndef BMALGO__H__INCLUDED__
#define BMALGO__H__INCLUDED__

#include "bm.h"
#include "bmfunc.h"
#include "bmdef.h"

namespace bm
{

/*! \defgroup setalgo Set algorithms 
 *  Set algorithms 
 *  \ingroup bmagic
 */

/*! \defgroup distance Distance metrics 
 *  Algorithms to compute binary distance metrics
 *  \ingroup setalgo
 */


/*! 
    \brief    Distance metrics codes defined for vectors A and B
    \ingroup  distance
*/
enum distance_metric
{
    COUNT_AND,       //!< (A & B).count()
    COUNT_XOR,       //!< (A ^ B).count()
    COUNT_OR,        //!< (A | B).count()
    COUNT_SUB_AB,    //!< (A - B).count()
    COUNT_SUB_BA,    //!< (B - A).count()
    COUNT_A,         //!< A.count()
    COUNT_B          //!< B.count()
};

/*! 
    \brief Distance metric descriptor, holds metric code and result.
    \sa distance_operation
*/

struct distance_metric_descriptor
{
     distance_metric   metric;
     bm::id_t          result;
     
     distance_metric_descriptor(distance_metric m)
     : metric(m),
       result(0)
    {}
    distance_metric_descriptor()
    : metric(bm::COUNT_XOR),
      result(0)
    {}
    
    /*! 
        \brief Sets metric result to 0
    */
    void reset()
    {
        result = 0;
    }
};


/*!
    \brief Internal function computes different distance metrics.
    \internal 
    \ingroup  distance
     
*/
inline
void combine_count_operation_with_block(const bm::word_t* blk,
                                        unsigned gap,
                                        const bm::word_t* arg_blk,
                                        int arg_gap,
                                        bm::word_t* temp_blk,
                                        distance_metric_descriptor* dmit,
                                        distance_metric_descriptor* dmit_end)
                                            
{
     gap_word_t* res=0;
     
     gap_word_t* g1 = BMGAP_PTR(blk);
     gap_word_t* g2 = BMGAP_PTR(arg_blk);
     
     if (gap) // first block GAP-type
     {
         if (arg_gap)  // both blocks GAP-type
         {
             gap_word_t tmp_buf[bm::gap_max_buff_len * 3]; // temporary result
             
             for (distance_metric_descriptor* it = dmit;it < dmit_end; ++it)
             {
                 distance_metric_descriptor& dmd = *it;
                 
                 switch (dmd.metric)
                 {
                 case bm::COUNT_AND:
                     res = gap_operation_and(g1, g2, tmp_buf);
                     break;
                 case bm::COUNT_OR:
                     res = gap_operation_or(g1, g2, tmp_buf);
                     break;
                 case bm::COUNT_SUB_AB:
                     res = gap_operation_sub(g1, g2, tmp_buf); 
                     break;
                 case bm::COUNT_SUB_BA:
                     res = gap_operation_sub(g2, g1, tmp_buf); 
                     break;
                 case bm::COUNT_XOR:
                     res = gap_operation_xor(g1, g2, tmp_buf); 
                    break;
                 case bm::COUNT_A:
                    res = g1;
                    break;
                 case bm::COUNT_B:
                    res = g2;
                    break;
                 } // switch
                 
                 if (res)
                     dmd.result += gap_bit_count(res);
                    
             } // for it
             
             return;

         }
         else // first block - GAP, argument - BITSET
         {
             for (distance_metric_descriptor* it = dmit;it < dmit_end; ++it)
             {
                 distance_metric_descriptor& dmd = *it;
                 
                 switch (dmd.metric)
                 {
                 case bm::COUNT_AND:
                     if (arg_blk)
                        dmd.result += gap_bitset_and_count(arg_blk, g1);
                     break;
                 case bm::COUNT_OR:
                     if (!arg_blk)
                        dmd.result += gap_bit_count(g1);
                     else
                        dmd.result += gap_bitset_or_count(arg_blk, g1); 
                     break;
                 case bm::COUNT_SUB_AB:
                     gap_convert_to_bitset((bm::word_t*) temp_blk, g1);
                     dmd.result += 
                       bit_operation_sub_count((bm::word_t*)temp_blk, 
                          ((bm::word_t*)temp_blk) + bm::set_block_size,
                           arg_blk);
                 
                     break;
                 case bm::COUNT_SUB_BA:
                     dmd.metric = bm::COUNT_SUB_AB; // recursive call to SUB_AB
                     combine_count_operation_with_block(arg_blk,
                                                        arg_gap,
                                                        blk,
                                                        gap,
                                                        temp_blk,
                                                        it, it+1);
                     dmd.metric = bm::COUNT_SUB_BA; // restore status quo
                     break;
                 case bm::COUNT_XOR:
                     if (!arg_blk)
                        dmd.result += gap_bit_count(g1);
                     else
                        dmd.result += gap_bitset_xor_count(arg_blk, g1);
                     break;
                 case bm::COUNT_A:
                    if (g1)
                        dmd.result += gap_bit_count(g1);
                    break;
                 case bm::COUNT_B:
                    if (arg_blk)
                    {
                        dmd.result += 
                          bit_block_calc_count(arg_blk, 
                                               arg_blk + bm::set_block_size);
                    }
                    break;
                 } // switch
                                     
             } // for it
             
             return;
         
         }
     } 
     else // first block is BITSET-type
     {     
         if (arg_gap) // second argument block is GAP-type
         {
             for (distance_metric_descriptor* it = dmit;it < dmit_end; ++it)
             {
                 distance_metric_descriptor& dmd = *it;
                 
                 switch (dmd.metric)
                 {
                 case bm::COUNT_AND:
                     if (blk) 
                        dmd.result += gap_bitset_and_count(blk, g2);                         
                     break;
                 case bm::COUNT_OR:
                     if (!blk)
                        dmd.result += gap_bit_count(g2);
                     else
                        dmd.result += gap_bitset_or_count(blk, g2);
                     break;
                 case bm::COUNT_SUB_AB:
                     if (blk)
                        dmd.result += gap_bitset_sub_count(blk, g2);
                     break;
                 case bm::COUNT_SUB_BA:
                     dmd.metric = bm::COUNT_SUB_AB; // recursive call to SUB_AB
                     combine_count_operation_with_block(arg_blk,
                                                        arg_gap,
                                                        blk,
                                                        gap,
                                                        temp_blk,
                                                        it, it+1);
                     dmd.metric = bm::COUNT_SUB_BA; // restore status quo
                     break;
                 case bm::COUNT_XOR:
                     if (!blk)
                        dmd.result += gap_bit_count(g2);
                     else
                        dmd.result += gap_bitset_xor_count(blk, g2); 
                    break;
                 case bm::COUNT_A:
                    if (blk)
                    {
                        dmd.result += 
                            bit_block_calc_count(blk, 
                                                 blk + bm::set_block_size);
                    }
                    break;
                 case bm::COUNT_B:
                    if (g2)
                        dmd.result += gap_bit_count(g2);
                    break;
                 } // switch
                                     
             } // for it
             
             return;
         }
     }

     // --------------------------------------------
     //
     // Here we combine two plain bitblocks 

     const bm::word_t* blk_end;
     const bm::word_t* arg_end;

     blk_end = blk + (bm::set_block_size);
     arg_end = arg_blk + (bm::set_block_size);


     for (distance_metric_descriptor* it = dmit; it < dmit_end; ++it)
     {
         distance_metric_descriptor& dmd = *it;

         switch (dmd.metric)
         {
         case bm::COUNT_AND:
             dmd.result += 
                bit_operation_and_count(blk, blk_end, arg_blk);
             break;
         case bm::COUNT_OR:
             dmd.result += 
                bit_operation_or_count(blk, blk_end, arg_blk);
             break;
         case bm::COUNT_SUB_AB:
             dmd.result += 
                bit_operation_sub_count(blk, blk_end, arg_blk);
             break;
         case bm::COUNT_SUB_BA:
             dmd.result += 
                bit_operation_sub_count(arg_blk, arg_end, blk);
             break;
         case bm::COUNT_XOR:
             dmd.result += 
                bit_operation_xor_count(blk, blk_end, arg_blk);
             break;
         case bm::COUNT_A:
            if (blk)
                dmd.result += bit_block_calc_count(blk, blk_end);
            break;
         case bm::COUNT_B:
            if (arg_blk)
                dmd.result += bit_block_calc_count(arg_blk, arg_end);
            break;
         } // switch

     } // for it
     
     

}

/*!
    \brief Distance computing template function.

    Function receives two bitvectors and an array of distance metrics
    (metrics pipeline). Function computes all metrics saves result into
    corresponding pipeline results (distance_metric_descriptor::result)
    An important detail is that function reuses metric descriptors, 
    incrementing received values. It allows you to accumulate results 
    from different calls in the pipeline.
    
    \param bv1      - argument bitvector 1 (A)
    \param bv2      - argument bitvector 2 (B)
    \param dmit     - pointer to first element of metric descriptors array
                      Input-Output parameter, receives metric code as input,
                      computation is added to "result" field
    \param dmit_end - pointer to (last+1) element of metric descriptors array
    \ingroup  distance
    
*/

template<class BV>
void distance_operation(const BV& bv1, 
                        const BV& bv2, 
                        distance_metric_descriptor* dmit,
                        distance_metric_descriptor* dmit_end)
{
    const typename BV::blocks_manager& bman1 = bv1.get_blocks_manager();
    const typename BV::blocks_manager& bman2 = bv2.get_blocks_manager();
    
    bm::word_t* temp_blk = 0;
    
    {
        for (distance_metric_descriptor* it = dmit; it < dmit_end; ++it)
        {
            if (it->metric == bm::COUNT_SUB_AB || 
                it->metric == bm::COUNT_SUB_BA)
            {
                temp_blk = bv1.allocate_tempblock();
                break;
            }
        }
    }
    
  
    bm::word_t*** blk_root = bman1.get_rootblock();
    unsigned block_idx = 0;
    unsigned i, j;
    
    const bm::word_t* blk;
    const bm::word_t* arg_blk;
    bool  blk_gap;
    bool  arg_gap;

    BM_SET_MMX_GUARD

    for (i = 0; i < bman1.top_block_size(); ++i)
    {
        bm::word_t** blk_blk = blk_root[i];

        if (blk_blk == 0) // not allocated
        {
            const bm::word_t* const* bvbb = bman2.get_topblock(i);
            if (bvbb == 0) 
            {
                block_idx += bm::set_array_size;
                continue;
            }

            blk = 0;
            blk_gap = false;

            for (j = 0; j < bm::set_array_size; ++j,++block_idx)
            {                
                arg_blk = bman2.get_block(i, j);
                arg_gap = BM_IS_GAP(bman2, arg_blk, block_idx);

                if (!arg_blk) 
                    continue;
                combine_count_operation_with_block(blk, blk_gap,
                                                   arg_blk, arg_gap,
                                                   temp_blk,
                                                   dmit, dmit_end);
            } // for j
            continue;
        }

        for (j = 0; j < bm::set_array_size; ++j, ++block_idx)
        {
            blk = blk_blk[j];
            arg_blk = bman2.get_block(i, j);

            if (blk == 0 && arg_blk == 0)
                continue;
                
            arg_gap = BM_IS_GAP(bman2, arg_blk, block_idx);
            blk_gap = BM_IS_GAP(bman1, blk, block_idx);
            
            combine_count_operation_with_block(blk, blk_gap,
                                               arg_blk, arg_gap,
                                               temp_blk,
                                               dmit, dmit_end);
            

        } // for j

    } // for i
    
    if (temp_blk)
    {
        bv1.free_tempblock(temp_blk);
    }

}



/*!
   \brief Computes bitcount of AND operation of two bitsets
   \param bv1 - Argument bit-vector.
   \param bv2 - Argument bit-vector.
   \return bitcount of the result
   \ingroup  distance
*/
template<class BV>
bm::id_t count_and(const BV& bv1, const BV& bv2)
{
    distance_metric_descriptor dmd(bm::COUNT_AND);
    
    distance_operation(bv1, bv2, &dmd, &dmd+1);
    return dmd.result;
}


/*!
   \brief Computes bitcount of XOR operation of two bitsets
   \param bv1 - Argument bit-vector.
   \param bv2 - Argument bit-vector.
   \return bitcount of the result
   \ingroup  distance
*/
template<class BV>
bm::id_t count_xor(const BV& bv1, const BV& bv2)
{
    distance_metric_descriptor dmd(bm::COUNT_XOR);
    
    distance_operation(bv1, bv2, &dmd, &dmd+1);
    return dmd.result;
}


/*!
   \brief Computes bitcount of SUB operation of two bitsets
   \param bv1 - Argument bit-vector.
   \param bv2 - Argument bit-vector.
   \return bitcount of the result
   \ingroup  distance
*/
template<class BV>
bm::id_t count_sub(const BV& bv1, const BV& bv2)
{
    distance_metric_descriptor dmd(bm::COUNT_SUB_AB);
    
    distance_operation(bv1, bv2, &dmd, &dmd+1);
    return dmd.result;
}


/*!    
   \brief Computes bitcount of OR operation of two bitsets
   \param bv1 - Argument bit-vector.
   \param bv2 - Argument bit-vector.
   \return bitcount of the result
   \ingroup  distance
*/
template<class BV>
bm::id_t count_or(const BV& bv1, const BV& bv2)
{
    distance_metric_descriptor dmd(bm::COUNT_OR);
    
    distance_operation(bv1, bv2, &dmd, &dmd+1);
    return dmd.result;
}


/**
    \brief Internal algorithms scans the input for the block range limit
    \internal
*/
template<class It>
It block_range_scan(It  first, It last, unsigned nblock)
{
    It right;
    for (right = first; right != last; ++right)
    {
        unsigned nb = unsigned((*right) >> bm::set_block_shift);
        if (nb != nblock)
            break;
    }
    return right;
}

/**
    \brief OR Combine bitvector and the iterable sequence 

    Algorithm combines bvector with sequence of integers 
    (represents another set). When the agrument set is sorted 
    this method can give serious increase in performance.
    
    \param bv     - destination bitvector
    \param first  - first element of the iterated sequence
    \param last   - last element of the iterated sequence
    
    \ingroup setalgo
*/
template<class BV, class It>
void combine_or(BV& bv, It  first, It last)
{
    typename BV::blocks_manager& bman = bv.get_blocks_manager();

    while (first < last)
    {
        unsigned nblock = unsigned((*first) >> bm::set_block_shift);     
        It right = block_range_scan(first, last, nblock);

        // now we have one in-block array of bits to set
        
        label1:
        
        int block_type;
        bm::word_t* blk = 
            bman.check_allocate_block(nblock, 
                                      true, 
                                      bv.get_new_blocks_strat(), 
                                      &block_type);
        if (!blk) 
            continue;
                        
        if (block_type == 1) // gap
        {            
            bm::gap_word_t* gap_blk = BMGAP_PTR(blk);
            unsigned threshold = bm::gap_limit(gap_blk, bman.glen());
            
            for (; first < right; ++first)
            {
                unsigned is_set;
                unsigned nbit   = (*first) & bm::set_block_mask; 
                
                unsigned new_block_len =
                    gap_set_value(true, gap_blk, nbit, &is_set);
                if (new_block_len > threshold) 
                {
                    bman.extend_gap_block(nblock, gap_blk);
                    ++first;
                    goto label1;  // block pointer changed - goto reset
                }
            }
        }
        else // bit block
        {
            for (; first < right; ++first)
            {
                unsigned nbit   = unsigned(*first & bm::set_block_mask); 
                unsigned nword  = unsigned(nbit >> bm::set_word_shift); 
                nbit &= bm::set_word_mask;
                blk[nword] |= (bm::word_t)1 << nbit;
            } // for
        }
    } // while
    
    bv.forget_count();
}


/**
    \brief XOR Combine bitvector and the iterable sequence 

    Algorithm combines bvector with sequence of integers 
    (represents another set). When the agrument set is sorted 
    this method can give serious increase in performance.
    
    \param bv     - destination bitvector
    \param first  - first element of the iterated sequence
    \param last   - last element of the iterated sequence
    
    \ingroup setalgo
*/
template<class BV, class It>
void combine_xor(BV& bv, It  first, It last)
{
    typename BV::blocks_manager& bman = bv.get_blocks_manager();

    while (first < last)
    {
        unsigned nblock = unsigned((*first) >> bm::set_block_shift);     
        It right = block_range_scan(first, last, nblock);

        // now we have one in-block array of bits to set
        
        label1:
        
        int block_type;
        bm::word_t* blk = 
            bman.check_allocate_block(nblock, 
                                      true, 
                                      bv.get_new_blocks_strat(), 
                                      &block_type,
                                      false /* no null return */);
        BM_ASSERT(blk); 
                        
        if (block_type == 1) // gap
        {
            bm::gap_word_t* gap_blk = BMGAP_PTR(blk);
            unsigned threshold = bm::gap_limit(gap_blk, bman.glen());
            
            for (; first < right; ++first)
            {
                unsigned is_set;
                unsigned nbit   = (*first) & bm::set_block_mask; 
                
                is_set = gap_test(gap_blk, nbit);
                BM_ASSERT(is_set <= 1);
                is_set ^= 1; 
                
                unsigned new_block_len =
                    gap_set_value(is_set, gap_blk, nbit, &is_set);
                if (new_block_len > threshold) 
                {
                    bman.extend_gap_block(nblock, gap_blk);
                    ++first;
                    goto label1;  // block pointer changed - goto reset
                }
            }
        }
        else // bit block
        {
            for (; first < right; ++first)
            {
                unsigned nbit   = unsigned(*first & bm::set_block_mask); 
                unsigned nword  = unsigned(nbit >> bm::set_word_shift); 
                nbit &= bm::set_word_mask;
                blk[nword] ^= (bm::word_t)1 << nbit;
            } // for
        }
    } // while
    
    bv.forget_count();
}



/**
    \brief SUB Combine bitvector and the iterable sequence 

    Algorithm combines bvector with sequence of integers 
    (represents another set). When the agrument set is sorted 
    this method can give serious increase in performance.
    
    \param bv     - destination bitvector
    \param first  - first element of the iterated sequence
    \param last   - last element of the iterated sequence
    
    \ingroup setalgo
*/
template<class BV, class It>
void combine_sub(BV& bv, It  first, It last)
{
    typename BV::blocks_manager& bman = bv.get_blocks_manager();

    while (first < last)
    {
        unsigned nblock = unsigned((*first) >> bm::set_block_shift);     
        It right = block_range_scan(first, last, nblock);

        // now we have one in-block array of bits to set
        
        label1:
        
        int block_type;
        bm::word_t* blk = 
            bman.check_allocate_block(nblock, 
                                      false, 
                                      bv.get_new_blocks_strat(), 
                                      &block_type);

        if (!blk)
            continue;
                        
        if (block_type == 1) // gap
        {
            bm::gap_word_t* gap_blk = BMGAP_PTR(blk);
            unsigned threshold = bm::gap_limit(gap_blk, bman.glen());
            
            for (; first < right; ++first)
            {
                unsigned is_set;
                unsigned nbit   = (*first) & bm::set_block_mask; 
                
                is_set = gap_test(gap_blk, nbit);
                if (!is_set)
                    continue;
                
                unsigned new_block_len =
                    gap_set_value(false, gap_blk, nbit, &is_set);
                if (new_block_len > threshold) 
                {
                    bman.extend_gap_block(nblock, gap_blk);
                    ++first;
                    goto label1;  // block pointer changed - goto reset
                }
            }
        }
        else // bit block
        {
            for (; first < right; ++first)
            {
                unsigned nbit   = unsigned(*first & bm::set_block_mask); 
                unsigned nword  = unsigned(nbit >> bm::set_word_shift); 
                nbit &= bm::set_word_mask;
                blk[nword] &= ~((bm::word_t)1 << nbit);
            } // for
        }
    } // while
    
    bv.forget_count();
}

/**
    \brief AND Combine bitvector and the iterable sequence 

    Algorithm combines bvector with sequence of integers 
    (represents another set). When the agrument set is sorted 
    this method can give serious increase in performance.
    
    \param bv     - destination bitvector
    \param first  - first element of the iterated sequence
    \param last   - last element of the iterated sequence
    
    \ingroup setalgo
*/
template<class BV, class It>
void combine_and(BV& bv, It  first, It last)
{
    BV bv_tmp;
    combine_or(bv_tmp, first, last);
    bv &= bv_tmp;
}

/*!
    \brief Compute number of bit intervals (GAPs) in the bitvector
    
    Algorithm traverses bit vector and count number of uninterrupted
    intervals of 1s and 0s.
    <pre>
    For example: 
      00001111100000 - gives us 3 intervals
      10001111100000 - 4 intervals
      00000000000000 - 1 interval
      11111111111111 - 1 interval
    </pre>
    \return Number of intervals
    \ingroup setalgo
*/
template<class BV>
bm::id_t count_intervals(const BV& bv)
{
    const typename BV::blocks_manager& bman = bv.get_blocks_manager();

    bm::word_t*** blk_root = bman.get_rootblock();
    typename BV::blocks_manager::block_count_change_func func(bman);
    for_each_block(blk_root, bman.top_block_size(), bm::set_array_size, func);

    return func.count();        
}


} // namespace bm

#include "bmundef.h"

#endif
