/*
Copyright(c) 2002-2005 Anatoliy Kuznetsov(anatoliy_kuznetsov at yahoo.com)

PermiFor more information please visit:  http://bmagic.sourceforge.net
ssion is hereby granted, free of charge, to any person 
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

#ifndef BM__H__INCLUDED__
#define BM__H__INCLUDED__

#include <string.h>
#include <assert.h>
#include <limits.h>

// define BM_NO_STL if you use BM in "STL free" environment and want
// to disable any references to STL headers
#ifndef BM_NO_STL
# include <iterator>
#endif

#include "bmconst.h"
#include "bmdef.h"


// Vector based optimizations are incompatible with 64-bit optimization
// which is considered a form of vectorization
#ifdef BMSSE2OPT
# undef BM64OPT
# define BMVECTOPT
# include "bmsse2.h"
#endif

#include "bmfwd.h"
#include "bmfunc.h"
#include "bmvmin.h"
#include "encoding.h"
#include "bmalloc.h"

namespace bm
{


#ifdef BMCOUNTOPT

# define BMCOUNT_INC ++count_;
# define BMCOUNT_DEC --count_;
# define BMCOUNT_VALID(x) count_is_valid_ = x;
# define BMCOUNT_SET(x) count_ = x; count_is_valid_ = true;
# define BMCOUNT_ADJ(x) if (x) ++count_; else --count_;

#else

# define BMCOUNT_INC
# define BMCOUNT_DEC
# define BMCOUNT_VALID(x)
# define BMCOUNT_SET(x)
# define BMCOUNT_ADJ(x)

#endif



typedef bm::miniset<bm::block_allocator, bm::set_total_blocks> mem_save_set;


/** @defgroup bmagic BitMagic C++ Library
 *  For more information please visit:  http://bmagic.sourceforge.net
 *  
 */


/** @defgroup bvector The Main bvector<> Group
 *  This is the main group. It includes bvector template: front end of the bm library.
 *  @ingroup bmagic 
 */


/*!
   @brief Block allocation strategies.
   @ingroup bvector
*/
enum strategy
{
    BM_BIT = 0, //!< No GAP compression strategy. All new blocks are bit blocks.
    BM_GAP = 1  //!< GAP compression is ON.
};


/*!
   @brief bitvector with runtime compression of bits.
   @ingroup bvector
*/

template<class Alloc, class MS> 
class bvector
{
public:

    typedef Alloc  allocator_type;

    /*!
       @brief Structure with statistical information about bitset's memory 
              allocation details. 
       @ingroup bvector
    */
    struct statistics
    {
        /// Number of bit blocks.
        unsigned bit_blocks; 
        /// Number of GAP blocks.
        unsigned gap_blocks;  
        /// Estimated maximum of memory required for serialization.
        unsigned max_serialize_mem;
        /// Memory used by bitvector including temp and service blocks
        unsigned  memory_used;
        /// Array of all GAP block lengths in the bvector.
        gap_word_t   gap_length[bm::set_total_blocks];
        /// GAP lengths used by bvector
        gap_word_t  gap_levels[bm::gap_levels];
    };

    /**
        @brief Class reference implements an object for bit assignment.
        Since C++ does not provide with build-in bit type supporting l-value 
        operations we have to emulate it.

        @ingroup bvector
    */
    class reference
    {
    public:
        reference(bvector<Alloc, MS>& bv, bm::id_t position) 
        : bv_(bv),
          position_(position)
        {}

        reference(const reference& ref)
        : bv_(ref.bv_), 
          position_(ref.position_)
        {
            bv_.set(position_, ref.bv_.get_bit(position_));
        }
        
        operator bool() const
        {
            return bv_.get_bit(position_);
        }

        const reference& operator=(const reference& ref) const
        {
            bv_.set(position_, (bool)ref);
            return *this;
        }

        const reference& operator=(bool value) const
        {
            bv_.set(position_, value);
            return *this;
        }

        bool operator==(const reference& ref) const
        {
            return bool(*this) == bool(ref);
        }

        /*! Bitwise AND. Performs operation: bit = bit AND value */
        const reference& operator&=(bool value) const
        {
            bv_.set(position_, value);
            return *this;
        }

        /*! Bitwise OR. Performs operation: bit = bit OR value */
        const reference& operator|=(bool value) const
        {
            if (value != bv_.get_bit(position_))
            {
                bv_.set_bit(position_);
            }
            return *this;
        }

        /*! Bitwise exclusive-OR (XOR). Performs operation: bit = bit XOR value */
        const reference& operator^=(bool value) const
        {
            bv_.set(position_, value != bv_.get_bit(position_));
            return *this;
        }

        /*! Logical Not operator */
        bool operator!() const
        {
            return !bv_.get_bit(position_);
        }

        /*! Bit Not operator */
        bool operator~() const
        {
            return !bv_.get_bit(position_);
        }

        /*! Negates the bit value */
        reference& flip()
        {
            bv_.flip(position_);
            return *this;
        }

    private:
        bvector<Alloc, MS>& bv_;       //!< Reference variable on the parent.
        bm::id_t            position_; //!< Position in the parent bitvector.
    };

    typedef bool const_reference;

    /*!
        @brief Base class for all iterators.
        @ingroup bvector
    */
    class iterator_base
    {
    friend class bvector;
    public:
        iterator_base() : block_(0) {}

        bool operator==(const iterator_base& it) const
        {
            return (position_ == it.position_) && (bv_ == it.bv_);
        }

        bool operator!=(const iterator_base& it) const
        {
            return ! operator==(it);
        }

        bool operator < (const iterator_base& it) const
        {
            return position_ < it.position_;
        }

        bool operator <= (const iterator_base& it) const
        {
            return position_ <= it.position_;
        }

        bool operator > (const iterator_base& it) const
        {
            return position_ > it.position_;
        }

        bool operator >= (const iterator_base& it) const
        {
            return position_ >= it.position_;
        }

        /**
           \fn bool bm::bvector::iterator_base::valid() const
           \brief Checks if iterator is still valid. Analog of != 0 comparison for pointers.
           \returns true if iterator is valid.
        */
        bool valid() const
        {
            return position_ != bm::id_max;
        }

        /**
           \fn bool bm::bvector::iterator_base::invalidate() 
           \brief Turns iterator into an invalid state.
        */
        void invalidate()
        {
            position_ = bm::id_max;
        }

    public:

        /** Information about current bitblock. */
        struct bitblock_descr
        {
            const bm::word_t*   ptr;      //!< Word pointer.
            unsigned            bits[32]; //!< Unpacked list of ON bits
            unsigned            idx;      //!< Current position in the bit list
            unsigned            cnt;      //!< Number of ON bits
            bm::id_t            pos;      //!< Last bit position before 
        };

        /** Information about current DGAP block. */
        struct dgap_descr
        {
            const gap_word_t*   ptr;       //!< Word pointer.
            gap_word_t          gap_len;   //!< Current dgap length.
        };

    protected:
        bm::bvector<Alloc, MS>* bv_;         //!< Pointer on parent bitvector
        bm::id_t                position_;   //!< Bit position (bit idx)
        const bm::word_t*       block_;      //!< Block pointer.(NULL-invalid)
        unsigned                block_type_; //!< Type of block. 0-Bit, 1-GAP
        unsigned                block_idx_;  //!< Block index

        /*! Block type dependent information for current block. */
        union block_descr
        {
            bitblock_descr   bit_;  //!< BitBlock related info.
            dgap_descr       gap_;  //!< DGAP block related info.
        } bdescr_;
    };

    /*!
        @brief Output iterator iterator designed to set "ON" bits based on
        their indeces.
        STL container can be converted to bvector using this iterator
        @ingroup bvector
    */
    class insert_iterator
    {
    public:
#ifndef BM_NO_STL
        typedef std::output_iterator_tag  iterator_category;
#endif
        typedef unsigned value_type;
        typedef void difference_type;
        typedef void pointer;
        typedef void reference;

        insert_iterator(bvector<Alloc, MS>& bvect)
         : bvect_(bvect)
        {}
        
        insert_iterator& operator=(bm::id_t n)
        {
            bvect_.set(n);
            return *this;
        }
        
        /*! Returns *this without doing nothing */
        insert_iterator& operator*() { return *this; }
        /*! Returns *this. This iterator does not move */
        insert_iterator& operator++() { return *this; }
        /*! Returns *this. This iterator does not move */
        insert_iterator& operator++(int) { return *this; }
        
    protected:
        bm::bvector<Alloc, MS>&   bvect_;
    };

    /*!
        @brief Constant input iterator designed to enumerate "ON" bits
        @ingroup bvector
    */
    class enumerator : public iterator_base
    {
    public:
#ifndef BM_NO_STL
        typedef std::input_iterator_tag  iterator_category;
#endif
        typedef unsigned   value_type;
        typedef unsigned   difference_type;
        typedef unsigned*  pointer;
        typedef unsigned&  reference;

    public:
        enumerator() : iterator_base() {}
        enumerator(const bvector<Alloc, MS>* bvect, int position)
            : iterator_base()
        { 
            this->bv_ = const_cast<bvector<Alloc, MS>*>(bvect);
            if (position == 0)
            {
                go_first();
            }
            else
            {
                this->invalidate();
            }
        }

        bm::id_t operator*() const
        { 
            return this->position_; 
        }

        enumerator& operator++()
        {
            return this->go_up();
        }

        enumerator operator++(int)
        {
            enumerator tmp = *this;
            this->go_up();
            return tmp;
        }


        void go_first()
        {
            BM_ASSERT(this->bv_);

        #ifdef BMCOUNTOPT
            if (this->bv_->count_is_valid_ && 
                this->bv_->count_ == 0)
            {
                this->invalidate();
                return;
            }
        #endif

            typename bm::bvector<Alloc, MS>::blocks_manager* bman = 
                                                &(this->bv_->blockman_);
            bm::word_t*** blk_root = bman->blocks_root();

            this->block_idx_ = this->position_= 0;
            unsigned i, j;

            for (i = 0; i < bman->top_block_size(); ++i)
            {
                bm::word_t** blk_blk = blk_root[i];

                if (blk_blk == 0) // not allocated
                {
                    this->block_idx_ += bm::set_array_size;
                    this->position_ += bm::bits_in_array;
                    continue;
                }


                for (j = 0; j < bm::set_array_size; ++j,++(this->block_idx_))
                {
                    this->block_ = blk_blk[j];

                    if (this->block_ == 0)
                    {
                        this->position_ += bits_in_block;
                        continue;
                    }

                    if (BM_IS_GAP((*bman), this->block_, this->block_idx_))
                    {
                        this->block_type_ = 1;
                        if (search_in_gapblock())
                        {
                            return;
                        }
                    }
                    else
                    {
                        this->block_type_ = 0;
                        if (search_in_bitblock())
                        {
                            return;
                        }
                    }
            
                } // for j

            } // for i

            this->invalidate();
        }

