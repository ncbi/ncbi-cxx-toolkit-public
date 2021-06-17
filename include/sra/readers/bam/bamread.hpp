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
#include <corelib/ncbi_fast.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objtools/readers/iidmapper.hpp>

#include <sra/readers/bam/bamread_base.hpp>
#include <unordered_map>

struct VFSManager;

//#include <align/bam.h>
struct BAMFile;
struct BAMAlignment;

//#include <align/align-access.h>
struct AlignAccessMgr;
struct AlignAccessDB;
struct AlignAccessRefSeqEnumerator;
struct AlignAccessAlignmentEnumerator;

#include <sra/readers/bam/bamindex.hpp>

BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(objects);

class CSeq_entry;
class CBioseq;
class CSeq_align;
class CSeq_id;
class CBamFileAlign;
class CBamMgr;
class CBamDb;


SPECIALIZE_BAM_REF_TRAITS(AlignAccessMgr, const);
SPECIALIZE_BAM_REF_TRAITS(AlignAccessDB,  const);
SPECIALIZE_BAM_REF_TRAITS(AlignAccessRefSeqEnumerator, );
SPECIALIZE_BAM_REF_TRAITS(AlignAccessAlignmentEnumerator, );
SPECIALIZE_BAM_REF_TRAITS(BAMFile, const);
SPECIALIZE_BAM_REF_TRAITS(BAMAlignment, const);
SPECIALIZE_BAM_REF_TRAITS(VFSManager, );


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
    virtual const char* GetErrCodeString(void) const override;
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


class NCBI_BAMREAD_EXPORT CBamVFSManager
    : public CBamRef<VFSManager>
{
    typedef CBamRef<VPath> TParent;
public:
    CBamVFSManager(void)
        {
            x_Init();
        }

private:
    void x_Init();
};


class NCBI_BAMREAD_EXPORT CBamMgr
{
public:
    CBamMgr(void);

    const CBamVFSManager& GetVFSManager() const
    {
        return m_VFSMgr;
    }
    const CBamRef<const AlignAccessMgr>& GetAlignAccessMgr() const
    {
        return m_AlignAccessMgr;
    }

private:
    CBamRef<const AlignAccessMgr> m_AlignAccessMgr;
    CBamVFSManager m_VFSMgr;
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
    static int GetDebugLevel();
    
