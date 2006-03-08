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

#ifndef BMSERIAL__H__INCLUDED__
#define BMSERIAL__H__INCLUDED__

/*! \defgroup bvserial bvector serialization  
 *  bvector serialization
 *  \ingroup bmagic 
 *
 */

#ifndef BM__H__INCLUDED__
#define BM__H__INCLUDED__

#include "bm.h"

#endif

#include "encoding.h"
#include "bmdef.h"


// Serialization related constants


const unsigned char set_block_end   = 0;   //!< End of serialization
const unsigned char set_block_1zero = 1;   //!< One all-zero block
const unsigned char set_block_1one  = 2;   //!< One block all-set (1111...)
const unsigned char set_block_8zero = 3;   //!< Up to 256 zero blocks
const unsigned char set_block_8one  = 4;   //!< Up to 256 all-set blocks
const unsigned char set_block_16zero= 5;   //!< Up to 65536 zero blocks
const unsigned char set_block_16one = 6;   //!< UP to 65536 all-set blocks
const unsigned char set_block_32zero= 7;   //!< Up to 4G zero blocks
const unsigned char set_block_32one = 8;   //!< UP to 4G all-set blocks
const unsigned char set_block_azero = 9;   //!< All other blocks zero
const unsigned char set_block_aone  = 10;  //!< All other blocks one

const unsigned char set_block_bit     = 11;  //!< Plain bit block
const unsigned char set_block_sgapbit = 12;  //!< SGAP compressed bitblock
const unsigned char set_block_sgapgap = 13;  //!< SGAP compressed GAP block
const unsigned char set_block_gap     = 14;  //!< Plain GAP block
const unsigned char set_block_gapbit  = 15;  //!< GAP compressed bitblock 
const unsigned char set_block_arrbit  = 16;  //!< List of bits ON
const unsigned char set_block_bit_interval = 17; //!< Interval block
const unsigned char set_block_arrgap  = 18;  //!< List of bits ON (GAP block)






#define SER_NEXT_GRP(enc, nb, B_1ZERO, B_8ZERO, B_16ZERO, B_32ZERO) \
   if (nb == 1) \
      enc.put_8(B_1ZERO); \
   else if (nb < 256) \
   { \
      enc.put_8(B_8ZERO); \
      enc.put_8((unsigned char)nb); \
   } \
   else if (nb < 65536) \
   { \
      enc.put_8(B_16ZERO); \
      enc.put_16((unsigned short)nb); \
   } \
   else \
   {\
      enc.put_8(B_32ZERO); \
      enc.put_32(nb); \
   }


#define BM_SET_ONE_BLOCKS(x) \
    {\
         unsigned end_block = i + x; \
         for (;i < end_block; ++i) \
            bman.set_block_all_set(i); \
    } \
    --i



