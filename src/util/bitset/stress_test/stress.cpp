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
* Author:  Anatoliy Kuznetsov
*
*
*/

//#define BM64OPT
//#define BM_SET_MMX_GUARD
//#define BMSSE2OPT
#define BMCOUNTOPT

#include <ncbi_pch.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <memory.h>
#include <iostream>
#include <time.h>
#include <util/bitset/bm.h>
#include <util/bitset/bmalgo.h>

using namespace bm;
using namespace std;

#include "rlebtv.h"
#include <util/bitset/encoding.h>
#include <limits.h>



#define POOL_SIZE 5000

//#define MEM_POOL


template<class T> T* pool_allocate(T** pool, int& i, size_t n)
{
    return i ? pool[i--] : (T*) ::malloc(n * sizeof(T));
}

inline void* pool_allocate2(void** pool, int& i, size_t n)
{
    return i ? pool[i--] : malloc(n * sizeof(void*));
}



template<class T> void pool_free(T** pool, int& i, T* p)
{
    i < POOL_SIZE ? (free(p),(void*)0) : pool[++i]=p;
}


class pool_block_allocator
{
public:

    static bm::word_t* allocate(size_t n, const void *)
    {
        int *idx = 0;
        bm::word_t** pool = 0;

        switch (n)
        {
        case bm::set_block_size:
            idx = &bit_blocks_idx_;
            pool = free_bit_blocks_;
            break;

        case 64:
            idx = &gap_blocks_idx0_;
            pool = gap_bit_blocks0_;
            break;

        case 128:
            idx = &gap_blocks_idx1_;
            pool = gap_bit_blocks1_;
            break;
        
        case 256:
            idx = &gap_blocks_idx2_;
            pool = gap_bit_blocks2_;
            break;

        case 512:
            idx = &gap_blocks_idx3_;
            pool = gap_bit_blocks3_;
            break;

        default:
            assert(0);
        }

        return pool_allocate(pool, *idx, n);
    }

    static void deallocate(bm::word_t* p, size_t n)
    {
        int *idx = 0;
        bm::word_t** pool = 0;

        switch (n)
        {
        case bm::set_block_size:
            idx = &bit_blocks_idx_;
            pool = free_bit_blocks_;
            break;

        case 64:
            idx = &gap_blocks_idx0_;
            pool = gap_bit_blocks0_;
            break;

        case 128:
            idx = &gap_blocks_idx1_;
            pool = gap_bit_blocks1_;
            break;
        
        case 256:
            idx = &gap_blocks_idx2_;
            pool = gap_bit_blocks2_;
            break;

        case 512:
            idx = &gap_blocks_idx3_;
            pool = gap_bit_blocks3_;
            break;

        default:
            assert(0);
        }

        pool_free(pool, *idx, p);
    }

private:
    static bm::word_t* free_bit_blocks_[];
    static int         bit_blocks_idx_;

    static bm::word_t* gap_bit_blocks0_[];
    static int         gap_blocks_idx0_;

    static bm::word_t* gap_bit_blocks1_[];
    static int         gap_blocks_idx1_;

    static bm::word_t* gap_bit_blocks2_[];
    static int         gap_blocks_idx2_;

    static bm::word_t* gap_bit_blocks3_[];
    static int         gap_blocks_idx3_;
};

bm::word_t* pool_block_allocator::free_bit_blocks_[POOL_SIZE];
int pool_block_allocator::bit_blocks_idx_ = 0;

bm::word_t* pool_block_allocator::gap_bit_blocks0_[POOL_SIZE];
int pool_block_allocator::gap_blocks_idx0_ = 0;

bm::word_t* pool_block_allocator::gap_bit_blocks1_[POOL_SIZE];
int pool_block_allocator::gap_blocks_idx1_ = 0;

bm::word_t* pool_block_allocator::gap_bit_blocks2_[POOL_SIZE];
int pool_block_allocator::gap_blocks_idx2_ = 0;

bm::word_t* pool_block_allocator::gap_bit_blocks3_[POOL_SIZE];
int pool_block_allocator::gap_blocks_idx3_ = 0;




class pool_ptr_allocator
{
public:

    static void* allocate(size_t n, const void *)
    {
        return pool_allocate2(free_ptr_blocks_, ptr_blocks_idx_, n);
    }

    static void deallocate(void* p, size_t)
    {
        pool_free(free_ptr_blocks_, ptr_blocks_idx_, p);
    }

private:
    static void*  free_ptr_blocks_[];
    static int    ptr_blocks_idx_;
};

void* pool_ptr_allocator::free_ptr_blocks_[POOL_SIZE];
int pool_ptr_allocator::ptr_blocks_idx_ = 0;


//#define MEM_DEBUG
 
#ifdef MEM_DEBUG


class dbg_block_allocator
{
public:
static unsigned na_;
static unsigned nf_;

    static bm::word_t* allocate(size_t n, const void *)
    {
        ++na_;
        assert(n);
        bm::word_t* p =
            (bm::word_t*) ::malloc((n+1) * sizeof(bm::word_t));
        *p = n;
        return ++p;
    }

    static void deallocate(bm::word_t* p, size_t n)
    {
        ++nf_;
        --p;
        if(*p != n)
        {
            printf("Block memory deallocation error!\n");
            exit(1);
        }
        ::free(p);
    }

    static int balance()
    {
        return nf_ - na_;
    }
};

unsigned dbg_block_allocator::na_ = 0;
unsigned dbg_block_allocator::nf_ = 0;

class dbg_ptr_allocator
{
public:
static unsigned na_;
static unsigned nf_;

    static void* allocate(size_t n, const void *)
    {
        ++na_;
        assert(sizeof(size_t) == sizeof(void*));
        void* p = ::malloc((n+1) * sizeof(void*));
        size_t* s = (size_t*) p;
        *s = n;
        return (void*)++s;
    }

    static void deallocate(void* p, size_t n)
    {
        ++nf_;
        size_t* s = (size_t*) p;
        --s;
        if(*s != n)
        {
            printf("Ptr memory deallocation error!\n");
            exit(1);
        }
        ::free(s);
    }

    static int balance()
    {
        return nf_ - na_;
    }

};

unsigned dbg_ptr_allocator::na_ = 0;
unsigned dbg_ptr_allocator::nf_ = 0;


typedef mem_alloc<dbg_block_allocator, dbg_ptr_allocator> dbg_alloc;

typedef bm::bvector<dbg_alloc> bvect;
typedef bm::bvector_mini<dbg_block_allocator> bvect_mini;

#else

#ifdef MEM_POOL

typedef mem_alloc<pool_block_allocator, pool_ptr_allocator> pool_alloc;
typedef bm::bvector<pool_alloc> bvect;
typedef bm::bvector_mini<bm::block_allocator> bvect_mini;


#else

typedef bm::bvector<> bvect;
typedef bm::bvector_mini<bm::block_allocator> bvect_mini;

#endif

#endif

//const unsigned BITVECT_SIZE = 100000000 * 8;

// This this setting program will consume around 150M of RAM
const unsigned BITVECT_SIZE = 100000000 * 2;

const unsigned ITERATIONS = 100000;
const unsigned PROGRESS_PRINT = 2000000;



void CheckVectors(bvect_mini &bvect_min, 
                  bvect      &bvect_full,
                  unsigned size,
                  bool     detailed = false);


unsigned random_minmax(unsigned min, unsigned max)
{
    unsigned r = (rand() << 16) | rand();
    return r % (max-min) + min;
}


void FillSets(bvect_mini* bvect_min, 
              bvect* bvect_full,
              unsigned min, 
              unsigned max,
              unsigned fill_factor)
{
    unsigned i;
    unsigned id;

    //Random filling
    if(fill_factor == 0)
    {
        unsigned n_id = (max - min) / 100;
        printf("random filling : %i\n", n_id);
        for (i = 0; i < n_id; i++)
        {
            id = random_minmax(min, max);
            bvect_min->set_bit(id);
            bvect_full->set_bit(id);
            if (PROGRESS_PRINT)
            {
                if ( (i % PROGRESS_PRINT) == 0)
                {
                    cout << "+" << flush;
                }
            }
        }
        cout << endl;
    }
    else
    {
        printf("fill_factor random filling : factor = %i\n", fill_factor);

        for(i = 0; i < fill_factor; i++)
        {
            int k = rand() % 10;
            if (k == 0)
                k+=2;

            //Calculate start
            unsigned start = min + (max - min) / (fill_factor * k);

            //Randomize start
            start += random_minmax(1, (max - min) / (fill_factor * 10));

            if (start > max)
            {
                start = min;
            }
            
            //Calculate end 
            unsigned end = start + (max - start) / (fill_factor *2);

            //Randomize end
            end -= random_minmax(1, (max - start) / (fill_factor * 10));

            if (end > max )
            {
                end = max;
            }

            
            if (fill_factor > 1)
            {
                for(; start < end;)
                {
                    int r = rand() % 8;

                    if (r > 7)
                    {
                        int inc = rand() % 3;
                        ++inc;
                        unsigned end2 = start + rand() % 1000;
                        if (end2 > end)
                            end2 = end;
                        while (start < end2)
                        {
                            bvect_min->set_bit(start);
                            bvect_full->set_bit(start);  
                            start += inc;
                        }

                        if (PROGRESS_PRINT)
                        {
                            if ( ( ((i+1)*(end-start))  % PROGRESS_PRINT) == 0)
                            {
                                cout << "+" << flush;
                            }
                        }

                        continue;
                    }

                    if (r)
                    {
                        bvect_min->set_bit(start);
                        bvect_full->set_bit(start);
                        ++start;
                    }
                    else
                    {
                        start+=r;
                        bvect_min->set_bit(start);
                        bvect_full->set_bit(start);
                    }

                    if (PROGRESS_PRINT)
                    {
                        if ( ( ((i+1)*(end-start))  % PROGRESS_PRINT) == 0)
                        {
                            cout << "+" << flush;
                        }
                    }
                }

            }
            else
            {
                int c = rand() % 15;
                if (c == 0)
                    ++c;
                for(; start < end; ++start)
                {
                    bvect_min->set_bit(start);
                    bvect_full->set_bit(start);

                    if (start % c)
                    {
                        start += c;
                    }

                    if (PROGRESS_PRINT)
                    {
                        if ( ( ((i+1)*(end-start))  % PROGRESS_PRINT) == 0)
                        {
                            cout << "+" << flush;
                        }
                    }

                }
            }
            cout << endl;

        }
    }
}

//
// Interval filling.
// 111........111111........111111..........11111111.......1111111...
//


void FillSetsIntervals(bvect_mini* bvect_min, 
              bvect* bvect_full,
              unsigned min, 
              unsigned max,
              unsigned fill_factor,
              bool set_flag=true)
{

    while(fill_factor==0)
    {
        fill_factor=rand()%10;
    }

    cout << "Intervals filling. Factor=" 
         <<  fill_factor << endl << endl;

    unsigned i, j;
    unsigned factor = 70 * fill_factor;
    for (i = min; i < max; ++i)
    {
        unsigned len, end; 

        do
        {
            len = rand() % factor;
            end = i+len;
            
        } while (end >= max);
/*
        if (set_flag == false)
        {
            cout << "Cleaning: " << i << "-" << end << endl;
        }
        else
        {
            cout << "Add: " << i << "-" << end << endl;
        }
*/
        if (i < end)
        {
            bvect_full->set_range(i, end-1, set_flag);
        }
       
        for (j = i; j < end; ++j)
        {
            if (set_flag)
            {
                bvect_min->set_bit(j);
                //bvect_full->set_bit(j);
            }
            else
            {
                bvect_min->clear_bit(j);
                //bvect_full->clear_bit(j);
/*
        if (g_cnt_check)
        {
            bool b = bvect_full->count_check();
            if(!b)
            {
                cout << "Count check failed (clear)!" << endl;
                cout << "bit=" << j << endl;
                exit(1);
            }
        }
*/
            }

                           
        } // j

//cout << "Checking range filling " << from << "-" << to << endl;
//CheckVectors(*bvect_min, *bvect_full, 100000000);


        i = end;


        len = rand() % (factor* 10 * bm::gap_max_bits);
        if (len % 2)
        {
            len *= rand() % (factor * 10);
        }

        i+=len;

        if ( (len % 6) == 0)  
        {
/*
if (set_flag == false)
{
    cout << "Additional Cleaning: " << i << "-" << end << endl;
}
*/
            for(unsigned k=0; k < 1000 && i < max; k+=3,i+=3)
            {
                if (set_flag)
                {
                    bvect_min->set_bit(i);
                    bvect_full->set_bit(i);            
                }
                else
                {
                    bvect_min->clear_bit(j);
                    bvect_full->clear_bit(j);
                }

            }
        }

    } // for i

}

void FillSetClearIntervals(bvect_mini* bvect_min, 
              bvect* bvect_full,
              unsigned min, 
              unsigned max,
              unsigned fill_factor)
{
    FillSetsIntervals(bvect_min, bvect_full, min, max, fill_factor, true);
    FillSetsIntervals(bvect_min, bvect_full, min, max, fill_factor, false);
}


void FillSetsRandom(bvect_mini* bvect_min, 
              bvect* bvect_full,
              unsigned min, 
              unsigned max,
              unsigned fill_factor)
{
    unsigned diap = max - min;

    unsigned count;


    switch (fill_factor)
    {
    case 0:
        count = diap / 1000;
        break;
    case 1:
        count = diap / 100;
        break;
    default:
        count = diap / 10;
        break;

    }

    for (unsigned i = 0; i < count; ++i)
    {
        unsigned bn = rand() % count;
        bn += min;

        if (bn > max)
        {
            bn = max;
        }
        bvect_min->set_bit(bn);
        bvect_full->set_bit(bn);   
        
        if ( (i  % PROGRESS_PRINT) == 0)
        {
            cout << "+" << flush;
        }
    }
    cout << "Ok" << endl;

}


//
//  Quasi random filling with choosing randomizing method.
//
//
void FillSetsRandomMethod(bvect_mini* bvect_min, 
                          bvect* bvect_full,
                          unsigned min, 
                          unsigned max,
                          int optimize = 0,
                          int method = -1)
{
    if (method == -1)
    {
        method = rand() % 5;
    }
    unsigned factor;
//method = 4;
    switch (method)
    {

    case 0:
        cout << "Random filling: method - FillSets - factor(0)" << endl;
        FillSets(bvect_min, bvect_full, min, max, 0);
        break;

    case 1:
        cout << "Random filling: method - FillSets - factor(random)" << endl;
        factor = rand()%3;
        FillSets(bvect_min, bvect_full, min, max, factor?factor:1);
        break;

    case 2:
        cout << "Random filling: method - Set-Clear Intervals - factor(random)" << endl;
        factor = rand()%10;
        FillSetClearIntervals(bvect_min, bvect_full, min, max, factor);
        break;
    case 3:
        cout << "Random filling: method - FillRandom - factor(random)" << endl;
        factor = rand()%3;
        FillSetsRandom(bvect_min, bvect_full, min, max, factor?factor:1);
        break;

    default:
        cout << "Random filling: method - Set Intervals - factor(random)" << endl;
        factor = rand()%10;
        FillSetsIntervals(bvect_min, bvect_full, min, max, factor);
        break;

    } // switch

    if (optimize && (method <= 1))
    {
        cout << "Vector optimization..." << flush;
        bvect_full->optimize();
        cout << "OK" << endl;
    }
}



void print_mv(const bvect_mini &bvect_min, unsigned size)
{
    unsigned i;
    for (i = 0; i < size; ++i)
    {
        bool bflag = bvect_min.is_bit_true(i) != 0; 

        if (bflag)
            printf("1");
        else
            printf("0");
        if ((i % 31) == 0 && (i != 0))
            printf(".");
    }

    printf("\n");
}

void print_gap(const gap_vector& gap_vect, unsigned size)
{
    const gap_word_t *buf = gap_vect.get_buf();
    unsigned len = gap_length(buf);
    printf("[%i:]", *buf++ & 1);

    for (unsigned i = 1; i < len; ++i)
    {
        printf("%i,", *buf++);
    }

    printf("\n");
}

void CheckGAPMin(const gap_vector& gapv, const bvect_mini& bvect_min, unsigned len)
{
    for (unsigned i = 0; i < len; ++i)
    {
        int bit1 = (gapv.is_bit_true(i) == 1);
        int bit2 = (bvect_min.is_bit_true(i) != 0);
        if(bit1 != bit2)
        {
           cout << "Bit comparison failed. " << "Bit N=" << i << endl;
           assert(0);
           exit(1);
        }
    }
}

void CheckIntervals(const bvect& bv, unsigned max_bit)
{
    unsigned cnt0 = count_intervals(bv);
    unsigned cnt1 = 1;
    bool bit_prev = bv.test(0);
    for (unsigned i = 1; i < max_bit; ++i)
    {
        bool bit = bv.test(i);
        cnt1 += bit_prev ^ bit;
        bit_prev = bit;
    }
    if (cnt0 != cnt1)
    {
        cout << "CheckIntervals error. " << "bm count=" << cnt0
             << " Control = " << cnt1 << endl;
        exit(1);
    }
}

