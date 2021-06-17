#ifndef CACHE_MANAGER__HPP_INCLUDED
#define CACHE_MANAGER__HPP_INCLUDED
/*  $Id$
* ===========================================================================
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
* ===========================================================================
*
*  Author:  Aleksey Grichenko
*
*  File Description: Cache manager interface
*
*/

#include <corelib/ncbistd.hpp>
#include <util/cache/icache.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class NCBI_XREADER_EXPORT CReaderCacheManager
{
public:
    enum ECacheType {
        fCache_Id   = 1<<0,
        fCache_Blob = 1<<1,
        fCache_Any  = fCache_Id | fCache_Blob
    };
    typedef int TCacheType;
    struct NCBI_XREADER_EXPORT SReaderCacheInfo
    {
        SReaderCacheInfo(ICache& cache, ECacheType cache_type);
        ~SReaderCacheInfo(void);

        AutoPtr<ICache> m_Cache;
        TCacheType      m_Type;
    };
    typedef vector<SReaderCacheInfo> TCaches;
    typedef TPluginManagerParamTree TCacheParams;

    CReaderCacheManager(void);
    virtual ~CReaderCacheManager(void);

    virtual void RegisterCache(ICache& cache, ECacheType cache_type) = 0;
    virtual TCaches& GetCaches(void) = 0;
    virtual ICache* FindCache(ECacheType cache_type,
                              const TCacheParams* params) = 0;

private:
    // to prevent copying
    CReaderCacheManager(const CReaderCacheManager&);
    void operator=(const CReaderCacheManager&);
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // CACHE_MANAGER__HPP_INCLUDED
