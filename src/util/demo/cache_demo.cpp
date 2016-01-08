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
 * Author:  Aleksey Grichenko
 *
 * File Description:
 *   Generic cache demo
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiobj.hpp>
#include <util/ncbi_cache.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;


class CCacheDemoApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run (void);
private:
    void SimpleCacheDemo(void);     // Simple cache
    void HeapCacheDemo(void);       // Allocation of elements on heap
    void MemoryCacheDemo(void);     // Limit memory used by cache
    void EmptyOnOverflowDemo(void); // Empty the cache on overflow
};


void CCacheDemoApp::Init(void)
{
    return;
}


int CCacheDemoApp::Run(void)
{
    SimpleCacheDemo();
    HeapCacheDemo();
    MemoryCacheDemo();
    EmptyOnOverflowDemo();
    return 0;
}


// Simple CObject-based cache element. Can be stored in the demo caches
// using CRef<>-s. Allocates data of the specified size.
class CObjElement : public CObject
{
public:
    CObjElement(size_t size) : m_Size(size), m_Data(new char[size]) {}
    ~CObjElement(void) { delete[] m_Data; }
    size_t GetSize(void) const { return m_Size; }
private:
    size_t m_Size;
    char*  m_Data;
};


void CCacheDemoApp::SimpleCacheDemo(void)
{
    // Elements of the cache are CObject-derived.
    // The cache does not require any special allocator/remover since
    // elemnts are stored in CRef<>-s.
    // The cache uses default locking (CMutex).
    typedef CCache<int, CRef<CObjElement> > TSimpleCache;
    TSimpleCache cache(100);
    // Allocate and insert elements into the cache. Check result.
    for (int i = 0; i < 1000; i++) {
        CRef<CObjElement> element(new CObjElement(i*10));
        TSimpleCache::EAddResult result;
        cache.Add(i, element, 1, 0, &result);
        _ASSERT(result == TSimpleCache::eAdd_Inserted);
    }
    // Try to get the elements from the cache. Some will be missing
    // since cache size is less than total number of elements inserted.
    for (int i = 0; i < 1000; i++) {
        CRef<CObjElement> element = cache.Get(i);
        if (element) {
            // If present, check the element's size.
            _ASSERT(element->GetSize() == (size_t)i*10);
        }
        if (i % 10 == 0) {
            cache.Remove(i);
        }
    }
}


// The elements will be allocated on heap but are not CObject-derived.
// This requires special handler (see below). The internal array of
// chars is also allocated on heap.
class CHeapElement
{
public:
    CHeapElement(size_t size) : m_Size(size), m_Data(new char[size]) {}
    ~CHeapElement(void) { delete[] m_Data; }
    size_t GetSize(void) const { return m_Size; }
private:
    size_t m_Size;
    char*  m_Data;
};


// Remove_element handler for dynamically allocated objects.
class CDemoHandler_HeapAlloc
{
public:
    // Delete each object upon removal from the cache.
    void RemoveElement(const int& /*key*/, CHeapElement* value)
    {
        delete value;
    }
    // Inserting an element does not require special processing.
    void InsertElement(const int& /*key*/, CHeapElement* /*value*/) {}
    // Possibility to insert an element depends only on the cache size.
    ECache_InsertFlag CanInsertElement(const int& /*key*/,
                                       const CHeapElement* value)
    {
        if ( !value )
            return eCache_DoNotCache;
        return eCache_CheckSize;
    }
    CHeapElement* CreateValue(const int& /*key*/) { return NULL; }
};


void CCacheDemoApp::HeapCacheDemo(void)
{
    // Define cache type, override the default insert/remove handler.
    // Set lock type to CNoMutex to avoid locking overhead.
    typedef CHeapElement* TElement;
    typedef CCache<int, TElement,
                   CDemoHandler_HeapAlloc, CNoMutex> THeapCache;

    THeapCache cache(100);
    // Fill the cache with dynamically allocated objects.
    for (int i = 0; i < 1000; i++) {
        CHeapElement* element = new CHeapElement(i*10);
        THeapCache::EAddResult result;
        cache.Add(i, element, 1, 0, &result);
        _ASSERT(result == THeapCache::eAdd_Inserted);
    }
    // Get the objects back if still available.
    for (int i = 0; i < 1000; i++) {
        CHeapElement* element = cache.Get(i);
        if (element) {
            _ASSERT(element->GetSize() == (size_t)i*10);
        }
    }
}


// Max memory size allowed for the cache. This number includes only
// number of bytes allocated for the internal buffers, as
// returned by GetSize().
const size_t kMaxMemCacheSize = 5000;

// Special insert/remove handler which checks total memory
// used by the elements rather than number of elements
class CDemoHandler_MemSize
{
public:
    CDemoHandler_MemSize(void) : m_Total(0) {}
    ~CDemoHandler_MemSize(void) { _ASSERT(m_Total == 0); }