template<class T> void CheckCountRange(const T& vect, 
                                       unsigned left, 
                                       unsigned right,
                                       unsigned* block_count_arr=0)
{
    unsigned cnt1 = vect.count_range(left, right, block_count_arr);
    unsigned cnt2 = 0;
//cout << endl;
    for (unsigned i = left; i <= right; ++i)
    {
        if (vect.test(i))
        {
//            cout << i << " " << flush;
            ++cnt2;
        }
    }
//cout << endl;
    if (cnt1 != cnt2)
    {
        cout << "Bitcount range failed!" << "left=" << left 
             << " right=" << right << endl
             << "count_range()=" << cnt1 
             << " check=" << cnt2;
        exit(1);
    }
}


unsigned BitCountChange(unsigned word)
{
    unsigned count = 1;
    unsigned bit_prev = word & 1;
    word >>= 1;
    for (unsigned i = 1; i < 32; ++i)
    {
        unsigned bit = word & 1;
        count += bit ^ bit_prev;
        bit_prev = bit;
        word >>= 1;
    }
    return count;
}


void DetailedCheckVectors(const bvect_mini &bvect_min, 
                          const bvect      &bvect_full,
                          unsigned size)
{
    cout << "Detailed check" << endl;

    //bvect_full.stat();

    // detailed bit by bit comparison. Paranoia check.

    unsigned i;
    for (i = 0; i < size; ++i)
    {
        bool bv_m_flag = bvect_min.is_bit_true(i) != 0; 
        bool bv_f_flag = bvect_full.get_bit(i) != 0;

        if (bv_m_flag != bv_f_flag)
        {
            printf("Bit %u is non conformant. vect_min=%i vect_full=%i\n",
                i, (int)bv_m_flag, (int)bv_f_flag);

            cout << "Non-conformant block number is: " << unsigned(i >>  bm::set_block_shift) << endl;
//            throw 110;
            exit(1);
        }

        if (PROGRESS_PRINT)
        {
            if ( (i % PROGRESS_PRINT) == 0)
            {
                printf(".");
            }
        }
             
    }
    
    printf("\n detailed check ok.\n");

}


// vectors comparison check

void CheckVectors(bvect_mini &bvect_min, 
                  bvect      &bvect_full,
                  unsigned size,
                  bool     detailed)
{
    cout << "\nVectors checking...bits to compare = " << size << endl;

    cout << "Bitcount summary : " << endl;
    unsigned min_count = bvect_min.bit_count();
    cout << "minvector count = " << min_count << endl;
    unsigned count = bvect_full.count();
    unsigned full_count = bvect_full.recalc_count();
    cout << "fullvector re-count = " << full_count << endl;
    
    if (min_count != full_count)
    {
        cout << "fullvector count = " << count << endl;
        cout << "Count comparison failed !!!!" << endl;
        bvect_full.stat();
        DetailedCheckVectors(bvect_min, bvect_full, size);

        exit(1);  
    } 

    if (full_count)
    {
        bool any = bvect_full.any();
        if (!any)
        {
            cout << "Anycheck failed!" << endl;
            exit(1);
        }
    }

    // get_next comparison

    cout << "Positive bits comparison..." << flush;
    unsigned nb_min = bvect_min.get_first();
    unsigned nb_ful = bvect_full.get_first();

    bvect::counted_enumerator en = bvect_full.first();
    unsigned nb_en = *en;
    if (nb_min != nb_ful)
    {
         cout << "!!!! First bit comparison failed. Full id = " 
              << nb_ful << " Min id = " << nb_min 
              << endl;

         bool bit_f = bvect_full.get_bit(nb_ful);
         cout << "Full vector'd bit #" << nb_ful << "is:" 
              << bit_f << endl;

         bool bit_m = (bvect_min.is_bit_true(nb_min) == 1);
         cout << "Min vector'd bit #" << nb_min << "is:" 
              << bit_m << endl;


         bvect_full.stat();

         DetailedCheckVectors(bvect_min, bvect_full, size);

         exit(1);
    }

    if (full_count)
    {
       unsigned bit_count = 1;
       unsigned en_prev = nb_en;

       do
       {
           nb_min = bvect_min.get_next(nb_min);
           if (nb_min == 0)
           {
               break;
           }

           en_prev = nb_en;
           ++en;

           nb_en = *en;
//           nb_en = bvect_full.get_next(nb_en);

           ++bit_count;

           if (nb_en != nb_min)
           {
               nb_ful = bvect_full.get_next(en_prev);
               cout << "!!!!! next bit comparison failed. Full id = " 
                    << nb_ful << " Min id = " << nb_min 
                    << " Enumerator = " << nb_en
                    << endl;

     //          bvect_full.stat();

     //          DetailedCheckVectors(bvect_min, bvect_full, size);

               exit(1);
           }
            if ( (bit_count % PROGRESS_PRINT) == 0)
           {
                cout << "." << flush;
            }

       } while (en.valid());
       if (bit_count != min_count)
       {
           cout << " Bit count failed."  
                << " min = " << min_count 
                << " bit = " << bit_count 
                << endl;
           exit(1);
       }
    }

    cout << "OK" << endl;

    return;
    

}


void ClearAllTest()
{
    bvect     bvect_full;

    for (int i = 0; i < 100000; ++i)
    {
        bvect_full.set_bit(i);
    }
    bvect_full.optimize();
    bvect_full.clear();

    bvect_full.stat();

    int count = bvect_full.count();
    assert(count == 0);
    bvect_full.stat();
}


void WordCmpTest()
{
    cout << "---------------------------- WordCmp test" << endl;

    for (int i = 0; i < 10000000; ++i)
    {
        unsigned w1 = rand();
        unsigned w2 = rand();
        int res = wordcmp0(w1, w2);
        int res2 = wordcmp(w1, w2);
        if (res != res2)
        {
            printf("WordCmp failed !\n");
            exit(1);
        }

        res = wordcmp0((unsigned)0U, (unsigned)w2);
        res2 = wordcmp((unsigned)0U, (unsigned)w2);

        if (res != res2)
        {
            printf("WordCmp 0 test failed !\n");
            exit(1);
        }

        res = wordcmp0((unsigned)~0U, (unsigned)w2);
        res2 = wordcmp((unsigned)~0U, (unsigned)w2);

        if (res != res2)
        {
            printf("WordCmp ~0 test failed !\n");
            exit(1);
        }

        res = wordcmp0((unsigned)w2, (unsigned)0);
        res2 = wordcmp((unsigned)w2, (unsigned)0);

        if (res != res2)
        {
            printf("WordCmp 0-2 test failed !\n");
            exit(1);
        }

    }

    cout << "Ok." << endl;
}

void BasicFunctionalityTest()
{
    cout << "---------------------------- Basic functinality test" << endl;

    assert(ITERATIONS < BITVECT_SIZE);


    bvect_mini     bvect_min(BITVECT_SIZE);
    bvect          bvect_full;
    bvect          bvect_full1;

    printf("\nBasic functionality test.\n");
    
    // filling vectors with regular values

    unsigned i;
    for (i = 0; i < ITERATIONS; ++i)
    {
        bvect_min.set_bit(i);
        bvect_full.set_bit(i);
    }
    
    bvect_full1.set_range(0, ITERATIONS-1);
    
    CheckCountRange(bvect_full, 0, ITERATIONS);
    CheckCountRange(bvect_full, 10, ITERATIONS+10);

    if (bvect_full1 != bvect_full)
    {
        cout << "set_range failed!" << endl;
        bvect_full1.stat();
        exit(1);
    }

    bvect_full.stat();
    bvect_full1.stat();

    // checking the results
    unsigned count_min = 0;
    for (i = 0; i < ITERATIONS; ++i)
    {
        if (bvect_min.is_bit_true(i))
            ++count_min;
    }


    
    unsigned count_full = bvect_full.count();

    if (count_min == count_full)
    {
        printf("simple count test ok.\n");
    }
    else
    {
        printf("simple count test failed count_min = %i  count_full = %i\n", 
               count_min, count_full);
        exit(1);
    }


    // detailed vectors verification

    CheckVectors(bvect_min, bvect_full, ITERATIONS);

    // now clearning

    for (i = 0; i < ITERATIONS; i+=2)
    {
        bvect_min.clear_bit(i);
        bvect_full.clear_bit(i);
        bvect_full1.set_range(i, i, false);
    }

    CheckVectors(bvect_min, bvect_full, ITERATIONS);
    CheckVectors(bvect_min, bvect_full1, ITERATIONS);

    for (i = 0; i < ITERATIONS; ++i)
    {
        bvect_min.clear_bit(i);
    }
    bvect_full.clear();

    CheckVectors(bvect_min, bvect_full, ITERATIONS);

    cout << "Random step filling" << endl;

    for (i = rand()%10; i < ITERATIONS; i+=rand()%10)
    {
        bvect_min.clear_bit(i);
        bvect_full.clear_bit(i);
    }
    
    CheckVectors(bvect_min, bvect_full, ITERATIONS);

    bvect bv1;
    bvect bv2;

    bv1[10] = true;
    bv1[1000] = true;

    bv2[200] = bv2[700] = bv2[500] = true;

    bv1.swap(bv2);

    if (bv1.count() != 3)
    {
        cout << "Swap test failed!" << endl;
        exit(1);
    }

    if (bv2.count() != 2)
    {
        cout << "Swap test failed!" << endl;
        exit(1);
    }
}

void SimpleRandomFillTest()
{
    assert(ITERATIONS < BITVECT_SIZE);

    cout << "-------------------------- SimpleRandomFillTest" << endl;

    {
    printf("Simple random fill test 1.");
    bvect_mini   bvect_min(BITVECT_SIZE);
    bvect      bvect_full;
    bvect_full.set_new_blocks_strat(bm::BM_BIT);


    unsigned iter = ITERATIONS / 5;

    printf("\nSimple Random fill test ITERATIONS = %i\n", iter);

    bvect_min.set_bit(0);
    bvect_full.set_bit(0);

    unsigned i;
    for (i = 0; i < iter; ++i)
    {
        unsigned num = ::rand() % iter;
        bvect_min.set_bit(num);
        bvect_full.set_bit(num);
        if ((i % 1000) == 0) cout << "." << flush;
        CheckCountRange(bvect_full, 0, num);
        CheckCountRange(bvect_full, num, num+iter);
    }

    CheckVectors(bvect_min, bvect_full, iter);
    CheckCountRange(bvect_full, 0, iter);

    printf("Simple random fill test 2.");

    for(i = 0; i < iter; ++i)
    {
        unsigned num = ::rand() % iter;
        bvect_min.clear_bit(num);
        bvect_full.clear_bit(num);
    }

    CheckVectors(bvect_min, bvect_full, iter);
    }


    {
    printf("\nSimple random fill test 3.\n");
    bvect_mini   bvect_min(BITVECT_SIZE);
    bvect      bvect_full(bm::BM_GAP);


    unsigned iter = ITERATIONS;

    printf("\nSimple Random fill test ITERATIONS = %i\n", iter);

    unsigned i;
    for(i = 0; i < iter; ++i)
    {
        unsigned num = ::rand() % iter;
        bvect_min.set_bit(num);
        bvect_full.set_bit(num);
        CheckCountRange(bvect_full, 0, 65535);
        CheckCountRange(bvect_full, 0, num);
        CheckCountRange(bvect_full, num, num+iter);
        if ((i % 1000) == 0) cout << "." << flush;
    }

    CheckVectors(bvect_min, bvect_full, iter);

    printf("Simple random fill test 4.");

    for(i = 0; i < iter; ++i)
    {
        unsigned num = ::rand() % iter;
        bvect_min.clear_bit(num);
        bvect_full.clear_bit(num);
        CheckCountRange(bvect_full, 0, num);
        CheckCountRange(bvect_full, num, num+iter);
        if ((i % 1000) == 0) cout << "." << flush;
    }

    CheckVectors(bvect_min, bvect_full, iter);
    CheckCountRange(bvect_full, 0, iter);
    }


}




void RangeRandomFillTest()
{
    assert(ITERATIONS < BITVECT_SIZE);

    cout << "----------------------------------- RangeRandomFillTest" << endl;

    {
    bvect_mini   bvect_min(BITVECT_SIZE);
    bvect     bvect_full;

    printf("Range Random fill test\n");

    unsigned min = BITVECT_SIZE / 2;
    unsigned max = BITVECT_SIZE / 2 + ITERATIONS;
    if (max > BITVECT_SIZE) 
        max = BITVECT_SIZE - 1;

    FillSets(&bvect_min, &bvect_full, min, max, 0);

    CheckVectors(bvect_min, bvect_full, BITVECT_SIZE);
    CheckCountRange(bvect_full, min, max);

    }

    
    {
    bvect_mini   bvect_min(BITVECT_SIZE);
    bvect     bvect_full;

    printf("Range Random fill test\n");

    unsigned min = BITVECT_SIZE / 2;
    unsigned max = BITVECT_SIZE / 2 + ITERATIONS;
    if (max > BITVECT_SIZE) 
        max = BITVECT_SIZE - 1;

    FillSetsIntervals(&bvect_min, &bvect_full, min, max, 4);

    CheckVectors(bvect_min, bvect_full, BITVECT_SIZE);
    CheckCountRange(bvect_full, min, max);
    }
    

}

void AndOperationsTest()
{
    assert(ITERATIONS < BITVECT_SIZE);

    cout << "----------------------------------- AndOperationTest" << endl;

    {

    bvect_mini   bvect_min1(256);
    bvect_mini   bvect_min2(256);
    bvect        bvect_full1;
    bvect        bvect_full2;

    bvect_full1.set_new_blocks_strat(bm::BM_GAP);
    bvect_full2.set_new_blocks_strat(bm::BM_GAP);



    printf("AND test\n");

    bvect_min1.set_bit(1);
    bvect_min1.set_bit(12);
    bvect_min1.set_bit(13);

    bvect_min2.set_bit(12);
    bvect_min2.set_bit(13);

    bvect_min1.combine_and(bvect_min2);

    bvect_full1.set_bit(1);
    bvect_full1.set_bit(12);
    bvect_full1.set_bit(13);

    bvect_full2.set_bit(12);
    bvect_full2.set_bit(13);

    bm::id_t predicted_count = bm::count_and(bvect_full1, bvect_full2);

    bvect_full1.bit_and(bvect_full2);

    bm::id_t count = bvect_full1.count();
    if (count != predicted_count)
    {
        cout << "Predicted count error!" << endl;
        exit(1);
    }

    CheckVectors(bvect_min1, bvect_full1, 256);
    CheckCountRange(bvect_full1, 0, 256);

    }

    {

    bvect_mini   bvect_min1(BITVECT_SIZE);
    bvect_mini   bvect_min2(BITVECT_SIZE);
    bvect        bvect_full1;
    bvect        bvect_full2;


    printf("AND test stage 1.\n");

    for (int i = 0; i < 112; ++i)
    {
        bvect_min1.set_bit(i);
        bvect_full1.set_bit(i);

        bvect_min2.set_bit(i);
        bvect_full2.set_bit(i);

    }

    CheckVectors(bvect_min1, bvect_full1, BITVECT_SIZE/10+10);
    CheckCountRange(bvect_full1, 0, BITVECT_SIZE/10+10);

//    FillSets(&bvect_min1, &bvect_full1, 1, BITVECT_SIZE/7, 0);
//    FillSets(&bvect_min2, &bvect_full2, 1, BITVECT_SIZE/7, 0);

    bvect_min1.combine_and(bvect_min2);

    bm::id_t predicted_count = bm::count_and(bvect_full1,bvect_full2);

    bvect_full1.bit_and(bvect_full2);

    bm::id_t count = bvect_full1.count();
    if (count != predicted_count)
    {
        cout << "Predicted count error!" << endl;
        exit(1);
    }

    CheckVectors(bvect_min1, bvect_full1, BITVECT_SIZE/10+10);
    CheckCountRange(bvect_full1, 0, BITVECT_SIZE/10+10);

    }


    {

    bvect_mini   bvect_min1(BITVECT_SIZE);
    bvect_mini   bvect_min2(BITVECT_SIZE);
    bvect        bvect_full1;
    bvect        bvect_full2;

    bvect_full1.set_new_blocks_strat(bm::BM_GAP);
    bvect_full2.set_new_blocks_strat(bm::BM_GAP);

    printf("AND test stage 2.\n");


    FillSets(&bvect_min1, &bvect_full1, 1, BITVECT_SIZE/7, 0);
    FillSets(&bvect_min2, &bvect_full2, 1, BITVECT_SIZE/7, 0);

    bm::id_t predicted_count = bm::count_and(bvect_full1,bvect_full2);

    bvect_min1.combine_and(bvect_min2);

    bvect_full1.bit_and(bvect_full2);

    bm::id_t count = bvect_full1.count();
    if (count != predicted_count)
    {
        cout << "Predicted count error!" << endl;
        bvect_full1.stat();
        exit(1);
    }

    CheckVectors(bvect_min1, bvect_full1, BITVECT_SIZE/10+10);
    CheckCountRange(bvect_full1, 0, BITVECT_SIZE/10+10);

    }

    {

    bvect_mini   bvect_min1(BITVECT_SIZE);
    bvect_mini   bvect_min2(BITVECT_SIZE);
    bvect        bvect_full1;
    bvect        bvect_full2;

    bvect_full1.set_new_blocks_strat(bm::BM_BIT);
    bvect_full2.set_new_blocks_strat(bm::BM_BIT);

    cout << "------------------------------" << endl;
    printf("AND test stage 3.\n");


    FillSets(&bvect_min1, &bvect_full1, 1, BITVECT_SIZE/5, 2);
    FillSets(&bvect_min2, &bvect_full2, 1, BITVECT_SIZE/5, 2);

    bvect_min1.combine_and(bvect_min2);

    bm::id_t predicted_count = bm::count_and(bvect_full1, bvect_full2);

    bvect_full1.bit_and(bvect_full2);

    bm::id_t count = bvect_full1.count();
    if (count != predicted_count)
    {
        cout << "Predicted count error!" << endl;
        exit(1);
    }

    CheckVectors(bvect_min1, bvect_full1, BITVECT_SIZE);
    CheckCountRange(bvect_full1, 0, BITVECT_SIZE);

    bvect_full1.optimize();
    CheckVectors(bvect_min1, bvect_full1, BITVECT_SIZE);
    CheckCountRange(bvect_full1, 0, BITVECT_SIZE);
    CheckCountRange(bvect_full1, BITVECT_SIZE/2, BITVECT_SIZE);


    }


}