        enumerator& go_up()
        {
            // Current block search.
            ++this->position_;
            typedef typename iterator_base::block_descr block_descr_type;
            
            block_descr_type* bdescr = &(this->bdescr_);

            switch (this->block_type_)
            {
            case 0:   //  BitBlock
                {

                // check if we can get the value from the 
                // bits cache

                unsigned idx = ++(bdescr->bit_.idx);
                if (idx < bdescr->bit_.cnt)
                {
                    this->position_ = bdescr->bit_.pos + 
                                      bdescr->bit_.bits[idx];
                    return *this; 
                }
                this->position_ += 31 - bdescr->bit_.bits[--idx];

                const bm::word_t* pend = this->block_ + bm::set_block_size;

                while (++(bdescr->bit_.ptr) < pend)
                {
                    bm::word_t w = *(bdescr->bit_.ptr);
                    if (w)
                    {
                        bdescr->bit_.idx = 0;
                        bdescr->bit_.pos = this->position_;
                        bdescr->bit_.cnt = bm::bit_list(w, bdescr->bit_.bits); 
                        this->position_ += bdescr->bit_.bits[0];
                        return *this;
                    }
                    else
                    {
                        this->position_ += 32;
                    }
                }
    
                }
                break;

            case 1:   // DGAP Block
                {
                    if (--(bdescr->gap_.gap_len))
                    {
                        return *this;
                    }

                    // next gap is "OFF" by definition.
                    if (*(bdescr->gap_.ptr) == bm::gap_max_bits - 1)
                    {
                        break;
                    }
                    gap_word_t prev = *(bdescr->gap_.ptr);
                    register unsigned val = *(++(bdescr->gap_.ptr));

                    this->position_ += val - prev;
                    // next gap is now "ON"

                    if (*(bdescr->gap_.ptr) == bm::gap_max_bits - 1)
                    {
                        break;
                    }
                    prev = *(bdescr->gap_.ptr);
                    val = *(++(bdescr->gap_.ptr));
                    bdescr->gap_.gap_len = val - prev;
                    return *this;  // next "ON" found;
                }

            default:
                BM_ASSERT(0);

            } // switch


            // next bit not present in the current block
            // keep looking in the next blocks.
            ++(this->block_idx_);
            unsigned i = this->block_idx_ >> bm::set_array_shift;
            unsigned top_block_size = this->bv_->blockman_.top_block_size();
            for (; i < top_block_size; ++i)
            {
                bm::word_t** blk_blk = this->bv_->blockman_.blocks_[i];
                if (blk_blk == 0)
                {
                    this->block_idx_ += bm::set_array_size;
                    this->position_ += bm::bits_in_array;
                    continue;
                }

                unsigned j = this->block_idx_ & bm::set_array_mask;

                for(; j < bm::set_array_size; ++j, ++(this->block_idx_))
                {
                    this->block_ = blk_blk[j];

                    if (this->block_ == 0)
                    {
                        this->position_ += bm::bits_in_block;
                        continue;
                    }

                    if (BM_IS_GAP((this->bv_->blockman_), 
                                   this->block_, 
                                   this->block_idx_))
                    {
                        this->block_type_ = 1;
                        if (search_in_gapblock())
                        {
                            return *this;
                        }
                    }
                    else
                    {
                        this->block_type_ = 0;
                        if (search_in_bitblock())
                        {
                            return *this;
                        }
                    }

            
                } // for j

            } // for i


            this->invalidate();
            return *this;
        }


    private:
        bool search_in_bitblock()
        {
            BM_ASSERT(this->block_type_ == 0);
            
            typedef typename iterator_base::block_descr block_descr_type;
            block_descr_type* bdescr = &(this->bdescr_);            

            // now lets find the first bit in block.
            bdescr->bit_.ptr = this->block_;

            const word_t* ptr_end = this->block_ + bm::set_block_size;

            do
            {
                register bm::word_t w = *(bdescr->bit_.ptr);

                if (w)  
                {
                    bdescr->bit_.idx = 0;
                    bdescr->bit_.pos = this->position_;
                    bdescr->bit_.cnt = 
                              bm::bit_list(w, bdescr->bit_.bits);
                    this->position_ += bdescr->bit_.bits[0];

                    return true;
                }
                else
                {
                    this->position_ += 32;
                }

            } 
            while (++(bdescr->bit_.ptr) < ptr_end);

            return false;
        }

        bool search_in_gapblock()
        {
            BM_ASSERT(this->block_type_ == 1);

            typedef typename iterator_base::block_descr block_descr_type;
            block_descr_type* bdescr = &(this->bdescr_);            

            bdescr->gap_.ptr = BMGAP_PTR(this->block_);
            unsigned bitval = *(bdescr->gap_.ptr) & 1;

            ++(bdescr->gap_.ptr);

            do
            {
                register unsigned val = *(bdescr->gap_.ptr);

                if (bitval)
                {
                    gap_word_t* first = BMGAP_PTR(this->block_) + 1;
                    if (bdescr->gap_.ptr == first)
                    {
                        bdescr->gap_.gap_len = val + 1;
                    }
                    else
                    {
                        bdescr->gap_.gap_len = 
                             val - *(bdescr->gap_.ptr-1);
                    }
           
                    return true;
                }
                this->position_ += val + 1;

                if (val == bm::gap_max_bits - 1)
                {
                    break;
                }

                bitval ^= 1;
                ++(bdescr->gap_.ptr);

            } while (1);

            return false;
        }

    };
    
    /*!
        @brief Constant input iterator designed to enumerate "ON" bits
        counted_enumerator keeps bitcount, ie number of ON bits starting
        from the position 0 in the bit string up to the currently enumerated bit
        
        When increment operator called current position is increased by 1.
        
        @ingroup bvector
    */
    class counted_enumerator : public enumerator
    {
    public:
#ifndef BM_NO_STL
        typedef std::input_iterator_tag  iterator_category;
#endif
        counted_enumerator() : bit_count_(0){}
        
        counted_enumerator(const enumerator& en)
        : enumerator(en)
        {
            if (this->valid())
                bit_count_ = 1;
        }
        
        counted_enumerator& operator=(const enumerator& en)
        {
            enumerator* me = this;
            *me = en;
            if (this->valid())
                this->bit_count_ = 1;
            return *this;
        }
        
        counted_enumerator& operator++()
        {
            this->go_up();
            if (this->valid())
                ++(this->bit_count_);
            return *this;
        }

        counted_enumerator operator++(int)
        {
            counted_enumerator tmp(*this);
            this->go_up();
            if (this->valid())
                ++bit_count_;
            return tmp;
        }
        
        /*! @brief Number of bits ON starting from the .
        
            Method returns number of ON bits fromn the bit 0 to the current bit 
            For the first bit in bitvector it is 1, for the second 2 
        */
        bm::id_t count() const { return bit_count_; }
        
    private:
        bm::id_t   bit_count_;
    };

    friend class iterator_base;
    friend class enumerator;

public:

#ifdef BMCOUNTOPT
    bvector(strategy          strat = BM_BIT, 
            const gap_word_t* glevel_len=bm::gap_len_table<true>::_len,
            bm::id_t          max_bits = 0,
            const Alloc&      alloc = Alloc()) 
    : count_(0),
      count_is_valid_(true),
      blockman_(glevel_len, max_bits, alloc),
      new_blocks_strat_(strat)
    {}

    bvector(const bm::bvector<Alloc, MS>& bvect)
     : count_(bvect.count_),
       count_is_valid_(bvect.count_is_valid_),
       blockman_(bvect.blockman_),
       new_blocks_strat_(bvect.new_blocks_strat_)
    {}

#else
    /*!
        \brief Constructs bvector class
        \param strat - operation mode strategy, 
                       BM_BIT - default strategy, bvector use plain bitset blocks,
                       (performance oriented strategy).
                       BM_GAP - memory effitent strategy, bvector allocates blocks
                       as array of intervals(gaps) and convert blocks into plain bitsets
                       only when enthropy grows.
        \param glevel_len - pointer on C-style array keeping GAP block sizes. 
                            (Put bm::gap_len_table_min<true>::_len for GAP memory saving mode)
        \param max_bits - maximum number of bits addressable by bvector, 0 means "no limits" (recommended)
        \sa bm::gap_len_table bm::gap_len_table_min
    */
    bvector(strategy          strat      = BM_BIT,
            const gap_word_t* glevel_len = bm::gap_len_table<true>::_len,
            bm::id_t          max_bits   = 0,
            const Alloc&       alloc      = Alloc()) 
    : blockman_(glevel_len, max_bits, alloc),
      new_blocks_strat_(strat)
    {}

    bvector(const bvector<Alloc, MS>& bvect)
        :  blockman_(bvect.blockman_),
           new_blocks_strat_(bvect.new_blocks_strat_)
    {}

#endif

    bvector& operator = (const bvector<Alloc, MS>& bvect)
    {
        if (bvect.blockman_.top_block_size() != blockman_.top_block_size())
        {
            blockman_.set_top_block_size(bvect.blockman_.top_block_size());
        }
        else
        {
            clear(false);
        }
        bit_or(bvect);
        return *this;
    }

    reference operator[](bm::id_t n)
    {
        assert(n < bm::id_max);
        return reference(*this, n);
    }


    bool operator[](bm::id_t n) const
    {
        assert(n < bm::id_max);
        return get_bit(n);
    }

    void operator &= (const bvector<Alloc, MS>& bvect)
    {
        bit_and(bvect);
    }

    void operator ^= (const bvector<Alloc, MS>& bvect)
    {
        bit_xor(bvect);
    }

    void operator |= (const bvector<Alloc, MS>& bvect)
    {
        bit_or(bvect);
    }

    void operator -= (const bvector<Alloc, MS>& bvect)
    {
        bit_sub(bvect);
    }

    bool operator < (const bvector<Alloc, MS>& bvect) const
    {
        return compare(bvect) < 0;
    }

    bool operator <= (const bvector<Alloc, MS>& bvect) const
    {
        return compare(bvect) <= 0;
    }

    bool operator > (const bvector<Alloc, MS>& bvect) const
    {
        return compare(bvect) > 0;
    }

    bool operator >= (const bvector<Alloc, MS>& bvect) const
    {
        return compare(bvect) >= 0;
    }

    bool operator == (const bvector<Alloc, MS>& bvect) const
    {
        return compare(bvect) == 0;
    }

    bool operator != (const bvector<Alloc, MS>& bvect) const
    {
        return compare(bvect) != 0;
    }

    bvector<Alloc, MS> operator~() const
    {
        return bvector<Alloc, MS>(*this).invert();
    }
    
    Alloc get_allocator() const
    {
        return blockman_.get_allocator();
    } 


    /*!
        \brief Sets bit n if val is true, clears bit n if val is false
        \param n - index of the bit to be set
        \param val - new bit value
        \return *this
    */
    bvector<Alloc, MS>& set(bm::id_t n, bool val = true)
    {
        // calculate logical block number
        unsigned nblock = unsigned(n >>  bm::set_block_shift); 

        int block_type;

        bm::word_t* blk = 
            blockman_.check_allocate_block(nblock, 
                                           val, 
                                           get_new_blocks_strat(), 
                                           &block_type);

        if (!blk) return *this;

        // calculate word number in block and bit
        unsigned nbit   = unsigned(n & bm::set_block_mask); 

        if (block_type == 1) // gap
        {
            unsigned is_set;
            unsigned new_block_len;
            
            new_block_len = 
                gap_set_value(val, BMGAP_PTR(blk), nbit, &is_set);
            if (is_set)
            {
                BMCOUNT_ADJ(val)

                unsigned threshold = 
                bm::gap_limit(BMGAP_PTR(blk), blockman_.glen());

                if (new_block_len > threshold) 
                {
                    extend_gap_block(nblock, BMGAP_PTR(blk));
                }
            }
        }
        else  // bit block
        {
            unsigned nword  = unsigned(nbit >> bm::set_word_shift); 
            nbit &= bm::set_word_mask;

            bm::word_t* word = blk + nword;
            bm::word_t  mask = (((bm::word_t)1) << nbit);

            if (val)
            {
                if ( ((*word) & mask) == 0 )
                {
                    *word |= mask; // set bit
                    BMCOUNT_INC;
                }
            }
            else
            {
                if ((*word) & mask)
                {
                    *word &= ~mask; // clear bit
                    BMCOUNT_DEC;
                }
            }
        }
        
        return *this;
    }

    /*!
       \brief Sets every bit in this bitset to 1.
       \return *this
    */
    bvector<Alloc, MS>& set()
    {
        blockman_.set_all_one();
        set(bm::id_max, false);
        BMCOUNT_SET(id_max);
        return *this;
    }


