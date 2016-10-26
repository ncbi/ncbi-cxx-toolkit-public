#ifndef SRA__READER__BAM__BAMREAD__HPP
#define SRA__READER__BAM__BAMREAD__HPP
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
 *   Access to BAM files
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbiobj.hpp>

#include <objects/seqloc/Na_strand.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objtools/readers/iidmapper.hpp>

#include <align/bam.h>
#include <align/align-access.h>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;
class CBioseq;
class CSeq_align;
class CSeq_id;
class CBamFileAlign;

/////////////////////////////////////////////////////////////////////////////
//  CBamException
/////////////////////////////////////////////////////////////////////////////

#ifndef NCBI_EXCEPTION3_VAR
/// Create an instance of the exception with one additional parameter.
# define NCBI_EXCEPTION3_VAR(name, exc_cls, err_code, msg, extra1, extra2) \
    exc_cls name(DIAG_COMPILE_INFO, 0, exc_cls::err_code, msg,          \
                 extra1, extra2)
#endif

#ifndef NCBI_EXCEPTION3
# define NCBI_EXCEPTION3(exc_cls, err_code, msg, extra1, extra2)        \
    NCBI_EXCEPTION3_VAR(NCBI_EXCEPTION_EMPTY_NAME,                      \
                        exc_cls, err_code, msg, extra1, extra2)
#endif

#ifndef NCBI_THROW3
# define NCBI_THROW3(exc_cls, err_code, msg, extra1, extra2)            \
    throw NCBI_EXCEPTION3(exc_cls, err_code, msg, extra1, extra2)
#endif

#ifndef NCBI_RETHROW3
# define NCBI_RETHROW3(prev_exc, exc_cls, err_code, msg, extra1, extra2) \
    throw exc_cls(DIAG_COMPILE_INFO, &(prev_exc), exc_cls::err_code, msg, \
                  extra1, extra2)
#endif


class CBamRcFormatter
{
public:
    explicit CBamRcFormatter(rc_t rc)
        : m_RC(rc)
        {
        }

    rc_t GetRC(void) const
        {
            return m_RC;
        }
private:
    rc_t m_RC;
};
NCBI_BAMREAD_EXPORT
ostream& operator<<(ostream& out, const CBamRcFormatter& f);

class NCBI_BAMREAD_EXPORT CBamException
    : EXCEPTION_VIRTUAL_BASE public CException
{
public:
    /// Error types that CBamXxx classes can generate.
    enum EErrCode {
        eOtherError,
        eNullPtr,       ///< Null pointer error
        eAddRefFailed,  ///< AddRef failed
        eInvalidArg,    ///< Invalid argument error
        eInitFailed,    ///< Initialization failed
        eNoData,        ///< Data not found
        eBadCIGAR       ///< Bad CIGAR string
    };
    /// Constructors.
    CBamException(const CDiagCompileInfo& info,
                  const CException* prev_exception,
                  EErrCode err_code,
                  const string& message,
                  EDiagSev severity = eDiag_Error);
    CBamException(const CDiagCompileInfo& info,
                  const CException* prev_exception,
                  EErrCode err_code,
                  const string& message,
                  rc_t rc,
                  EDiagSev severity = eDiag_Error);
    CBamException(const CDiagCompileInfo& info,
                  const CException* prev_exception,
                  EErrCode err_code,
                  const string& message,
                  rc_t rc,
                  const string& param,
                  EDiagSev severity = eDiag_Error);
    CBamException(const CBamException& other);

    ~CBamException(void) throw();

    /// Report "non-standard" attributes.
    virtual void ReportExtra(ostream& out) const;

    virtual const char* GetType(void) const;

    /// Translate from the error code value to its string representation.
    typedef int TErrCode;
    virtual TErrCode GetErrCode(void) const;

    /// Translate from the error code value to its string representation.
    virtual const char* GetErrCodeString(void) const;

    rc_t GetRC(void) const
        {
            return m_RC;
        }

    const string& GetParam(void) const
        {
            return m_Param;
        }

    static void ReportError(const char* msg, rc_t rc);

protected:
    /// Constructor.
    CBamException(void);

    /// Helper clone method.
    virtual const CException* x_Clone(void) const;

private:
    rc_t   m_RC;
    string m_Param;
};


template<class Object>
struct CBamRefTraits
{
};