void OrOperationsTest()
{
    assert(ITERATIONS < BITVECT_SIZE);

    cout << "----------------------------------- OrOperationTest" << endl;

    {

    bvect_mini   bvect_min1(256);
    bvect_mini   bvect_min2(256);
    bvect        bvect_full1;
    bvect        bvect_full2;

    bvect_full1.set_new_blocks_strat(bm::BM_GAP);
    bvect_full2.set_new_blocks_strat(bm::BM_GAP);



    printf("OR test\n");

    bvect_min1.set_bit(1);
    bvect_min1.set_bit(12);
    bvect_min1.set_bit(13);

    bvect_min2.set_bit(12);
    bvect_min2.set_bit(13);

    bvect_min1.combine_or(bvect_min2);

    bvect_full1.set_bit(1);
    bvect_full1.set_bit(12);
    bvect_full1.set_bit(13);

    bvect_full2.set_bit(12);
    bvect_full2.set_bit(13);
    
    bm::id_t predicted_count = bm::count_or(bvect_full1, bvect_full2);    

    bvect_full1.bit_or(bvect_full2);

    bm::id_t count = bvect_full1.count();
    if (count != predicted_count)
    {
        cout << "Predicted count error!" << endl;
        cout << predicted_count << " " << count << endl;
        bvect_full1.stat();
        exit(1);
    }


    CheckVectors(bvect_min1, bvect_full1, 256);
    CheckCountRange(bvect_full1, 0, 256);
    CheckCountRange(bvect_full1, 128, 256);
    }

    {

    bvect_mini   bvect_min1(BITVECT_SIZE);
    bvect_mini   bvect_min2(BITVECT_SIZE);
    bvect        bvect_full1;
    bvect        bvect_full2;

    bvect_full1.set_new_blocks_strat(bm::BM_GAP);
    bvect_full2.set_new_blocks_strat(bm::BM_GAP);

    printf("OR test stage 2.\n");


    FillSets(&bvect_min1, &bvect_full1, 1, BITVECT_SIZE/7, 0);
    FillSets(&bvect_min2, &bvect_full2, 1, BITVECT_SIZE/7, 0);

    bvect_min1.combine_or(bvect_min2);

    bm::id_t predicted_count = bm::count_or(bvect_full1, bvect_full2);    

    bvect_full1.bit_or(bvect_full2);

    bm::id_t count = bvect_full1.count();
    if (count != predicted_count)
    {
        cout << "Predicted count error!" << endl;
        exit(1);
    }

    CheckVectors(bvect_min1, bvect_full1, BITVECT_SIZE/10+10);
    CheckCountRange(bvect_full1, 0, BITVECT_SIZE/10+10);

    }

    {

    bvect_mini   bvect_min1(BITVECT_SIZE);
    bvect_mini   bvect_min2(BITVECT_SIZE);
    bvect        bvect_full1;
    bvect        bvect_full2;

    bvect_full1.set_new_blocks_strat(bm::BM_BIT);
    bvect_full2.set_new_blocks_strat(bm::BM_BIT);

    cout << "------------------------------" << endl;
    printf("OR test stage 3.\n");


    FillSets(&bvect_min1, &bvect_full1, 1, BITVECT_SIZE/5, 2);
    FillSets(&bvect_min2, &bvect_full2, 1, BITVECT_SIZE/5, 2);

    bvect_min1.combine_or(bvect_min2);
    
    bm::id_t predicted_count = bm::count_or(bvect_full1, bvect_full2);    

    bvect_full1.bit_or(bvect_full2);

    bm::id_t count = bvect_full1.count();
    if (count != predicted_count)
    {
        cout << "Predicted count error!" << endl;
        exit(1);
    }

    CheckVectors(bvect_min1, bvect_full1, BITVECT_SIZE);

    bvect_full1.optimize();

    CheckVectors(bvect_min1, bvect_full1, BITVECT_SIZE);
    CheckCountRange(bvect_full1, 0, BITVECT_SIZE);


    }
    
    cout << "Testing combine_or" << endl;
    
    {
    
    bvect        bvect_full1;
    bvect        bvect_full2;
    bvect_mini   bvect_min1(BITVECT_SIZE);
    
    bvect_full1.set_new_blocks_strat(bm::BM_GAP);
    bvect_full2.set_new_blocks_strat(bm::BM_GAP);

    unsigned ids[10000];
    unsigned to_add = 10000;
    
    unsigned bn = 0;
    for (unsigned i = 0; i < to_add; ++i)
    {
        ids[i] = bn;
        bvect_full2.set(bn);
        bvect_min1.set_bit(bn);
        bn += 15;
    }
    
    unsigned* first = ids;
    unsigned* last = ids + to_add;
    
    bm::combine_or(bvect_full1, first, last);

    CheckVectors(bvect_min1, bvect_full1, BITVECT_SIZE);
    
    bm::combine_or(bvect_full1, first, last);
    CheckVectors(bvect_min1, bvect_full1, BITVECT_SIZE);
    
    }
    
    
    {
    unsigned ids[] = {0, 65536, 65535, 65535*3, 65535*2, 10};
    unsigned to_add = sizeof(ids)/sizeof(unsigned);
    bvect        bvect_full1;
    bvect        bvect_full2;    
    bvect_mini   bvect_min1(BITVECT_SIZE);

    bvect_full1.set_new_blocks_strat(bm::BM_GAP);
    bvect_full2.set_new_blocks_strat(bm::BM_GAP);
    
    unsigned bn = 0;
    for (unsigned i = 0; i < to_add; ++i)
    {
        ids[i] = bn;
        bvect_full2.set(bn);
        bvect_min1.set_bit(bn);
        bn += 15;
    }
    
    unsigned* first = ids;
    unsigned* last = ids + to_add;
    
    bm::combine_or(bvect_full1, first, last);
    CheckVectors(bvect_min1, bvect_full1, BITVECT_SIZE);

    bm::combine_or(bvect_full1, first, last);
    CheckVectors(bvect_min1, bvect_full1, BITVECT_SIZE);    
    }
    

}



void SubOperationsTest()
{
    assert(ITERATIONS < BITVECT_SIZE);

    cout << "----------------------------------- SubOperationTest" << endl;

    {

    bvect_mini   bvect_min1(256);
    bvect_mini   bvect_min2(256);
    bvect        bvect_full1;
    bvect        bvect_full2;

    bvect_full1.set_new_blocks_strat(bm::BM_GAP);
    bvect_full2.set_new_blocks_strat(bm::BM_GAP);



    printf("SUB test\n");

    bvect_min1.set_bit(1);
    bvect_min1.set_bit(12);
    bvect_min1.set_bit(13);

    bvect_min2.set_bit(12);
    bvect_min2.set_bit(13);

    bvect_min1.combine_sub(bvect_min2);

    bvect_full1.set_bit(1);
    bvect_full1.set_bit(12);
    bvect_full1.set_bit(13);

    bvect_full2.set_bit(12);
    bvect_full2.set_bit(13);

    bm::id_t predicted_count = bm::count_sub(bvect_full1, bvect_full2);

    bvect_full1.bit_sub(bvect_full2);
    
    bm::id_t count = bvect_full1.count();
    if (count != predicted_count)
    {
        cout << "Predicted count error!" << endl;
        exit(1);
    }

    CheckVectors(bvect_min1, bvect_full1, 256);
    CheckCountRange(bvect_full1, 0, 256);

    }

    {

    bvect_mini   bvect_min1(BITVECT_SIZE);
    bvect_mini   bvect_min2(BITVECT_SIZE);
    bvect        bvect_full1;
    bvect        bvect_full2;

    bvect_full1.set_new_blocks_strat(bm::BM_GAP);
    bvect_full2.set_new_blocks_strat(bm::BM_GAP);

    printf("SUB test stage 2.\n");


    FillSets(&bvect_min1, &bvect_full1, 1, BITVECT_SIZE/7, 0);
    FillSets(&bvect_min2, &bvect_full2, 1, BITVECT_SIZE/7, 0);

    bvect_min1.combine_sub(bvect_min2);

    bm::id_t predicted_count = bm::count_sub(bvect_full1, bvect_full2);

    bvect_full1.bit_sub(bvect_full2);
    
    bm::id_t count = bvect_full1.count();
    if (count != predicted_count)
    {
        cout << "Predicted count error!" << endl;
        cout << predicted_count << " " << count << endl;
bvect_full1.stat();    
        
        exit(1);
    }
    

    CheckVectors(bvect_min1, bvect_full1, BITVECT_SIZE/10+10);
    CheckCountRange(bvect_full1, 0, BITVECT_SIZE/10+10);

    }

    {

    bvect_mini   bvect_min1(BITVECT_SIZE);
    bvect_mini   bvect_min2(BITVECT_SIZE);
    bvect        bvect_full1;
    bvect        bvect_full2;

    bvect_full1.set_new_blocks_strat(bm::BM_BIT);
    bvect_full2.set_new_blocks_strat(bm::BM_BIT);

    cout << "------------------------------" << endl;
    printf("SUB test stage 3.\n");


    FillSets(&bvect_min1, &bvect_full1, 1, BITVECT_SIZE/5, 2);
    FillSets(&bvect_min2, &bvect_full2, 1, BITVECT_SIZE/5, 2);

    bvect_min1.combine_sub(bvect_min2);
    
    bm::id_t predicted_count = bm::count_sub(bvect_full1, bvect_full2);

    bvect_full1.bit_sub(bvect_full2);

    bm::id_t count = bvect_full1.count();
    if (count != predicted_count)
    {
        cout << "Predicted count error!" << endl;
        exit(1);
    }


    CheckVectors(bvect_min1, bvect_full1, BITVECT_SIZE);

    bvect_full1.optimize();
    CheckVectors(bvect_min1, bvect_full1, BITVECT_SIZE);
    CheckCountRange(bvect_full1, 0, BITVECT_SIZE);


    }

}