namespace bm
{

/// \internal
enum serialization_header_mask {
    BM_HM_DEFAULT = 1,
    BM_HM_RESIZE  = (1 << 1), // resized vector
    BM_HM_ID_LIST = (1 << 2), // id list stored
    BM_HM_NO_BO   = (1 << 3)  // no byte-order
};


/// Bit mask flags for serialization algorithm
enum serialization_flags {
    BM_NO_BYTE_ORDER = 1      ///< save no byte-order info (save some space)
};

/*!
   \brief Saves bitvector into memory.

   Function serializes content of the bitvector into memory.
   Serialization adaptively uses compression(variation of GAP encoding) 
   when it is benefitial. 

   \param buf - pointer on target memory area. No range checking in the
   function. It is responsibility of programmer to allocate sufficient 
   amount of memory using information from calc_stat function.

   \param temp_block - pointer on temporary memory block. Cannot be 0; 
   If you want to save memory across multiple bvectors
   allocate temporary block using allocate_tempblock and pass it to 
   serialize.
   (Of course serialize does not deallocate temp_block.)

   \param serialization_flags
   Flags controlling serilization (bit-mask) 
   (use OR-ed serialization flags)

   \return Size of serialization block.
   \sa calc_stat, serialization_flags
*/
/*
 Serialization format:
 <pre>

 | HEADER | BLOCKS |

 Header structure:
   BYTE : Serialization header (bit mask of BM_HM_*)
   BYTE : Byte order ( 0 - Big Endian, 1 - Little Endian)
   INT16: Reserved (0)
   INT16: Reserved Flags (0)

 </pre>
*/
template<class BV>
unsigned serialize(const BV& bv, 
                   unsigned char* buf, 
                   bm::word_t*    temp_block,
                   unsigned       serialization_flags = 0)
{
    BM_ASSERT(temp_block);
    
    typedef typename BV::blocks_manager_type blocks_manager_type;
    const blocks_manager_type& bman = bv.get_blocks_manager();

    gap_word_t*  gap_temp_block = (gap_word_t*) temp_block;
    
    
    bm::encoder enc(buf, 0);

    // Header

    unsigned char header_flag = 0;
    if (bv.size() == bm::id_max) // no dynamic resize
        header_flag |= BM_HM_DEFAULT;
    else 
        header_flag |= BM_HM_RESIZE;

    if (serialization_flags & BM_NO_BYTE_ORDER) 
        header_flag |= BM_HM_NO_BO;

    enc.put_8(header_flag);

    if (!(serialization_flags & BM_NO_BYTE_ORDER))
    {
        ByteOrder bo = globals<true>::byte_order();
        enc.put_8((unsigned char)bo);
    }

    unsigned i,j;

    // keep GAP levels information
    enc.put_16(bman.glen(), bm::gap_levels);

    // save size (only if bvector has been down-sized)
    if (header_flag & BM_HM_RESIZE) 
    {
        enc.put_32(bv.size());
    }


    // save blocks.

    for (i = 0; i < bm::set_total_blocks; ++i)
    {
        bm::word_t* blk = bman.get_block(i);

        // -----------------------------------------
        // Empty or ONE block serialization

        bool flag;
        flag = bman.is_block_zero(i, blk);
        if (flag)
        {
        zero_block:
            if (bman.is_no_more_blocks(i+1)) 
            {
                enc.put_8(set_block_azero);
                break; 
            }

            // Look ahead for similar blocks
            for(j = i+1; j < bm::set_total_blocks; ++j)
            {
               bm::word_t* blk_next = bman.get_block(j);
               if (flag != bman.is_block_zero(j, blk_next))
                   break;
            }
            if (j == bm::set_total_blocks)
            {
                enc.put_8(set_block_azero);
                break; 
            }
            else
            {
               unsigned nb = j - i;
               SER_NEXT_GRP(enc, nb, set_block_1zero, 
                                     set_block_8zero, 
                                     set_block_16zero, 
                                     set_block_32zero) 
            }
            i = j - 1;
            continue;
        }
        else
        {
            flag = bman.is_block_one(i, blk);
            if (flag)
            {
                // Look ahead for similar blocks
                for(j = i+1; j < bm::set_total_blocks; ++j)
                {
                   bm::word_t* blk_next = bman.get_block(j);
                   if (flag != bman.is_block_one(j, blk_next))
                       break;
                }
                if (j == bm::set_total_blocks)
                {
                    enc.put_8(set_block_aone);
                    break;
                }
                else
                {
                   unsigned nb = j - i;
                   SER_NEXT_GRP(enc, nb, set_block_1one, 
                                         set_block_8one, 
                                         set_block_16one, 
                                         set_block_32one) 
                }
                i = j - 1;
                continue;
            }
        }

        // ------------------------------
        // GAP serialization

        if (BM_IS_GAP(bman, blk, i))
        {
            gap_word_t* gblk = BMGAP_PTR(blk);
            unsigned len = gap_length(gblk);
            unsigned bc = gap_bit_count(gblk);

            if (bc+1 < len-1) 
            {
                gap_word_t len;
                len = gap_convert_to_arr(gap_temp_block,
                                         gblk,
                                         bm::gap_equiv_len-10);
                if (len == 0) 
                {
                    goto save_as_gap;
                }
                enc.put_8(set_block_arrgap);
                enc.put_16(len);
                enc.put_16(gap_temp_block, len);
            }
            else 
            {
            save_as_gap:
                enc.put_8(set_block_gap);
                enc.put_16(gblk, len-1);
            }
            continue;
        }
        
        // -------------------------------
        // BIT BLOCK serialization

        // Try to reduce the size up to the reasonable limit.
        /*
        unsigned len = bm::bit_convert_to_gap(gap_temp_block, 
                                              blk, 
                                              bm::GAP_MAX_BITS, 
                                              bm::GAP_EQUIV_LEN-64);
        */
        gap_word_t len;

        len = bit_convert_to_arr(gap_temp_block, 
                                 blk, 
                                 bm::gap_max_bits, 
                                 bm::gap_equiv_len-64);

        if (len)  // reduced
        {
            enc.put_8(set_block_arrbit);
            if (sizeof(gap_word_t) == 2)
            {
                enc.put_16(len);
            }
            else
            {
                enc.put_32(len);
            }
            enc.put_16(gap_temp_block, len);
        }
        else
        {
            unsigned head_idx, tail_idx;
            bit_find_head_tail(blk, &head_idx, &tail_idx);

            if (head_idx == (unsigned)-1) // zero block
            {
                goto zero_block;
            }
            else  // full block
            if (head_idx == 0 && tail_idx == bm::set_block_size-1)
            {
                enc.put_8(set_block_bit);
                enc.put_32(blk, bm::set_block_size);
            } 
            else  // interval block
            {
                enc.put_8(set_block_bit_interval);
                enc.put_16((short)head_idx);
                enc.put_16((short)tail_idx);
                enc.put_32(blk + head_idx, tail_idx - head_idx + 1);
            }
        }

    }

    enc.put_8(set_block_end);


    unsigned encoded_size = enc.size();
    
    // check if bit-vector encoding is inefficient 
    // (can happen for very sparse vectors)

    bm::id_t cnt = bv.count();
    unsigned id_capacity = cnt * sizeof(bm::id_t);
    id_capacity += 16;
    if (id_capacity < encoded_size) 
    {
        // plain list of ints is better than serialization
        bm::encoder enc(buf, 0);
        header_flag = BM_HM_ID_LIST;
        if (bv.size() != bm::id_max) // no dynamic resize
            header_flag |= BM_HM_RESIZE;
        if (serialization_flags & BM_NO_BYTE_ORDER) 
            header_flag |= BM_HM_NO_BO;

        enc.put_8(header_flag);
        if (!(serialization_flags & BM_NO_BYTE_ORDER))
        {
            ByteOrder bo = globals<true>::byte_order();
            enc.put_8((unsigned char)bo);
        }

        if (bv.size() != bm::id_max) // no dynamic resize
        {
            enc.put_32(bv.size());
        }

        enc.put_32(cnt);
        typename BV::enumerator en(bv.first());
        for (;en.valid(); ++en,--cnt) 
        {
            bm::id_t id = *en;
            enc.put_32(id);
        }
        return enc.size();
    } // if capacity

    return encoded_size;

}

/*!
   @brief Saves bitvector into memory.
   Allocates temporary memory block for bvector.
*/

template<class BV>
unsigned serialize(BV& bv, 
                   unsigned char* buf, 
                   unsigned  serialization_flags=0)
{
    typename BV::blocks_manager_type& bman = bv.get_blocks_manager();

    return serialize(bv, buf, bman.check_allocate_tempblock());
}


template<class BV, class DEC>
class deserializer
{
public:
    typedef BV bvector_type;
    typedef DEC decoder_type;
public:
    static
    unsigned deserialize(bvector_type&        bv, 
                         const unsigned char* buf, 
                         bm::word_t*          temp_block);
protected:
   typedef typename BV::blocks_manager_type blocks_manager_type;
   typedef typename BV::allocator_type allocator_type;

};


/*!
    @brief Bitvector deserialization from memory.

    @param buf - pointer on memory which keeps serialized bvector
    @param temp_block - pointer on temporary block, 
            if NULL bvector allocates own.
    @return Number of bytes consumed by deserializer.

    Function desrializes bitvector from memory block containig results
    of previous serialization. Function does not remove bits 
    which are currently set. Effectively it means OR logical operation 
    between current bitset and previously serialized one.
*/
template<class BV>
unsigned deserialize(BV& bv, 
                     const unsigned char* buf, 
                     bm::word_t* temp_block=0)
{
    ByteOrder bo_current = globals<true>::byte_order();

    bm::decoder dec(buf);
    unsigned char header_flag = dec.get_8();
    ByteOrder bo = bo_current;
    if (!(header_flag & BM_HM_NO_BO))
    {
        bo = (bm::ByteOrder) dec.get_8();
    }

    if (bo_current == bo)
    {
        return 
            deserializer<BV, bm::decoder>::deserialize(bv, buf, temp_block);
    }
    switch (bo_current) 
    {
    case BigEndian:
        return 
        deserializer<BV, bm::decoder_big_endian>::deserialize(bv, 
                                                              buf, 
                                                              temp_block);
    case LittleEndian:
        return 
        deserializer<BV, bm::decoder_little_endian>::deserialize(bv, 
                                                                 buf, 
                                                                 temp_block);
    default:
        BM_ASSERT(0);
    };
    return 0;
}



template<class BV, class DEC>
unsigned deserializer<BV, DEC>::deserialize(bvector_type&        bv, 
                                            const unsigned char* buf,
                                            bm::word_t*          temp_block)
{
    blocks_manager_type& bman = bv.get_blocks_manager();

    bm::wordop_t* tmp_buf = 
        temp_block ? (bm::wordop_t*) temp_block 
                   : (bm::wordop_t*)bman.check_allocate_tempblock();

    temp_block = (word_t*)tmp_buf;

    gap_word_t   gap_temp_block[set_block_size*2+10];

    decoder_type dec(buf);

    bv.forget_count();

    BM_SET_MMX_GUARD

    // Reading header

    unsigned char header_flag =  dec.get_8();
    if (!(header_flag & BM_HM_NO_BO))
    {
        /*ByteOrder bo = (bm::ByteOrder)*/dec.get_8();
    }

    if (header_flag & BM_HM_ID_LIST)
    {
        // special case: the next comes plain list of integers
        if (header_flag & BM_HM_RESIZE)
        {
            unsigned bv_size = dec.get_32();
            if (bv_size > bv.size())
            {
                bv.resize(bv_size);
            }
        }

        for (unsigned cnt = dec.get_32(); cnt; --cnt) {
            bm::id_t id = dec.get_32();
            bv.set(id);
        } // for
        return dec.size();
    }

    unsigned i;

    gap_word_t glevels[bm::gap_levels];
    // keep GAP levels information
    for (i = 0; i < bm::gap_levels; ++i)
    {
        glevels[i] = dec.get_16();
    }

    if (header_flag & (1 << 1))
    {
        unsigned bv_size = dec.get_32();
        if (bv_size > bv.size())
        {
            bv.resize(bv_size);
        }
    }


    // Reading blocks

    unsigned char btype;
    unsigned nb;

    for (i = 0; i < bm::set_total_blocks; ++i)
    {
        // get the block type

        btype = dec.get_8();


        // In a few next blocks of code we simply ignoring all coming zero blocks.

        if (btype == set_block_azero || btype == set_block_end)
        {
            break;
        }

        if (btype == set_block_1zero)
        {
            continue;
        }

        if (btype == set_block_8zero)
        {
            nb = dec.get_8();
            i += nb-1;
            continue;
        }

        if (btype == set_block_16zero)
        {
            nb = dec.get_16();
            i += nb-1;
            continue;
        }

        if (btype == set_block_32zero)
        {
            nb = dec.get_32();
            i += nb-1;
            continue;
        }

        // Now it is turn of all-set blocks (111)

        if (btype == set_block_aone)
        {
            for (unsigned j = i; j < bm::set_total_blocks; ++j)
            {
                bman.set_block_all_set(j);
            }
            break;
        }

        if (btype == set_block_1one)
        {
            bman.set_block_all_set(i);
            continue;
        }

        if (btype == set_block_8one)
        {
            BM_SET_ONE_BLOCKS(dec.get_8());
            continue;
        }

        if (btype == set_block_16one)
        {
            BM_SET_ONE_BLOCKS(dec.get_16());
            continue;
        }

        if (btype == set_block_32one)
        {
            BM_SET_ONE_BLOCKS(dec.get_32());
            continue;
        }

        bm::word_t* blk = bman.get_block(i);


        if (btype == set_block_bit) 
        {
            if (blk == 0)
            {
                blk = bman.get_allocator().alloc_bit_block();
                bman.set_block(i, blk);
                dec.get_32(blk, bm::set_block_size);
                continue;                
            }
            dec.get_32(temp_block, bm::set_block_size);
            bv.combine_operation_with_block(i, 
                                            temp_block, 
                                            0, BM_OR);
            continue;
        }

        if (btype == set_block_bit_interval) 
        {

            unsigned head_idx, tail_idx;
            head_idx = dec.get_16();
            tail_idx = dec.get_16();

            if (blk == 0)
            {
                blk = bman.get_allocator().alloc_bit_block();
                bman.set_block(i, blk);
                for (unsigned i = 0; i < head_idx; ++i)
                {
                    blk[i] = 0;
                }
                dec.get_32(blk + head_idx, tail_idx - head_idx + 1);
                for (unsigned j = tail_idx + 1; j < bm::set_block_size; ++j)
                {
                    blk[j] = 0;
                }
                continue;
            }
            bit_block_set(temp_block, 0);
            dec.get_32(temp_block + head_idx, tail_idx - head_idx + 1);

            bv.combine_operation_with_block(i, 
                                            temp_block,
                                            0, BM_OR);
            continue;
        }

        if (btype == set_block_gap || btype == set_block_gapbit)
        {
            gap_word_t gap_head = 
                sizeof(gap_word_t) == 2 ? dec.get_16() : dec.get_32();

            unsigned len = gap_length(&gap_head);
            int level = gap_calc_level(len, bman.glen());
            --len;
            if (level == -1)  // Too big to be GAP: convert to BIT block
            {
                *gap_temp_block = gap_head;
                dec.get_16(gap_temp_block+1, len - 1);
                //dec.memcpy(gap_temp_block+1, (len-1) * sizeof(gap_word_t));
                gap_temp_block[len] = gap_max_bits - 1;

                if (blk == 0)  // block does not exist yet
                {
                    blk = bman.get_allocator().alloc_bit_block();
                    bman.set_block(i, blk);
                    gap_convert_to_bitset(blk, gap_temp_block);                
                }
                else  // We have some data already here. Apply OR operation.
                {
                    gap_convert_to_bitset(temp_block, 
                                          gap_temp_block);

                    bv.combine_operation_with_block(i, 
                                                    temp_block, 
                                                    0, 
                                                    BM_OR);
                }

                continue;
            } // level == -1

            set_gap_level(&gap_head, level);

            if (blk == 0)
            {
                gap_word_t* gap_blk = 
                  bman.get_allocator().alloc_gap_block(level, bman.glen());
                gap_word_t* gap_blk_ptr = BMGAP_PTR(gap_blk);
                *gap_blk_ptr = gap_head;
                set_gap_level(gap_blk_ptr, level);
                bman.set_block(i, (bm::word_t*)gap_blk);
                bman.set_block_gap(i);
                for (unsigned k = 1; k < len; ++k) 
                {
                     gap_blk[k] = dec.get_16();
                }
                gap_blk[len] = bm::gap_max_bits - 1;
            }
            else
            {
                *gap_temp_block = gap_head;
                for (unsigned k = 1; k < len; ++k) 
                {
                     gap_temp_block[k] = dec.get_16();
                }
                gap_temp_block[len] = bm::gap_max_bits - 1;

                bv.combine_operation_with_block(i, 
                                               (bm::word_t*)gap_temp_block, 
                                                1, 
                                                BM_OR);
            }

            continue;
        }

        if (btype == set_block_arrgap) 
        {
            gap_word_t len = dec.get_16();
            int block_type;
            unsigned k = 0;

        get_block_ptr:
            bm::word_t* blk =
                bman.check_allocate_block(i,
                                          true,
                                          bv.get_new_blocks_strat(),
                                          &block_type,
                                          false /* no null return*/);

            // block is all 1, no need to do anything
            if (!blk)
            {
                for (; k < len; ++k)
                {
                    dec.get_16();
                }
            }

            if (block_type == 1) // gap
            {            
                bm::gap_word_t* gap_blk = BMGAP_PTR(blk);
                unsigned threshold = bm::gap_limit(gap_blk, bman.glen());
                
                for (; k < len; ++k)
                {
                    gap_word_t bit_idx = dec.get_16();
                    unsigned is_set;
                    unsigned new_block_len =
                        gap_set_value(true, gap_blk, bit_idx, &is_set);
                    if (new_block_len > threshold) 
                    {
                        bman.extend_gap_block(i, gap_blk);
                        ++k;
                        goto get_block_ptr;  // block pointer changed - reset
                    }

                } // for
            }
            else // bit block
            {
                // Get the array one by one and set the bits.
                for(;k < len; ++k)
                {
                    gap_word_t bit_idx = dec.get_16();
                    or_bit_block(blk, bit_idx, 1);
                }
            }
            continue;
        }

        if (btype == set_block_arrbit)
        {
            gap_word_t len = 
                sizeof(gap_word_t) == 2 ? dec.get_16() : dec.get_32();

            // check the block type.
            if (bman.is_block_gap(i))
            {
                // Here we most probably does not want to keep
                // the block GAP since generic bitblock offers better
                // performance.
                blk = bman.convert_gap2bitset(i);
            }
            else
            {
                if (blk == 0)  // block does not exists yet
                {
                    blk = bman.get_allocator().alloc_bit_block();
                    bit_block_set(blk, 0);
                    bman.set_block(i, blk);
                }
            }

            // Get the array one by one and set the bits.
            for (unsigned k = 0; k < len; ++k)
            {
                gap_word_t bit_idx = dec.get_16();
                or_bit_block(blk, bit_idx, 1);
            }

            continue;
        }
/*
        if (btype == set_block_gapbit)
        {
            gap_word_t gap_head = 
                sizeof(gap_word_t) == 2 ? dec.get_16() : dec.get_32();

            unsigned len = gap_length(&gap_head)-1;

            *gap_temp_block = gap_head;
            dec.memcpy(gap_temp_block+1, (len-1) * sizeof(gap_word_t));
            gap_temp_block[len] = GAP_MAX_BITS - 1;

            if (blk == 0)  // block does not exists yet
            {
                blk = A::alloc_bit_block();
                blockman_.set_block(i, blk);
                gap_convert_to_bitset(blk, 
                                      gap_temp_block, 
                                      bm::SET_BLOCK_SIZE);                
            }
            else  // We have some data already here. Apply OR operation.
            {
                gap_convert_to_bitset(temp_block, 
                                      gap_temp_block, 
                                      bm::SET_BLOCK_SIZE);

                combine_operation_with_block(i, 
                                             temp_block, 
                                             0, 
                                             BM_OR);
            }

            continue;
        }
*/
        BM_ASSERT(0); // unknown block type


    } // for i

    return dec.size();

}



} // namespace bm

#include "bmundef.h"

#endif
