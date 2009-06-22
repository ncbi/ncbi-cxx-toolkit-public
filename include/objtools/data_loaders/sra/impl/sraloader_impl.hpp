#ifndef OBJTOOLS_DATA_LOADERS_SRA___SRALOADER_IMPL__HPP
#define OBJTOOLS_DATA_LOADERS_SRA___SRALOADER_IMPL__HPP

/*  $Id $
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
 * File Description: SRA file data loader
 *
 * ===========================================================================
 */


#include <corelib/ncbistd.hpp>
#include <corelib/ncbimtx.hpp>
#include <objtools/data_loaders/sra/sraloader.hpp>
#include <sra/sradb.h>
#include <sra/ncbi-sradb.h>
#include <vdb/types.h>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDataLoader;

template<class Object,
         rc_t (*FRelease)(const Object*),
         rc_t (*FAddRef)(const Object*)>
class CSraRef
{
    typedef CSraRef<Object, FRelease, FAddRef> TSelf;
public:
    typedef Object TObject;
    CSraRef(void)
        : m_Object(0)
        {
        }
    CSraRef(const TSelf& ref)
        : m_Object(s_AddRef(ref))
        {
        }
    TSelf& operator=(const TSelf& ref)
        {
            if ( this != &ref ) {
                Release();
                m_Object = s_AddRef(ref);
            }
            return *this;
        }
    ~CSraRef(void)
        {
            Release();
        }

    void Release(void)
        {
            if ( m_Object ) {
                FRelease(m_Object);
                m_Object = 0;
            }
        }
    
    operator const TObject*(void) const
        {
            if ( !m_Object ) {
                NCBI_THROW_FMT(CSraException, eNullPtr,
                               "Null SRA pointer");
            }
            return m_Object;
        }

    DECLARE_OPERATOR_BOOL(m_Object);

protected:
    const TObject** x_InitPtr(void)
        {
            Release();
            return &m_Object;
        }

protected:
    static const TObject* s_AddRef(const TSelf& ref)
        {
            const TObject* obj = ref.m_Object;
            if ( obj ) {
                if ( rc_t rc = FAddRef(obj) ) {
                    NCBI_THROW_FMT(CSraException, eAddRefFailed,
                                   "Cannot add ref: "<<rc);
                }
            }
            return obj;
        }

private:
    const TObject* m_Object;
};

class CSraMgr;
class CSraRun;
class CSraColumn;

class CSraMgr : public CSraRef<SRAMgr, SRAMgrRelease, SRAMgrAddRef>
{
public:
    CSraMgr(const string& rep_path, const string& vol_path);

    CRef<CSeq_entry> GetSpotEntry(const string& sra) const;
    CRef<CSeq_entry> GetSpotEntry(const string& sra,
                                  AutoPtr<CSraRun>& run) const;
};

class CSraColumn : public CSraRef<SRAColumn, SRAColumnRelease, SRAColumnAddRef>
{
public:
    CSraColumn(void)
        {
        }
    CSraColumn(const CSraRun& run, const char* name, const char* type)
        {
            Init(run, name, type);
        }

    void Init(const CSraRun& run, const char* name, const char* type);
};


class CSraRun : public CSraRef<SRATable, SRATableRelease, SRATableAddRef>
{
public:
    CSraRun(void)
        {
        }
    CSraRun(const CSraMgr& mgr, const string& acc)
        {
            Init(mgr, acc);
        }

    void Init(const CSraMgr& mgr, const string& acc);

    const string& GetAccession(void) const
        {
            return m_Acc;
        }

    CRef<CSeq_entry> GetSpotEntry(spotid_t spot_id) const;
 
private:
    string m_Acc;
    CSraColumn m_Name;
    CSraColumn m_Read;
    CSraColumn m_Qual;
    CSraColumn m_SDesc;
    CSraColumn m_RDesc;
};


class CSraValue
{
public:
    CSraValue(const CSraColumn& col, spotid_t id)
        : m_Error(0), m_Data(0), m_Bitoffset(0), m_Bitlen(0), m_Len(0)
        {
            m_Error = SRAColumnRead(col, id, &m_Data, &m_Bitoffset, &m_Bitlen);
            if ( m_Error ) {
                return;
            }
            if ( m_Bitoffset ) {
                m_Error = 1;
            }
            else {
                m_Len = (m_Bitlen+7)>>3;
            }
        }

    size_t GetLength(void) const
        {
            return m_Len;
        }

    DECLARE_OPERATOR_BOOL(!m_Error);

protected:
    rc_t m_Error;
    const void* m_Data;
    bitsz_t m_Bitoffset;
    bitsz_t m_Bitlen;
    size_t m_Len;
};

class CSraStringValue : public CSraValue
{
public:
    CSraStringValue(const CSraColumn& col, spotid_t id)
        : CSraValue(col, id)
        {
        }
    const char* data(void) const
        {
            return static_cast<const char*>(m_Data);
        }
    size_t size(void) const
        {
            return GetLength();
        }
    operator string(void) const
        {
            return string(data(), size());
        }
    string Value(void) const
        {
            return *this;
        }
};

template<class V>
class CSraValueFor : public CSraValue
{
public:
    typedef V TValue;
    CSraValueFor(const CSraColumn& col, spotid_t id)
        : CSraValue(col, id)
        {
        }
    const TValue& Value(void) const
        {
            return *static_cast<const TValue*>(m_Data);
        }
    const TValue* operator->(void) const
        {
            return &Value();
        }
    const TValue& operator[](size_t i) const
        {
            return static_cast<const TValue*>(m_Data)[i];
        }
};

typedef CSraValueFor<uint16_t> CSraUInt16Value;
typedef CSraValueFor<char> CSraBytesValue;


class CSRADataLoader_Impl : public CObject
{
public:
    CSRADataLoader_Impl(void);
    ~CSRADataLoader_Impl(void);

    CRef<CSeq_entry> LoadSRAEntry(const string& accession,
                                  unsigned spot_id);

private:
    // mutex guarding input into the map
    CMutex m_Mutex;
    CSraMgr m_Mgr;
    CSraRun m_Run;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // OBJTOOLS_DATA_LOADERS_SRA___SRALOADER_IMPL__HPP
