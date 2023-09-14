#ifndef SRA__READER__SRA__VDBCACHE__HPP
#define SRA__READER__SRA__VDBCACHE__HPP
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
 * Authors:  Eugene Vasilchenko
 *
 * File Description:
 *   Cache of opened VDB files with limits on number and time of open files
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbitime.hpp>
#include <util/limited_size_map.hpp>
#include <sra/readers/sra/vdbread.hpp>


BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(objects);


class NCBI_SRAREAD_EXPORT CVDBCacheWithExpiration
{
public:
    explicit
    CVDBCacheWithExpiration(size_t size_limit,
                        unsigned force_reopen_seconds = CDeadline::eInfinite,
                        unsigned rechech_seconds = CDeadline::eInfinite);
    virtual ~CVDBCacheWithExpiration();

    size_t get_size_limit() const
    {
        return m_CacheMap.get_size_limit();
    }
    unsigned GetForceReopenSecond() const
    {
        return m_ForceReopenSeconds;
    }
    unsigned GetRecheckSecond() const
    {
        return m_RecheckSeconds;
    }
    void set_size_limit(size_t limit);
    void SetForceReopenSeconds(unsigned seconds);
    void SetRecheckSeconds(unsigned seconds);

    class CExpirationInfo : public CObject
    {
    public:
        CExpirationInfo(const CVDBCacheWithExpiration& cache, const string& acc_or_path);
        virtual ~CExpirationInfo() override;
        
        bool IsExpired(const CVDBCacheWithExpiration& cache, const string& acc_or_path) const;
        
    private:
        CExpirationInfo(const CExpirationInfo&) = delete;
        void operator=(const CExpirationInfo&) = delete;
        
        CDeadline    m_ForceReopenDeadline;
        mutable CDeadline m_RecheckDeadline; // re-check deadline will be updated repeatedly
        string       m_DereferencedPath;
        CTime        m_Timestamp;
    };
    class CSlot : public CObject
    {
    public:
        CSlot();
        virtual ~CSlot() override;

        typedef CMutex TSlotMutex;
        TSlotMutex& GetSlotMutex()
            {
                return m_SlotMutex;
            }

        template<class Object>
        CRef<Object> GetObject() const
            {
                return Ref(dynamic_cast<Object*>(m_Object.GetNCPointerOrNull()));
            }
        void SetObject(CObject* object)
            {
                m_Object = object;
            }
        void ResetObject()
            {
                SetObject(nullptr);
            }

        bool IsExpired(const CVDBCacheWithExpiration& cache,
                       const string& acc_or_path) const;
        void UpdateExpiration(const CVDBCacheWithExpiration& cache,
                              const string& acc_or_path);
        
    private:
        friend class CVDBCacheWithExpiration;

        CSlot(const CSlot&) = delete;
        void operator=(const CSlot&) = delete;
        
        TSlotMutex   m_SlotMutex;
        CRef<CExpirationInfo> m_ExpirationInfo;
        CRef<CObject> m_Object;
    };
    
    CRef<CSlot> GetSlot(const string& acc_or_path);

    CRef<CObject> GetObject(CSlot& slot) const;
    void SetObject(CSlot& slot, CObject* object) const;
    
    void Set(const string& acc_or_path, CRef<CSlot> slot);

    CMutex& GetCacheMutex() {
        return m_CacheMutex;
    }

protected:
    friend class CSlot;
    friend class CExpirationInfo;
    
    CVDBCacheWithExpiration(const CVDBCacheWithExpiration&) = delete;
    void operator=(const CVDBCacheWithExpiration&) = delete;

    bool x_IsExpired(const string& acc_or_path, CSlot& slot) const;

private:
    typedef limited_size_map<string, CRef<CSlot>> TCacheMap;

    CVDBMgr    m_Mgr;
    CMutex     m_CacheMutex;
    TCacheMap  m_CacheMap;
    unsigned   m_ForceReopenSeconds;
    unsigned   m_RecheckSeconds;
};


END_NAMESPACE(objects);
END_NCBI_NAMESPACE;

#endif // SRA__READER__SRA__VDBCACHE__HPP