    /*!
        \brief Sets all bits in the specified closed interval [left,right]
        
        \param left  - interval start
        \param right - interval end (closed interval)
        \param value - value to set interval in
        
        \return *this
    */
    bvector<Alloc, MS>& set_range(bm::id_t left,
                                  bm::id_t right,
                                  bool     value = true)
    {
        if (right < left)
        {
            return set_range(right, left, value);
        }

        BMCOUNT_VALID(false)
        BM_SET_MMX_GUARD

        // calculate logical number of start and destination blocks
        unsigned nblock_left  = unsigned(left  >>  bm::set_block_shift);
        unsigned nblock_right = unsigned(right >>  bm::set_block_shift);

        bm::word_t* block = blockman_.get_block(nblock_left);
        bool left_gap = BM_IS_GAP(blockman_, block, nblock_left);

        unsigned nbit_left  = unsigned(left  & bm::set_block_mask); 
        unsigned nbit_right = unsigned(right & bm::set_block_mask); 

        unsigned r = 
           (nblock_left == nblock_right) ? nbit_right :(bm::bits_in_block-1);

        // Set bits in the starting block

        bm::gap_word_t tmp_gap_blk[5] = {0,};
        gap_init_range_block(tmp_gap_blk, 
                             nbit_left, r, value, bm::bits_in_block);

        combine_operation_with_block(nblock_left, 
                                     left_gap, 
                                     block,
                                     (bm::word_t*) tmp_gap_blk,
                                     1,
                                     value ? BM_OR : BM_AND);

        if (nblock_left == nblock_right)  // in one block
            return *this;

        // Set (or clear) all blocks between left and right
        
        unsigned nb_to = nblock_right + (nbit_right ==(bm::bits_in_block-1));
                
        if (value)
        {
            for (unsigned nb = nblock_left+1; nb < nb_to; ++nb)
            {
                block = blockman_.get_block(nb);
                if (IS_FULL_BLOCK(block)) 
                    continue;

                bool is_gap = BM_IS_GAP(blockman_, block, nb);

                blockman_.set_block(nb, FULL_BLOCK_ADDR);
                blockman_.set_block_bit(nb);
                
                if (is_gap)
                {
                    blockman_.get_allocator().free_gap_block(BMGAP_PTR(block), 
                                                             blockman_.glen());
                }
                else
                {
                    blockman_.get_allocator().free_bit_block(block);
                }
                
            } // for
        }
        else // value == 0
        {
            for (unsigned nb = nblock_left+1; nb < nb_to; ++nb)
            {
                block = blockman_.get_block(nb);
                if (block == 0)  // nothing to do
                    continue;

                blockman_.set_block(nb, 0);
                blockman_.set_block_bit(nb);

                bool is_gap = BM_IS_GAP(blockman_, block, nb);

                if (is_gap) 
                {
                    blockman_.get_allocator().free_gap_block(BMGAP_PTR(block),
                                                             blockman_.glen());
                }
                else
                {
                    blockman_.get_allocator().free_bit_block(block);
                }


            } // for
        } // if value else 

        if (nb_to > nblock_right)
            return *this;

        block = blockman_.get_block(nblock_right);
        bool right_gap = BM_IS_GAP(blockman_, block, nblock_right);

        gap_init_range_block(tmp_gap_blk, 
                             0, nbit_right, value, bm::bits_in_block);

        combine_operation_with_block(nblock_right, 
                                     right_gap, 
                                     block,
                                     (bm::word_t*) tmp_gap_blk,
                                     1,
                                     value ? BM_OR : BM_AND);

        return *this;
    }


    /*!
       \brief Sets bit n.
       \param n - index of the bit to be set. 
    */
    void set_bit(bm::id_t n)
    {
        set(n, true);
    }
    
    /*! Function erturns insert iterator for this bitvector */
    insert_iterator inserter()
    {
        return insert_iterator(*this);
    }


    /*!
       \brief Clears bit n.
       \param n - bit's index to be cleaned.
    */
    void clear_bit(bm::id_t n)
    {
        set(n, false);
    }


    /*!
       \brief Clears every bit in the bitvector.

       \param free_mem if "true" (default) bvector frees the memory,
       otherwise sets blocks to 0.
    */
    void clear(bool free_mem = false)
    {
        blockman_.set_all_zero(free_mem);
        BMCOUNT_SET(0);
    }

    /*!
       \brief Clears every bit in the bitvector.
       \return *this;
    */
    bvector<Alloc, MS>& reset()
    {
        clear();
        return *this;
    }


    /*!
       \brief Returns count of bits which are 1.
       \return Total number of bits ON. 
    */
    bm::id_t count() const
    {
    #ifdef BMCOUNTOPT
        if (count_is_valid_) return count_;
    #endif
        word_t*** blk_root = blockman_.get_rootblock();
        typename blocks_manager::block_count_func func(blockman_);
        for_each_nzblock(blk_root, blockman_.top_block_size(), 
                                   bm::set_array_size, func);

        BMCOUNT_SET(func.count());
        return func.count();
    }

    /*! \brief Computes bitcount values for all bvector blocks
        \param arr - pointer on array of block bit counts
        \return Index of the last block counted. 
        This number +1 gives you number of arr elements initialized during the
        function call.
    */
    unsigned count_blocks(unsigned* arr) const
    {
        bm::word_t*** blk_root = blockman_.get_rootblock();
        typename blocks_manager::block_count_arr_func func(blockman_, &(arr[0]));
        for_each_nzblock(blk_root, blockman_.top_block_size(), 
                                   bm::set_array_size, func);
        return func.last_block();
    }

    /*!
       \brief Returns count of 1 bits in the given diapason.
       \param left - index of first bit start counting from
       \param right - index of last bit 
       \param block_count_arr - optional parameter (bitcount by bvector blocks)
              calculated by count_blocks method. Used to improve performance of
              wide range searches
       \return Total number of bits ON. 
    */
    bm::id_t count_range(bm::id_t left, 
                         bm::id_t right, 
                         unsigned* block_count_arr=0) const
    {
        assert(left <= right);

        unsigned count = 0;

        // calculate logical number of start and destination blocks
        unsigned nblock_left  = unsigned(left  >>  bm::set_block_shift);
        unsigned nblock_right = unsigned(right >>  bm::set_block_shift);

        const bm::word_t* block = blockman_.get_block(nblock_left);
        bool left_gap = BM_IS_GAP(blockman_, block, nblock_left);

        unsigned nbit_left  = unsigned(left  & bm::set_block_mask); 
        unsigned nbit_right = unsigned(right & bm::set_block_mask); 

        unsigned r = 
           (nblock_left == nblock_right) ? nbit_right : (bm::bits_in_block-1);

        typename blocks_manager::block_count_func func(blockman_);

        if (block)
        {
            if ((nbit_left == 0) && (r == (bm::bits_in_block-1))) // whole block
            {
                if (block_count_arr)
                {
                    count += block_count_arr[nblock_left];
                }
                else
                {
                    func(block, nblock_left);
                }
            }
            else
            {
                if (left_gap)
                {
                    count += gap_bit_count_range(BMGAP_PTR(block), 
                                                (gap_word_t)nbit_left,
                                                (gap_word_t)r);
                }
                else
                {
                    count += bit_block_calc_count_range(block, nbit_left, r);
                }
            }
        }

        if (nblock_left == nblock_right)  // in one block
        {
            return count + func.count();
        }

        for (unsigned nb = nblock_left+1; nb < nblock_right; ++nb)
        {
            block = blockman_.get_block(nb);
            if (block_count_arr)
            {
                count += block_count_arr[nb];
            }
            else 
            {
                if (block)
                    func(block, nb);
            }
        }
        count += func.count();

        block = blockman_.get_block(nblock_right);
        bool right_gap = BM_IS_GAP(blockman_, block, nblock_right);

        if (block)
        {
            if (right_gap)
            {
                count += gap_bit_count_range(BMGAP_PTR(block),
                                            (gap_word_t)0,
                                            (gap_word_t)nbit_right);
            }
            else
            {
                count += bit_block_calc_count_range(block, 0, nbit_right);
            }
        }

        return count;
    }


    bm::id_t recalc_count()
    {
        BMCOUNT_VALID(false)
        return count();
    }
    
    /*!
        Disables count cache. Next call to count() or recalc_count()
        restores count caching.
        
        @note Works only if BMCOUNTOPT enabled(defined). 
        Othewise does nothing.
    */
    void forget_count()
    {
        BMCOUNT_VALID(false)    
    }

    /*!
        \brief Inverts all bits.
    */
    bvector<Alloc, MS>& invert()
    {
        BMCOUNT_VALID(false)
        BM_SET_MMX_GUARD

        bm::word_t*** blk_root = blockman_.get_rootblock();
        typename blocks_manager::block_invert_func func(blockman_);    
        for_each_block(blk_root, blockman_.top_block_size(),
                                 bm::set_array_size, func);
        set(bm::id_max, false);
        return *this;
    }


    /*!
       \brief returns true if bit n is set and false is bit n is 0. 
       \param n - Index of the bit to check.
       \return Bit value (1 or 0)
    */
    bool get_bit(bm::id_t n) const
    {    
        BM_ASSERT(n < bm::id_max);

        // calculate logical block number
        unsigned nblock = unsigned(n >>  bm::set_block_shift); 

        const bm::word_t* block = blockman_.get_block(nblock);

        if (block)
        {
            // calculate word number in block and bit
            unsigned nbit = unsigned(n & bm::set_block_mask); 
            unsigned is_set;

            if (BM_IS_GAP(blockman_, block, nblock))
            {
                is_set = gap_test(BMGAP_PTR(block), nbit);
            }
            else 
            {
                unsigned nword  = unsigned(nbit >> bm::set_word_shift); 
                nbit &= bm::set_word_mask;

                is_set = (block[nword] & (((bm::word_t)1) << nbit));
            }
            return is_set != 0;
        }
        return false;
    }

    /*!
       \brief returns true if bit n is set and false is bit n is 0. 
       \param n - Index of the bit to check.
       \return Bit value (1 or 0)
    */
    bool test(bm::id_t n) const 
    { 
        return get_bit(n); 
    }

    /*!
       \brief Returns true if any bits in this bitset are set, and otherwise returns false.
       \return true if any bit is set
    */
    bool any() const
    {
    #ifdef BMCOUNTOPT
        if (count_is_valid_ && count_) return true;
    #endif
        
        word_t*** blk_root = blockman_.get_rootblock();
        typename blocks_manager::block_any_func func(blockman_);
        return for_each_nzblock_if(blk_root, blockman_.top_block_size(),
                                             bm::set_array_size, func);
    }

    /*!
        \brief Returns true if no bits are set, otherwise returns false.
    */
    bool none() const
    {
        return !any();
    }

    /*!
       \brief Flips bit n
       \return *this
    */
    bvector<Alloc, MS>& flip(bm::id_t n) 
    {
        set(n, !get_bit(n));
        return *this;
    }

    /*!
       \brief Flips all bits
       \return *this
    */
    bvector<Alloc, MS>& flip() 
    {
        return invert();
    }

    /*! \brief Exchanges content of bv and this bitvector.
    */
    void swap(bvector<Alloc, MS>& bv)
    {
        blockman_.swap(bv.blockman_);
#ifdef BMCOUNTOPT
        BMCOUNT_VALID(false)
        bv.recalc_count();
#endif
    }


    /*!
       \fn bm::id_t bvector::get_first() const
       \brief Gets number of first bit which is ON.
       \return Index of the first 1 bit.
       \sa get_next
    */
    bm::id_t get_first() const { return check_or_next(0); }

    /*!
       \fn bm::id_t bvector::get_next(bm::id_t prev) const
       \brief Finds the number of the next bit ON.
       \param prev - Index of the previously found bit. 
       \return Index of the next bit which is ON or 0 if not found.
       \sa get_first
    */
    bm::id_t get_next(bm::id_t prev) const
    {
        return (++prev == bm::id_max) ? 0 : check_or_next(prev);
    }

    /*!
       @brief Calculates bitvector statistics.

       @param st - pointer on statistics structure to be filled in. 

       Function fills statistics structure containing information about how 
       this vector uses memory and estimation of max. amount of memory 
       bvector needs to serialize itself.

       @sa statistics
    */
    void calc_stat(struct statistics* st) const;

    /*!
       \brief Logical OR operation.
       \param vect - Argument vector.
    */
    bm::bvector<Alloc, MS>& bit_or(const  bm::bvector<Alloc, MS>& vect)
    {
        BMCOUNT_VALID(false);
        combine_operation(vect, BM_OR);
        return *this;
    }

    /*!
       \brief Logical AND operation.
       \param vect - Argument vector.
    */
    bm::bvector<Alloc, MS>& bit_and(const bm::bvector<Alloc, MS>& vect)
    {
        BMCOUNT_VALID(false);
        combine_operation(vect, BM_AND);
        return *this;
    }

    /*!
       \brief Logical XOR operation.
       \param vect - Argument vector.
    */
    bm::bvector<Alloc, MS>& bit_xor(const bm::bvector<Alloc, MS>& vect)
    {
        BMCOUNT_VALID(false);
        combine_operation(vect, BM_XOR);
        return *this;
    }

