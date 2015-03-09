#ifndef SRA__READER__SRA__SRAREAD__HPP
#define SRA__READER__SRA__SRAREAD__HPP
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
 *   Access to SRA files
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbiobj.hpp>

#include <sra/readers/sra/sdk.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seq_inst.hpp>

#include <sra/sradb.h>

// SRA SDK structures
struct SRAMgr;
struct SRAPath;
struct SRATable;
struct SRAColumn;

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;


template<>
struct CSraRefTraits<SRAPath>
{
    static rc_t x_Release(const SRAPath* t) { return 0; }
    static rc_t x_AddRef (const SRAPath* t) { return 0; }
};

DECLARE_SRA_REF_TRAITS(SRAMgr, const);
DECLARE_SRA_REF_TRAITS(SRAColumn, const);
DECLARE_SRA_REF_TRAITS(SRATable, const);

class CSraPath;
class CSraMgr;
class CSraRun;
class CSraColumn;

class NCBI_SRAREAD_EXPORT CSraPath
    : public CSraRef<SRAPath>
{
public:
    explicit CSraPath(ENull /*null*/)
        {
        }
    CSraPath(void);
    NCBI_DEPRECATED_CTOR(CSraPath(const string& rep_path,
                                  const string& vol_path));

    NCBI_DEPRECATED
    static string GetDefaultRepPath(void);
    NCBI_DEPRECATED
    static string GetDefaultVolPath(void);

    NCBI_DEPRECATED
    void AddRepPath(const string& rep_path);

    NCBI_DEPRECATED
    void AddVolPath(const string& vol_path);

    string FindAccPath(const string& acc) const;
};

class NCBI_SRAREAD_EXPORT CSraMgr
    : public CSraRef<const SRAMgr>
{
public:
    enum ETrim {
        eNoTrim,
        eTrim
    };

    CSraMgr(ETrim trim = eNoTrim);

    NCBI_DEPRECATED
    CSraMgr(const string& rep_path, const string& vol_path,
            ETrim trim = eNoTrim);

    spotid_t GetSpotInfo(const string& sra, CSraRun& run);

    CRef<CSeq_entry> GetSpotEntry(const string& sra);
    CRef<CSeq_entry> GetSpotEntry(const string& sra, CSraRun& run);

    CSraPath& GetPath(void) const;
    string FindAccPath(const string& acc) const;

    bool GetTrim(void) const
        {
            return m_Trim;
        }

    NCBI_DEPRECATED
    static void RegisterFunctions(void);

protected:
    void x_Init(void);

private:
    mutable CSraPath m_Path;
    bool m_Trim;
};

class NCBI_SRAREAD_EXPORT CSraColumn
    : public CSraRef<const SRAColumn>
{
public:
    CSraColumn(void)
        {
        }
    CSraColumn(const CSraRun& run, const char* name, const char* type)
        {
            Init(run, name, type);
        }

    rc_t TryInitRc(const CSraRun& run, const char* name, const char* type);
    void Init(const CSraRun& run, const char* name, const char* type);
};


class NCBI_SRAREAD_EXPORT CSraRun
    : public CSraRef<const SRATable>
{
public:
    CSraRun(void)
        {
        }
    CSraRun(CSraMgr& mgr, const string& acc)
        {
            Init(mgr, acc);
        }

    void Init(CSraMgr& mgr, const string& acc);

    const string& GetAccession(void) const
        {
            return m_Acc;
        }

    CSeq_inst::TMol GetSequenceType(spotid_t spot_id, uint8_t read_id) const;
    TSeqPos GetSequenceLength(spotid_t spot_id, uint8_t read_id) const;
    CRef<CSeq_entry> GetSpotEntry(spotid_t spot_id) const;

protected:
    void x_DoInit(CSraMgr& mgr, const string& acc);
 
private:
    string m_Acc;
    bool m_Trim;
    CSraColumn m_Name;
    CSraColumn m_Read;
    CSraColumn m_Qual;
    CSraColumn m_SDesc;
    CSraColumn m_RDesc;
    CSraColumn m_TrimStart;
};


class NCBI_SRAREAD_EXPORT CSraValue
{
public:
    enum ECheckRc {
        eCheckRc,
        eNoCheckRc
    };
    CSraValue(const CSraColumn& col, spotid_t id,
              ECheckRc check_rc = eCheckRc);

    size_t GetLength(void) const
        {
            return m_Len;
        }

    rc_t GetRC(void) const
        {
            return m_Error;
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
    CSraStringValue(const CSraColumn& col, spotid_t id,
                    ECheckRc check_rc = eCheckRc)
        : CSraValue(col, id, check_rc)
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
    CSraValueFor(const CSraColumn& col, spotid_t id,
                 CSraValue::ECheckRc check_rc = CSraValue::eCheckRc)
        : CSraValue(col, id, check_rc)
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


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // SRA__READER__SRA__SRAREAD__HPP