void XorOperationsTest()
{
    assert(ITERATIONS < BITVECT_SIZE);

    cout << "----------------------------------- XorOperationTest" << endl;

    {

    bvect_mini   bvect_min1(256);
    bvect_mini   bvect_min2(256);
    bvect        bvect_full1;
    bvect        bvect_full2;

    bvect_full1.set_new_blocks_strat(bm::BM_GAP);
    bvect_full2.set_new_blocks_strat(bm::BM_GAP);



    printf("XOR test\n");

    bvect_min1.set_bit(1);
    bvect_min1.set_bit(12);
    bvect_min1.set_bit(13);

    bvect_min2.set_bit(12);
    bvect_min2.set_bit(13);

    bvect_min1.combine_xor(bvect_min2);

    bvect_full1.set_bit(1);
    bvect_full1.set_bit(12);
    bvect_full1.set_bit(13);

    bvect_full2.set_bit(12);
    bvect_full2.set_bit(13);

    bm::id_t predicted_count = bm::count_xor(bvect_full1, bvect_full2);

    bvect_full1.bit_xor(bvect_full2);

    bm::id_t count = bvect_full1.count();
    if (count != predicted_count)
    {
        cout << "1.Predicted count error!" << endl;
        exit(1);
    }

    CheckVectors(bvect_min1, bvect_full1, 256);
    CheckCountRange(bvect_full1, 0, 256);
    CheckCountRange(bvect_full1, 128, 256);

    }

    {
        bvect  bvect1;
        bvect_mini  bvect_min1(BITVECT_SIZE);

        bvect  bvect2;
        bvect_mini  bvect_min2(BITVECT_SIZE);


        for (int i = 0; i < 150000; ++i)
        {
            bvect2.set_bit(i);
            bvect_min2.set_bit(i);
        }

        bvect2.optimize();

        bm::id_t predicted_count = bm::count_xor(bvect1, bvect2);

        bvect1.bit_xor(bvect2);
        
        bm::id_t count = bvect1.count();
        if (count != predicted_count)
        {
            cout << "2.Predicted count error!" << endl;
            exit(1);
        }
        
        bvect_min1.combine_xor(bvect_min2);
        CheckVectors(bvect_min1, bvect1, BITVECT_SIZE, true);
        CheckCountRange(bvect1, 0, BITVECT_SIZE);
    }


    {
        bvect  bvect1;
        bvect_mini  bvect_min1(BITVECT_SIZE);

        bvect  bvect2;
        bvect_mini  bvect_min2(BITVECT_SIZE);


        for (int i = 0; i < 150000; ++i)
        {
            bvect1.set_bit(i);
            bvect_min1.set_bit(i);
        }

        bvect1.optimize();
        
        bm::id_t predicted_count = bm::count_xor(bvect1, bvect2);

        bvect1.bit_xor(bvect2);

        bm::id_t count = bvect1.count();
        if (count != predicted_count)
        {
            cout << "3.Predicted count error!" << endl;
            exit(1);
        }
        
        bvect_min1.combine_xor(bvect_min2);
        CheckVectors(bvect_min1, bvect1, BITVECT_SIZE, true);
    }


    {
        bvect  bvect1;
        bvect_mini  bvect_min1(BITVECT_SIZE);

        bvect  bvect2;
        bvect_mini  bvect_min2(BITVECT_SIZE);


        for (int i = 0; i < 150000; ++i)
        {
            bvect1.set_bit(i);
            bvect_min1.set_bit(i);
            bvect2.set_bit(i);
            bvect_min2.set_bit(i);
        }

        bvect1.optimize();
        
        bm::id_t predicted_count = bm::count_xor(bvect1, bvect2);

        bvect1.bit_xor(bvect2);

        bm::id_t count = bvect1.count();
        if (count != predicted_count)
        {
            cout << "4.Predicted count error!" << endl;
            cout << count << " " << predicted_count << endl;
            
            exit(1);
        }
        
        bvect_min1.combine_xor(bvect_min2);
        CheckVectors(bvect_min1, bvect1, BITVECT_SIZE, true);
    }



    {

    bvect_mini   bvect_min1(BITVECT_SIZE);
    bvect_mini   bvect_min2(BITVECT_SIZE);
    bvect        bvect_full1;
    bvect        bvect_full2;

    bvect_full1.set_new_blocks_strat(bm::BM_GAP);
    bvect_full2.set_new_blocks_strat(bm::BM_GAP);

    printf("XOR test stage 2.\n");


    FillSets(&bvect_min1, &bvect_full1, 1, BITVECT_SIZE/7, 0);
    FillSets(&bvect_min2, &bvect_full2, 1, BITVECT_SIZE/7, 0);

    bvect_min1.combine_xor(bvect_min2);
    
    bm::id_t predicted_count = bm::count_xor(bvect_full1, bvect_full2);
    
    bvect_full1.bit_xor(bvect_full2);
    
    bm::id_t count = bvect_full1.count();
    if (count != predicted_count)
    {
        cout << "5.Predicted count error!" << endl;
        cout << count << " " << predicted_count << endl;
        bvect_full1.stat();
        exit(1);
    }

    CheckVectors(bvect_min1, bvect_full1, BITVECT_SIZE/10+10);
    CheckCountRange(bvect_full1, 0, BITVECT_SIZE/10+10);

    }

    {

    bvect_mini   bvect_min1(BITVECT_SIZE);
    bvect_mini   bvect_min2(BITVECT_SIZE);
    bvect        bvect_full1;
    bvect        bvect_full2;

    bvect_full1.set_new_blocks_strat(bm::BM_BIT);
    bvect_full2.set_new_blocks_strat(bm::BM_BIT);

    cout << "------------------------------" << endl;
    printf("XOR test stage 3.\n");


    FillSets(&bvect_min1, &bvect_full1, 1, BITVECT_SIZE/5, 2);
    FillSets(&bvect_min2, &bvect_full2, 1, BITVECT_SIZE/5, 2);

    bm::id_t predicted_count = bm::count_xor(bvect_full1, bvect_full2);

    bvect_min1.combine_xor(bvect_min2);

    bvect_full1.bit_xor(bvect_full2);

    bm::id_t count = bvect_full1.count();
    if (count != predicted_count)
    {
        cout << "6.Predicted count error!" << endl;
        exit(1);
    }


    CheckVectors(bvect_min1, bvect_full1, BITVECT_SIZE);

    bvect_full1.optimize();
    CheckVectors(bvect_min1, bvect_full1, BITVECT_SIZE);
    CheckCountRange(bvect_full1, 0, BITVECT_SIZE);


    }


    cout << "Testing combine_xor" << endl;
    
    {
    
    bvect        bvect_full1;
    bvect        bvect_full2;
    bvect_mini   bvect_min1(BITVECT_SIZE);
    
    bvect_full1.set_new_blocks_strat(bm::BM_GAP);
    bvect_full2.set_new_blocks_strat(bm::BM_GAP);

    unsigned ids[10000];
    unsigned to_add = 10000;
    
    unsigned bn = 0;
    for (unsigned i = 0; i < to_add; ++i)
    {
        ids[i] = bn;
        bvect_full2.set(bn);
        bvect_min1.set_bit(bn);
        bn += 15;
    }
    
    unsigned* first = ids;
    unsigned* last = ids + to_add;
    
    bm::combine_xor(bvect_full1, first, last);

    CheckVectors(bvect_min1, bvect_full1, BITVECT_SIZE);
    
    bm::combine_xor(bvect_full1, first, last);
    if (bvect_full1.count())
    {
        cout << "combine_xor count failed!" << endl;
        exit(1);
    }
    
    }

    {
    
    bvect        bvect_full1;
    bvect        bvect_full2;
    bvect_mini   bvect_min1(BITVECT_SIZE);
    
    bvect_full1.set_new_blocks_strat(bm::BM_GAP);
    bvect_full2.set_new_blocks_strat(bm::BM_GAP);

    unsigned ids[10000]={0,};
    unsigned to_add = 10000;
    
    for (unsigned i = 0; i < to_add; i+=100)
    {
        ids[i] = i;
        bvect_full2.set(i);
        bvect_min1.set_bit(i);
    }
    unsigned* first = ids;
    unsigned* last = ids + to_add;
    
    bm::combine_xor(bvect_full1, first, last);

    CheckVectors(bvect_min1, bvect_full1, BITVECT_SIZE);
    
    bm::combine_xor(bvect_full1, first, last);
    if (bvect_full1.count())
    {
        cout << "combine_xor count failed!" << endl;
        exit(1);
    }
    
    }

    
    {
    unsigned ids[] = {0, 65536, 65535, 65535*3, 65535*2, 10};
    unsigned to_add = sizeof(ids)/sizeof(unsigned);
    bvect        bvect_full1;
    bvect        bvect_full2;    
    bvect_mini   bvect_min1(BITVECT_SIZE);

    bvect_full1.set_new_blocks_strat(bm::BM_BIT);
    bvect_full2.set_new_blocks_strat(bm::BM_BIT);
    
    unsigned bn = 0;
    for (unsigned i = 0; i < to_add; ++i)
    {
        ids[i] = bn;
        bvect_full2.set(bn);
        bvect_min1.set_bit(bn);
        bn += 15;
    }
    
    unsigned* first = ids;
    unsigned* last = ids + to_add;
    
    bm::combine_xor(bvect_full1, first, last);
    CheckVectors(bvect_min1, bvect_full1, BITVECT_SIZE);

    bm::combine_xor(bvect_full1, first, last);
    if (bvect_full1.count())
    {
        cout << "combine_xor count failed!" << endl;
        exit(1);
    }
    }
    
    
    {
    unsigned ids[] = {0, 65536, 65535, 65535*3, 65535*2, 10};
    unsigned to_add = sizeof(ids)/sizeof(unsigned);
    bvect        bvect_full1;
    bvect        bvect_full2;    
    bvect_mini   bvect_min1(BITVECT_SIZE);

    bvect_full1.set_new_blocks_strat(bm::BM_GAP);
    bvect_full2.set_new_blocks_strat(bm::BM_GAP);
    
    unsigned bn = 0;
    for (unsigned i = 0; i < to_add; ++i)
    {
        ids[i] = bn;
        bvect_full2.set(bn);
        bvect_min1.set_bit(bn);
        bn += 15;
    }
    
    unsigned* first = ids;
    unsigned* last = ids + to_add;
    
    bm::combine_xor(bvect_full1, first, last);
    CheckVectors(bvect_min1, bvect_full1, BITVECT_SIZE);

    bm::combine_xor(bvect_full1, first, last);
    if (bvect_full1.count())
    {
        cout << "combine_xor count failed!" << endl;
        exit(1);
    }
    }

}


void ComparisonTest()
{
    cout << "-------------------------------------- ComparisonTest" << endl;

    bvect_mini   bvect_min1(BITVECT_SIZE);
    bvect_mini   bvect_min2(BITVECT_SIZE);
    bvect        bvect_full1;
    bvect        bvect_full2;
    int res1, res2;

    bvect_full1.set_bit(31); 
    bvect_full2.set_bit(63); 

    res1 = bvect_full1.compare(bvect_full2);
    if (res1 != 1)
    {
        printf("Comparison test failed 1\n");
        exit(1);
    }

    bvect_full1.clear();
    bvect_full2.clear();

    bvect_min1.set_bit(10);
    bvect_min2.set_bit(10);

    bvect_full1.set_bit(10);
    bvect_full2.set_bit(10);

    res1 = bvect_min1.compare(bvect_min2);
    res2 = bvect_full1.compare(bvect_full2);

    if (res1 != res2)
    {
        printf("Comparison test failed 1\n");
        exit(1);
    }

    printf("Comparison 2.\n");

    bvect_min1.set_bit(11);
    bvect_full1.set_bit(11);

    res1 = bvect_min1.compare(bvect_min2);
    res2 = bvect_full1.compare(bvect_full2);

    if (res1 != res2 && res1 != 1)
    {
        printf("Comparison test failed 2\n");
        exit(1);
    }

    res1 = bvect_min2.compare(bvect_min1);
    res2 = bvect_full2.compare(bvect_full1);

    if (res1 != res2 && res1 != -1)
    {
        printf("Comparison test failed 2.1\n");
        exit(1);
    }

    printf("Comparison 3.\n");

    bvect_full1.optimize();

    res1 = bvect_min1.compare(bvect_min2);
    res2 = bvect_full1.compare(bvect_full2);

    if (res1 != res2 && res1 != 1)
    {
        printf("Comparison test failed 3\n");
        exit(1);
    }

    res1 = bvect_min2.compare(bvect_min1);
    res2 = bvect_full2.compare(bvect_full1);

    if (res1 != res2 && res1 != -1)
    {
        printf("Comparison test failed 3.1\n");
        exit(1);
    }

    printf("Comparison 4.\n");

    bvect_full2.optimize();

    res1 = bvect_min1.compare(bvect_min2);
    res2 = bvect_full1.compare(bvect_full2);

    if (res1 != res2 && res1 != 1)
    {
        printf("Comparison test failed 4\n");
        exit(1);
    }

    res1 = bvect_min2.compare(bvect_min1);
    res2 = bvect_full2.compare(bvect_full1);

    if (res1 != res2 && res1 != -1)
    {
        printf("Comparison test failed 4.1\n");
        exit(1);
    }

    printf("Comparison 5.\n");

    unsigned i;
    for (i = 0; i < 65536; ++i)
    {
        bvect_full1.set_bit(i);
    }

    res1 = bvect_min1.compare(bvect_min2);
    res2 = bvect_full1.compare(bvect_full2);

    if (res1 != res2 && res1 != 1)
    {
        printf("Comparison test failed 5\n");
        exit(1);
    }

    bvect_full1.optimize();

    res1 = bvect_min2.compare(bvect_min1);
    res2 = bvect_full2.compare(bvect_full1);

    if (res1 != res2 && res1 != -1)
    {
        printf("Comparison test failed 5.1\n");
        exit(1);
    }

}

void DesrializationTest2()
{
   bvect  bvtotal;
   unsigned size = BITVECT_SIZE - 10;


   bvect  bv1;
   bvect  bv2;
   int i;
   for (i = 10; i < 165536; i+=2)
   {
      bv1.set_bit(i);
   }

   bv1.optimize();
   bv1.stat();

   struct bvect::statistics st1;
   bv1.calc_stat(&st1);


   unsigned char* sermem = new unsigned char[st1.max_serialize_mem];

   unsigned slen2 = bv1.serialize(sermem);
   assert(slen2);
   slen2 = 0;

   bvtotal.deserialize(sermem);
   bvtotal.optimize();

   for (i = 55000; i < 165536; ++i)
   {
      bv2.set_bit(i);
   }
   bv2.optimize();
   bv2.stat();

   struct bvect::statistics st2;
   bv2.calc_stat(&st2);

   unsigned char* sermem2 = new unsigned char[st2.max_serialize_mem];

   unsigned slen = bv2.serialize(sermem2);
   assert(slen);
   slen = 0;

   bvtotal.deserialize(sermem2);
   bvtotal.stat();

//   bvtotal.optimize();
 //  bvtotal.stat();

   bvtotal.deserialize(sermem2);

   bvtotal.deserialize(sermem);

   delete [] sermem;
   delete [] sermem2;


   bvtotal.clear();

   int clcnt = 0;

   int repetitions = 25;
   for (i = 0; i < repetitions; ++i)
   {
        cout << endl << "Deserialization STEP " << i << endl;

        bvect_mini*   bvect_min1= new bvect_mini(size);
        bvect*        bvect_full1= new bvect();

        FillSetsRandomMethod(bvect_min1, bvect_full1, 1, size, 1);

       struct bvect::statistics st;
       bvect_full1->calc_stat(&st);

       unsigned char* sermem = new unsigned char[st.max_serialize_mem];

       unsigned slen = bvect_full1->serialize(sermem);

       unsigned char* smem = new unsigned char[slen];
       ::memcpy(smem, sermem, slen);

//       cout << "Serialized vector" << endl;
//       bvect_full1->stat();

//       cout << "Before deserialization" << endl;
//       bvtotal.stat();
       bvtotal.deserialize(smem);

//       cout << "After deserialization" << endl;
//       bvtotal.stat();

       bvtotal.optimize();

//       cout << "After optimization" << endl;
//       bvtotal.stat();


       if (++clcnt == 5)
       {
          clcnt = 0;
          bvtotal.clear();

//          cout << "Post clear." << endl;
//          bvtotal.stat();

       }

       delete [] sermem;
       delete [] smem;
       delete bvect_min1;
       delete bvect_full1;

   } // for i

}


void StressTest(int repetitions)
{

   unsigned RatioSum = 0;
   unsigned SRatioSum = 0;
   unsigned DeltaSum = 0;
   unsigned SDeltaSum = 0;

   unsigned clear_count = 0;

   bvect  bvtotal;
   bvtotal.set_new_blocks_strat(bm::BM_GAP);


   cout << "----------------------------StressTest" << endl;

   unsigned size = BITVECT_SIZE - 10;
//size = BITVECT_SIZE / 10;
   int i;
   for (i = 0; i < repetitions; ++i)
   {
        cout << endl << " - - - - - - - - - - - - STRESS STEP " << i << endl;

        switch (rand() % 3)
        {
        case 0:
            size = BITVECT_SIZE / 10;
            break;
        case 1:
            size = BITVECT_SIZE / 2;
            break;
        default:
            size = BITVECT_SIZE - 10;
            break;
        } // switch


        bvect_mini*   bvect_min1= new bvect_mini(size);
        bvect_mini*   bvect_min2= new bvect_mini(size);
        bvect*        bvect_full1= new bvect();
        bvect*        bvect_full2= new bvect();

        bvect_full1->set_new_blocks_strat(i&1 ? bm::BM_GAP : bm::BM_BIT);
        bvect_full2->set_new_blocks_strat(i&1 ? bm::BM_GAP : bm::BM_BIT);

        int opt = rand() % 2;

        unsigned start1 = 0;

        switch (rand() % 3)
        {
        case 1:
            start1 += size / 5;
            break;
        default:
            break;
        }

        unsigned start2 = 0;

        switch (rand() % 3)
        {
        case 1:
            start2 += size / 5;
            break;
        default:
            break;
        }
/*
        if (i == 3)
        {
            g_cnt_check = 1;
        }
*/
        FillSetsRandomMethod(bvect_min1, bvect_full1, start1, size, opt);
        FillSetsRandomMethod(bvect_min2, bvect_full2, start2, size, opt);



        unsigned arr[bm::set_total_blocks]={0,};
        bm::id_t cnt = bvect_full1->count();
        unsigned last_block = bvect_full1->count_blocks(arr);
        unsigned sum = bm::sum_arr(&arr[0], &arr[last_block+1]);

        if (sum != cnt)
        {
            cout << "Error in function count_blocks." << endl;
            cout << "Array sum = " << sum << endl;
            cout << "BitCount = " << cnt << endl;
            cnt = bvect_full1->count();
            for (unsigned i = 0; i <= last_block; ++i)
            {
                if (arr[i])
                {
                    cout << "[" << i << ":" << arr[i] << "]";
                }
            }
            cout << endl;
            cout << "================" << endl;
            bvect_full1->stat();


            exit(1);
        }

        CheckCountRange(*bvect_full1, start1, BITVECT_SIZE, arr);
        CheckIntervals(*bvect_full1, BITVECT_SIZE);


        

        CheckCountRange(*bvect_full2, start2, BITVECT_SIZE);

        CheckCountRange(*bvect_full1, 0, start1, arr);
        CheckCountRange(*bvect_full2, 0, start2);


/*        
        cout << "!!!!!!!!!!!!!!!" << endl;
        CheckVectors(*bvect_min1, *bvect_full1, size);
        cout << "!!!!!!!!!!!!!!!" << endl;
        CheckVectors(*bvect_min2, *bvect_full2, size);
        cout << "!!!!!!!!!!!!!!!" << endl;
 
        
         bvect_full1->stat();
         cout << " --" << endl;
         bvect_full2->stat();
*/

        int operation = rand()%4;
//operation = 2;

        switch(operation)
        {
        case 0:
            cout << "Operation OR" << endl;
            bvect_min1->combine_or(*bvect_min2);
            break;

        case 1:
            cout << "Operation SUB" << endl;
            bvect_min1->combine_sub(*bvect_min2);
            break;

        case 2:
            cout << "Operation XOR" << endl;
            bvect_min1->combine_xor(*bvect_min2);
            break;

        default:
            cout << "Operation AND" << endl;
            bvect_min1->combine_and(*bvect_min2);
            break;
        }

        int cres1 = bvect_min1->compare(*bvect_min2);

        delete bvect_min2;

        switch(operation)
        {
        case 0:
            {
            cout << "Operation OR" << endl;

            bm::id_t predicted_count = bm::count_or(*bvect_full1, *bvect_full2);
            
            bvect_full1->bit_or(*bvect_full2);
            
            bm::id_t count = bvect_full1->count();

            if (count != predicted_count)
            {
                cout << "Predicted count error!" << endl;
                cout << "Count = " << count << "Predicted count = " << predicted_count << endl;
                exit(1);
            }
            
            }
            break;

        case 1:
            {
            cout << "Operation SUB" << endl;
            
            bm::id_t predicted_count = bm::count_sub(*bvect_full1, *bvect_full2);
            
            bvect_full1->bit_sub(*bvect_full2);
            
            bm::id_t count = bvect_full1->count();

            if (count != predicted_count)
            {
                cout << "Predicted count error!" << endl;
                cout << "Count = " << count << "Predicted count = " << predicted_count << endl;
                exit(1);
            }
            
            
            }
            break;

        case 2:
            {
            cout << "Operation XOR" << endl;
           
            bm::id_t predicted_count = bm::count_xor(*bvect_full1, *bvect_full2);
            
            bvect_full1->bit_xor(*bvect_full2);
            
            bm::id_t count = bvect_full1->count();

            if (count != predicted_count)
            {
                cout << "Predicted count error!" << endl;
                cout << "Count = " << count << "Predicted count = " << predicted_count << endl;
                exit(1);
            }
            
            }
            
            break;

        default:
            {
            cout << "Operation AND" << endl;

            bm::id_t predicted_count = bm::count_and(*bvect_full1, *bvect_full2);

            bvect_full1->bit_and(*bvect_full2);

            bm::id_t count = bvect_full1->count();

            if (count != predicted_count)
            {
                cout << "Predicted count error!" << endl;
                cout << "Count = " << count << "Predicted count = " << predicted_count << endl;
                exit(1);
            }

            }
            break;
        }

        cout << "Operation comparison" << endl;
        CheckVectors(*bvect_min1, *bvect_full1, size);

        int cres2 = bvect_full1->compare(*bvect_full2);

        CheckIntervals(*bvect_full1, BITVECT_SIZE);

        if (cres1 != cres2)
        {
            cout << cres1 << " " << cres2 << endl;
            cout << bvect_full1->get_first() << " " << bvect_full1->count() << endl;
            cout << bvect_full2->get_first() << " " << bvect_full2->count() << endl;

           // bvect_full1->stat(1000);
            cout << endl;
           // bvect_full2->stat(1000);
            printf("Bitset comparison operation failed.\n");
            exit(1);
        }

        delete bvect_full2;


        struct bvect::statistics st1;
        bvect_full1->calc_stat(&st1);
        bvect_full1->optimize();
        bvect_full1->optimize_gap_size();
        struct bvect::statistics st2;
        bvect_full1->calc_stat(&st2);

        unsigned Ratio = (st2.memory_used * 100)/st1.memory_used;
        RatioSum+=Ratio;
        DeltaSum+=st1.memory_used - st2.memory_used;

        cout << "Optimization statistics: " << endl  
             << "   MemUsedBefore=" << st1.memory_used
             << "   MemUsed=" << st2.memory_used 
             << "   Ratio=" << Ratio << "%"
             << "   Delta=" << st1.memory_used - st2.memory_used
             << endl;
                
        cout << "Optimization comparison" << endl;

        CheckVectors(*bvect_min1, *bvect_full1, size);

        bvect_full1->set_gap_levels(gap_len_table_min<true>::_len);
        CheckVectors(*bvect_min1, *bvect_full1, size);
        CheckIntervals(*bvect_full1, BITVECT_SIZE);

        //CheckCountRange(*bvect_full1, 0, size);


        // Serialization
        bvect_full1->calc_stat(&st2);

        cout << "Memory allocation: " << st2.max_serialize_mem << endl;
        unsigned char* sermem = new unsigned char[st2.max_serialize_mem];

//    bvect_full1->stat();

        cout << "Serialization...";
        unsigned slen = bvect_full1->serialize(sermem);
        cout << "Ok" << endl;

        delete bvect_full1;

        unsigned SRatio = (slen*100)/st2.memory_used;
        SRatioSum+=SRatio;
        SDeltaSum+=st2.memory_used - slen;


        cout << "Serialized mem_max = " << st2.max_serialize_mem 
             << " size= " << slen 
             << " Ratio=" << SRatio << "%"
             << " Delta=" << st2.memory_used - slen
             << endl;

        bvect*        bvect_full3= new bvect();
        unsigned char* new_sermem = new unsigned char[slen];
        memcpy(new_sermem, sermem, slen);
        delete [] sermem;

        cout << "Deserialization...";

        bvect_full3->deserialize(new_sermem);

        bvtotal.deserialize(new_sermem);

        cout << "Ok." << endl;
        delete [] new_sermem;

        cout << "Optimization...";
        bvtotal.optimize();
        cout << "Ok." << endl;

        ++clear_count;

        if (clear_count == 4)
        {
           bvtotal.clear();
           clear_count = 0;
        }

        cout << "Serialization comparison" << endl;
        CheckVectors(*bvect_min1, *bvect_full3, size, true);


        delete bvect_min1;
        delete bvect_full3;

    }

    --i;
    cout << "Repetitions:" << i <<
            " AVG optimization ratio:" << RatioSum/i 
         << " AVG Delta:" << DeltaSum/i
         << endl
         << " AVG serialization Ratio:"<< SRatioSum/i
         << " Delta:" << SDeltaSum/i
         << endl;
}