    /*!
       \brief Logical SUB operation.
       \param vect - Argument vector.
    */
    bm::bvector<Alloc, MS>& bit_sub(const bm::bvector<Alloc, MS>& vect)
    {
        BMCOUNT_VALID(false);
        combine_operation(vect, BM_SUB);
        return *this;
    }


    /*!
       \brief Sets new blocks allocation strategy.
       \param strat - Strategy code 0 - bitblocks allocation only.
                      1 - Blocks mutation mode (adaptive algorithm)
    */
    void set_new_blocks_strat(strategy strat) 
    { 
        new_blocks_strat_ = strat; 
    }

    /*!
       \brief Returns blocks allocation strategy.
       \return - Strategy code 0 - bitblocks allocation only.
                 1 - Blocks mutation mode (adaptive algorithm)
       \sa set_new_blocks_strat
    */
    strategy  get_new_blocks_strat() const 
    { 
        return new_blocks_strat_; 
    }

    void stat(unsigned blocks=0) const;

    /*! 
        \brief Optimization mode
        Every next level means additional checks (better compression vs time)
        \sa optimize
    */
    enum optmode
    {
        opt_free_0    = 1, ///< Free unused 0 blocks
        opt_free_01   = 2, ///< Free unused 0 and 1 blocks
        opt_compress  = 3  ///< compress blocks when possible
    };

    /*!
       \brief Optimize memory bitvector's memory allocation.
   
       Function analyze all blocks in the bitvector, compresses blocks 
       with a regular structure, frees some memory. This function is recommended
       after a bulk modification of the bitvector using set_bit, clear_bit or
       logical operations.
       
       @sa optmode, optimize_gap_size
    */
    void optimize(bm::word_t* temp_block=0, optmode opt_mode = opt_compress)
    {
        word_t*** blk_root = blockman_.blocks_root();

        if (!temp_block)
            temp_block = blockman_.check_allocate_tempblock();

        typename 
          blocks_manager::block_opt_func  opt_func(blockman_, 
                                                   temp_block, 
                                                   (int)opt_mode);
        for_each_nzblock(blk_root, blockman_.top_block_size(),
                                   bm::set_array_size, opt_func);
    }

    
    void optimize_gap_size()
    {
        struct bvector<Alloc, MS>::statistics st;
        calc_stat(&st);

        if (!st.gap_blocks)
            return;

        gap_word_t opt_glen[bm::gap_levels];
        ::memcpy(opt_glen, st.gap_levels, bm::gap_levels * sizeof(*opt_glen));

        improve_gap_levels(st.gap_length, 
                                st.gap_length + st.gap_blocks, 
                                opt_glen);
        
        set_gap_levels(opt_glen);
    }


    /*!
        @brief Sets new GAP lengths table. All GAP blocks will be reallocated 
        to match the new scheme.

        @param glevel_len - pointer on C-style array keeping GAP block sizes. 
    */
    void set_gap_levels(const gap_word_t* glevel_len)
    {
        word_t*** blk_root = blockman_.blocks_root();

        typename 
            blocks_manager::gap_level_func  gl_func(blockman_, glevel_len);

        for_each_nzblock(blk_root, blockman_.top_block_size(),
                                   bm::set_array_size, gl_func);

        blockman_.set_glen(glevel_len);
    }

    /*!
        \brief Lexicographical comparison with a bitvector.

        Function compares current bitvector with the provided argument 
        bit by bit and returns -1 if our bitvector less than the argument, 
        1 - greater, 0 - equal.
    */
    int compare(const bvector<Alloc, MS>& bvect) const
    {
        int res;
        unsigned bn = 0;

        for (unsigned i = 0; i < blockman_.top_block_size(); ++i)
        {
            const bm::word_t* const* blk_blk = blockman_.get_topblock(i);
            const bm::word_t* const* arg_blk_blk = 
                                   bvect.blockman_.get_topblock(i);

            if (blk_blk == arg_blk_blk) 
            {
                bn += bm::set_array_size;
                continue;
            }

            for (unsigned j = 0; j < bm::set_array_size; ++j, ++bn)
            {
                const bm::word_t* arg_blk = 
                                    arg_blk_blk ? arg_blk_blk[j] : 0;
                const bm::word_t* blk = blk_blk ? blk_blk[j] : 0;

                if (blk == arg_blk) continue;

                // If one block is zero we check if the other one has at least 
                // one bit ON

                if (!blk || !arg_blk)  
                {
                    const bm::word_t*  pblk;
                    bool is_gap;

                    if (blk)
                    {
                        pblk = blk;
                        res = 1;
                        is_gap = BM_IS_GAP((*this), blk, bn);
                    }
                    else
                    {
                        pblk = arg_blk;
                        res = -1;
                        is_gap = BM_IS_GAP(bvect, arg_blk, bn);
                    }

                    if (is_gap)
                    {
                        if (!gap_is_all_zero(BMGAP_PTR(pblk), bm::gap_max_bits))
                        {
                            return res;
                        }
                    }
                    else
                    {
                        bm::wordop_t* blk1 = (wordop_t*)pblk;
                        bm::wordop_t* blk2 = 
                            (wordop_t*)(pblk + bm::set_block_size);
                        if (!bit_is_all_zero(blk1, blk2))
                        {
                            return res;
                        }
                    }

                    continue;
                }

                bool arg_gap = BM_IS_GAP(bvect, arg_blk, bn);
                bool gap = BM_IS_GAP((*this), blk, bn);

                if (arg_gap != gap)
                {
                    bm::wordop_t temp_blk[bm::set_block_size_op]; 
                    bm::wordop_t* blk1;
                    bm::wordop_t* blk2;

                    if (gap)
                    {
                        gap_convert_to_bitset((bm::word_t*)temp_blk, 
                                              BMGAP_PTR(blk));

                        blk1 = (bm::wordop_t*)temp_blk;
                        blk2 = (bm::wordop_t*)arg_blk;
                    }
                    else
                    {
                        gap_convert_to_bitset((bm::word_t*)temp_blk, 
                                              BMGAP_PTR(arg_blk));

                        blk1 = (bm::wordop_t*)blk;
                        blk2 = (bm::wordop_t*)temp_blk;

                    }                        
                    res = bitcmp(blk1, blk2, bm::set_block_size_op);  

                }
                else
                {
                    if (gap)
                    {
                        res = gapcmp(BMGAP_PTR(blk), BMGAP_PTR(arg_blk));
                    }
                    else
                    {
                        res = bitcmp((bm::wordop_t*)blk, 
                                     (bm::wordop_t*)arg_blk, 
                                      bm::set_block_size_op);
                    }
                }

                if (res != 0)
                {
                    return res;
                }
            

            } // for j

        } // for i

        return 0;
    }


    /*! @brief Allocates temporary block of memory. 

        Temp block can be passed to bvector functions requiring some temp memory
        for their operation. (like serialize)
        
        @note method is marked const, but it's not quite true, since
        it can in some cases modify the state of the block allocator
        (if it has a state). (Can be important in MT programs).

        @sa free_tempblock
    */
    bm::word_t* allocate_tempblock() const
    {
        blocks_manager* bm = const_cast<blocks_manager*>(&blockman_);
        return bm->get_allocator().alloc_bit_block();
    }

    /*! @brief Frees temporary block of memory. 

        @note method is marked const, but it's not quite true, since
        it can in some cases modify the state of the block allocator
        (if it has a state). (Can be important in MT programs).

        @sa allocate_tempblock
    */
    void free_tempblock(bm::word_t* block) const
    {
        blocks_manager* bm = const_cast<blocks_manager*>(&blockman_);
        bm->get_allocator().free_bit_block(block);
    }

    /**
       \brief Returns enumerator pointing on the first non-zero bit.
    */
    enumerator first() const
    {
        typedef typename bvector<Alloc, MS>::enumerator enumerator_type;
        return enumerator_type(this, 0);
    }

    /**
       \fn bvector::enumerator bvector::end() const
       \brief Returns enumerator pointing on the next bit after the last.
    */
    enumerator end() const
    {
        typedef typename bvector<Alloc, MS>::enumerator enumerator_type;
        return enumerator_type(this, 1);
    }


    const bm::word_t* get_block(unsigned nb) const 
    { 
        return blockman_.get_block(nb); 
    }
    
private:

    bm::id_t check_or_next(bm::id_t prev) const
    {
        for (;;)
        {
            unsigned nblock = unsigned(prev >> bm::set_block_shift); 
            if (nblock >= bm::set_total_blocks) break;

            if (blockman_.is_subblock_null(nblock >> bm::set_array_shift))
            {
                prev += (bm::set_blkblk_mask + 1) -
                              (prev & bm::set_blkblk_mask);
            }
            else
            {
                unsigned nbit = unsigned(prev & bm::set_block_mask);

                const bm::word_t* block = blockman_.get_block(nblock);
    
                if (block)
                {
                    if (IS_FULL_BLOCK(block)) return prev;
                    if (BM_IS_GAP(blockman_, block, nblock))
                    {
                        if (gap_find_in_block(BMGAP_PTR(block), nbit, &prev))
                        {
                            return prev;
                        }
                    }
                    else
                    {
                        if (bit_find_in_block(block, nbit, &prev)) 
                        {
                            return prev;
                        }
                    }
                }
                else
                {
                    prev += (bm::set_block_mask + 1) - 
                                (prev & bm::set_block_mask);
                }

            }
            if (!prev) break;
        }

        return 0;
    }
    

    void combine_operation(const bm::bvector<Alloc, MS>& bvect, 
                           bm::operation                 opcode)
    {
        typedef void (*block_bit_op)(bm::word_t*, const bm::word_t*);
        typedef void (*block_bit_op_next)(bm::word_t*, 
                                          const bm::word_t*, 
                                          bm::word_t*, 
                                          const bm::word_t*);
        
        block_bit_op      bit_func;
        switch (opcode)
        {
        case BM_AND:
            bit_func = bit_block_and;
            break;
        case BM_OR:
            bit_func = bit_block_or;
            break;
        case BM_SUB:
            bit_func = bit_block_sub;
            break;
        case BM_XOR:
            bit_func = bit_block_xor;
            break;
        }           
       
        
        bm::word_t*** blk_root = blockman_.blocks_root();
        unsigned block_idx = 0;
        unsigned i, j;

        BM_SET_MMX_GUARD

        for (i = 0; i < blockman_.top_block_size(); ++i)
        {
            bm::word_t** blk_blk = blk_root[i];

            if (blk_blk == 0) // not allocated
            {
                const bm::word_t* const* bvbb = 
                                bvect.blockman_.get_topblock(i);
                if (bvbb == 0) 
                {
                    block_idx += bm::set_array_size;
                    continue;
                }

                for (j = 0; j < bm::set_array_size; ++j,++block_idx)
                {
                    const bm::word_t* arg_blk = bvect.blockman_.get_block(i, j);

                    if (arg_blk != 0)
                    {
                       bool arg_gap = BM_IS_GAP(bvect.blockman_, arg_blk, block_idx);
                       
                       combine_operation_with_block(block_idx, 0, 0, 
                                                    arg_blk, arg_gap, 
                                                    opcode);
                    }

                } // for k
                continue;
            }

            for (j = 0; j < bm::set_array_size; ++j, ++block_idx)
            {
                
                bm::word_t* blk = blk_blk[j];
                const bm::word_t* arg_blk = bvect.blockman_.get_block(i, j);

                if (arg_blk || blk)
                {
                   bool arg_gap = BM_IS_GAP(bvect.blockman_, arg_blk, block_idx);
                   bool gap = BM_IS_GAP((*this).blockman_, blk, block_idx);
                   
                   // Optimization branch. Statistically two bit blocks
                   // are bumping into each other quite frequently and
                   // this peace of code tend to be executed often and 
                   // program does not go into nested calls.
                   // But logically this branch can be eliminated without
                   // loosing any functionality.
/*                   
                   if (!gap && !arg_gap)
                   {
                       if (IS_VALID_ADDR(blk) && arg_blk)
                       {
                       
                            if (bit_func2 && (j < bm::set_array_size-1))
                            {
                                bm::word_t* blk2 = blk_blk[j+1];
                                const bm::word_t* arg_blk2 = bvect.get_block(i, j+1);
                                
                                bool arg_gap2 = BM_IS_GAP(bvect, arg_blk2, block_idx + 1);
                                bool gap2 = BM_IS_GAP((*this), blk2, block_idx + 1);
                               
                               if (!gap2 && !arg_gap2 && blk2 && arg_blk2)
                               {
                                    bit_func2(blk, arg_blk, blk2, arg_blk2);
                                    ++j;
                                    ++block_idx;
                                    continue;
                               }
                                
                            }
                            
                            bit_func(blk, arg_blk);
                            continue;
                       }
                   } // end of optimization branch...
*/                   
                   combine_operation_with_block(block_idx, gap, blk, 
                                                arg_blk, arg_gap,
                                                opcode);
                }

            } // for j

        } // for i

    }