    bool UsesAlignAccessDB() const
        {
            return m_AADB;
        }
    bool UsesRawIndex() const
        {
            return m_RawDB;
        }
    CBamRawDb& GetRawDb()
        {
            return *m_RawDB;
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

    struct STagInfo {
        bool operator==(CTempString n) const
            {
                return n.size() == 2 && n[0] == name[0] && n[1] == name[1];
            }
        
        char name[2];
        mutable CRef<CObject_id> id_cache;
    };
    typedef vector<STagInfo> TTagList;
    const TTagList& GetIncludedAlignTags() const
        {
            return m_IncludedAlignTags;
        }
    // return true if tag was included in this call, false if it was included before
    bool IncludeAlignTag(CTempString tag);
    // return true if tag was excluded in this call, false if it wasn't included before
    bool ExcludeAlignTag(CTempString tag);

    CRef<CSeq_id> GetRefSeq_id(const string& label) const;
    CRef<CSeq_id> GetShortSeq_id(const string& str, bool external = false) const;

    TSeqPos GetRefSeqLength(const string& str) const;

    string GetHeaderText(void) const;

#define HAVE_NEW_PILEUP_COLLECTOR

#ifdef HAVE_NEW_PILEUP_COLLECTOR

    struct SPileupValues;

    class NCBI_BAMREAD_EXPORT ICollectPileupCallback
    {
    public:
        virtual ~ICollectPileupCallback();

        // count and previously added values or zeros are multiple of 16
        virtual void AddZerosBy16(TSeqPos count) = 0;
        // count and previously added values or zeros are multiple of 16
        virtual void AddValuesBy16(TSeqPos count, const SPileupValues& values) = 0;
        // final add of less then 16 values
        virtual void AddValuesTail(TSeqPos count, const SPileupValues& values) = 0;
    };

    struct NCBI_BAMREAD_EXPORT SPileupValues
    {
        typedef Uint4 TCount;
        
        enum {
            kStat_A = 0,
            kStat_C = 1,
            kStat_G = 2,
            kStat_T = 3,
            kStat_Gap = 4,
            kStat_Match = 5,
            kStat_Intron = 6,
            kNumStat_ACGT = 4,
            kNumStat = 7
        };

        enum EIntronMode {
            eNoCountIntron,
            eCountIntron
        };
        
        TSeqPos m_RefFrom; // current values array start on ref sequence
        TSeqPos m_RefToOpen; // current values array end on ref sequence
        TSeqPos m_RefStop; // limit of pileup collection on ref sequence
        TSeqPos m_SplitACGTLen;
        EIntronMode m_IntronMode;
        
        bool count_introns() const
        {
            return m_IntronMode != eNoCountIntron;
        }

        struct SCountACGT {
            TCount cc[kNumStat_ACGT];
        };
        CSimpleBufferT<SCountACGT> cc_acgt;
        CSimpleBufferT<TCount> cc_split_acgt[kNumStat_ACGT];
        CSimpleBufferT<TCount> cc_gap;
        CSimpleBufferT<TCount> cc_match;
        CSimpleBufferT<TCount> cc_intron;
        TCount max_count[kNumStat];
        
        SPileupValues();
        explicit SPileupValues(CRange<TSeqPos> ref_range,
                               EIntronMode intron_mode = eNoCountIntron);

        void initialize(CRange<TSeqPos> ref_range,
                        EIntronMode intron_mode = eNoCountIntron);
        void finalize(ICollectPileupCallback* callback);

        const TCount* get_acgt_counts() const
            {
                return cc_acgt.data()->cc;
            }
        void make_split_acgt(TSeqPos len);
        const TCount* get_split_acgt_counts(int k, TSeqPos len) const
            {
                if ( m_SplitACGTLen < len ) {
                    const_cast<SPileupValues*>(this)->make_split_acgt(len);
                }
                return cc_split_acgt[k].data();
            }
        const TCount* get_match_counts() const
            {
                return cc_match.data();
            }
        const TCount* get_gap_counts() const
            {
                return cc_gap.data();
            }
        const TCount* get_intron_counts() const
            {
                return cc_intron.data();
            }
    
        void add_match_graph_pos(TSeqPos pos)
            {
                cc_match[pos] += 1;
            }
        void add_match_graph_range(TSeqPos pos, TSeqPos end)
            {
                for ( ; pos < end; ++pos ) {
                    cc_match[pos] += 1;
                }
            }
        void add_gap_graph_range(TSeqPos pos, TSeqPos end)
            {
                _ASSERT(pos < end);
                cc_gap[pos] += 1;
                cc_gap[end] -= 1;
            }
        void add_intron_graph_range(TSeqPos pos, TSeqPos end)
            {
                _ASSERT(pos < end);
                cc_intron[pos] += 1;
                cc_intron[end] -= 1;
            }
        void add_bases_graph_range(TSeqPos pos, TSeqPos end,
                                   CTempString read, TSeqPos read_pos);
        void add_bases_graph_range_raw(TSeqPos pos, TSeqPos end,
                                       CTempString read, TSeqPos read_pos);

        static const TSeqPos FLUSH_SIZE = 512;

        void decode_gap(TSeqPos len);
        void decode_intron(TSeqPos len);
        void advance_current_beg(TSeqPos ref_pos, ICollectPileupCallback* callback);
        void advance_current_end(TSeqPos ref_end);
        // update pileup collection start, the alignments should be coming sorted by start
        void update_current_ref_start(TSeqPos ref_pos, ICollectPileupCallback* callback)
            {
                if ( callback && ref_pos >= m_RefFrom+FLUSH_SIZE && 2*ref_pos >= m_RefFrom+m_RefToOpen ) {
                    advance_current_beg(ref_pos, callback);
                }
            }
        bool trim_ref_range(TSeqPos& ref_pos, TSeqPos& ref_end)
            {
                _ASSERT(ref_pos < m_RefStop);
                if ( ref_end <= m_RefFrom ) {
                    // completely before
                    return false;
                }
                if ( ref_pos < m_RefFrom ) {
                    ref_pos = m_RefFrom;
                }
                if ( ref_end > m_RefStop ) {
                    ref_end = m_RefStop;
                }
                if ( ref_end > m_RefToOpen ) {
                    advance_current_end(ref_end);
                }
                return true;
            }
        bool trim_ref_range(TSeqPos& ref_pos, TSeqPos& ref_end, TSeqPos& read_pos)
            {
                _ASSERT(ref_pos < m_RefStop);
                if ( ref_end <= m_RefFrom ) {
                    // completely before
                    return false;
                }
                if ( ref_pos < m_RefFrom ) {
                    // skip read
                    read_pos += m_RefFrom - ref_pos;
                    ref_pos = m_RefFrom;
                }
                if ( ref_end > m_RefStop ) {
                    ref_end = m_RefStop;
                }
                if ( ref_end > m_RefToOpen ) {
                    advance_current_end(ref_end);
                }
                return true;
            }
        void add_match_ref_range(TSeqPos ref_pos, TSeqPos ref_end)
            {
                if ( trim_ref_range(ref_pos, ref_end) ) {
                    add_match_graph_range(ref_pos - m_RefFrom,
                                          ref_end - m_RefFrom);
                }
            }
        void add_gap_ref_range(TSeqPos ref_pos, TSeqPos ref_end)
            {
                if ( trim_ref_range(ref_pos, ref_end) ) {
                    add_gap_graph_range(ref_pos - m_RefFrom,
                                        ref_end - m_RefFrom);
                }
            }
        void add_intron_ref_range(TSeqPos ref_pos, TSeqPos ref_end)
            {
                if ( count_introns() && trim_ref_range(ref_pos, ref_end) ) {
                    add_intron_graph_range(ref_pos - m_RefFrom,
                                           ref_end - m_RefFrom);
                }
            }
        void add_bases_ref_range(TSeqPos ref_pos, TSeqPos ref_end,
                                 const CTempString& read, TSeqPos read_pos)
            {
                if ( trim_ref_range(ref_pos, ref_end, read_pos) ) {
                    add_bases_graph_range(ref_pos - m_RefFrom,
                                          ref_end - m_RefFrom,
                                          read, read_pos);
                }
            }
        void add_bases_ref_range_raw(TSeqPos ref_pos, TSeqPos ref_end,
                                     const CTempString& read, TSeqPos read_pos)
            {
                if ( trim_ref_range(ref_pos, ref_end, read_pos) ) {
                    add_bases_graph_range_raw(ref_pos - m_RefFrom,
                                              ref_end - m_RefFrom,
                                              read, read_pos);
                }
            }

        void update_max_counts(TSeqPos len);
        
        TCount get_max_count(int type) const
            {
                return max_count[type];
            }
    };

    size_t CollectPileup(SPileupValues& values,
                         const string& ref_id,
                         CRange<TSeqPos> graph_range,
                         Uint1 map_quality = 0,
                         ICollectPileupCallback* callback = 0,
                         SPileupValues::EIntronMode intron_mode = SPileupValues::eNoCountIntron,
                         TSeqPos gap_to_intron_threshold = kInvalidSeqPos) const;
#endif //HAVE_NEW_PILEUP_COLLECTOR

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
    TTagList m_IncludedAlignTags;
    typedef unordered_map<string, TSeqPos> TRefSeqLengths;
    mutable AutoPtr<TRefSeqLengths> m_RefSeqLengths;
    typedef unordered_map<string, CRef<CSeq_id> > TRefSeqIds;
    mutable AutoPtr<TRefSeqIds> m_RefSeqIds;
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
    explicit CBamString(size_t cap)
        : m_Size(0)
        {
            reserve(cap);
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
    void reserve(size_t min_capacity)
        {
            if ( capacity() < min_capacity ) {
                x_reserve(min_capacity);
            }
        }

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

    char* data()
        {
            return m_Buffer.get();
        }
    void resize(size_t sz)
        {
            _ASSERT(sz+1 <= capacity());
            m_Size = sz;
            data()[sz] = '\0';
        }

private:
    size_t m_Size;
    size_t m_Capacity;
    AutoArray<char> m_Buffer;

    void x_reserve(size_t min_capacity);
    
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

    IIdMapper* GetIdMapper(void) const
        {
            return m_DB->GetIdMapper();
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

    const CBamDb* m_DB;
    CRef<SAADBImpl> m_AADBImpl;
    CRef< CObjectFor<CBamRawDb> > m_RawDB;
    size_t m_RefIndex;
    mutable CRef<CSeq_id> m_CachedRefSeq_id;
};


class NCBI_BAMREAD_EXPORT CBamFileAlign
    : public CBamRef<const BAMAlignment>
{
public:
    explicit CBamFileAlign(const CBamAlignIterator& iter);

    Int4 GetRefSeqIndex(void) const;
    
    Uint2 GetFlags(void) const;
    // returns false if BAM flags are not available
    bool TryGetFlags(Uint2& flags) const;
};


class NCBI_BAMREAD_EXPORT CBamAlignIterator
{
public:
    enum ESearchMode {
        eSearchByOverlap,
        eSearchByStart
    };

    CBamAlignIterator(void);
    explicit
    CBamAlignIterator(const CBamDb& bam_db);
    CBamAlignIterator(const CBamDb& bam_db,
                      const string& ref_id,
                      TSeqPos ref_pos,
                      TSeqPos window = 0,
                      ESearchMode search_mode = eSearchByOverlap);
    CBamAlignIterator(const CBamDb& bam_db,
                      const string& ref_id,
                      TSeqPos ref_pos,
                      TSeqPos window,
                      CBamIndex::EIndexLevel min_level,
                      CBamIndex::EIndexLevel max_level,
                      ESearchMode search_mode = eSearchByOverlap);
    ~CBamAlignIterator();

    CBamAlignIterator(const CBamAlignIterator& iter);
    CBamAlignIterator& operator=(const CBamAlignIterator& iter);

    DECLARE_OPERATOR_BOOL(m_AADBImpl || m_RawImpl);

    IIdMapper* GetIdMapper(void) const
        {
            return m_DB->GetIdMapper();
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

    Int4 GetRefSeqIndex(void) const;
    CTempString GetRefSeqId(void) const;
    TSeqPos GetRefSeqPos(void) const;

    // next segment in template (mate)
    Int4 GetNextRefSeqIndex() const; // -1 if no next segment
    CTempString GetNextRefSeqId(void) const; // "" if no next segment
    TSeqPos GetNextRefSeqPos() const; // kInvalidSeqPos if no next segment

    CTempString GetShortSeqId(void) const;
    CTempString GetShortSeqAcc(void) const;
    CTempString GetShortSequence(void) const;
    TSeqPos GetShortSequenceLength(void) const;

    pair< COpenRange<TSeqPos>, COpenRange<TSeqPos> > GetCIGARAlignment(void) const;
    TSeqPos GetCIGARPos(void) const;
    CTempString GetCIGAR(void) const;
    TSeqPos GetCIGARRefSize(void) const;
    TSeqPos GetCIGARShortSize(void) const;

    // raw CIGAR access
    Uint2 GetRawCIGAROpsCount() const;
    Uint4 GetRawCIGAROp(Uint2 index) const;
    void GetRawCIGAR(vector<Uint4>& raw_cigar) const;
    
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
    bool IsSecondary(void) const;

    Uint2 GetFlags(void) const;
    // returns false if BAM flags are not available
    bool TryGetFlags(Uint2& flags) const;

    CBamAuxIterator GetAuxIterator() const;

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
    bool x_HasAmbiguousMatch() const;
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

        SAADBImpl(const CBamDb::SAADBImpl& db,
                  AlignAccessAlignmentEnumerator* ptr);
        
        TSeqPos GetRefSeqPos() const;
        
        void x_InvalidateBuffers();
        bool x_HasAmbiguousMatch() const;
    };
    struct SRawImpl : public CObject {
        CRef< CObjectFor<CBamRawDb> > m_RawDB;
        CBamRawAlignIterator m_Iter;
        mutable CBamString m_ShortSequence;
        mutable CBamString m_CIGAR;

        explicit
        SRawImpl(CObjectFor<CBamRawDb>& db);
        SRawImpl(CObjectFor<CBamRawDb>& db,
                 const string& ref_label,
                 TSeqPos ref_pos,
                 TSeqPos window,
                 ESearchMode search_mode);
        SRawImpl(CObjectFor<CBamRawDb>& db,
                 const string& ref_label,
                 TSeqPos ref_pos,
                 TSeqPos window,
                 CBamIndex::EIndexLevel min_level,
                 CBamIndex::EIndexLevel max_level,
                 ESearchMode search_mode);
        
        void x_InvalidateBuffers();
    };

    const CBamDb* m_DB;
    CRef<SAADBImpl> m_AADBImpl;
    CRef<SRawImpl> m_RawImpl;
    
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

    typedef CRef<CObject_id> TObjectIdCache;
    struct SCreateCache;
    mutable AutoPtr<SCreateCache> m_CreateCache;

    SCreateCache& x_GetCreateCache(void) const;
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


END_NAMESPACE(objects);
END_NCBI_NAMESPACE;

#endif // SRA__READER__BAM__BAMREAD__HPP