void GAPCheck()
{
   cout << "-------------------------------------------GAPCheck" << endl;

    {

    gap_vector   gapv(0);
    bvect_mini  bvect_min(bm::gap_max_bits);

    unsigned i;
    for( i = 0; i < 454; ++i)
    {
        bvect_min.set_bit(i);
        gapv.set_bit(i);
    }

    for(i = 0; i < 254; ++i)
    {
        bvect_min.clear_bit(i);
        gapv.clear_bit(i);
    }

    for(i = 5; i < 10; ++i)
    {
        bvect_min.set_bit(i);
        gapv.set_bit(i);
    }

    for( i = 0; i < bm::gap_max_bits; ++i)
    {
        int bit1 = (gapv.is_bit_true(i) == 1);
        int bit2 = (bvect_min.is_bit_true(i) != 0);
        int bit3 = (gapv.test(i) == 1);
        if (bit1 != bit2)
        {
            cout << "problem with bit comparison " << i << endl;
            exit(1);
        }
        if (bit1 != bit3)
        {
            cout << "problem with bit test comparison " << i << endl;
            exit(1);
        }

    }

    }


   {
   gap_vector gapv(1);
   int bit = gapv.is_bit_true(65535);

   if (bit != 1)
   {
      cout << "Bit != 1" << endl;
      exit(1);
   }
   
   int i;
   for (i = 0; i < 65536; ++i)
   {
        bit = gapv.is_bit_true(i);
        if (bit != 1)
        {
            cout << "2.Bit != 1" << endl;
            exit(1);
        }
   }
   unsigned cnt = gapv.count_range(0, 65535);
   if (cnt != 65536)
   {
       cout << "count_range failed:" << cnt << endl;
       exit(1);
   }
   
   CheckCountRange(gapv, 10, 20);
   CheckCountRange(gapv, 0, 20);

   printf("gapv 1 check ok\n");
   }

   {
   gap_vector gapv(0);


   int bit = gapv.is_bit_true(65535);
   int bit1 = gapv.test(65535);
   if(bit != 0)
   {
      cout << "Bit != 0" << endl;
      exit(1);
   }
      
   int i;
   for (i = 0; i < 65536; ++i)
   {
        bit = gapv.is_bit_true(i);
        bit1 = gapv.test(i);
        if (bit != 0)
        {
            cout << "2.Bit != 0 bit =" << i << endl;
            exit(1);
        }
        if (bit1 != 0)
        {
            cout << "2.Bit test != 0 bit =" << i << endl;
            exit(1);
        }
   }
   unsigned cnt = gapv.count_range(0, 65535);
   if (cnt != 0)
   {
       cout << "count_range failed:" << cnt << endl;
       exit(1);
   }
   CheckCountRange(gapv, 10, 20);
   CheckCountRange(gapv, 0, 20);

   printf("gapv 2 check ok\n");
   }

   {
   gap_vector gapv(0);

   gapv.set_bit(1);
   gapv.set_bit(0);

   gapv.control();
   CheckCountRange(gapv, 0, 20);

   int bit = gapv.is_bit_true(0);

   if (bit != 1)
   {
      cout << "Trouble" << endl;
      exit(1);
   }
   
   bit = gapv.is_bit_true(1);
   if (bit != 1)
   {
      cout << "Trouble 2." << endl;
      exit(1);
   }


   bit = gapv.is_bit_true(2);
   if(bit != 0)
   {
      cout << "Trouble 3." << endl;
      exit(1);
   }

   }

   {
   gap_vector gapv(0);

   gapv.set_bit(0);
   gapv.control();
   gapv.set_bit(1);
   gapv.control();

   gapv.set_bit(4);
   gapv.control();
   gapv.set_bit(5);
   gapv.control();
   CheckCountRange(gapv, 4, 5);
   CheckCountRange(gapv, 3, 5);

   gapv.set_bit(3);
   CheckCountRange(gapv, 3, 3);
   CheckCountRange(gapv, 3, 5);

   gapv.control();
   
   int bit = gapv.is_bit_true(0);
   if(bit!=1)
   {
      cout << "Bug" << endl;
   }
   bit = gapv.is_bit_true(1);
   if(bit!=1)
   {
      cout << "Bug2" << endl;
   }

   gapv.control();
   gapv.set_bit(4);
   gapv.control();


   printf("gapv 3 check ok\n");
   }

   {
        gap_vector gapv(0);
        bvect_mini   bvect_min(bm::gap_max_bits);
cout << "++++++1" << endl;
print_gap(gapv, 10);
        gapv.set_bit(bm::gap_max_bits-1);
        gapv.control();
        print_gap(gapv, 10);

//cout << "++++++2" << endl;
//cout << "m_buf=" << bvect_min.m_buf << endl;

        bvect_min.set_bit(bm::gap_max_bits-1);
cout << "++++++3" << endl;
        gapv.set_bit(5);
        print_gap(gapv,15);
        gapv.control();
        bvect_min.set_bit(5);
cout << "++++++4" << endl;

        CheckCountRange(gapv, 13, 150);
        gapv.control();

        unsigned i;
        for (i = 0; i < bm::gap_max_bits; ++i)
        {
            if (i == 65535)
                printf("%i\n", i);
            int bit1 = (gapv.is_bit_true(i) == 1);
            int bit2 = (bvect_min.is_bit_true(i) != 0);
            int bit3 = (gapv.test(i) == 1);
            if (bit1 != bit2)
            {
                cout << "problem with bit comparison " << i << endl;
            }
            if (bit1 != bit3)
            {
                cout << "problem with bit test comparison " << i << endl;
            }

        }

        gapv.clear_bit(5);
        bvect_min.clear_bit(5);
        gapv.control();

        for ( i = 0; i < bm::gap_max_bits; ++i)
        {
            if (i == 65535)
                printf("%i\n", i);
            int bit1 = (gapv.is_bit_true(i) == 1);
            int bit2 = (bvect_min.is_bit_true(i) != 0);
            int bit3 = (gapv.test(i) == 1);
            if (bit1 != bit2)
            {
                cout << "2.problem with bit comparison " << i << endl;
            }
            if (bit1 != bit3)
            {
                cout << "2.problem with bit test comparison " << i << endl;
            }
        }
   printf("gapv check 4 ok.\n");
   }

   {
        gap_vector gapv(0);
        bvect_mini   bvect_min(65536);
        
        int i;
        for (i = 10; i > 0; i-=2)
        {
            bvect_min.set_bit(i);
            gapv.set_bit(i);
            gapv.control();
            CheckCountRange(gapv, 0, i);

            int bit1 = (gapv.is_bit_true(i) == 1);
            int bit2 = (bvect_min.is_bit_true(i) != 0);
            int bit3 = (gapv.test(i) != 0);
            if (bit1 != bit2)
            {
                cout << "3.problem with bit comparison " << i << endl;
            }
            if (bit1 != bit3)
            {
                cout << "3.problem with bit test comparison " << i << endl;
            }

        }
        for (i = 0; i < (int)bm::gap_max_bits; ++i)
        {
            int bit1 = (gapv.is_bit_true(i) == 1);
            int bit2 = (bvect_min.is_bit_true(i) != 0);
            int bit3 = (gapv.test(i) == 1);

            if (bit1 != bit2)
            {
                cout << "3.problem with bit comparison " << i << endl;
            }
            if (bit1 != bit3)
            {
                cout << "3.problem with bit test comparison " << i << endl;
            }
        }
   printf("gapv check 5 ok.\n");
   }

   {
        gap_vector gapv(0);
        bvect_mini   bvect_min(bm::gap_max_bits);
        
        int i;
        for (i = 0; i < 25; ++i)
        {
            unsigned id = random_minmax(0, bm::gap_max_bits);
            bvect_min.set_bit(id);
            gapv.set_bit(id);
            gapv.control();
            CheckCountRange(gapv, 0, id);
            CheckCountRange(gapv, id, 65535);
        }

        for (i = 0; i < (int)bm::gap_max_bits; ++i)
        {
            int bit1 = (gapv.is_bit_true(i) == 1);
            int bit2 = (bvect_min.is_bit_true(i) != 0);
            if (bit1 != bit2)
            {
                cout << "4.problem with bit comparison " << i << endl;
            }
        }

        for (i = bm::gap_max_bits; i < 0; --i)
        {
            int bit1 = (gapv.is_bit_true(i) == 1);
            int bit2 = (bvect_min.is_bit_true(i) != 0);
            if (bit1 != bit2)
            {
                cout << "5.problem with bit comparison " << i << endl;
            }
        }
   printf("gapv check 6 ok.\n");

   }

   printf("gapv random bit set check ok.\n");


   // conversion functions test
   
   {
   // aligned position test
   bvect        bvect;

   bvect.set_bit(1);
   bvect.clear();


   unsigned* buf = (unsigned*) bvect.get_block(0);

   bm::or_bit_block(buf, 0, 4);
   unsigned cnt = bm::bit_block_calc_count_range(buf, 0, 3);
   assert(cnt == 4);
   
   bool bit = bvect.get_bit(0);
   assert(bit);
   bit = bvect.get_bit(1);
   assert(bit);
   bit = bvect.get_bit(2);
   assert(bit);
   bit = bvect.get_bit(3);
   assert(bit);
   bit = bvect.get_bit(4);
   assert(bit==0);

   bm::or_bit_block(buf, 0, 36); 
   cnt = bm::bit_block_calc_count_range(buf, 0, 35);
   assert(cnt == 36);

   for (int i = 0; i < 36; ++i)
   {
        bit = (bvect.get_bit(i) != 0);
        assert(bit);
   }
   bit = (bvect.get_bit(36) != 0);
   assert(bit==0);

   unsigned count = bvect.recalc_count();
   assert(count == 36);   
   
   cout << "Aligned position test ok." << endl; 

   }


   {
   // unaligned position test
   bvect   bvect;

   bvect.set_bit(0);
   bvect.clear();

   unsigned* buf = (unsigned*) bvect.get_block(0);

   bm::or_bit_block(buf, 5, 32);
   bool bit = (bvect.get_bit(4) != 0);
   assert(bit==0);
   unsigned cnt = bm::bit_block_calc_count_range(buf, 5, 5+32-1);
   assert(cnt == 32);
   cnt = bm::bit_block_calc_count_range(buf, 5, 5+32);
   assert(cnt == 32);

   int i;
   for (i = 5; i < 4 + 32; ++i)
   {
        bit = bvect.get_bit(i);
        assert(bit);
   }
   int count = bvect.recalc_count();
   assert(count==32);

   cout << "Unaligned position ok." << endl;

   } 

   // random test
   {
   cout << "random test" << endl;

   bvect   bvect;

   bvect.set_bit(0);
   bvect.clear();

   unsigned* buf = (unsigned*) bvect.get_block(0);
   for (int i = 0; i < 5000; ++i)
   {
        unsigned start = rand() % 65535;
        unsigned end = rand() % 65535;
        if (start > end)
        {
            unsigned tmp = end;
            end = start;
            start = tmp;
        }
        unsigned len = end - start;
        if (len)
        {
           bm::or_bit_block(buf, start, len);
           unsigned cnt = bm::bit_block_calc_count_range(buf, start, end);
           if (cnt != len)
           {
            cout << "random test: count_range comparison failed. " 
                 << " LEN = " << len << " cnt = " << cnt
                 << endl;
                 exit(1);
           }

           unsigned count = bvect.recalc_count();

           if (count != len)
           {
            cout << "random test: count comparison failed. " 
                 << " LEN = " << len << " count = " << count
                 << endl;
                 exit(1);
           }            

           for (unsigned j = start; j < end; ++j)
           {
                bool bit = bvect.get_bit(j);
                if (!bit)
                {
                    cout << "random test: bit comparison failed. bit#" 
                         << i << endl;
                    exit(1);
                } 
           } // for j

        } 
        bvect.clear();

        if ((i % 100)==0)
        {
            cout << "*" << flush;
        }
   } // for i

   cout << endl << "Random test Ok." << endl;

   }


   // conversion test
 
   cout << "Conversion test" << endl;
    
   {
   
   gap_vector gapv(0);
   bvect   bvect;

   gapv.set_bit(0);
   gapv.set_bit(2);
   gapv.set_bit(10);
   gapv.set_bit(11);
   gapv.set_bit(12);
   
   CheckCountRange(gapv, 3, 15);

   print_gap(gapv, 100);
   bvect.set_bit(0);
   bvect.clear();

   unsigned* buf = (unsigned*) bvect.get_block(0);


   gapv.convert_to_bitset(buf);


   unsigned bitcount = bvect.recalc_count();


   if (bitcount != 5)
   {
      cout << "test failed: bitcout = " << bitcount << endl;
      exit(1);
   }


   gap_vector gapv1(0);
   gap_word_t* gap_buf = gapv1.get_buf();
   *gap_buf = 0;
   bit_convert_to_gap(gap_buf, buf, bm::gap_max_bits, bm::gap_max_buff_len);
   print_gap(gapv1, 100);

   bitcount = gapv1.bit_count();
   if(bitcount != 5)
   {
      cout << "2.test_failed: bitcout = " << bitcount << endl;
      exit(1);
   }

   printf("conversion test ok.\n");
    
   }

   // gap AND test

   {
   // special case 1: operand is all 1
   gap_vector gapv1(0);
   gapv1.set_bit(2);
   gap_vector gapv2(1); 

   gapv1.combine_and(gapv2.get_buf());
   gapv1.control();
   print_gap(gapv1, 0);

   int count = gapv1.bit_count();
   assert(count == 1);
   int bit = gapv1.is_bit_true(2);
   if(bit == 0)
   {
      cout << "Wrong bit" << endl;
      exit(1);
   }
   CheckCountRange(gapv1, 0, 17);

   }

   {
   // special case 2: src is all 1
   gap_vector gapv1(1);
   gap_vector gapv2(0); 
   gapv2.set_bit(2);

   gapv1.combine_and(gapv2.get_buf());
   gapv1.control();
   print_gap(gapv1, 0);

   int count = gapv1.bit_count();
   assert(count == 1);
   int bit = gapv1.is_bit_true(2);
   assert(bit);

   }

   {
   gap_vector gapv;
   gap_vector gapv1(0);

   gapv1.set_bit(3);
   gapv1.set_bit(4);
   print_gap(gapv1, 0);

   gap_vector gapv2(0); 
   gapv2.set_bit(2);
   gapv2.set_bit(3);
   print_gap(gapv2, 0);

   bm::gap_buff_op((gap_word_t*)gapv.get_buf(), 
                         gapv1.get_buf(), 0,
                         gapv2.get_buf(), 0, bm::and_op); 
   print_gap(gapv, 0);
   gapv.control();


    int bit1 = (gapv.is_bit_true(3) == 1);
    if(bit1 == 0)
    {
       cout << "Checking failed." << endl;
       exit(0);
    }

   gapv1.combine_or(gapv2);
   print_gap(gapv1, 0);
   gapv1.control();

   }

   {
        printf("gap AND test 1.\n");
        gap_vector gapv1(0);
        gap_vector gapv2(0);
        bvect_mini   bvect_min1(65536);
        bvect_mini   bvect_min2(65536);

        gapv1.set_bit(65535);
        bvect_min1.set_bit(65535);
        gapv1.set_bit(4);
        bvect_min1.set_bit(4);

        gapv2.set_bit(65535);
        bvect_min2.set_bit(65535);
        gapv2.set_bit(3);
        bvect_min2.set_bit(3);
        CheckCountRange(gapv2, 3, 65535);

        gapv2.control();

        printf("vect1:"); print_gap(gapv1, 0);
        printf("vect2:");print_gap(gapv2, 0);

        gapv1.combine_and(gapv2.get_buf());
        printf("vect1:");print_gap(gapv1, 0);

        gapv1.control();
        unsigned bit1 = gapv1.is_bit_true(65535);
        assert(bit1);

        bvect_min1.combine_and(bvect_min2);
        CheckGAPMin(gapv1, bvect_min1, bm::gap_max_bits);
   }

   {
        printf("gap random AND test.\n");
        gap_vector gapv1(0);
        gap_vector gapv2(0);
        bvect_mini   bvect_min1(65536);
        bvect_mini   bvect_min2(65536);
        
        int i;
        for (i = 0; i < 25; ++i)
        {
            unsigned id = random_minmax(0, 65535);
            bvect_min1.set_bit(id);
            gapv1.set_bit(id);
            gapv1.control();
            CheckCountRange(gapv1, 0, id);
            CheckCountRange(gapv1, id, 65535);
        }
        for (i = 0; i < 25; ++i)
        {
            unsigned id = random_minmax(0, 65535);
            bvect_min2.set_bit(id);
            gapv2.set_bit(id);
            gapv2.control();
        }

        gapv1.combine_and(gapv2.get_buf());
        gapv1.control();
        gapv2.control();
        bvect_min1.combine_and(bvect_min2);

        CheckGAPMin(gapv1, bvect_min1, bm::gap_max_bits);

        printf("gap random AND test ok.\n");

   }

   {
        printf("gap OR test.\n");

        gap_vector gapv1(0);
        gap_vector gapv2(0);

        gapv1.set_bit(2);
        gapv2.set_bit(3);

        gapv1.combine_or(gapv2);
        gapv1.control();
        print_gap(gapv1, 0);   
        int bit1 = (gapv1.is_bit_true(0) == 1);
        assert(bit1==0);
        bit1=(gapv1.is_bit_true(2) == 1);
        assert(bit1);
        bit1=(gapv1.is_bit_true(3) == 1);
        assert(bit1);
   }

   {
        printf("gap XOR test.\n");

        gap_vector gapv1(0);
        gap_vector gapv2(0);

        gapv1.set_bit(2);
        gapv2.set_bit(3);
        gapv1.set_bit(4);
        gapv2.set_bit(4);
        print_gap(gapv1, 0);   
        print_gap(gapv2, 0);   

        gapv1.combine_xor(gapv2);
        gapv1.control();
        print_gap(gapv1, 0);   
        int bit1 = (gapv1.is_bit_true(0) == 0);
        assert(bit1);
        bit1=(gapv1.is_bit_true(2) == 1);
        assert(bit1);
        bit1=(gapv1.is_bit_true(3) == 1);
        assert(bit1);
        bit1=(gapv1.is_bit_true(4) == 0);
        assert(bit1);

   }


   {
        int i;
        printf("gap random OR test.\n");
        gap_vector gapv1(0);
        gap_vector gapv2(0);
        bvect_mini   bvect_min1(bm::gap_max_bits);
        bvect_mini   bvect_min2(bm::gap_max_bits);
        
        for (i = 0; i < 10; ++i)
        {
            unsigned id = random_minmax(0, 100);
            bvect_min1.set_bit(id);
            gapv1.set_bit(id);
            gapv1.control();
        }
        for (i = 0; i < 10; ++i)
        {
            unsigned id = random_minmax(0, 100);
            bvect_min2.set_bit(id);
            gapv2.set_bit(id);
            gapv2.control();
        }

        print_mv(bvect_min1, 64);
        print_mv(bvect_min2, 64);

        gapv1.combine_or(gapv2);
        gapv1.control();
        gapv2.control();
        bvect_min1.combine_or(bvect_min2);

        print_mv(bvect_min1, 64);

        CheckGAPMin(gapv1, bvect_min1, bm::gap_max_bits);

        printf("gap random OR test ok.\n");

   }


   {
        int i;
        printf("gap random SUB test.\n");
        gap_vector gapv1(0);
        gap_vector gapv2(0);
        bvect_mini   bvect_min1(bm::gap_max_bits);
        bvect_mini   bvect_min2(bm::gap_max_bits);
        
        for (i = 0; i < 25; ++i)
        {
            unsigned id = random_minmax(0, 100);
            bvect_min1.set_bit(id);
            gapv1.set_bit(id);
            gapv1.control();
        }
        for (i = 0; i < 25; ++i)
        {
            unsigned id = random_minmax(0, 100);
            bvect_min2.set_bit(id);
            gapv2.set_bit(id);
            gapv2.control();
        }

        print_mv(bvect_min1, 64);
        print_mv(bvect_min2, 64);

        gapv1.combine_sub(gapv2);
        gapv1.control();
        gapv2.control();
        bvect_min1.combine_sub(bvect_min2);

        print_mv(bvect_min1, 64);

        CheckGAPMin(gapv1, bvect_min1, bm::gap_max_bits);

        printf("gap random SUB test ok.\n");
   }

   {
       printf("GAP comparison test.\n");

       gap_vector gapv1(0);
       gap_vector gapv2(0);

       gapv1.set_bit(3);
       gapv2.set_bit(3);

       int res = gapv1.compare(gapv2);
       if (res != 0)
       {
           printf("GAP comparison failed!");
           exit(1);
       }

       gapv1.set_bit(4);
       gapv2.set_bit(4);

       res = gapv1.compare(gapv2);
       if (res != 0)
       {
           printf("GAP comparison failed!");
           exit(1);
       }

       gapv1.set_bit(0);
       gapv1.set_bit(1);

       res = gapv1.compare(gapv2);
       if (res != 1)
       {
           printf("GAP comparison failed!");
           exit(1);
       }

       gapv2.set_bit(0);
       gapv2.set_bit(1);
       res = gapv1.compare(gapv2);
       if (res != 0)
       {
           printf("GAP comparison failed!");
           exit(1);
       }

       gapv1.clear_bit(1);

       res = gapv1.compare(gapv2);
       if (res != -1)
       {
           printf("GAP comparison failed!");
           exit(1);
       }


   }


}