    void combine_operation_with_block(unsigned nb,
                                      unsigned gap,
                                      bm::word_t* blk,
                                      const bm::word_t* arg_blk,
                                      int arg_gap,
                                      bm::operation opcode)
    {
         if (gap) // our block GAP-type
         {
             if (arg_gap)  // both blocks GAP-type
             {
                 gap_word_t tmp_buf[bm::gap_max_buff_len * 3]; // temporary result
             
                 gap_word_t* res;
                 switch (opcode)
                 {
                 case BM_AND:
                     res = gap_operation_and(BMGAP_PTR(blk), 
                                             BMGAP_PTR(arg_blk), 
                                             tmp_buf);
                     break;
                 case BM_OR:
                     res = gap_operation_or(BMGAP_PTR(blk), 
                                            BMGAP_PTR(arg_blk), 
                                            tmp_buf);
                     break;
                 case BM_SUB:
                     res = gap_operation_sub(BMGAP_PTR(blk), 
                                             BMGAP_PTR(arg_blk), 
                                             tmp_buf);
                     break;
                 case BM_XOR:
                     res = gap_operation_xor(BMGAP_PTR(blk), 
                                             BMGAP_PTR(arg_blk), 
                                             tmp_buf);
                     break;
                 default:
                     assert(0);
                     res = 0;
                 }

                 assert(res == tmp_buf);
                 unsigned res_len = bm::gap_length(res);

                 assert(!(res == tmp_buf && res_len == 0));

                 // if as a result of the operation gap block turned to zero
                 // we can now replace it with NULL
                 if (gap_is_all_zero(res, bm::gap_max_bits))
                 {
                     blockman_.set_block(nb, 0);
                     blockman_.set_block_bit(nb);
                     blockman_.get_allocator().free_gap_block(BMGAP_PTR(blk), 
                                                             blockman_.glen());
                     return;
                 }

                 // mutation check

                 int level = gap_level(BMGAP_PTR(blk));
                 unsigned threshold = blockman_.glen(level)-4;
                 int new_level = gap_calc_level(res_len, blockman_.glen());

                 if (new_level == -1)
                 {
                     blockman_.convert_gap2bitset(nb, res);
                     return;
                 }

                 if (res_len > threshold)
                 {
                     set_gap_level(res, new_level);
                     gap_word_t* new_blk = 
                         blockman_.allocate_gap_block(new_level, res);

                     bm::word_t* p = (bm::word_t*)new_blk;
                     BMSET_PTRGAP(p);

                     blockman_.set_block_ptr(nb, p);
                     blockman_.get_allocator().free_gap_block(BMGAP_PTR(blk), 
                                                             blockman_.glen());
                     return;
                 }

                 // gap opeartion result is in the temporary buffer
                 // we copy it back to the gap_block

                 set_gap_level(tmp_buf, level);
                 ::memcpy(BMGAP_PTR(blk), tmp_buf, res_len * sizeof(gap_word_t));

                 return;
             }
             else // argument is BITSET-type (own block is GAP)
             {
                 // since we can not combine blocks of mixed type
                 // we need to convert our block to bitset
                 
                 if (arg_blk == 0)  // Combining against an empty block
                 {
                    if (opcode == BM_OR  || 
                        opcode == BM_SUB || 
                        opcode == BM_XOR)
                    {
                        return; // nothing to do
                    }
                        
                    if (opcode == BM_AND) // ("Value" AND  0) == 0
                    {
                        blockman_.set_block_ptr(nb, 0);
                        blockman_.set_block_bit(nb);
                        blockman_.get_allocator().
                                       free_gap_block(BMGAP_PTR(blk),
                                                    blockman_.glen());
                        return;
                    }
                 }

                 blk = blockman_.convert_gap2bitset(nb, BMGAP_PTR(blk));
             }
         } 
         else // our block is BITSET-type
         {
             if (arg_gap) // argument block is GAP-type
             {
                if (IS_VALID_ADDR(blk))
                {
                    // special case, maybe we can do the job without 
                    // converting the GAP argument to bitblock
                    switch (opcode)
                    {
                    case BM_OR:
                         gap_add_to_bitset(blk, BMGAP_PTR(arg_blk));
                         return;                         
                    case BM_SUB:
                         gap_sub_to_bitset(blk, BMGAP_PTR(arg_blk));
                         return;
                    case BM_XOR:
                         gap_xor_to_bitset(blk, BMGAP_PTR(arg_blk));
                         return;
                    case BM_AND:
                         gap_and_to_bitset(blk, BMGAP_PTR(arg_blk));
                         return;
                         
                    } // switch
                 }
                 
                 // the worst case we need to convert argument block to 
                 // bitset type.

                 gap_word_t* temp_blk = (gap_word_t*) blockman_.check_allocate_tempblock();
                 arg_blk = 
                     gap_convert_to_bitset_smart((bm::word_t*)temp_blk, 
                                                 BMGAP_PTR(arg_blk), 
                                                 bm::gap_max_bits);
             
             }   
         }
     
         // Now here we combine two plain bitblocks using supplied bit function.
         bm::word_t* dst = blk;

         bm::word_t* ret; 
         if (dst == 0 && arg_blk == 0)
         {
             return;
         }

         switch (opcode)
         {
         case BM_AND:
             ret = bit_operation_and(dst, arg_blk);
             goto copy_block;
         case BM_XOR:
             ret = bit_operation_xor(dst, arg_blk);
             if (ret && (ret == arg_blk) && IS_FULL_BLOCK(dst))
             {
                 ret = blockman_.get_allocator().alloc_bit_block();
#ifdef BMVECTOPT
                VECT_XOR_ARR_2_MASK(ret, 
                                    arg_blk, 
                                    arg_blk + bm::set_block_size, 
                                    bm::all_bits_mask);
#else
                 bm::wordop_t* dst_ptr = (wordop_t*)ret;
                 const bm::wordop_t* wrd_ptr = (wordop_t*) arg_blk;
                 const bm::wordop_t* wrd_end = 
                    (wordop_t*) (arg_blk + bm::set_block_size);

                 do
                 {
                     dst_ptr[0] = bm::all_bits_mask ^ wrd_ptr[0];
                     dst_ptr[1] = bm::all_bits_mask ^ wrd_ptr[1];
                     dst_ptr[2] = bm::all_bits_mask ^ wrd_ptr[2];
                     dst_ptr[3] = bm::all_bits_mask ^ wrd_ptr[3];

                     dst_ptr+=4;
                     wrd_ptr+=4;

                 } while (wrd_ptr < wrd_end);
#endif
                 break;
             }
             goto copy_block;
         case BM_OR:
             ret = bit_operation_or(dst, arg_blk);
         copy_block:
             if (ret && (ret == arg_blk) && !IS_FULL_BLOCK(ret))
             {
                ret = blockman_.get_allocator().alloc_bit_block();
                bit_block_copy(ret, arg_blk);
             }
             break;

         case BM_SUB:
             ret = bit_operation_sub(dst, arg_blk);
             if (ret && ret == arg_blk)
             {
                 ret = blockman_.get_allocator().alloc_bit_block();
#ifdef BMVECTOPT
                 VECT_ANDNOT_ARR_2_MASK(ret, 
                                        arg_blk,
                                        arg_blk + bm::set_block_size,
                                        bm::all_bits_mask);
#else

                 bm::wordop_t* dst_ptr = (wordop_t*)ret;
                 const bm::wordop_t* wrd_ptr = (wordop_t*) arg_blk;
                 const bm::wordop_t* wrd_end = 
                    (wordop_t*) (arg_blk + bm::set_block_size);

                 do
                 {
                     dst_ptr[0] = bm::all_bits_mask & ~wrd_ptr[0];
                     dst_ptr[1] = bm::all_bits_mask & ~wrd_ptr[1];
                     dst_ptr[2] = bm::all_bits_mask & ~wrd_ptr[2];
                     dst_ptr[3] = bm::all_bits_mask & ~wrd_ptr[3];

                     dst_ptr+=4;
                     wrd_ptr+=4;

                 } while (wrd_ptr < wrd_end);
#endif
             }
             break;
         default:
             assert(0);
             ret = 0;
         }

         if (ret != dst) // block mutation
         {
             blockman_.set_block(nb, ret);
             blockman_.get_allocator().free_bit_block(dst);
         }
    }

public:
    void combine_operation_with_block(unsigned nb,
                                      const bm::word_t* arg_blk,
                                      int arg_gap,
                                      bm::operation opcode)
    {
        bm::word_t* blk = const_cast<bm::word_t*>(get_block(nb));
        bool gap = BM_IS_GAP((*this), blk, nb);
        combine_operation_with_block(nb, gap, blk, arg_blk, arg_gap, opcode);
    }
private:
    void combine_count_operation_with_block(unsigned nb,
                                            const bm::word_t* arg_blk,
                                            int arg_gap,
                                            bm::operation opcode)
    {
        const bm::word_t* blk = get_block(nb);
        bool gap = BM_IS_GAP((*this), blk, nb);
        combine_count_operation_with_block(nb, gap, blk, arg_blk, arg_gap, opcode);
    }


    /**
       \brief Extends GAP block to the next level or converts it to bit block.
       \param nb - Block's linear index.
       \param blk - Blocks's pointer 
    */
    void extend_gap_block(unsigned nb, gap_word_t* blk)
    {
        blockman_.extend_gap_block(nb, blk);
    }


public:

    /** 
        Embedded class managing bit-blocks on very low level.
        Includes number of functor classes used in different bitset algorithms. 
    */
    class blocks_manager
    {
    friend class enumerator;
    friend class block_free_func;

    public:
    
        typedef Alloc allocator_type;
    
    public:

        /** Base functor class */
        class bm_func_base
        {
        public:
            bm_func_base(blocks_manager& bman) : bm_(bman) {}

        protected:
            blocks_manager&  bm_;
        };

        /** Base functor class connected for "constant" functors*/
        class bm_func_base_const
        {
        public:
            bm_func_base_const(const blocks_manager& bman) : bm_(bman) {}

        protected:
            const blocks_manager&  bm_;
        };

        /** Base class for bitcounting functors */
        class block_count_base : public bm_func_base_const
        {
        protected:
            block_count_base(const blocks_manager& bm) 
                : bm_func_base_const(bm) {}

            bm::id_t block_count(const bm::word_t* block, unsigned idx) const
            {
                id_t count = 0;
                if (IS_FULL_BLOCK(block))
                {
                    count = bm::bits_in_block;
                }
                else
                {
                    if (BM_IS_GAP(this->bm_, block, idx))
                    {
                        count = gap_bit_count(BMGAP_PTR(block));
                    }
                    else // bitset
                    {
                        count = 
                            bit_block_calc_count(block, 
                                                 block + bm::set_block_size);
                    }
                }
                return count;
            }
        };


        /** Bitcounting functor */
        class block_count_func : public block_count_base
        {
        public:
            block_count_func(const blocks_manager& bm) 
                : block_count_base(bm), count_(0) {}

            bm::id_t count() const { return count_; }

            void operator()(const bm::word_t* block, unsigned idx)
            {
                count_ += this->block_count(block, idx);
            }

        private:
            bm::id_t count_;
        };

        /** Bitcounting functor filling the block counts array*/
        class block_count_arr_func : public block_count_base
        {
        public:
            block_count_arr_func(const blocks_manager& bm, unsigned* arr) 
                : block_count_base(bm), arr_(arr), last_idx_(0) 
            {
                arr_[0] = 0;
            }

