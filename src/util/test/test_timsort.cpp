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
 * Author: Eugene Vasilchenko
 *
 * File Description:
 *   Test program for timsort implementation
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbiutil.hpp>
#include <util/timsort.hpp>
#include <util/random_gen.hpp>

#include <common/test_assert.h>  /* This header must go last */

BEGIN_NCBI_SCOPE

class CTestTimSort : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);

    template<class V, size_t S>
    void TestArray(const V (&arr0)[S]);

    CRandom m_Rand;
};


template<class C>
struct ContainerTraits
{
    typedef typename C::value_type value_type;
    typedef typename C::iterator pointer;
    typedef typename pointer::difference_type difference_type;
    typedef typename pointer::iterator_category iterator_category;
    typedef value_type& reference;
};

template<class E, size_t S>
struct ContainerTraits<E[S]>
{
    typedef E value_type;
    typedef E* pointer;
    typedef ssize_t difference_type;
    typedef std::random_access_iterator_tag iterator_category;
    typedef value_type& reference;
};


template<class C>
class CCheckedArrayIterator
{
public:
    typedef typename ContainerTraits<C>::iterator_category iterator_category;
    typedef typename ContainerTraits<C>::difference_type difference_type;
    typedef typename ContainerTraits<C>::pointer pointer;
    typedef typename ContainerTraits<C>::value_type value_type;
    typedef typename ContainerTraits<C>::reference reference;

    CCheckedArrayIterator(pointer ptr, pointer begin, pointer end)
        : m_Ptr(ptr),
          m_ArrayBegin(begin),
          m_ArrayEnd(end)
        {
            x_CheckPtr();
        }
    
    void x_CheckValue(void) const {
        _ASSERT(m_Ptr >= m_ArrayBegin);
        _ASSERT(m_Ptr < m_ArrayEnd);
    }
    void x_CheckPtr(void) const {
        _ASSERT(m_Ptr >= m_ArrayBegin);
        _ASSERT(m_Ptr <= m_ArrayEnd);
    }
    void x_CheckSame(const CCheckedArrayIterator& iter) const {
        _ASSERT(iter.m_ArrayBegin == m_ArrayBegin);
        _ASSERT(iter.m_ArrayEnd == m_ArrayEnd);
    }
    
    reference operator*() const {
        x_CheckValue();
        return *m_Ptr;
    }
    pointer operator->() const {
        x_CheckValue();
        return m_Ptr;
    }
    CCheckedArrayIterator& operator++() {
        _ASSERT(m_Ptr < m_ArrayEnd);
        ++m_Ptr;
        return *this;
    }
    CCheckedArrayIterator& operator--() {
        _ASSERT(m_Ptr > m_ArrayBegin);
        --m_Ptr;
        return *this;
    }
    CCheckedArrayIterator operator++(int) {
        CCheckedArrayIterator ret(*this);
        ++*this;
        return ret;
    }
    CCheckedArrayIterator operator--(int) {
        CCheckedArrayIterator ret(*this);
        --*this;
        return ret;
    }
    CCheckedArrayIterator& operator+=(difference_type offset) {
        _ASSERT(offset >= m_ArrayBegin-m_Ptr);
        _ASSERT(offset <= m_ArrayEnd-m_Ptr);
        m_Ptr += offset;
        return *this;
    }
    CCheckedArrayIterator& operator-=(difference_type offset) {
        return *this += -offset;
    }
    CCheckedArrayIterator operator+(difference_type offset) const {
        CCheckedArrayIterator ret(*this);
        ret += offset;
        return ret;
    }
    CCheckedArrayIterator operator-(difference_type offset) const {
        return *this + -offset;
    }
    difference_type operator-(const CCheckedArrayIterator& iter) const {
        x_CheckSame(iter);
        return m_Ptr - iter.m_Ptr;
    }