    void RemoveElement(const int& /*key*/, CRef<CObjElement> value)
    {
        // Decrease total memory used upon element removal.
        m_Total -= value->GetSize();
    }
    void InsertElement(const int& /*key*/, const CRef<CObjElement>& value)
    {
        // Increase total memory used upon element insertion.
        m_Total += value->GetSize();
    }
    ECache_InsertFlag CanInsertElement(const int& /*key*/,
                                       const CRef<CObjElement>& value)
    {
        if ( !value )
            return eCache_DoNotCache;
        // Check if there's enough memory available for the new element
        if ( m_Total + value->GetSize() <= kMaxMemCacheSize ) {
            // Ok to insert
            return eCache_CanInsert;
        }
        else if (value->GetSize() > kMaxMemCacheSize) {
            // The element is too big for the cache, do not try to insert
            return eCache_DoNotCache;
        }
        // The element can be inserted, but some older elements need to be
        // removed. The function may be called multiple times until there's
        // enough memory for the insertion.
        return eCache_NeedCleanup;
    }
    CRef<CObjElement> CreateValue(const int& /*key*/)
        { return CRef<CObjElement>(NULL); }
private:
    // Total memory used by the cached elements
    size_t m_Total;
};


void CCacheDemoApp::MemoryCacheDemo(void)
{
    // Override the default insert/remove handler.
    typedef CRef<CObjElement> TElement;
    typedef CCache<int, TElement, CDemoHandler_MemSize> TMemCache;

    // Cache capacity must be positive. The real capacity depends on
    // the max memory allowed by the handler.
    TMemCache cache(1);
    // Fill the cache with elements of different size.
    for (int i = 0; i < 1000; i++) {
        TElement element(new CObjElement(i*10));
        TMemCache::EAddResult result;
        cache.Add(i, element, 1, 0, &result);
        // The element should be inserted only if its size is less than
        // the max. allowed size of the cache.
        if (element->GetSize() <= kMaxMemCacheSize) {
            _ASSERT(result == TMemCache::eAdd_Inserted);
        }
        else {
            _ASSERT(result == TMemCache::eAdd_NotInserted);
        }
    }
    for (int i = 0; i < 1000; i++) {
        TElement element = cache.Get(i);
        if (element) {
            _ASSERT(element->GetSize() == (size_t)i*10);
        }
    }
}


// Max number of elements allowed in the cache.
const size_t kMaxOverflowCacheSize = 100;

// Special cache handling - elements are inserted as usual unless
// cache capacity is exceeded. After that all elements are removed
// and the cache is filled again.
class CDemoHandler_EmptyOnOverflow
{
public:
    CDemoHandler_EmptyOnOverflow(void) : m_Count(0), m_Overflow(false) {}
    ~CDemoHandler_EmptyOnOverflow(void) { _ASSERT(m_Count == 0); }

    void RemoveElement(const int& /*key*/, CRef<CObjElement> /*value*/)
    {
        // Count the elements removed.
        _ASSERT(m_Count > 0);
        m_Count--;
        if ( m_Overflow  &&  m_Count == 0 ) {
            // Reset overflow flag if all elements have been removed.
            m_Overflow = false;
        }
    }
    void InsertElement(const int& /*key*/, const CRef<CObjElement>& /*value*/)
    {
        // Count the elements inserted.
        m_Count++;
    }
    ECache_InsertFlag CanInsertElement(const int& /*key*/,
                                       const CRef<CObjElement>& value)
    {
        if ( !value )
            return eCache_DoNotCache;
        // If the overflow is detected, set the flag and request
        // clean up until the cache is empty.
        if (m_Overflow  ||  m_Count >= kMaxOverflowCacheSize) {
            m_Overflow = true;
            return eCache_NeedCleanup;
        }
        // If there's no overflow, allow to insert the new element.
        return eCache_CanInsert;
    }
    CRef<CObjElement> CreateValue(const int& /*key*/)
        { return CRef<CObjElement>(NULL); }

private:
    size_t m_Count;
    bool   m_Overflow;
};


void CCacheDemoApp::EmptyOnOverflowDemo(void)
{
    // Override the default insert/remove handler
    typedef CRef<CObjElement> TElement;
    typedef CCache<int, TElement, CDemoHandler_EmptyOnOverflow> TEmptyOnOverflowCache;

    // Cache capacity must be positive. The real capacity depends on
    // the max number of elements allowed by the handler.
    TEmptyOnOverflowCache cache(1);
    for (int i = 0; i < 1000; i++) {
        TElement element(new CObjElement(i*10));
        TEmptyOnOverflowCache::EAddResult result;
        cache.Add(i, element, 1, 0, &result);
        if ( i % kMaxOverflowCacheSize == 0 ) {
            _ASSERT(cache.GetSize() == 1);
        }
    }
    for (int i = 0; i < 1000; i++) {
        TElement element = cache.Get(i);
        if (element) {
            _ASSERT(element->GetSize() == (size_t)i*10);
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[]) 
{
    CCacheDemoApp app;
    return app.AppMain(argc, argv);
}