            void operator()(const bm::word_t* block, unsigned idx)
            {
                while (++last_idx_ < idx)
                {
                    arr_[last_idx_] = 0;
                }
                arr_[idx] = this->block_count(block, idx);
                last_idx_ = idx;
            }

            unsigned last_block() const { return last_idx_; }

        private:
            unsigned*  arr_;
            unsigned   last_idx_;
        };

        /** bit value change counting functor */
        class block_count_change_func : public bm_func_base_const
        {
        public:
            block_count_change_func(const blocks_manager& bm) 
                : bm_func_base_const(bm),
                  count_(0),
                  prev_block_border_bit_(0)
            {}

            bm::id_t block_count(const bm::word_t* block, unsigned idx)
            {
                bm::id_t count = 0;
                bm::id_t first_bit;
                
                if (IS_FULL_BLOCK(block) || (block == 0))
                {
                    count = 1;
                    if (idx)
                    {
                        first_bit = block ? 1 : 0;
                        count -= !(prev_block_border_bit_ ^ first_bit);
                    }
                    prev_block_border_bit_ = block ? 1 : 0;
                }
                else
                {
                    if (BM_IS_GAP(this->bm_, block, idx))
                    {
                        gap_word_t* gap_block = BMGAP_PTR(block);
                        count = gap_length(gap_block) - 1;
                        if (idx)
                        {
                            first_bit = gap_test(gap_block, 0);
                            count -= !(prev_block_border_bit_ ^ first_bit);
                        }
                            
                        prev_block_border_bit_ = 
                           gap_test(gap_block, gap_max_bits-1);
                    }
                    else // bitset
                    {
                        count = bit_block_calc_count_change(block,
                                                  block + bm::set_block_size);
                        if (idx)
                        {
                            first_bit = block[0] & 1;
                            count -= !(prev_block_border_bit_ ^ first_bit);
                        }
                        prev_block_border_bit_ = 
                            block[set_block_size-1] >> ((sizeof(block[0]) * 8) - 1);
                        
                    }
                }
                return count;
            }
            
            bm::id_t count() const { return count_; }

            void operator()(const bm::word_t* block, unsigned idx)
            {
                count_ += block_count(block, idx);
            }

        private:
            bm::id_t   count_;
            bm::id_t   prev_block_border_bit_;
        };


        /** Functor detects if any bit set*/
        class block_any_func : public bm_func_base_const
        {
        public:
            block_any_func(const blocks_manager& bm) 
                : bm_func_base_const(bm) 
            {}

            bool operator()(const bm::word_t* block, unsigned idx)
            {
                if (IS_FULL_BLOCK(block)) return true;

                if (BM_IS_GAP(this->bm_, block, idx)) // gap block
                {
                    if (!gap_is_all_zero(BMGAP_PTR(block), bm::gap_max_bits))
                    {
                        return true;
                    }
                }
                else  // bitset
                {
                    bm::wordop_t* blk1 = (wordop_t*)block;
                    bm::wordop_t* blk2 = 
                        (wordop_t*)(block + bm::set_block_size);
                    if (!bit_is_all_zero(blk1, blk2))
                    {
                        return true;
                    }
                }
                return false;
            }
        };

        /*! Change GAP level lengths functor */
        class gap_level_func : public bm_func_base
        {
        public:
            gap_level_func(blocks_manager& bm, const gap_word_t* glevel_len)
                : bm_func_base(bm),
                  glevel_len_(glevel_len)
            {
                BM_ASSERT(glevel_len);
            }

            void operator()(bm::word_t* block, unsigned idx)
            {
                blocks_manager& bman = this->bm_;
                
                if (!BM_IS_GAP(bman, block, idx))
                    return;

                gap_word_t* gap_blk = BMGAP_PTR(block);

                // TODO: Use the same code as in the optimize functor
                if (gap_is_all_zero(gap_blk, bm::gap_max_bits))
                {
                    bman.set_block_ptr(idx, 0);
                    goto free_block;
                }
                else 
                if (gap_is_all_one(gap_blk, bm::gap_max_bits))
                {
                    bman.set_block_ptr(idx, FULL_BLOCK_ADDR);
                free_block:
                    bman.get_allocator().free_gap_block(gap_blk, 
                                                        bman.glen());
                    bman.set_block_bit(idx);
                    return;
                }

                unsigned len = gap_length(gap_blk);
                int level = gap_calc_level(len, glevel_len_);
                if (level == -1)
                {
                    bm::word_t* blk = 
                        bman.get_allocator().alloc_bit_block();
                    bman.set_block_ptr(idx, blk);
                    bman.set_block_bit(idx);
                    gap_convert_to_bitset(blk, gap_blk);
                }
                else
                {
                    gap_word_t* gap_blk_new = 
                        bman.allocate_gap_block(level, gap_blk, glevel_len_);

                    bm::word_t* p = (bm::word_t*) gap_blk_new;
                    BMSET_PTRGAP(p);
                    bman.set_block_ptr(idx, p);
                }
                bman.get_allocator().free_gap_block(gap_blk, bman.glen());
            }

        private:
            const gap_word_t* glevel_len_;
        };


        /*! Bitblock optimization functor */
        class block_opt_func : public bm_func_base
        {
        public:
            block_opt_func(blocks_manager& bm, 
                           bm::word_t*     temp_block,
                           int             opt_mode) 
                : bm_func_base(bm),
                  temp_block_(temp_block),
                  opt_mode_(opt_mode)
            {
                BM_ASSERT(temp_block);
            }

            void operator()(bm::word_t* block, unsigned idx)
            {
                blocks_manager& bman = this->bm_;
                if (IS_FULL_BLOCK(block)) return;

                gap_word_t* gap_blk;

                if (BM_IS_GAP(bman, block, idx)) // gap block
                {
                    gap_blk = BMGAP_PTR(block);

                    if (gap_is_all_zero(gap_blk, bm::gap_max_bits))
                    {
                        bman.set_block_ptr(idx, 0);
                        goto free_block;
                    }
                    else 
                    if (gap_is_all_one(gap_blk, bm::gap_max_bits))
                    {
                        bman.set_block_ptr(idx, FULL_BLOCK_ADDR);
                    free_block:
                        bman.get_allocator().free_gap_block(gap_blk, 
                                                            bman.glen());
                        bman.set_block_bit(idx);
                    }
                }
                else // bit block
                {
                    if (opt_mode_ < 3) // free_01 optimization
                    {  
                        bm::wordop_t* blk1 = (wordop_t*)block;
                        bm::wordop_t* blk2 = 
                            (wordop_t*)(block + bm::set_block_size);
                    
                        bool b = bit_is_all_zero(blk1, blk2);
                        if (b)
                        {
                            bman.get_allocator().free_bit_block(block);
                            bman.set_block_ptr(idx, 0);
                            return;
                        }
                        if (opt_mode_ == 2) // check if it is all 1 block
                        {
                            b = is_bits_one(blk1, blk2);
                            if (b) 
                            {
                                bman.get_allocator().free_bit_block(block);
                                bman.set_block_ptr(idx, FULL_BLOCK_ADDR);
                                return;
                            }
                        }
                    }
                
                    // try to compress
                
                    gap_word_t* tmp_gap_blk = (gap_word_t*)temp_block_;
                    *tmp_gap_blk = bm::gap_max_level << 1;

                    unsigned threashold = bman.glen(bm::gap_max_level)-4;

                    unsigned len = bit_convert_to_gap(tmp_gap_blk, 
                                                      block, 
                                                      bm::gap_max_bits, 
                                                      threashold);


                    if (!len) return;
                    
                    // convertion successful
                    
                    bman.get_allocator().free_bit_block(block);

                    // check if new gap block can be eliminated

                    if (gap_is_all_zero(tmp_gap_blk, bm::gap_max_bits))
                    {
                        bman.set_block_ptr(idx, 0);
                    }
                    else if (gap_is_all_one(tmp_gap_blk, bm::gap_max_bits))
                    {
                        bman.set_block_ptr(idx, FULL_BLOCK_ADDR);
                    }
                    else
                    {
                        int level = bm::gap_calc_level(len, bman.glen());

                        gap_blk = 
                           bman.allocate_gap_block(level, tmp_gap_blk);
                        bman.set_block_ptr(idx, (bm::word_t*)gap_blk);
                        bman.set_block_gap(idx);
                    }
                    
         
                }

            }
        private:
            bm::word_t*   temp_block_;
            int           opt_mode_;
        };

        /** Bitblock invert functor */
        class block_invert_func : public bm_func_base
        {
        public:
            block_invert_func(blocks_manager& bm) 
                : bm_func_base(bm) {}

            void operator()(bm::word_t* block, unsigned idx)
            {
                if (!block)
                {
                    this->bm_.set_block(idx, FULL_BLOCK_ADDR);
                }
                else
                if (IS_FULL_BLOCK(block))
                {
                    this->bm_.set_block_ptr(idx, 0);
                }
                else
                {
                    if (BM_IS_GAP(this->bm_, block, idx)) // gap block
                    {
                        gap_invert(BMGAP_PTR(block));
                    }
                    else  // bit block
                    {
                        bm::wordop_t* wrd_ptr = (wordop_t*) block;
                        bm::wordop_t* wrd_end = 
                                (wordop_t*) (block + bm::set_block_size);
                        bit_invert(wrd_ptr, wrd_end);
                    }
                }

            }
        };



    private:


        /** Set block zero functor */
        class block_zero_func : public bm_func_base
        {
        public:
            block_zero_func(blocks_manager& bm, bool free_mem) 
            : bm_func_base(bm),
              free_mem_(free_mem)
            {}

            void operator()(bm::word_t* block, unsigned idx)
            {
                blocks_manager& bman = this->bm_;
                if (IS_FULL_BLOCK(block))
                {
                    bman.set_block_ptr(idx, 0);
                }
                else
                {
                    if (BM_IS_GAP(bman, block, idx))
                    {
                        gap_set_all(BMGAP_PTR(block), bm::gap_max_bits, 0);
                    }
                    else  // BIT block
                    {
                        if (free_mem_)
                        {
                            bman.get_allocator().free_bit_block(block);
                            bman.set_block_ptr(idx, 0);
                        }
                        else
                        {
                            bit_block_set(block, 0);
                        }
                    }
                }
            }
        private:
            bool free_mem_; //!< If "true" frees bitblocks memsets to '0'
        };

        /** Fill block with all-one bits functor */
        class block_one_func : public bm_func_base
        {
        public:
            block_one_func(blocks_manager& bm) : bm_func_base(bm) {}

            void operator()(bm::word_t* block, unsigned idx)
            {
                if (!IS_FULL_BLOCK(block))
                {
                    this->bm_.set_block_all_set(idx);
                }
            }
        };


        /** Block deallocation functor */
        class block_free_func : public bm_func_base
        {
        public:
            block_free_func(blocks_manager& bm) : bm_func_base(bm) {}

            void operator()(bm::word_t* block, unsigned idx)
            {
                blocks_manager& bman = this->bm_;
                if (BM_IS_GAP(bman, block, idx)) // gap block
                {
                    bman.get_allocator().free_gap_block(BMGAP_PTR(block),
                                                        bman.glen());
                }
                else
                {
                    bman.get_allocator().free_bit_block(block);
                }
            }
        };


        /** Block copy functor */
        class block_copy_func : public bm_func_base
        {
        public:
            block_copy_func(blocks_manager&        bm_target, 
                            const blocks_manager&  bm_src) 
                : bm_func_base(bm_target), 
                  bm_src_(bm_src) 
            {}

            void operator()(bm::word_t* block, unsigned idx)
            {
                bool gap = bm_src_.is_block_gap(idx);
                bm::word_t* new_blk;
                
                blocks_manager& bman = this->bm_;

                if (gap)
                {
                    bm::gap_word_t* gap_block = BMGAP_PTR(block); 
                    unsigned level = gap_level(gap_block);
                    new_blk = (bm::word_t*)
                       bman.get_allocator().alloc_gap_block(level, 
                                                            bman.glen());
                    int len = gap_length(BMGAP_PTR(block));
                    ::memcpy(new_blk, gap_block, len * sizeof(gap_word_t));
                    BMSET_PTRGAP(new_blk);
                }
                else
                {
                    if (IS_FULL_BLOCK(block))
                    {
                        new_blk = block;
                    }
                    else
                    {
                        new_blk = bman.get_allocator().alloc_bit_block();
                        bit_block_copy(new_blk, block);
                    }
                }
                bman.set_block(idx, new_blk);
            }

