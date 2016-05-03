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
 *   Test for generic cache
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/test_mt.hpp>
#include <util/ncbi_cache.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;


class CElement
{
public:
    CElement(size_t size) : m_Data(new char[size]) {}
    ~CElement(void) { delete[] m_Data; }
private:
    char* m_Data;
};


class CTestHandler
{
public:
    void RemoveElement(const Uint8& /*key*/, CElement* value)
    {
        delete value;
    }
    void InsertElement(const Uint8& /*key*/, CElement* /*value*/) {}
    ECache_InsertFlag CanInsertElement(const Uint8& /*key*/, CElement* value)
    {
        if ( !value ) {
            return eCache_CheckSize; // eCache_DoNotCache;
        }
        return eCache_CheckSize;
    }
    CElement* CreateValue(const Uint8& /*key*/) { return NULL; }
};


typedef CElement* TElement;
typedef CMutex    TCacheLock;

typedef CCache<Uint8, TElement,
               CTestHandler, TCacheLock, unsigned char> TUCharCache;
typedef CCache<Uint8, TElement,
               CTestHandler, TCacheLock, unsigned int>  TUIntCache;
typedef CCache<Uint8, TElement,
               CTestHandler, TCacheLock, Uint8> TUInt8Cache;
typedef CCache<Uint8, TElement,
               CTestHandler, TCacheLock, signed char>   TIntCache;


class CTestCacheApp : public CThreadedApp
{
public:
    virtual bool Thread_Run(int idx);
protected:
    virtual bool TestApp_Init(void);
    virtual bool TestApp_Exit(void);
    virtual bool TestApp_Args(CArgDescriptions& args);
private:
    Uint8 m_ElementDataSize;
    Uint8 m_CacheSize;

    auto_ptr<TUCharCache> m_UCharCache;
    auto_ptr<TUIntCache>  m_UIntCache;
    auto_ptr<TUInt8Cache> m_UInt8Cache;
    auto_ptr<TIntCache>   m_IntCache;
};


template<class TCache>
bool s_TestCache(TCache&                    cache,
                 typename TCache::TSizeType data_size,
                 typename TCache::TSizeType cache_size)
{
    typedef typename TCache::TSizeType TSize;
    for (Uint8 i = 1; i < 2*cache_size; i++) {
        if ( !cache.Get(i) ) {
            TElement elt(new CElement(size_t(data_size)));
            cache.Add(i, elt, TSize(i%5));
        }
    }
    if (TSize(2*cache_size) > 0) {
        cache.SetCapacity(TSize(2*cache_size));
    }
    else {
        cache.SetCapacity(cache_size/2);
    }
    for (Uint8 i = 3*cache_size; i > 0; i--) {
        if ( !cache.Get(i) ) {
            TElement elt(new CElement(size_t(data_size)));
            cache.Add(i, elt);
        }
    }
    for (Uint8 i = 0; i < 3*cache_size; i++) {
        cache.Get(i);
    }
    cache.SetCapacity(cache_size);

    return true;
}


bool CTestCacheApp::Thread_Run(int /*idx*/)
{
    if ( m_UCharCache.get() ) {
        s_TestCache(*m_UCharCache,
            (unsigned char)(m_ElementDataSize),
            (unsigned char)(m_CacheSize));
    }
    if ( m_UIntCache.get() ) {
        s_TestCache(*m_UIntCache,
            (unsigned int)(m_ElementDataSize),
            (unsigned int)(m_CacheSize));
    }
    if ( m_UInt8Cache.get() ) {
        s_TestCache(*m_UInt8Cache,
            m_ElementDataSize,
            m_CacheSize);
    }
    if ( m_IntCache.get() ) {
        s_TestCache(*m_IntCache,
            (signed char)(m_ElementDataSize),
            (signed char)(m_CacheSize));
    }
    return true;
}


bool CTestCacheApp::TestApp_Init(void)
{
    NcbiCout << NcbiEndl
             << "Testing cache with "
             << NStr::IntToString(s_NumThreads)
             << " threads..."
             << NcbiEndl;

    const CArgs& args = GetArgs();
    m_CacheSize = args["cache_size"].AsInt8();
    m_ElementDataSize = args["element_size"].AsInt8();
    if (args["size_type"].AsString() == "ubyte") {
        m_UCharCache.reset(new TUCharCache((unsigned char)(m_CacheSize)));
    }
    else if (args["size_type"].AsString() == "uint") {
        m_UIntCache.reset(new TUIntCache((unsigned int)(m_CacheSize)));
    }
    else if (args["size_type"].AsString() == "uint8") {
        m_UInt8Cache.reset(new TUInt8Cache(m_CacheSize));
    }
    else if (args["size_type"].AsString() == "byte") {
        m_IntCache.reset(new TIntCache((signed char)(m_CacheSize)));
    }
    return true;
}


bool CTestCacheApp::TestApp_Args(CArgDescriptions& args)
{
    args.AddDefaultKey("element_size", "ElementSize",
        "Size of a cache element",
        CArgDescriptions::eInt8, "100000");
    args.AddDefaultKey("cache_size", "CacheSize",
        "Initial size of the cache",
        CArgDescriptions::eInt8, "200");
    args.AddDefaultKey("size_type", "SizeType",
        "Cache size type",
        CArgDescriptions::eString, "ubyte");
    args.SetConstraint("size_type",
        &(*new CArgAllow_Strings, "ubyte", "uint", "uint8", "byte"));
    return true;
}


bool CTestCacheApp::TestApp_Exit(void)
{
    NcbiCout << "Test completed successfully!"
             << NcbiEndl << NcbiEndl;
    return true;
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[]) 
{
    CTestCacheApp app;
    return app.AppMain(argc, argv);
}