// -----------------------------------------------------------------------------

void MutationTest()
{

    cout << "--------------------------------- MutationTest" << endl;

    bvect_mini     bvect_min(BITVECT_SIZE);
    bvect          bvect_full;

    printf("\nMutation test.\n");

    bvect_full.set_new_blocks_strat(bm::BM_GAP);

    bvect_full.set_bit(5);
    bvect_full.set_bit(5);

    bvect_min.set_bit(5);

    bvect_full.set_bit(65535);
    bvect_full.set_bit(65537);
    bvect_min.set_bit(65535);
    bvect_min.set_bit(65537);

    bvect_min.set_bit(100000);
    bvect_full.set_bit(100000);

bvect_full.stat();
    // detailed vectors verification
    ::CheckVectors(bvect_min, bvect_full, ITERATIONS, false);

    int i;
    for (i = 5; i < 20000; i+=3)
    {
        bvect_min.set_bit(i);
        bvect_full.set_bit(i);
    }
bvect_full.stat();
    ::CheckVectors(bvect_min, bvect_full, ITERATIONS, false);

    for (i = 100000; i < 200000; i+=3)
    {
        bvect_min.set_bit(i);
        bvect_full.set_bit(i);
    }

    ::CheckVectors(bvect_min, bvect_full, 300000);

    // set-clear functionality

    {
        printf("Set-clear functionality test.");

        bvect_mini     bvect_min(BITVECT_SIZE);
        bvect          bvect_full;
        bvect_full.set_new_blocks_strat(bm::BM_GAP);

        int i;
        for (i = 100000; i < 100010; ++i)
        {
            bvect_min.set_bit(i);
            bvect_full.set_bit(i);            
        }
        ::CheckVectors(bvect_min, bvect_full, 300000);

        for (i = 100000; i < 100010; ++i)
        {
            bvect_min.clear_bit(i);
            bvect_full.clear_bit(i);            
        }
        ::CheckVectors(bvect_min, bvect_full, 300000);
        
        bvect_full.optimize();
        CheckVectors(bvect_min, bvect_full, 65536);//max+10);
    }

}

void MutationOperationsTest()
{

   cout << "------------------------------------ MutationOperationsTest" << endl;

   printf("Mutation operations test 1.\n");
   {
    bvect_mini   bvect_min1(BITVECT_SIZE);
    bvect_mini   bvect_min2(BITVECT_SIZE);
    bvect        bvect_full1;
    bvect        bvect_full2;

    bvect_full1.set_new_blocks_strat(bm::BM_GAP);
    bvect_full2.set_new_blocks_strat(bm::BM_BIT);

    bvect_full1.set_bit(100);
    bvect_min1.set_bit(100);

    int i;
    for(i = 0; i < 10000; i+=2)
    {
       bvect_full2.set_bit(i);
       bvect_min2.set_bit(i);
    }
    bvect_full2.stat();
    CheckVectors(bvect_min2, bvect_full2, 65536, true);
    
    bvect_min1.combine_and(bvect_min2);
    bvect_full1.bit_and(bvect_full2);

    CheckVectors(bvect_min1, bvect_full1, 65536);//max+10);

   }

   printf("Mutation operations test 2.\n");
   {
    unsigned delta = 65536;
    bvect_mini   bvect_min1(BITVECT_SIZE);
    bvect_mini   bvect_min2(BITVECT_SIZE);
    bvect        bvect_full1;
    bvect        bvect_full2;

    bvect_full1.set_new_blocks_strat(bm::BM_GAP);
    bvect_full2.set_new_blocks_strat(bm::BM_GAP);

    int i;
    for(i = 0; i < 1000; i+=1)
    {
       bvect_full1.set_bit(delta+i);
       bvect_min1.set_bit(delta+i);
    }

    for(i = 0; i < 100; i+=2)
    {
       bvect_full2.set_bit(delta+i);
       bvect_min2.set_bit(delta+i);
    }
//    CheckVectors(bvect_min2, bvect_full2, 65536);
    
    bvect_min1.combine_and(bvect_min2);
    bvect_full1.bit_and(bvect_full2);

    CheckVectors(bvect_min1, bvect_full1, 65536);//max+10);
    bvect_full1.optimize();
    CheckVectors(bvect_min1, bvect_full1, 65536);//max+10);

   }

   {
    bvect_mini   bvect_min1(BITVECT_SIZE);
    bvect        bvect_full1;

    bvect_full1.set_bit(3);
    bvect_min1.set_bit(3);

    struct bvect::statistics st;
    bvect_full1.calc_stat(&st);

    // serialization

    unsigned char* sermem = new unsigned char[st.max_serialize_mem];
    unsigned slen = bvect_full1.serialize(sermem);
    cout << "BVECTOR SERMEM=" << slen << endl;


    bvect        bvect_full3;
    bvect_full3.deserialize(sermem);
    bvect_full3.stat();
    CheckVectors(bvect_min1, bvect_full3, 100, true);
   }


   printf("Mutation operations test 3.\n");
   {
    bvect_mini   bvect_min1(BITVECT_SIZE);
    bvect_mini   bvect_min2(BITVECT_SIZE);
    bvect        bvect_full1;
    bvect        bvect_full2;

    bvect_full1.set_new_blocks_strat(bm::BM_GAP);
    bvect_full2.set_new_blocks_strat(bm::BM_GAP);

   
    unsigned min = BITVECT_SIZE / 2 - ITERATIONS;
    unsigned max = BITVECT_SIZE / 2 + ITERATIONS;
    if (max > BITVECT_SIZE) 
        max = BITVECT_SIZE - 1;

    unsigned len = max - min;

    FillSets(&bvect_min1, &bvect_full1, min, max, 0);
    FillSets(&bvect_min1, &bvect_full1, 0, len, 5);
    printf("Bvect_FULL 1 STAT\n");
    bvect_full1.stat();
//    CheckVectors(bvect_min1, bvect_full1, max+10, false);
    FillSets(&bvect_min2, &bvect_full2, min, max, 0);
    FillSets(&bvect_min2, &bvect_full2, 0, len, 0);
    printf("Bvect_FULL 2 STAT\n");
    bvect_full2.stat();
//    CheckVectors(bvect_min2, bvect_full2, max+10);
    

    bvect_min1.combine_and(bvect_min2);
    bvect_full1.bit_and(bvect_full2);
    printf("Bvect_FULL 1 STAT after AND\n");
    bvect_full1.stat();

    CheckVectors(bvect_min1, bvect_full1, max+10, false);

    struct bvect::statistics st;
    bvect_full1.calc_stat(&st);
    cout << "BVECTOR: GAP=" << st.gap_blocks << " BIT=" << st.bit_blocks 
         << " MEM=" << st.memory_used << " SERMAX=" << st.max_serialize_mem
         << endl;
    cout << "MINIVECT: " << bvect_min1.mem_used() << endl;

    bvect_full1.optimize();
    bvect_full1.stat();

    CheckVectors(bvect_min1, bvect_full1, max+10, false);

    bvect_full1.calc_stat(&st);
    cout << "BVECTOR: GAP=" << st.gap_blocks << " BIT=" << st.bit_blocks 
         << " MEM=" << st.memory_used << " SERMAX=" << st.max_serialize_mem
         << endl;
    cout << "MINIVECT: " << bvect_min1.mem_used() << endl;



    // serialization

    unsigned char* sermem = new unsigned char[st.max_serialize_mem];
    unsigned slen = bvect_full1.serialize(sermem);
    cout << "BVECTOR SERMEM=" << slen << endl;


    
    bvect        bvect_full3;
    bvect_full3.deserialize(sermem);
    bvect_full3.stat();
    CheckVectors(bvect_min1, bvect_full3, max+10, true);
    
    delete [] sermem;
    

    cout << "Copy constructor check." << endl;


    {
    bvect       bvect_full4(bvect_full3);
    bvect_full3.stat();
    CheckVectors(bvect_min1, bvect_full4, max+10, true);
    }
    

   }

}


