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

#include <sra/readers/bam/bamread_base.hpp>

//#include <align/bam.h>
struct BAMFile;
struct BAMAlignment;

//#include <align/align-access.h>
struct AlignAccessMgr;
struct AlignAccessDB;
struct AlignAccessRefSeqEnumerator;
struct AlignAccessAlignmentEnumerator;

#include <sra/readers/bam/bamindex.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;
class CBioseq;
class CSeq_align;
class CSeq_id;
class CBamFileAlign;


SPECIALIZE_BAM_REF_TRAITS(AlignAccessMgr, const);
SPECIALIZE_BAM_REF_TRAITS(AlignAccessDB,  const);
SPECIALIZE_BAM_REF_TRAITS(AlignAccessRefSeqEnumerator, );
SPECIALIZE_BAM_REF_TRAITS(AlignAccessAlignmentEnumerator, );
SPECIALIZE_BAM_REF_TRAITS(BAMFile, const);
SPECIALIZE_BAM_REF_TRAITS(BAMAlignment, const);


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
class CBamRefSeqIterator;
class CBamAlignIterator;

class NCBI_BAMREAD_EXPORT CBamMgr
    : public CBamRef<const AlignAccessMgr>
{
public:
    CBamMgr(void);
};

class NCBI_BAMREAD_EXPORT CBamDb
{
public:
    enum EUseAPI {
        eUseDefaultAPI,  // use underlying API determined by config
        eUseAlignAccess, // use VDB AlignAccess module
        eUseRawIndex     // use raw index and BAM file access
    };
    CBamDb(void)
        {
        }
    CBamDb(const CBamMgr& mgr,
           const string& db_name,
           EUseAPI use_api = eUseDefaultAPI);
    CBamDb(const CBamMgr& mgr,
           const string& db_name,
           const string& idx_name,
           EUseAPI use_api = eUseDefaultAPI);

    DECLARE_OPERATOR_BOOL(m_AADB || m_RawDB);
    
    static bool UseRawIndex(EUseAPI use_api);
    
    bool UsesAlignAccessDB() const
        {
            return m_AADB;
        }
    bool UsesRawIndex() const
        {
            return m_RawDB;
        }

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
    CRef<CSeq_id> GetShortSeq_id(const string& str, bool external = false) const;

    TSeqPos GetRefSeqLength(const string& str) const;

    string GetHeaderText(void) const;

private:
    friend class CBamRefSeqIterator;
    friend class CBamAlignIterator;

    struct SAADBImpl : public CObject
    {
        SAADBImpl(const CBamMgr& mgr, const string& db_name);
        SAADBImpl(const CBamMgr& mgr, const string& db_name, const string& idx_name);

        mutable CMutex m_Mutex;
        CBamRef<const AlignAccessDB> m_DB;
    };
    
