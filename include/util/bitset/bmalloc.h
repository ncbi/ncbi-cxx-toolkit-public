/*
Copyright (c) 2002,2003 Anatoliy Kuznetsov.

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
*/

#ifndef BMALLOC__H__INCLUDED__
#define BMALLOC__H__INCLUDED__

#include <malloc.h>

namespace bm
{


/*! @defgroup alloc Memory Allocation
 *  Memory allocation related units
 *
 *  @{
 */

/*! 
  @brief Default malloc based bitblock allocator class.

  Functions allocate and deallocate conform to STL allocator specs.
  @ingroup alloc
*/
class block_allocator
{
public:
    /**
    The member function allocates storage for an array of n bm::word_t 
    elements, by calling malloc. 
    @return pointer to the allocated memory. 
    */
    static bm::word_t* allocate(size_t n, const void *)
    {
#ifdef BMSSE2OPT
# ifdef _MSC_VER
        return (bm::word_t*) ::_aligned_malloc(n * sizeof(bm::word_t), 16);
#else
        return (bm::word_t*) ::_mm_malloc(n * sizeof(bm::word_t), 16);
# endif

#else  
        return (bm::word_t*) ::malloc(n * sizeof(bm::word_t));
#endif
    }

    /**
    The member function frees storage for an array of n bm::word_t 
    elements, by calling free. 
    */
    static void deallocate(bm::word_t* p, size_t)
    {
#ifdef BMSSE2OPT
# ifdef _MSC_VER
        ::_aligned_free(p);
#else
        ::_mm_free(p);
# endif

#else  
        ::free(p);
#endif
    }

};


// -------------------------------------------------------------------------

/*! @brief Default malloc based bitblock allocator class.

  Functions allocate and deallocate conform to STL allocator specs.
*/
class ptr_allocator
{
public:
    /**
    The member function allocates storage for an array of n void* 
    elements, by calling malloc. 
    @return pointer to the allocated memory. 
    */
    static void* allocate(size_t n, const void *)
    {
        return ::malloc(n * sizeof(void*));
    }

    /**
    The member function frees storage for an array of n bm::word_t 
    elements, by calling free. 
    */
    static void deallocate(void* p, size_t)
    {
        ::free(p);
    }
};

// -------------------------------------------------------------------------

/*! @brief BM style allocator adapter. 

  Template takes two parameters BA - allocator object for bit blocks and
  PA - allocator object for pointer blocks.
*/
template<class BA, class PA> class mem_alloc
{
public:

    /*! @brief Allocates and returns bit block.
    */
    static bm::word_t* alloc_bit_block()
    {
        return BA::allocate(bm::set_block_size, 0);
    }

    /*! @brief Frees bit block allocated by alloc_bit_block.
    */
    static void free_bit_block(bm::word_t* block)
    {
        if (IS_VALID_ADDR(block)) 
            BA::deallocate(block, bm::set_block_size);
    }

    /*! @brief Allocates GAP block using bit block allocator (BA).

        GAP blocks in BM library belong to levels. Each level has a 
        correspondent length described in bm::gap_len_table<>.
        
        @param level GAP block level.
    */
    static bm::gap_word_t* alloc_gap_block(unsigned level, 
                                           const gap_word_t* glevel_len)
    {
        assert(level < bm::gap_levels);
        unsigned len = 
            glevel_len[level] / (sizeof(bm::word_t) / sizeof(gap_word_t));

        return (bm::gap_word_t*)BA::allocate(len, 0);
    }

    /*! @brief Frees GAP block using bot block allocator (BA)
    */
    static void free_gap_block(bm::gap_word_t* block,
                               const gap_word_t* glevel_len)
    {
        if (IS_VALID_ADDR((bm::word_t*)block)) 
        {
            unsigned len = gap_capacity(block, glevel_len);
            len /= sizeof(bm::word_t) / sizeof(bm::gap_word_t);
            BA::deallocate((bm::word_t*)block, len);        
        }
    }

    /*! @brief Allocates block of pointers.
    */
    static void* alloc_ptr(unsigned size = bm::set_array_size)
    {
        return PA::allocate(size, 0);
    }

    /*! @brief Frees block of pointers.
    */
    static void free_ptr(void* p, unsigned size = bm::set_array_size)
    {
        PA::deallocate(p, size);
    }
};

typedef mem_alloc<block_allocator, ptr_allocator> standard_allocator;

/** @} */


} // namespace bm


#endif