void SerializationTest()
{

   cout << " ----------------------------------- SerializationTest" << endl;

   cout << "Serialization STEP 0" << endl;

   {
    unsigned size = BITVECT_SIZE/6000;


    bvect_mini*   bvect_min1= new bvect_mini(BITVECT_SIZE);
    bvect*        bvect_full1= new bvect();
    bvect*        bvect_full2= new bvect();

    bvect_full1->set_new_blocks_strat(bm::BM_BIT);
    bvect_full2->set_new_blocks_strat(bm::BM_BIT);

    for(unsigned i = 0; i < size; ++i)
    {
        bvect_full1->set_bit(i);
        bvect_min1->set_bit(i);
    }

    bvect_full1->optimize();
    CheckVectors(*bvect_min1, *bvect_full1, size, true);



    bvect::statistics st;
    bvect_full1->calc_stat(&st);
    unsigned char* sermem = new unsigned char[st.max_serialize_mem];
    unsigned slen = bvect_full1->serialize(sermem);
    cout << "Serialized mem_max = " << st.max_serialize_mem 
         << " size= " << slen 
         << " Ratio=" << (slen*100)/st.max_serialize_mem << "%"
         << endl;

    bvect_full2->deserialize(sermem);
    delete [] sermem;


    CheckVectors(*bvect_min1, *bvect_full2, size, true);


    delete bvect_full2;
    delete bvect_min1;
    delete bvect_full1;

    }


   {
    unsigned size = BITVECT_SIZE/6000;


    bvect_mini*   bvect_min1= new bvect_mini(BITVECT_SIZE);
    bvect*        bvect_full1= new bvect();
    bvect*        bvect_full2= new bvect();

    bvect_full1->set_new_blocks_strat(bm::BM_BIT);
    bvect_full2->set_new_blocks_strat(bm::BM_BIT);

        bvect_full1->set_bit(131072);
        bvect_min1->set_bit(131072);
    

    bvect_full1->optimize();

    bvect::statistics st;
    bvect_full1->calc_stat(&st);
    unsigned char* sermem = new unsigned char[st.max_serialize_mem];
    unsigned slen = bvect_full1->serialize(sermem);
    cout << "Serialized mem_max = " << st.max_serialize_mem 
         << " size= " << slen 
         << " Ratio=" << (slen*100)/st.max_serialize_mem << "%"
         << endl;

    bvect_full2->deserialize(sermem);
    delete [] sermem;

    CheckVectors(*bvect_min1, *bvect_full2, size, true);

    delete bvect_full2;
    delete bvect_min1;
    delete bvect_full1;

    }


    cout << "Serialization STEP 1." << endl;

    {
    bvect_mini   bvect_min1(BITVECT_SIZE);
    bvect        bvect_full1;

    bvect_full1.set_new_blocks_strat(bm::BM_GAP);
   
    unsigned min = BITVECT_SIZE / 2 - ITERATIONS;
    unsigned max = BITVECT_SIZE / 2 + ITERATIONS;
    if (max > BITVECT_SIZE) 
        max = BITVECT_SIZE - 1;

    unsigned len = max - min;

    FillSets(&bvect_min1, &bvect_full1, min, max, 0);
    FillSets(&bvect_min1, &bvect_full1, 0, len, 5);

    // shot some random bits

    int i;
    for (i = 0; i < 10000; ++i)
    {
        unsigned bit = rand() % BITVECT_SIZE;
        bvect_full1.set_bit(bit);
        bvect_min1.set_bit(bit);
    }

    bvect::statistics st;
    bvect_full1.calc_stat(&st);

    unsigned char* sermem = new unsigned char[st.max_serialize_mem];
    bvect_full1.stat();
    
    unsigned slen = bvect_full1.serialize(sermem);

    cout << "Serialized len = " << slen << endl;

    bvect        bvect_full3;
    bvect_full3.deserialize(sermem);
    CheckVectors(bvect_min1, bvect_full3, max+10, true);

    delete [] sermem;

    }


   cout << "Stage 2" << endl;

   {

    bvect_mini*   bvect_min1= new bvect_mini(BITVECT_SIZE);
//    bm::bvect_mini*   bvect_min2= new bm::bvect_mini(BITVECT_SIZE);
    bvect*        bvect_full1= new bvect();
    bvect*        bvect_full2= new bvect();

    bvect_full1->set_new_blocks_strat(bm::BM_GAP);
    bvect_full2->set_new_blocks_strat(bm::BM_GAP);

    FillSetsRandomMethod(bvect_min1, bvect_full1, 1, BITVECT_SIZE-10, 1);
//    FillSetsRandomMethod(bvect_min2, bvect_full2, 1, BITVECT_SIZE-10, 1);

//bvect_full1->stat();
cout << "Filling. OK." << endl;
    bvect::statistics st;
    bvect_full1->calc_stat(&st);
cout << st.max_serialize_mem << endl;
    unsigned char* sermem = new unsigned char[st.max_serialize_mem];
cout << "Serialization" << endl;
    unsigned slen = bvect_full1->serialize(sermem);

    cout << "Serialized mem_max = " << st.max_serialize_mem 
         << " size= " << slen 
         << " Ratio=" << (slen*100)/st.max_serialize_mem << "%"
         << endl;
cout << "Deserialization" << endl;
    bvect_full2->deserialize(sermem);
cout << "Deserialization ok" << endl;

    CheckVectors(*bvect_min1, *bvect_full2, BITVECT_SIZE, true);

    delete [] sermem;


    delete bvect_full2;
    delete bvect_min1;
    delete bvect_full1;

    }



   cout << "Stage 3" << endl;

   {

    bvect_mini*   bvect_min1= new bvect_mini(BITVECT_SIZE);
    bvect_mini*   bvect_min2= new bvect_mini(BITVECT_SIZE);
    bvect*        bvect_full1= new bvect();
    bvect*        bvect_full2= new bvect();

    bvect_full1->set_new_blocks_strat(bm::BM_GAP);
    bvect_full2->set_new_blocks_strat(bm::BM_GAP);


    FillSetsRandomMethod(bvect_min1, bvect_full1, 1, BITVECT_SIZE-10, 1);
    FillSetsRandomMethod(bvect_min2, bvect_full2, 1, BITVECT_SIZE-10, 1);

    bvect::statistics st;
    bvect_full1->calc_stat(&st);
    unsigned char* sermem = new unsigned char[st.max_serialize_mem];
    unsigned slen = bvect_full1->serialize(sermem);
    delete bvect_full1;

    cout << "Serialized mem_max = " << st.max_serialize_mem 
         << " size= " << slen 
         << " Ratio=" << (slen*100)/st.max_serialize_mem << "%"
         << endl;
    bvect_full2->deserialize(sermem);
    delete [] sermem;
    
    bvect_min2->combine_or(*bvect_min1);
    delete bvect_min1;

    CheckVectors(*bvect_min2, *bvect_full2, BITVECT_SIZE, true);


    delete bvect_full2;
    delete bvect_min2;    


    }

   cout << "Stage 4. " << endl;

   {
    unsigned size = BITVECT_SIZE/3;


    bvect_mini*   bvect_min1= new bvect_mini(BITVECT_SIZE);
    bvect*        bvect_full1= new bvect();
    bvect*        bvect_full2= new bvect();

    bvect_full1->set_new_blocks_strat(bm::BM_BIT);
    bvect_full2->set_new_blocks_strat(bm::BM_BIT);

    unsigned i;
    for(i = 0; i < 65000; ++i)
    {
        bvect_full1->set_bit(i);
        bvect_min1->set_bit(i);
    }

    for(i = 65536; i < 65536+65000; ++i)
    {
        bvect_full1->set_bit(i);
        bvect_min1->set_bit(i);
    }

    for (i = 65536*2; i < size/6; ++i)
    {
        bvect_full1->set_bit(i);
        bvect_min1->set_bit(i);
    }


    bvect_full1->optimize();

    bvect_full1->stat();


    bvect::statistics st;
    bvect_full1->calc_stat(&st);
    unsigned char* sermem = new unsigned char[st.max_serialize_mem];
    unsigned slen = bvect_full1->serialize(sermem);
    cout << "Serialized mem_max = " << st.max_serialize_mem 
         << " size= " << slen 
         << " Ratio=" << (slen*100)/st.max_serialize_mem << "%"
         << endl;
    
    unsigned char* new_sermem = new unsigned char[st.max_serialize_mem];
    ::memcpy(new_sermem, sermem, slen);

    bvect_full2->deserialize(new_sermem);
    delete [] sermem;
    delete [] new_sermem;

    CheckVectors(*bvect_min1, *bvect_full2, size, true);


    delete bvect_full2;
    delete bvect_min1;
    delete bvect_full1;

    }


}

void GetNextTest()
{
   cout << "-------------------------------------------- GetNextTest" << endl;

   int i;
   for(i = 0; i < 2; ++i)
   {
      cout << "Strategy " << i << endl;

   {
      bvect       bvect_full1;
      bvect_mini  bvect_min1(BITVECT_SIZE);

      bvect_full1.set_new_blocks_strat(i ? bm::BM_GAP : bm::BM_BIT);

      bvect_full1.set_bit(0);
      bvect_min1.set_bit(0);


      bvect_full1.set_bit(65536);
      bvect_min1.set_bit(65536);

      unsigned nbit1 = bvect_full1.get_first();
      unsigned nbit2 = bvect_min1.get_first();

      if (nbit1 != nbit2)
      {
         cout << "1. get_first failed() " <<  nbit1 << " " << nbit2 << endl;
         exit(1);
      }
      nbit1 = bvect_full1.get_next(nbit1);
      nbit2 = bvect_min1.get_next(nbit2);
      if ((nbit1 != nbit2) || (nbit1 != 65536))
      {
         cout << "1. get_next failed() " <<  nbit1 << " " << nbit2 << endl;
         exit(1);
      }
   }



   {
      bvect       bvect_full1;
      bvect_mini  bvect_min1(BITVECT_SIZE);
      bvect_full1.set_new_blocks_strat(i ? bm::BM_GAP : bm::BM_BIT);

      bvect_full1.set_bit(65535);
      bvect_min1.set_bit(65535);

      unsigned nbit1 = bvect_full1.get_first();
      unsigned nbit2 = bvect_min1.get_first();

      if ((nbit1 != nbit2) || (nbit1 != 65535))
      {
         cout << "1. get_first failed() " <<  nbit1 << " " << nbit2 << endl;
         exit(1);
      }
      nbit1 = bvect_full1.get_next(nbit1);
      nbit2 = bvect_min1.get_next(nbit2);
      if (nbit1 != nbit2 )
      {
         cout << "1. get_next failed() " <<  nbit1 << " " << nbit2 << endl;
         exit(1);
      }
   }

   {
      cout << "--------------" << endl;
      bvect       bvect_full1;
      bvect_mini  bvect_min1(BITVECT_SIZE);
      bvect_full1.set_new_blocks_strat(i ? bm::BM_GAP : bm::BM_BIT);

      bvect_full1.set_bit(655350);
      bvect_min1.set_bit(655350);

      unsigned nbit1 = bvect_full1.get_first();
      unsigned nbit2 = bvect_min1.get_first();

      if (nbit1 != nbit2 || nbit1 != 655350)
      {
         cout << "1. get_first failed() " <<  nbit1 << " " << nbit2 << endl;
         exit(1);
      }

      nbit1 = bvect_full1.get_next(nbit1);
      nbit2 = bvect_min1.get_next(nbit2);
      if (nbit1 != nbit2)
      {
         cout << "1. get_next failed() " <<  nbit1 << " " << nbit2 << endl;
         exit(1);
      }
   }


   {
   bvect       bvect_full1;
   bvect_mini  bvect_min1(BITVECT_SIZE);

   bvect_full1.set_new_blocks_strat(i ? bm::BM_GAP : bm::BM_BIT);

   bvect_full1.set_bit(256);
   bvect_min1.set_bit(256);

//   bvect_full1.clear_bit(256);
   bvect_full1.set_bit(65536);
   bvect_min1.set_bit(65536);

   unsigned nbit1 = bvect_full1.get_first();
   unsigned nbit2 = bvect_min1.get_first();

   if (nbit1 != nbit2)
   {
      cout << "get_first failed " <<  nbit1 << " " << nbit2 << endl;
      exit(1);
   }

   while (nbit1)
   {
      cout << nbit1 << endl;
      nbit1 = bvect_full1.get_next(nbit1);
      nbit2 = bvect_min1.get_next(nbit2);
      if (nbit1 != nbit2)
      {
         cout << "get_next failed " <<  nbit1 << " " << nbit2 << endl;
         exit(1);
      }

   } // while

   }

   
   }// for

}

// Test contributed by Maxim Shemanarev.

void MaxSTest()
{
   bvect vec;

   int i, j;
   unsigned id;
   for(i = 0; i < 100; i++)
   {
      int n = rand() % 2000 + 1;
      id = 1;
      for(j = 0; j < n; j++)
      {
         id += rand() % 10 + 1;
         vec.set_bit(id);

      }
      vec.optimize();
      vec.clear();
      fprintf(stderr, ".");
   }
}


void CalcBeginMask()
{
    printf("BeginMask:\n");

    int i;
    for (i = 0; i < 32; ++i)
    {
    unsigned mask = 0;

        for(int j = i; j < 32; ++j)
        {
            unsigned nbit  = j; 
            nbit &= bm::set_word_mask;
            bm::word_t  mask1 = (((bm::word_t)1) << j);

            mask |= mask1;
        }

        printf("0x%x, ", mask);
        
    } 
    printf("\n");
}

void CalcEndMask()
{
    printf("EndMask:\n");

    int i;
    for (i = 0; i < 32; ++i)
    {
    unsigned mask = 1;

        for(int j = i; j > 0; --j)
        {
            unsigned nbit  = j; 
            nbit &= bm::set_word_mask;
            bm::word_t  mask1 = (((bm::word_t)1) << j);

            mask |= mask1;
        }

        printf("0x%x,", mask);
        
    } 
    printf("\n");
}


void EnumeratorTest()
{
    cout << "-------------------------------------------- EnumeratorTest" << endl;

    {
    bvect bvect1;

    bvect1.set_bit(100);

    bvect::enumerator en = bvect1.first();

    if (*en != 100)
    {
        cout << "Enumerator error !" << endl;
        exit(1);
    }

    bvect1.clear_bit(100);

    bvect1.set_bit(2000000000);
    en.go_first();

    if (*en != 2000000000)
    {
        cout << "Enumerator error !" << endl;
        exit(1);
    }
    }

    {
        bvect bvect1;
        bvect1.set_bit(0);
        bvect1.set_bit(10);
        bvect1.set_bit(35);
        bvect1.set_bit(1000);
        bvect1.set_bit(2016519);
        bvect1.set_bit(2034779);
        bvect1.set_bit(bm::id_max-1);

        bvect::enumerator en = bvect1.first();

        unsigned num = bvect1.get_first();

        bvect::enumerator end = bvect1.end();
        while (en < end)
        {
            if (*en != num)
            {
                cout << "Enumeration comparison failed !" << 
                        " enumerator = " << *en <<
                        " get_next() = " << num << endl; 
                exit(1);
            }
            ++en;
            num = bvect1.get_next(num);
        }
        if (num != 0)
        {
            cout << "Enumeration error!" << endl;
            exit(1);
        }
    }
/*
    {
        bvect bvect1;
        bvect1.set();

        bvect::enumerator en = bvect1.first();

        unsigned num = bvect1.get_first();

        while (en < bvect1.end())
        {
            if (*en != num)
            {
                cout << "Enumeration comparison failed !" << 
                        " enumerator = " << *en <<
                        " get_next() = " << num << endl; 
                exit(1);
            }

            ++en;
            num = bvect1.get_next(num);
        }
        if (num != 0)
        {
            cout << "Enumeration error!" << endl;
            exit(1);
        }
    }
*/

    {
        bvect bvect1;

        int i;
        for(i = 0; i < 65536; ++i)
        {
            bvect1.set_bit(i);
        }
/*
        for(i = 65536*10; i < 65536*20; i+=3)
        {
            bvect1.set_bit(i);
        }
*/

        bvect::enumerator en = bvect1.first();

        unsigned num = bvect1.get_first();

        while (en < bvect1.end())
        {
            if (*en != num)
            {
                cout << "Enumeration comparison failed !" << 
                        " enumerator = " << *en <<
                        " get_next() = " << num << endl; 
                exit(1);
            }
            ++en;
            num = bvect1.get_next(num);
            if (num == 31)
            {
                num = num + 0;
            }
        }
        if (num != 0)
        {
            cout << "Enumeration error!" << endl;
            exit(1);
        }
    }


    {
    bvect bvect1;
    bvect1.set_new_blocks_strat(bm::BM_GAP);
    bvect1.set_bit(100);

    bvect::enumerator en = bvect1.first();

    if (*en != 100)
    {
        cout << "Enumerator error !" << endl;
        exit(1);
    }

    bvect1.clear_bit(100);

    bvect1.set_bit(2000000);
    en.go_first();

    if (*en != 2000000)
    {
        cout << "Enumerator error !" << endl;
        exit(1);
    }
    bvect1.stat();
    }

    {
        bvect bvect1;
        bvect1.set_new_blocks_strat(bm::BM_GAP);
        bvect1.set_bit(0);
        bvect1.set_bit(1);
        bvect1.set_bit(10);
        bvect1.set_bit(100);
        bvect1.set_bit(1000);

        bvect::enumerator en = bvect1.first();

        unsigned num = bvect1.get_first();

        while (en < bvect1.end())
        {
            if (*en != num)
            {
                cout << "Enumeration comparison failed !" << 
                        " enumerator = " << *en <<
                        " get_next() = " << num << endl; 
                exit(1);
            }
            ++en;
            num = bvect1.get_next(num);
        }
        if (num != 0)
        {
            cout << "Enumeration error!" << endl;
            exit(1);
        }
    }


}



