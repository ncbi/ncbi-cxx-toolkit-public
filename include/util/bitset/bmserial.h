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

   \return Size of serialization block.
   \sa calc_stat
*/
/*
 Serialization format:
 <pre>

 | HEADER | BLOCKS |

 Header structure:
   BYTE : Serialization type (0x1)
   BYTE : Byte order ( 0 - Big Endian, 1 - Little Endian)
   INT16: Reserved (0)
   INT16: Reserved Flags (0)

 </pre>
*/
template<class BV>
unsigned serialize(const BV& bv, unsigned char* buf, bm::word_t* temp_block)
{
    BM_ASSERT(temp_block);
    
    typedef typename BV::blocks_manager blocks_manager_type;
    const blocks_manager_type& bman = bv.get_blocks_manager();

    gap_word_t*  gap_temp_block = (gap_word_t*) temp_block;
    
    
    bm::encoder enc(buf, 0);

    // Header

    ByteOrder bo = globals<true>::byte_order();
        
    enc.put_8(1);
    enc.put_8((unsigned char)bo);

    unsigned i,j;

    // keep GAP levels information
    enc.put_16(bman.glen(), bm::gap_levels);
/*     
    for (i = 0; i < bm::gap_levels; ++i)
    {
        enc.put_16(bman.glen()[i]);
    }
*/
    // Blocks.

    for (i = 0; i < bm::set_total_blocks; ++i)
    {
        bm::word_t* blk = bman.get_block(i);

        // -----------------------------------------
        // Empty or ONE block serialization

        bool flag;
        flag = bman.is_block_zero(i, blk);
        if (flag)
        {
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
            enc.put_8(set_block_gap);
            enc.put_16(gblk, len-1);
            /*
            for (unsigned k = 0; k < (len-1); ++k)
            {
                enc.put_16(gblk[k]);
            }
            */
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
//            len = gap_length(gap_temp_block);
//            enc.put_8(SET_BLOCK_GAPBIT);
//            enc.memcpy(gap_temp_block, sizeof(gap_word_t) * (len - 1));
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
            enc.put_8(set_block_bit);
            enc.put_32(blk, bm::set_block_size);
            //enc.memcpy(blk, sizeof(bm::word_t) * bm::set_block_size);
        }

    }

    enc.put_8(set_block_end);
    return enc.size();

}

/*!
   @brief Saves bitvector into memory.
   Allocates temporary memory block for bvector.
*/

template<class BV>
unsigned serialize(BV& bv, unsigned char* buf)
{
    typename BV::blocks_manager& bman = bv.get_blocks_manager();

    return serialize(bv, buf, bman.check_allocate_tempblock());
}



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
unsigned deserialize(BV& bv, const unsigned char* buf, bm::word_t* temp_block=0)
{
    typedef typename BV::blocks_manager blocks_manager_type;
    blocks_manager_type& bman = bv.get_blocks_manager();

    typedef typename BV::allocator_type allocator_type;

    bm::wordop_t* tmp_buf = 
        temp_block ? (bm::wordop_t*) temp_block 
                   : (bm::wordop_t*)bman.check_allocate_tempblock();

    temp_block = (word_t*)tmp_buf;

    gap_word_t   gap_temp_block[set_block_size*2+10];


    ByteOrder bo_current = globals<true>::byte_order();
    bm::decoder dec(buf);

    bv.forget_count();

    BM_SET_MMX_GUARD

    // Reading header

    // unsigned char stype =  
    dec.get_8();
    ByteOrder bo = (bm::ByteOrder)dec.get_8();

    assert(bo == bo_current); // TO DO: Add Byte-Order convertions here

    unsigned i;

    gap_word_t glevels[bm::gap_levels];
    // keep GAP levels information
    for (i = 0; i < bm::gap_levels; ++i)
    {
        glevels[i] = dec.get_16();
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
                //dec.memcpy(blk, sizeof(bm::word_t) * bm::set_block_size);
                continue;                
            }
            dec.get_32(temp_block, bm::set_block_size);
            //dec.memcpy(temp_block, sizeof(bm::word_t) * bm::set_block_size);
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