        private:
            const blocks_manager&  bm_src_;
        };

    public:

        blocks_manager(const gap_word_t* glevel_len, 
                       bm::id_t          max_bits,
                       const Alloc&      alloc = Alloc())
            : blocks_(0),
              temp_block_(0),
              alloc_(alloc)
        {
            ::memcpy(glevel_len_, glevel_len, sizeof(glevel_len_));
            if (!max_bits)  // working in full-range mode
            {
                top_block_size_ = bm::set_array_size;
            }
            else  // limiting the working range
            {
                top_block_size_ = 
                    max_bits / (bm::set_block_size * sizeof(bm::word_t) * 
                                                       bm::set_array_size * 8);
                if (top_block_size_ < bm::set_array_size) ++top_block_size_;
            }

            // allocate first level descr. of blocks 
            blocks_ = (bm::word_t***) alloc_.alloc_ptr(top_block_size_); 
            ::memset(blocks_, 0, top_block_size_ * sizeof(bm::word_t**));
            volatile const char* vp = _copyright<true>::_p;
            char c = *vp;
            c = 0;
        }

        blocks_manager(const blocks_manager& blockman)
            : blocks_(0),
              top_block_size_(blockman.top_block_size_),
         #ifdef BM_DISBALE_BIT_IN_PTR
              gap_flags_(blockman.gap_flags_),
         #endif
              temp_block_(0),
              alloc_(blockman.alloc_)
        {
            ::memcpy(glevel_len_, blockman.glevel_len_, sizeof(glevel_len_));

            blocks_ = (bm::word_t***)alloc_.alloc_ptr(top_block_size_);
            ::memset(blocks_, 0, top_block_size_ * sizeof(bm::word_t**));

            blocks_manager* bm = 
               const_cast<blocks_manager*>(&blockman);

            word_t*** blk_root = bm->blocks_root();

            block_copy_func copy_func(*this, blockman);
            for_each_nzblock(blk_root, top_block_size_, 
                                       bm::set_array_size, copy_func);
        }

        
        void free_ptr(bm::word_t** ptr)
        {
            if (ptr) alloc_.free_ptr(ptr);
        }

        ~blocks_manager()
        {
            alloc_.free_bit_block(temp_block_);
            deinit_tree();
        }

        /**
           \brief Finds block in 2-level blocks array  
           \param nb - Index of block (logical linear number)
           \return block adress or NULL if not yet allocated
        */
        bm::word_t* get_block(unsigned nb) const
        {
            unsigned block_idx = nb >> bm::set_array_shift;
            if (block_idx >= top_block_size_) return 0;
            bm::word_t** blk_blk = blocks_[block_idx];
            if (blk_blk)
            {
               return blk_blk[nb & bm::set_array_mask]; // equivalent of %
            }
            return 0; // not allocated
        }

        /**
           \brief Finds block in 2-level blocks array
           \param i - top level block index
           \param j - second level block index
           \return block adress or NULL if not yet allocated
        */
        const bm::word_t* get_block(unsigned i, unsigned j) const
        {
            if (i >= top_block_size_) return 0;
            const bm::word_t* const* blk_blk = blocks_[i];
            return (blk_blk == 0) ? 0 : blk_blk[j];
        }

        /**
           \brief Function returns top-level block in 2-level blocks array
           \param i - top level block index
           \return block adress or NULL if not yet allocated
        */
        const bm::word_t* const * get_topblock(unsigned i) const
        {
            if (i >= top_block_size_) return 0;
            return blocks_[i];
        }
        
        /** 
            \brief Returns root block in the tree.
        */
        bm::word_t*** get_rootblock() const
        {
            blocks_manager* bm = 
               const_cast<blocks_manager*>(this);

            return bm->blocks_root();
        }

        void set_block_all_set(unsigned nb)
        {
            bm::word_t* block = this->get_block(nb);
            set_block(nb, const_cast<bm::word_t*>(FULL_BLOCK_ADDR));

          // If we keep block type flag in pointer itself we dp not need 
          // to clear gap bit 
          #ifdef BM_DISBALE_BIT_IN_PTR
            set_block_bit(nb);    
          #endif

            if (BM_IS_GAP((*this), block, nb))
            {
                alloc_.free_gap_block(BMGAP_PTR(block), glevel_len_);
            }
            else
            {
                alloc_.free_bit_block(block);
            }

        }


        /** 
            Function checks if block is not yet allocated, allocates it and sets to
            all-zero or all-one bits. 
    
            If content_flag == 1 (ALLSET block) requested and taken block is already ALLSET,
            function will return NULL

            initial_block_type and actual_block_type : 0 - bitset, 1 - gap
        */
        bm::word_t* check_allocate_block(unsigned nb, 
                                         unsigned content_flag,
                                         int      initial_block_type,
                                         int*     actual_block_type,
                                         bool     allow_null_ret=true)
        {
            bm::word_t* block = this->get_block(nb);

            if (!IS_VALID_ADDR(block)) // NULL block or ALLSET
            {
                // if we wanted ALLSET and requested block is ALLSET return NULL
                unsigned block_flag = IS_FULL_BLOCK(block);
                *actual_block_type = initial_block_type;
                if (block_flag == content_flag && allow_null_ret)
                {
                    return 0; // it means nothing to do for the caller
                }

                if (initial_block_type == 0) // bitset requested
                {
                    block = alloc_.alloc_bit_block();

                    // initialize block depending on its previous status

                    bit_block_set(block, block_flag ? 0xFF : 0);

                    set_block(nb, block);
                }
                else // gap block requested
                {
                    bm::gap_word_t* gap_block = allocate_gap_block(0);
                    gap_set_all(gap_block, bm::gap_max_bits, block_flag);
                    set_block(nb, (bm::word_t*)gap_block);

                    set_block_gap(nb);

                    return (bm::word_t*)gap_block;
                }

            }
            else // block already exists
            {
                *actual_block_type = BM_IS_GAP((*this), block, nb);
            }

            return block;
        }

        /*! @brief Fills all blocks with 0.
            @param free_mem - if true function frees the resources
        */
        void set_all_zero(bool free_mem)
        {
            block_zero_func zero_func(*this, free_mem);
            for_each_nzblock(blocks_, top_block_size_,
                                      bm::set_array_size, zero_func);
        }

        /*! Replaces all blocks with ALL_ONE block.
        */
        void set_all_one()
        {
            block_one_func func(*this);
            for_each_block(blocks_, top_block_size_, 
                                    bm::set_array_size, func);
        }

        /**
            Places new block into descriptors table, returns old block's address.
            Old block is not deleted.
        */
        bm::word_t* set_block(unsigned nb, bm::word_t* block)
        {
            bm::word_t* old_block;

            register unsigned nblk_blk = nb >> bm::set_array_shift;

            // If first level array not yet allocated, allocate it and
            // assign block to it
            if (blocks_[nblk_blk] == 0) 
            {
                blocks_[nblk_blk] = (bm::word_t**)alloc_.alloc_ptr();
                ::memset(blocks_[nblk_blk], 0, 
                    bm::set_array_size * sizeof(bm::word_t*));

                old_block = 0;
            }
            else
            {
                old_block = blocks_[nblk_blk][nb & bm::set_array_mask];
            }

            // NOTE: block will be replaced without freeing,
            // potential memory leak may lay here....
            blocks_[nblk_blk][nb & bm::set_array_mask] = block; // equivalent to %

            return old_block;

        }
        
        
        /**
            Places new block into blocks table.
        */
        void set_block_ptr(unsigned nb, bm::word_t* block)
        {
            blocks_[nb >> bm::set_array_shift][nb & bm::set_array_mask] = block;
        }
        

        /** 
           \brief Converts block from type gap to conventional bitset block.
           \param nb - Block's index. 
           \param gap_block - Pointer to the gap block, 
                              if NULL block nb will be taken
           \return new gap block's memory
        */
        bm::word_t* convert_gap2bitset(unsigned nb, gap_word_t* gap_block=0)
        {
            bm::word_t* block = this->get_block(nb);
            if (gap_block == 0)
            {
                gap_block = BMGAP_PTR(block);
            }

            BM_ASSERT(IS_VALID_ADDR((bm::word_t*)gap_block));
            BM_ASSERT(is_block_gap(nb)); // must be GAP type

            bm::word_t* new_block = alloc_.alloc_bit_block();

            gap_convert_to_bitset(new_block, gap_block);

            // new block will replace the old one(no deletion)
            set_block_ptr(nb, new_block); 

            alloc_.free_gap_block(BMGAP_PTR(block), glen());

          // If GAP flag is in block pointer no need to clean the gap bit twice
          #ifdef BM_DISBALE_BIT_IN_PTR
            set_block_bit(nb);
          #endif

            return new_block;
        }


        /**
           \brief Extends GAP block to the next level or converts it to bit block.
           \param nb - Block's linear index.
           \param blk - Blocks's pointer 
        */
        void extend_gap_block(unsigned nb, gap_word_t* blk)
        {
            unsigned level = bm::gap_level(blk);
            unsigned len = bm::gap_length(blk);
            if (level == bm::gap_max_level || len >= gap_max_buff_len)
            {
                convert_gap2bitset(nb);
            }
            else
            {
                bm::word_t* new_blk = (bm::word_t*)allocate_gap_block(++level, blk);

                BMSET_PTRGAP(new_blk);

                set_block_ptr(nb, new_blk);
                alloc_.free_gap_block(blk, glen());
            }
        }

        bool is_block_gap(unsigned nb) const 
        {
         #ifdef BM_DISBALE_BIT_IN_PTR
            return gap_flags_.test(nb)!=0;
         #else
            bm::word_t* block = get_block(nb);
            return BMPTR_TESTBIT0(block) != 0;
         #endif
        }

        void set_block_bit(unsigned nb) 
        { 
         #ifdef BM_DISBALE_BIT_IN_PTR
            gap_flags_.set(nb, false);
         #else
            bm::word_t* block = get_block(nb);
            block = (bm::word_t*) BMPTR_CLEARBIT0(block);
            set_block_ptr(nb, block);
         #endif
        }

        void set_block_gap(unsigned nb) 
        {
         #ifdef BM_DISBALE_BIT_IN_PTR
            gap_flags_.set(nb);
         #else
            bm::word_t* block = get_block(nb);
            block = (bm::word_t*)BMPTR_SETBIT0(block);
            set_block_ptr(nb, block);
         #endif
        }

        /**
           \fn bool bm::bvector::blocks_manager::is_block_zero(unsigned nb, bm::word_t* blk)
           \brief Checks all conditions and returns true if block consists of only 0 bits
           \param nb - Block's linear index.
           \param blk - Blocks's pointer 
           \returns true if all bits are in the block are 0.
        */
        bool is_block_zero(unsigned nb, const bm::word_t* blk) const
        {
           if (blk == 0) return true;

           if (BM_IS_GAP((*this), blk, nb)) // GAP
           {
               gap_word_t* b = BMGAP_PTR(blk);
               return gap_is_all_zero(b, bm::gap_max_bits);
           }
   
           // BIT
           for (unsigned i = 0; i <  bm::set_block_size; ++i)
           {
               if (blk[i] != 0)
                  return false;
           }
           return true;
        }


        /**
           \brief Checks if block has only 1 bits
           \param nb - Index of the block.
           \param blk - Block's pointer
           \return true if block consists of 1 bits.
        */
        bool is_block_one(unsigned nb, const bm::word_t* blk) const
        {
           if (blk == 0) return false;

           if (BM_IS_GAP((*this), blk, nb)) // GAP
           {
               gap_word_t* b = BMGAP_PTR(blk);
               return gap_is_all_one(b, bm::gap_max_bits);
           }
   
           // BIT block

           if (IS_FULL_BLOCK(blk))
           {
              return true;
           }
           return is_bits_one((wordop_t*)blk, 
                              (wordop_t*)(blk + bm::set_block_size));
        }