    bool operator==(const CCheckedArrayIterator& iter) const {
        x_CheckSame(iter);
        return m_Ptr == iter.m_Ptr;
    }
    bool operator!=(const CCheckedArrayIterator& iter) const {
        x_CheckSame(iter);
        return m_Ptr != iter.m_Ptr;
    }
    bool operator<(const CCheckedArrayIterator& iter) const {
        x_CheckSame(iter);
        return m_Ptr < iter.m_Ptr;
    }
    bool operator>(const CCheckedArrayIterator& iter) const {
        x_CheckSame(iter);
        return m_Ptr > iter.m_Ptr;
    }
    bool operator<=(const CCheckedArrayIterator& iter) const {
        x_CheckSame(iter);
        return m_Ptr <= iter.m_Ptr;
    }
    bool operator>=(const CCheckedArrayIterator& iter) const {
        x_CheckSame(iter);
        return m_Ptr >= iter.m_Ptr;
    }

private:
    pointer m_Ptr;
    pointer m_ArrayBegin;
    pointer m_ArrayEnd;
};

template<class C>
CCheckedArrayIterator<C> checked_begin(C& arr)
{
    return CCheckedArrayIterator<C>(begin(arr), begin(arr), end(arr));
}

template<class C>
CCheckedArrayIterator<C> checked_end(C &arr)
{
    return CCheckedArrayIterator<C>(end(arr), begin(arr), end(arr));
}

void CTestTimSort::Init(void)
{
    SetDiagPostLevel(eDiag_Info);

    auto_ptr<CArgDescriptions> d(new CArgDescriptions);

    d->SetUsageContext("test_timsort",
                       "test TimSort implementation");

    SetupArgDescriptions(d.release());
}

template<class V, size_t S>
void CTestTimSort::TestArray(const V (&arr0)[S])
{
    V arr[S];
    copy(begin(arr0), end(arr0), begin(arr));
    gfx::timsort(checked_begin(arr), checked_end(arr));
    for ( size_t i = 1; i < S; ++i ) {
        _ASSERT(arr[i-1] < arr[i]);
    }
}

int CTestTimSort::Run(void)
{
    //const CArgs& args = GetArgs();

    static int arr[300] = {
        118, 160, 23, 57, 58, 156, 250, 259, 17, 22, 119, 121, 141, 142, 152, 172, 189, 203, 251, 252, 285, 295, 296, 61, 162, 220, 292, 60, 97, 116, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 18, 19, 20, 21, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 59, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 117, 120, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 143, 144, 145, 146, 147, 148, 149, 150, 151, 153, 154, 155, 157, 158, 159, 161, 163, 164, 165, 166, 167, 168, 169, 170, 171, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 253, 254, 255, 256, 257, 258, 260, 261, 262, 263, 264, 265, 266, 267, 268, 269, 270, 271, 272, 273, 274, 275, 276, 277, 278, 279, 280, 281, 282, 283, 284, 286, 287, 288, 289, 290, 291, 293, 294, 297, 298, 299
    };
    TestArray(arr);

    for ( int t = 0; t < 100; ++t ) {
        size_t size = m_Rand.GetRandIndexSize_t(10000);
        vector<int> v0(size);
        for ( size_t i = 0; i < size; ++i )
            v0[i] = m_Rand.GetRand(1, 10000);
        size_t sort_count = m_Rand.GetRandIndexSize_t(5);
        for ( size_t s = 0; s < sort_count; ++s ) {
            size_t a = m_Rand.GetRandIndexSize_t(size + 1);
            size_t b = m_Rand.GetRandIndexSize_t(size + 1);
            if ( a > b ) swap(a, b);
            std::sort(v0.begin()+a, v0.begin()+b);
        }
        vector<int> v1 = v0;
        std::sort(begin(v1), end(v1));
        vector<int> v2 = v0;
        gfx::timsort(checked_begin(v2), checked_end(v2));
        _ASSERT(v1 == v2);
    }

    LOG_POST("All tests passed.");
    return 0;
}

END_NCBI_SCOPE

int main(int argc, const char* argv[])
{
    return NCBI_NS_NCBI::CTestTimSort().AppMain(argc, argv);
}