    string m_DbName;
    string m_IndexName;
    AutoPtr<IIdMapper> m_IdMapper;
    typedef map<string, TSeqPos> TRefSeqLengths;
    mutable AutoPtr<TRefSeqLengths> m_RefSeqLengths;
    CRef<SAADBImpl> m_AADB;
    CRef< CObjectFor<CBamRawDb> > m_RawDB;
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
    operator CTempString() const
        {
            return CTempString(data(), size());
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
    CBamRefSeqIterator();
    explicit CBamRefSeqIterator(const CBamDb& bam_db);

    CBamRefSeqIterator(const CBamRefSeqIterator& iter);
    CBamRefSeqIterator& operator=(const CBamRefSeqIterator& iter);

    DECLARE_OPERATOR_BOOL(m_AADBImpl || m_RawDB);

    void SetIdMapper(IIdMapper* idmapper, EOwnership ownership)
        {
            m_IdMapper.reset(idmapper, ownership);
        }
    IIdMapper* GetIdMapper(void) const
        {
            return m_IdMapper.get();
        }

    CBamRefSeqIterator& operator++(void);

    CTempString GetRefSeqId(void) const;
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

    struct SAADBImpl : public CObject
    {
        CBamRef<AlignAccessRefSeqEnumerator> m_Iter;
        mutable CBamString m_RefSeqIdBuffer;
    };

    CRef<SAADBImpl> m_AADBImpl;
    CRef< CObjectFor<CBamRawDb> > m_RawDB;
    size_t m_RefIndex;
    AutoPtr<IIdMapper> m_IdMapper;
    mutable CRef<CSeq_id> m_CachedRefSeq_id;
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
    explicit
    CBamAlignIterator(const CBamDb& bam_db);
    CBamAlignIterator(const CBamDb& bam_db,
                      const string& ref_id,
                      TSeqPos ref_pos,
                      TSeqPos window = 0);

    CBamAlignIterator(const CBamAlignIterator& iter);
    CBamAlignIterator& operator=(const CBamAlignIterator& iter);

    DECLARE_OPERATOR_BOOL(m_AADBImpl || m_RawImpl);

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

    bool UsesAlignAccessDB() const
        {
            return m_AADBImpl;
        }
    bool UsesRawIndex() const
        {
            return m_RawImpl;
        }
    CBamRawAlignIterator* GetRawIndexIteratorPtr() const
        {
            return m_RawImpl? &m_RawImpl.GetNCObject().m_Iter: 0;
        }
    
    CTempString GetRefSeqId(void) const;
    TSeqPos GetRefSeqPos(void) const;

    CTempString GetShortSeqId(void) const;
    CTempString GetShortSeqAcc(void) const;
    CTempString GetShortSequence(void) const;

    TSeqPos GetCIGARPos(void) const;
    CTempString GetCIGAR(void) const;
    TSeqPos GetCIGARRefSize(void) const;
    TSeqPos GetCIGARShortSize(void) const;

    // raw CIGAR access
    Uint2 GetRawCIGAROpsCount() const;
    Uint4 GetRawCIGAROp(Uint2 index) const;
    
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

    struct SAADBImpl : public CObject {
        CConstRef<CBamDb::SAADBImpl> m_DB;
        CMutexGuard m_Guard;
        CBamRef<AlignAccessAlignmentEnumerator> m_Iter;
        mutable CBamString m_RefSeqId;
        mutable CBamString m_ShortSeqId;
        mutable CBamString m_ShortSeqAcc;
        mutable CBamString m_ShortSequence;
        mutable uint64_t m_CIGARPos;
        mutable CBamString m_CIGAR;
        mutable int m_Strand;

        SAADBImpl(const CBamDb::SAADBImpl& db, AlignAccessAlignmentEnumerator* ptr);
        
        void x_InvalidateBuffers();
    };
    struct SRawImpl : public CObject {
        CRef< CObjectFor<CBamRawDb> > m_RawDB;
        CBamRawAlignIterator m_Iter;
        mutable string m_ShortSequence;
        mutable string m_CIGAR;

        explicit
        SRawImpl(CObjectFor<CBamRawDb>& db);
        SRawImpl(CObjectFor<CBamRawDb>& db,
                 const string& ref_label,
                 TSeqPos ref_pos,
                 TSeqPos window);
        
        void x_InvalidateBuffers();
    };

    CRef<SAADBImpl> m_AADBImpl;
    CRef<SRawImpl> m_RawImpl;
    
    AutoPtr<IIdMapper> m_IdMapper;
    CIRef<ISpotIdDetector> m_SpotIdDetector;
    enum EStrandValues {
        eStrand_not_read = -2,
        eStrand_not_set = -1
    };
    enum EBamFlagsAvailability {
        eBamFlags_NotTried,
        eBamFlags_NotAvailable,
        eBamFlags_Available
    };
    mutable EBamFlagsAvailability m_BamFlagsAvailability;
    mutable CRef<CSeq_id> m_RefSeq_id;
    mutable CRef<CSeq_id> m_ShortSeq_id;
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


inline
Uint2 CBamAlignIterator::GetRawCIGAROpsCount() const
{
    return m_RawImpl->m_Iter.GetCIGAROpsCount();
}


inline
Uint4 CBamAlignIterator::GetRawCIGAROp(Uint2 index) const
{
    return m_RawImpl->m_Iter.GetCIGAROp(index);
}


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // SRA__READER__BAM__BAMREAD__HPP