        /*! Returns temporary block, allocates if needed. */
        bm::word_t* check_allocate_tempblock()
        {
            return temp_block_ ? temp_block_ 
                               : (temp_block_ = alloc_.alloc_bit_block());
        }

        /*! Assigns new GAP lengths vector */
        void set_glen(const gap_word_t* glevel_len)
        {
            ::memcpy(glevel_len_, glevel_len, sizeof(glevel_len_));
        }


        bm::gap_word_t* allocate_gap_block(unsigned level, 
                                           gap_word_t* src = 0,
                                           const gap_word_t* glevel_len = 0)
        {
           if (!glevel_len)
               glevel_len = glevel_len_;
           gap_word_t* ptr = alloc_.alloc_gap_block(level, glevel_len);
           if (src)
           {
                unsigned len = gap_length(src);
                ::memcpy(ptr, src, len * sizeof(gap_word_t));
                // Reconstruct the mask word using the new level code.
                *ptr = ((len-1) << 3) | (level << 1) | (*src & 1);
           }
           else
           {
               *ptr = level << 1;
           }
           return ptr;
        }


        unsigned mem_used() const
        {
           unsigned mem_used = sizeof(*this);
           mem_used += temp_block_ ? sizeof(word_t) * bm::set_block_size : 0;
           mem_used += sizeof(bm::word_t**) * top_block_size_;

         #ifdef BM_DISBALE_BIT_IN_PTR
           mem_used += gap_flags_.mem_used() - sizeof(gap_flags_);
         #endif

           for (unsigned i = 0; i < top_block_size_; ++i)
           {
              mem_used += blocks_[i] ? sizeof(void*) * bm::set_array_size : 0;
           }

           return mem_used;
        }

        /** Returns true if second level block pointer is 0.
        */
        bool is_subblock_null(unsigned nsub) const
        {
           return blocks_[nsub] == NULL;
        }


        bm::word_t***  blocks_root()
        {
            return blocks_;
        }

        /*! \brief Returns current GAP level vector
        */
        const gap_word_t* glen() const
        {
            return glevel_len_;
        }

        /*! \brief Returns GAP level length for specified level
            \param level - level number
        */
        unsigned glen(unsigned level) const
        {
            return glevel_len_[level];
        }

        /*! \brief Swaps content 
            \param bm  another blocks manager
        */
        void swap(blocks_manager& bm)
        {
            word_t*** btmp = blocks_;
            blocks_ = bm.blocks_;
            bm.blocks_ = btmp;
         #ifdef BM_DISBALE_BIT_IN_PTR
            gap_flags_.swap(bm.gap_flags_);
         #endif
            gap_word_t gltmp[bm::gap_levels];
            
            ::memcpy(gltmp, glevel_len_, sizeof(glevel_len_));
            ::memcpy(glevel_len_, bm.glevel_len_, sizeof(glevel_len_));
            ::memcpy(bm.glevel_len_, gltmp, sizeof(glevel_len_));
        }

        /*! \brief Returns size of the top block array in the tree 
        */
        unsigned top_block_size() const
        {
            return top_block_size_;
        }

        /*! \brief Sets ne top level block size.
        */
        void set_top_block_size(unsigned value)
        {
            assert(value);
            if (value == top_block_size_) return;

            deinit_tree();
            top_block_size_ = value;
            // allocate first level descr. of blocks 
            blocks_ = (bm::word_t***)alloc_.alloc_ptr(top_block_size_); 
            ::memset(blocks_, 0, top_block_size_ * sizeof(bm::word_t**));
        }
        
        /** \brief Returns reference on the allocator
        */
        allocator_type& get_allocator() { return alloc_; }

        /** \brief Returns allocator
        */
        allocator_type get_allocator() const { return alloc_; }

    private:

        void operator =(const blocks_manager&);

        void deinit_tree()
        {
            if (blocks_ == 0) return;

            block_free_func  free_func(*this);
            for_each_nzblock(blocks_, top_block_size_, 
                                      bm::set_array_size, free_func);
                                      
            for(unsigned i = 0; i <  top_block_size_; ++i)
            {
                bm::word_t** blk_blk = blocks_[i];
                if (blk_blk) 
                    alloc_.free_ptr(blk_blk);
            }

            alloc_.free_ptr(blocks_, top_block_size_);
        }

    private:
        /// Tree of blocks.
        bm::word_t***                          blocks_;
        /// Size of the top level block array in blocks_ tree
        unsigned                               top_block_size_;
     #ifdef BM_DISBALE_BIT_IN_PTR
        /// mini bitvector used to indicate gap blocks
        MS                                     gap_flags_;
     #endif
        /// Temp block.
        bm::word_t*                            temp_block_; 
        /// vector defines gap block lengths for different levels 
        gap_word_t                             glevel_len_[gap_levels];
        /// allocator
        allocator_type                         alloc_;
    };
    
    const blocks_manager& get_blocks_manager() const
    {
        return blockman_;
    }

    blocks_manager& get_blocks_manager()
    {
        return blockman_;
    }


private:

// This block defines two additional hidden variables used for bitcount
// optimization, in rare cases can make bitvector thread unsafe.
#ifdef BMCOUNTOPT
    mutable id_t      count_;            //!< number of bits "ON" in the vector
    mutable bool      count_is_valid_;   //!< actualization flag
#endif

    blocks_manager    blockman_;         //!< bitblocks manager
    strategy          new_blocks_strat_; //!< blocks allocation strategy.
};





//---------------------------------------------------------------------

template<class Alloc, class MS> 
inline bvector<Alloc, MS> operator& (const bvector<Alloc, MS>& v1,
                                     const bvector<Alloc, MS>& v2)
{
#ifdef BM_USE_EXPLICIT_TEMP
    bvector<Alloc, MS> ret(v1);
    ret.bit_and(v2);
    return ret;
#else    
    return bvector<Alloc, MS>(v1).bit_and(v2);
#endif
}

//---------------------------------------------------------------------

template<class Alloc, class MS> 
inline bvector<Alloc, MS> operator| (const bvector<Alloc, MS>& v1,
                                     const bvector<Alloc>& v2)
{
#ifdef BM_USE_EXPLICIT_TEMP
    bvector<Alloc, MS> ret(v1);
    ret.bit_or(v2);
    return ret;
#else    
    return bvector<Alloc, MS>(v1).bit_or(v2);
#endif
}

//---------------------------------------------------------------------

template<class Alloc, class MS> 
inline bvector<Alloc, MS> operator^ (const bvector<Alloc, MS>& v1,
                                     const bvector<Alloc, MS>& v2)
{
#ifdef BM_USE_EXPLICIT_TEMP
    bvector<Alloc, MS> ret(v1);
    ret.bit_xor(v2);
    return ret;
#else    
    return bvector<Alloc, MS>(v1).bit_xor(v2);
#endif
}

//---------------------------------------------------------------------

template<class Alloc, class MS> 
inline bvector<Alloc, MS> operator- (const bvector<Alloc, MS>& v1,
                                     const bvector<Alloc, MS>& v2)
{
#ifdef BM_USE_EXPLICIT_TEMP
    bvector<Alloc, MS> ret(v1);
    ret.bit_sub(v2);
    return ret;
#else    
    return bvector<Alloc, MS>(v1).bit_sub(v2);
#endif
}




// -----------------------------------------------------------------------

template<typename Alloc, typename MS> 
void bvector<Alloc, MS>::calc_stat(typename bvector<Alloc, MS>::statistics* st) const
{
    st->bit_blocks = st->gap_blocks 
                   = st->max_serialize_mem 
                   = st->memory_used = 0;

    ::memcpy(st->gap_levels, 
             blockman_.glen(), sizeof(gap_word_t) * bm::gap_levels);

    unsigned empty_blocks = 0;
    unsigned blocks_memory = 0;
    gap_word_t* gapl_ptr = st->gap_length;

    st->max_serialize_mem = sizeof(id_t) * 4;

    unsigned block_idx = 0;

    // Walk the blocks, calculate statistics.
    for (unsigned i = 0; i < blockman_.top_block_size(); ++i)
    {
        const bm::word_t* const* blk_blk = blockman_.get_topblock(i);

        if (!blk_blk) 
        {
            block_idx += bm::set_array_size;
            st->max_serialize_mem += sizeof(unsigned) + 1;
            continue;
        }

        for (unsigned j = 0;j < bm::set_array_size; ++j, ++block_idx)
        {
            const bm::word_t* blk = blk_blk[j];
            if (IS_VALID_ADDR(blk))
            {
                st->max_serialize_mem += empty_blocks << 2;
                empty_blocks = 0;

                if (BM_IS_GAP(blockman_, blk, block_idx)) // gap block
                {
                    ++(st->gap_blocks);

                    bm::gap_word_t* gap_blk = BMGAP_PTR(blk);

                    unsigned mem_used = 
                        bm::gap_capacity(gap_blk, blockman_.glen()) 
                        * sizeof(gap_word_t);

                    *gapl_ptr = gap_length(gap_blk);

                    st->max_serialize_mem += *gapl_ptr * sizeof(gap_word_t);
                    blocks_memory += mem_used;

                    ++gapl_ptr;
                }
                else // bit block
                {
                    ++(st->bit_blocks);
                    unsigned mem_used = sizeof(bm::word_t) * bm::set_block_size;
                    st->max_serialize_mem += mem_used;
                    blocks_memory += mem_used;
                }
            }
            else
            {
                ++empty_blocks;
            }
        }
    }  


    st->max_serialize_mem += st->max_serialize_mem / 10; // 10% increment

    // Calc size of different odd and temporary things.

    st->memory_used += sizeof(*this) - sizeof(blockman_);
    st->memory_used += blockman_.mem_used();
    st->memory_used += blocks_memory;
}


// -----------------------------------------------------------------------



template<class Alloc, class MS> 
void bvector<Alloc, MS>::stat(unsigned blocks) const
{
    register bm::id_t count = 0;
    int printed = 0;

    if (!blocks)
    {
        blocks = bm::set_total_blocks;
    }

    unsigned nb;
    for (nb = 0; nb < blocks; ++nb)
    {
        register const bm::word_t* blk = blockman_.get_block(nb);

        if (blk == 0)
        {
           continue;
        }

        if (IS_FULL_BLOCK(blk))
        {
           if (blockman_.is_block_gap(nb)) // gap block
           {
               printf("[Alert!%i]", nb);
               assert(0);
           }
           
           unsigned start = nb; 
           for(unsigned i = nb+1; i < bm::set_total_blocks; ++i, ++nb)
           {
               blk = blockman_.get_block(nb);
               if (IS_FULL_BLOCK(blk))
               {
                 if (blockman_.is_block_gap(nb)) // gap block
                 {
                     printf("[Alert!%i]", nb);
                     assert(0);
                     --nb;
                     break;
                 }

               }
               else
               {
                  --nb;
                  break;
               }
           }


printf("{F.%i:%i}",start, nb);
            ++printed;
/*
            count += bm::SET_BLOCK_MASK + 1;

            register const bm::word_t* blk_end = blk + bm::SET_BLOCK_SIZE;
            unsigned count2 = ::bit_block_calc_count(blk, blk_end);
            assert(count2 == bm::SET_BLOCK_MASK + 1);
*/
        }
        else
        {
            if (blockman_.is_block_gap(nb)) // gap block
            {
               unsigned bc = gap_bit_count(BMGAP_PTR(blk));
               unsigned sum = gap_control_sum(BMGAP_PTR(blk));
               unsigned level = gap_level(BMGAP_PTR(blk));
                count += bc;
printf("[ GAP %i=%i:%i ]", nb, bc, level);
//printf("%i", count);
               if (sum != bm::gap_max_bits)
               {
                    printf("<=*");
               }
                ++printed;
            }
            else // bitset
            {
                const bm::word_t* blk_end = blk + bm::set_block_size;
                unsigned bc = bit_block_calc_count(blk, blk_end);

                count += bc;
printf("( BIT %i=%i )", nb, bc);
//printf("%i", count);
                ++printed;
                
            }
        }
        if (printed == 10)
        {
            printed = 0;
            printf("\n");
        }
    } // for nb
//    printf("\nCOUNT=%i\n", count);
    printf("\n");

}

//---------------------------------------------------------------------


} // namespace

#include "bmundef.h"

#endif