void BlockLevelTest()
{
    bvect  bv;
    bvect  bv2;

    bv.set_new_blocks_strat(bm::BM_GAP);
    bv2.set_new_blocks_strat(bm::BM_GAP);

    int i;
    for (i = 0; i < 500; i+=1)
    {
        bv.set_bit(i);
    }
    bv.stat();

    for (i = 0; i < 1000; i+=2)
    {
        bv2.set_bit(i);
    }
    bv2.stat();

    struct bvect::statistics st;
    bv2.calc_stat(&st);

    unsigned char* sermem = new unsigned char[st.max_serialize_mem];

    unsigned slen = bv2.serialize(sermem);
    assert(slen);
    slen = 0;
    
    bv.deserialize(sermem);
//    bv.optimize();

    bv.stat();

}

/*
__int64 CalcBitCount64(__int64 b)
{
    b = (b & 0x5555555555555555) + (b >> 1 & 0x5555555555555555);
    b = (b & 0x3333333333333333) + (b >> 2 & 0x3333333333333333);
    b = b + (b >> 4) & 0x0F0F0F0F0F0F0F0F;
    b = b + (b >> 8);
    b = b + (b >> 16);
    b = b + (b >> 32) & 0x0000007F;
    return b;
}

unsigned CalcBitCount32(unsigned b)
{
    b = (b & 0x55555555) + (b >> 1 & 0x55555555);
    b = (b & 0x33333333) + (b >> 2 & 0x33333333);
    b = b + (b >> 4) & 0x0F0F0F0F;
    b = b + (b >> 8);
    b = b + (b >> 16) & 0x0000003F;
    return b;
}
*/


void SyntaxTest()
{
    cout << "----------------------------- Syntax test." << endl;
    bvect bv1;

    bvect bv2(bv1);
    bvect bv3;
    bv3.swap(bv1);
     
    bv1[100] = true;
    bool v = bv1[100];
    assert(v);
    v = false;

    bv1[100] = false;

    bv2 |= bv1;
    bv2 &= bv1;
    bv2 ^= bv1;
    bv2 -= bv1;

    bv3 = bv1 | bv2;

    if (bv1 < bv2)
    {
    }

    bvect::reference ref = bv1[10];
    bool bn = !ref;
    bool bn2 = ~ref;

    bn = bn2 = false;

    ref.flip();

    bvect bvn = ~bv1;

    cout << "----------------------------- Syntax test ok." << endl;
}


void SetTest()
{
    unsigned cnt;
    bvect bv;

    bv.set();

    cnt = bv.count();
    if (cnt != bm::id_max)
    {
        cout << "Set test failed!." << endl;
        exit(1);
    }

    bv.invert();
    cnt = bv.count();
    if (cnt != 0)
    {
        cout << "Set invert test failed!." << endl;
        exit(1);
    }

    bv.set(0);
    bv.set(bm::id_max-1);
    cnt = bv.count();

    assert(cnt == 2);

    bv.invert();
    bv.stat();
    cnt = bv.count();

    if (cnt != bm::id_max-2)
    {
        cout << "Set invert test failed!." << endl;
        exit(1);
    }
}


template<class A, class B> void CompareMiniSet(const A& ms,
                                          const B& bvm)
{
    for (unsigned i = 0; i < bm::set_total_blocks; ++i)
    {
        bool ms_val = ms.test(i)!=0;
        bool bvm_val = bvm.is_bit_true(i)!=0;
        if (ms_val != bvm_val)
        {
            printf("MiniSet comparison error: %u\n",i);
            exit(1);
        }
    }
}

void MiniSetTest()
{
    cout << "----------------------- MiniSetTest" << endl;
    {
    bm::miniset<bm::block_allocator, bm::set_total_blocks> ms;
    bvect_mini bvm(bm::set_total_blocks);


    CompareMiniSet(ms, bvm);


    ms.set(1);
    bvm.set_bit(1);

    CompareMiniSet(ms, bvm);

    unsigned i;

    for (i = 1; i < 10; i++)
    {
        ms.set(i);
        bvm.set_bit(i);
    }
    CompareMiniSet(ms, bvm);

    for (i = 1; i < 10; i++)
    {
        ms.set(i, false);
        bvm.clear_bit(i);
    }
    CompareMiniSet(ms, bvm);


    for (i = 1; i < 10; i+=3)
    {
        ms.set(i);
        bvm.set_bit(i);
    }
    CompareMiniSet(ms, bvm);

    for (i = 1; i < 5; i+=3)
    {
        ms.set(i, false);
        bvm.clear_bit(i);
    }
    CompareMiniSet(ms, bvm);
    }


    {
    bm::miniset<bm::block_allocator, bm::set_total_blocks> ms;
    bvect_mini bvm(bm::set_total_blocks);


    ms.set(1);
    bvm.set_bit(1);

    CompareMiniSet(ms, bvm);

    unsigned i;
    for (i = 1; i < bm::set_total_blocks; i+=3)
    {
        ms.set(i);
        bvm.set_bit(i);
    }
    CompareMiniSet(ms, bvm);

    for (i = 1; i < bm::set_total_blocks/2; i+=3)
    {
        ms.set(i, false);
        bvm.clear_bit(i);
    }
    CompareMiniSet(ms, bvm);
    }


    {
    bm::bvmini<bm::set_total_blocks> ms(0);
    bvect_mini bvm(bm::set_total_blocks);


    CompareMiniSet(ms, bvm);


    ms.set(1);
    bvm.set_bit(1);

    CompareMiniSet(ms, bvm);

    unsigned i;

    for (i = 1; i < 10; i++)
    {
        ms.set(i);
        bvm.set_bit(i);
    }
    CompareMiniSet(ms, bvm);

    for (i = 1; i < 10; i++)
    {
        ms.set(i, false);
        bvm.clear_bit(i);
    }
    CompareMiniSet(ms, bvm);


    for (i = 1; i < bm::set_total_blocks; i+=3)
    {
        ms.set(i);
        bvm.set_bit(i);
    }
    CompareMiniSet(ms, bvm);

    for (i = 1; i < bm::set_total_blocks/2; i+=3)
    {
        ms.set(i, false);
        bvm.clear_bit(i);
    }
    CompareMiniSet(ms, bvm);
    }


    {
    bm::miniset<bm::block_allocator, bm::set_total_blocks> ms;
    bvect_mini bvm(bm::set_total_blocks);


    ms.set(1);
    bvm.set_bit(1);

    CompareMiniSet(ms, bvm);

    unsigned i;
    for (i = 1; i < 15; i+=3)
    {
        ms.set(i);
        bvm.set_bit(i);
    }
    CompareMiniSet(ms, bvm);

    for (i = 1; i < 7; i+=3)
    {
        ms.set(i, false);
        bvm.clear_bit(i);
    }
    CompareMiniSet(ms, bvm);
    }


    cout << "----------------------- MiniSetTest ok" << endl;
}


unsigned CalcBitCount32(unsigned b)
{
    b = (b & 0x55555555) + (b >> 1 & 0x55555555);
    b = (b & 0x33333333) + (b >> 2 & 0x33333333);
    b = b + (b >> 4) & 0x0F0F0F0F;
    b = b + (b >> 8);
    b = b + (b >> 16) & 0x0000003F;
    return b;
}


void PrintGapLevels(const gap_word_t* glevel)
{
    cout << "Gap levels:" << endl;
    unsigned i;
    for (i = 0; i < bm::gap_levels; ++i)
    {
        cout << glevel[i] << ",";
    }
    cout << endl;
}

void OptimGAPTest()
{
    gap_word_t    glevel[bm::gap_levels];
    ::memcpy(glevel, gap_len_table<true>::_len, bm::gap_levels * sizeof(gap_word_t));

    {
    gap_word_t  length[] = { 2, 2, 5, 5, 10, 11, 12 };
    unsigned lsize = sizeof(length) / sizeof(gap_word_t);

    bm::improve_gap_levels(length, length + lsize, glevel);

    PrintGapLevels(glevel);
    }

    {
    gap_word_t  length[] = { 3, 5, 15, 15, 100, 110, 120 };
    unsigned lsize = sizeof(length) / sizeof(gap_word_t);

    bm::improve_gap_levels(length, length + lsize, glevel);
    PrintGapLevels(glevel);
    }

    {
    gap_word_t  length[] = { 15, 80, 5, 3, 100, 110, 95 };
    unsigned lsize = sizeof(length) / sizeof(gap_word_t);

    bm::improve_gap_levels(length, length + lsize, glevel);
    PrintGapLevels(glevel);
    }

    {
    gap_word_t  length[] = 
    { 16,30,14,24,14,30,18,14,12,16,8,38,28,4,20,18,28,22,32,14,12,16,10,8,14,18,14,8,
      16,30,8,8,58,28,18,4,26,14,52,12,18,10,14,18,22,18,20,70,12,6,26,6,8,22,12,4,8,8,
      8,54,18,6,8,4,4,10,4,4,4,4,4,6,22,14,38,40,56,50,6,10,8,18,82,16,6,18,20,12,12,
      16,8,14,14,10,16,12,10,16,14,12,18,14,18,34,14,12,18,18,10,20,10,18,8,14,14,22,16,
      10,10,18,8,20,14,10,14,12,12,14,16,16,6,10,14,6,10,10,10,10,12,4,8,8,8,10,10,8,
      8,12,10,10,14,14,14,8,4,4,10,10,4,10,4,8,6,52,104,584,218
    };
    unsigned lsize = sizeof(length) / sizeof(gap_word_t);

    bm::improve_gap_levels(length, length + lsize, glevel);
    PrintGapLevels(glevel);
    }

    {
    gap_word_t  length[] = {
     30,46,26,4,4,68,72,6,10,4,6,14,6,42,198,22,12,4,6,24,12,8,18,4,6,10,6,4,6,6,12,6
    ,6,4,4,78,38,8,52,4,8,10,6,8,8,6,10,4,6,6,4,10,6,8,16,22,28,14,10,10,16,10,20,10
    ,14,12,8,18,4,8,10,6,10,4,6,12,16,12,6,4,8,4,14,14,6,8,4,10,10,8,8,6,8,6,8,4,8,4
    ,8,10,6,4,6 
    };
    unsigned lsize = sizeof(length) / sizeof(gap_word_t);

    bm::improve_gap_levels(length, length + lsize, glevel);
    PrintGapLevels(glevel);

    }

}

void BitCountChangeTest()
{
    cout << "---------------------------- BitCountChangeTest " << endl;

    unsigned i;
    for(i = 0xFFFFFFFF; i; i <<= 1) 
    { 
        unsigned a0 = bm::bit_count_change(i);
        unsigned a1 = BitCountChange(i);
        
        if (a0 != a1)
        {
            cout << hex 
                 << "Bit count change test failed!" 
                 << " i = " << i << " return = " 
                 << a0 << " check = " << a1
                 << endl;
            exit(1);
        }
    }

    cout << "---------------------------- STEP 2 " << endl;

    for(i = 0xFFFFFFFF; i; i >>= 1) 
    { 
        unsigned a0 = bm::bit_count_change(i);
        unsigned a1 = BitCountChange(i);
        
        if (a0 != a1)
        {
            cout << "Bit count change test failed!" 
                 << " i = " << i << " return = " 
                 << a0 << " check = " << a1
                 << endl;
            exit(1);
        }
    }

    cout << "---------------------------- STEP 3 " << endl;

    for (i = 0; i < 0xFFFFFFF; ++i)
    {
        unsigned a0 = bm::bit_count_change(i);
        unsigned a1 = BitCountChange(i);
        
        if (a0 != a1)
        {
            cout << "Bit count change test failed!" 
                 << " i = " << i << " return = " 
                 << a0 << " check = " << a1
                 << endl;
            exit(1);
        }
    }
   

    bm::word_t   arr[16] = {0,};
    arr[0] = (bm::word_t)(1 << 31);
    arr[1] = 1; //(bm::word_t)(1 << 31);
    
    bm::id_t cnt;
    
    cnt = bm::bit_count_change(arr[1]);
    cout << cnt << endl;
    if (cnt != 2)
    {
        cout << "0.count_change() failed " << cnt << endl;
        exit(1);
    }
    
    cnt = bm::bit_block_calc_count_change(arr, arr+4);
    
    if (cnt != 3)
    {
        cout << "1.count_intervals() failed " << cnt << endl;
        exit(1);
    }

    arr[0] = arr[1] = arr[2] = 0xFFFFFFFF;
    arr[3] = (bm::word_t)(0xFFFFFFFF >> 1);
    
    cnt = bm::bit_block_calc_count_change(arr, arr+4);
    cout << cnt << endl;
    
    if (cnt != 2)
    {
        cout << "1.1 count_intervals() failed " << cnt << endl;
        exit(1);
    }
    
 
    cout << "---------------------------- STEP 4 " << endl;

    bvect   bv1;
    cnt = bm::count_intervals(bv1);
    
    if (cnt != 1)
    {
        cout << "1.count_intervals() failed " << cnt << endl;
        exit(1);
    }
    CheckIntervals(bv1, 65536);
    
    bv1.invert();

    cnt = count_intervals(bv1);
    cout << "Inverted cnt=" << cnt << endl;
    
    if (cnt != 2)
    {
        cout << "2.inverted count_intervals() failed " << cnt << endl;
        exit(1);
    }
    
    bv1.invert();
        
    for (i = 10; i < 100000; ++i)
    {
        bv1.set(i);
    }
    
    cnt = count_intervals(bv1);
    
    if (cnt != 3)
    {
        cout << "3.count_intervals() failed " << cnt << endl;
        exit(1);
    }
    cout << "-----" << endl;
    CheckIntervals(bv1, 65536*2);
    cout << "Optmization..." << endl; 
    bv1.optimize();
    cnt = count_intervals(bv1);
    
    if (cnt != 3)
    {
        cout << "4.count_intervals() failed " << cnt << endl;
        exit(1);
    }
    
    CheckIntervals(bv1, 65536*2);

    cout << "---------------------------- BitCountChangeTest Ok." << endl;
}

/*
#define POWER_CHECK(w, mask) \
    (bm::bit_count_table<true>::_count[(w&mask) ^ ((w&mask)-1)])

void BitListTest()
{
    unsigned bits[64] = {0,};

    unsigned w = 0;

    w = (1 << 3) | 1;


    int bn3 = POWER_CHECK(w, 1 << 3) - 1;
    int bn2 = POWER_CHECK(w, 1 << 2) - 1;
    int bn0 = POWER_CHECK(w, 1 << 0) - 1;

    bit_list(w, bits+1);
  
}
*/


int main(void)
{
    time_t      start_time = time(0);
    time_t      finish_time;

    OptimGAPTest();

    CalcBeginMask();
    CalcEndMask();


//   cout << sizeof(__int64) << endl;

//   ::srand((unsigned)::time(NULL));

     MiniSetTest();

     SyntaxTest();

     SetTest();
    
     BitCountChangeTest();

     EnumeratorTest();

     BasicFunctionalityTest();

     ClearAllTest();

     GAPCheck();

     MaxSTest();

     GetNextTest();

     SimpleRandomFillTest();
     
     RangeRandomFillTest();

     AndOperationsTest();   
           
     OrOperationsTest();

     XorOperationsTest();

     SubOperationsTest();

     WordCmpTest();

     ComparisonTest();

     MutationTest();

     MutationOperationsTest();
   
     SerializationTest();

     DesrializationTest2();

     BlockLevelTest();

     StressTest(800);

    finish_time = time(0);


    cout << "Test execution time = " << finish_time - start_time << endl;

#ifdef MEM_DEBUG
    cout << "Number of BLOCK allocations = " <<  dbg_block_allocator::na_ << endl;
    cout << "Number of PTR allocations = " <<  dbg_ptr_allocator::na_ << endl << endl;

    assert(dbg_block_allocator::balance() == 0);
    assert(dbg_ptr_allocator::balance() == 0);
#endif

    return 0;
}