#define SPECIALIZE_BAM_REF_TRAITS(T, Const)                             \
    template<>                                                          \
    struct CBamRefTraits<Const T>                                       \
    {                                                                   \
        static rc_t x_Release(const T* t) { return T##Release(t); }     \
        static rc_t x_AddRef (const T* t) { return T##AddRef(t); }      \
    }

SPECIALIZE_BAM_REF_TRAITS(AlignAccessMgr, const);
SPECIALIZE_BAM_REF_TRAITS(AlignAccessDB,  const);
SPECIALIZE_BAM_REF_TRAITS(AlignAccessRefSeqEnumerator, );
SPECIALIZE_BAM_REF_TRAITS(AlignAccessAlignmentEnumerator, );
SPECIALIZE_BAM_REF_TRAITS(BAMFile, const);
SPECIALIZE_BAM_REF_TRAITS(BAMAlignment, const);


template<class Object>
class CBamRef
{
protected:
    typedef CBamRef<Object> TSelf;
    typedef CBamRefTraits<Object> TTraits;
public:
    typedef Object TObject;

    CBamRef(void)
        : m_Object(0)
        {
        }
    explicit CBamRef(const TSelf& ref)
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
    ~CBamRef(void)
        {
            Release();
        }

    void Release(void)
        {
            if ( m_Object ) {
                if ( rc_t rc = TTraits::x_Release(m_Object) ) {
                    CBamException::ReportError("Cannot release ref", rc);
                }
                m_Object = 0;
            }
        }
    
    TObject* GetPointer(void) const
        {
            if ( !m_Object ) {
                NCBI_THROW(CBamException, eNullPtr,
                           "Null BAM pointer");
            }
            return m_Object;
        }

    operator TObject*(void) const
        {
            return m_Object;
        }
    TObject* operator->(void) const
        {
            return GetPointer();
        }
    TObject& operator*(void) const
        {
            return *GetPointer();
        }

public:
    void SetReferencedPointer(TObject* ptr)
        {
            Release();
            m_Object = ptr;
        }

    TObject** x_InitPtr(void)
        {
            Release();
            return &m_Object;
        }

protected:
    static TObject* s_AddRef(const TSelf& ref)
        {
            TObject* obj = ref.m_Object;
            if ( obj ) {
                if ( rc_t rc = TTraits::x_AddRef(obj) ) {
                    NCBI_THROW2(CBamException, eAddRefFailed,
                                "Cannot add ref", rc);
                }
            }
            return obj;
        }

private:
    TObject* m_Object;
};


/////////////////////////////////////////////////////////////////////////////
//  CSrzException
/////////////////////////////////////////////////////////////////////////////

class NCBI_BAMREAD_EXPORT CSrzException : public CException
{
public:
    enum EErrCode {
        eOtherError,
        eBadFormat,     ///< Invalid SRZ accession format
        eNotFound       ///< Accession not found
    };
    virtual const char* GetErrCodeString(void) const;
    NCBI_EXCEPTION_DEFAULT(CSrzException,CException);
};


/////////////////////////////////////////////////////////////////////////////
//  CSrzPath
/////////////////////////////////////////////////////////////////////////////

#define SRZ_CONFIG_NAME "analysis.bam.cfg"

class NCBI_BAMREAD_EXPORT CSrzPath
{
public:
    CSrzPath(void);
    CSrzPath(const string& rep_path, const string& vol_path);

    static string GetDefaultRepPath(void);
    static string GetDefaultVolPath(void);

    void AddRepPath(const string& rep_path);
    void AddVolPath(const string& vol_path);

    enum EMissing {
        eMissing_Throw,
        eMissing_Empty
    };
    string FindAccPath(const string& acc, EMissing mising);
    string FindAccPath(const string& acc)
        {
            return FindAccPath(acc, eMissing_Throw);
        }
    string FindAccPathNoThrow(const string& acc)
        {
            return FindAccPath(acc, eMissing_Empty);
        }


protected:
    void x_Init(void);
    
private:
    vector<string> m_RepPath;
    vector<string> m_VolPath;
};


class CBamMgr;
class CBamDb;
class CBamAlignIterator;

class NCBI_BAMREAD_EXPORT CBamMgr
    : public CBamRef<const AlignAccessMgr>
{
public:
    CBamMgr(void);
};

class NCBI_BAMREAD_EXPORT CBamDb
    : public CBamRef<const AlignAccessDB>
{
public:
    CBamDb(void)
        {
        }
    CBamDb(const CBamMgr& mgr, const string& db_name);
    CBamDb(const CBamMgr& mgr, const string& db_name, const string& idx_name);

    const string& GetDbName(void) const
        {
            return m_DbName;
        }
    const string& GetIndexName(void) const
        {
            return m_IndexName;
        }

    void SetIdMapper(IIdMapper* idmapper, EOwnership ownership)
        {
            m_IdMapper.reset(idmapper, ownership);
        }
    IIdMapper* GetIdMapper(void) const
        {
            return m_IdMapper.get();
        }

    CRef<CSeq_id> GetRefSeq_id(const string& str) const;
    CRef<CSeq_id> GetShortSeq_id(const string& str) const;

    TSeqPos GetRefSeqLength(const string& str) const;

    string GetHeaderText(void) const;

private:
    string m_DbName;
    string m_IndexName;
    AutoPtr<IIdMapper> m_IdMapper;
    typedef map<string, TSeqPos> TRefSeqLengths;
    mutable TRefSeqLengths m_RefSeqLengths;
};


class NCBI_BAMREAD_EXPORT CBamString
{
public:
    CBamString(void)
        : m_Size(0), m_Capacity(0)
        {
        }
    explicit CBamString(size_t capacity)
        : m_Size(0)
        {
            reserve(capacity);
        }

    void clear()
        {
            m_Size = 0;
            if ( char* p = m_Buffer.get() ) {
                *p = '\0';
            }
        }
    size_t capacity() const
        {
            return m_Capacity;
        }
    void reserve(size_t min_capacity);

    size_t size() const
        {
            return m_Size;
        }
    bool empty(void) const
        {
            return m_Size == 0;
        }
    const char* data() const
        {
            return m_Buffer.get();
        }
    char operator[](size_t pos) const
        {
            return m_Buffer[pos];
        }
    operator string() const
        {
            return string(data(), size());
        }

protected:
    friend class CBamAlignIterator;
    friend class CBamRefSeqIterator;

    char* x_data()
        {
            return m_Buffer.get();
        }
    void x_resize(size_t size)
        {
            m_Size = size;
        }

private:
    size_t m_Size;
    size_t m_Capacity;
    AutoArray<char> m_Buffer;

private:
    CBamString(const CBamString&);
    void operator=(const CBamString&);
};


inline
CNcbiOstream& operator<<(CNcbiOstream& out, const CBamString& str)
{
    return out.write(str.data(), str.size());
}


class NCBI_BAMREAD_EXPORT CBamRefSeqIterator
{
public:
    explicit CBamRefSeqIterator(const CBamDb& bam_db);

    CBamRefSeqIterator(const CBamRefSeqIterator& iter);
    CBamRefSeqIterator& operator=(const CBamRefSeqIterator& iter);

    DECLARE_OPERATOR_BOOL(!m_LocateRC);

    void SetIdMapper(IIdMapper* idmapper, EOwnership ownership)
        {
            m_IdMapper.reset(idmapper, ownership);
        }
    IIdMapper* GetIdMapper(void) const
        {
            return m_IdMapper.get();
        }

    CBamRefSeqIterator& operator++(void);

    const CBamString& GetRefSeqId(void) const;
    CRef<CSeq_id> GetRefSeq_id(void) const;

    TSeqPos GetLength(void) const;

private:
    typedef rc_t (*TGetString)(const AlignAccessRefSeqEnumerator *self,
                               char *buffer, size_t bsize, size_t *size);

    void x_AllocBuffers(void);
    void x_InvalidateBuffers(void);

    void x_CheckValid(void) const;
    bool x_CheckRC(CBamString& buf,
                   rc_t rc, size_t size, const char* msg) const;
    void x_GetString(CBamString& buf,
                     const char* msg, TGetString func) const;

    CBamRef<AlignAccessRefSeqEnumerator> m_Iter;
    AutoPtr<IIdMapper> m_IdMapper;
    rc_t m_LocateRC;
    mutable CBamString m_RefSeqId;
    mutable CRef<CSeq_id> m_RefSeq_id;
};


class NCBI_BAMREAD_EXPORT CBamFileAlign
    : public CBamRef<const BAMAlignment>
{
public:
    explicit CBamFileAlign(const CBamAlignIterator& iter);

    Uint2 GetFlags(void) const;
    // returns false if BAM flags are not available
    bool TryGetFlags(Uint2& flags) const;
};


class NCBI_BAMREAD_EXPORT CBamAlignIterator
{
public:
    CBamAlignIterator(void);
    explicit CBamAlignIterator(const CBamDb& bam_db);
    CBamAlignIterator(const CBamDb& bam_db,
                      const string& ref_id,
                      TSeqPos ref_pos,
                      TSeqPos window = 0);

    CBamAlignIterator(const CBamAlignIterator& iter);
    CBamAlignIterator& operator=(const CBamAlignIterator& iter);

    DECLARE_OPERATOR_BOOL(!m_LocateRC);

    void SetIdMapper(IIdMapper* idmapper, EOwnership ownership)
        {
            m_IdMapper.reset(idmapper, ownership);
        }
    IIdMapper* GetIdMapper(void) const
        {
            return m_IdMapper.get();
        }

    /// ISpotIdDetector interface is used to detect spot id in case
    /// of incorrect flag combination.
    /// The actual type should be CObject derived.
    class NCBI_BAMREAD_EXPORT ISpotIdDetector {
    public:
        virtual ~ISpotIdDetector(void);

        // The AddSpotId() should append spot id to the short_id argument.
        virtual void AddSpotId(string& short_id,
                               const CBamAlignIterator* iter) = 0;
    };

    void SetSpotIdDetector(ISpotIdDetector* spot_id_detector)
        {
            m_SpotIdDetector = spot_id_detector;
        }
    ISpotIdDetector* GetSpotIdDetector(void) const
        {
            return m_SpotIdDetector.GetNCPointerOrNull();
        }

    CBamAlignIterator& operator++(void);

    const CBamString& GetRefSeqId(void) const;
    TSeqPos GetRefSeqPos(void) const;

    const CBamString& GetShortSeqId(void) const;
    const CBamString& GetShortSeqAcc(void) const;
    const CBamString& GetShortSequence(void) const;

    TSeqPos GetCIGARPos(void) const;
    const CBamString& GetCIGAR(void) const;
    TSeqPos GetCIGARRefSize(void) const;
    TSeqPos GetCIGARShortSize(void) const;

    CRef<CSeq_id> GetRefSeq_id(void) const;
    CRef<CSeq_id> GetShortSeq_id(void) const;
    CRef<CSeq_id> GetShortSeq_id(const string& str) const;
    void SetRefSeq_id(CRef<CSeq_id> seq_id);
    void SetShortSeq_id(CRef<CSeq_id> seq_id);

    CRef<CBioseq> GetShortBioseq(void) const;
    CRef<CSeq_align> GetMatchAlign(void) const;
    CRef<CSeq_entry> GetMatchEntry(void) const;
    CRef<CSeq_entry> GetMatchEntry(const string& annot_name) const;
    CRef<CSeq_annot> GetSeq_annot(void) const;
    CRef<CSeq_annot> GetSeq_annot(const string& annot_name) const;

    bool IsSetStrand(void) const;
    ENa_strand GetStrand(void) const;

    Uint1 GetMapQuality(void) const;

    bool IsPaired(void) const;
    bool IsFirstInPair(void) const;
    bool IsSecondInPair(void) const;

    Uint2 GetFlags(void) const;
    // returns false if BAM flags are not available
    bool TryGetFlags(Uint2& flags) const;

private:
    friend class CBamFileAlign;

    typedef rc_t (*TGetString)(const AlignAccessAlignmentEnumerator *self,
                               char *buffer, size_t bsize, size_t *size);
    typedef rc_t (*TGetString2)(const AlignAccessAlignmentEnumerator *self,
                                uint64_t *start_pos,
                                char *buffer, size_t bsize, size_t *size);

    void x_AllocBuffers(void);
    void x_InvalidateBuffers(void);

    void x_CheckValid(void) const;
    bool x_CheckRC(CBamString& buf,
                   rc_t rc, size_t size, const char* msg) const;
    void x_GetString(CBamString& buf,
                     const char* msg, TGetString func) const;
    void x_GetString(CBamString& buf, uint64_t& pos,
                     const char* msg, TGetString2 func) const;
    void x_GetCIGAR(void) const;
    void x_GetStrand(void) const;

    void x_MapId(CSeq_id& id) const;

    CRef<CSeq_entry> x_GetMatchEntry(const string* annot_name) const;
    CRef<CSeq_annot> x_GetSeq_annot(const string* annot_name) const;

    CBamRef<AlignAccessAlignmentEnumerator> m_Iter;
    AutoPtr<IIdMapper> m_IdMapper;
    CIRef<ISpotIdDetector> m_SpotIdDetector;
    rc_t m_LocateRC;
    mutable CBamString m_RefSeqId;
    mutable CBamString m_ShortSeqId;
    mutable CBamString m_ShortSeqAcc;
    mutable CBamString m_ShortSequence;
    mutable uint64_t m_CIGARPos;
    mutable CBamString m_CIGAR;
    mutable CRef<CSeq_id> m_RefSeq_id;
    mutable CRef<CSeq_id> m_ShortSeq_id;
    enum EStrandValues {
        eStrand_not_read = -2,
        eStrand_not_set = -1
    };
    mutable int m_Strand;
    enum EBamFlagsAvailability {
        eBamFlags_NotTried,
        eBamFlags_NotAvailable,
        eBamFlags_Available
    };
    mutable EBamFlagsAvailability m_BamFlagsAvailability;
};


inline
CRef<CSeq_entry>
CBamAlignIterator::GetMatchEntry(const string& annot_name) const
{
    return x_GetMatchEntry(&annot_name);
}


inline
CRef<CSeq_entry> CBamAlignIterator::GetMatchEntry(void) const
{
    return x_GetMatchEntry(0);
}


inline
CRef<CSeq_annot>
CBamAlignIterator::GetSeq_annot(const string& annot_name) const
{
    return x_GetSeq_annot(&annot_name);
}


inline
CRef<CSeq_annot> CBamAlignIterator::GetSeq_annot(void) const
{
    return x_GetSeq_annot(0);
}


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // SRA__READER__BAM__BAMREAD__HPP
