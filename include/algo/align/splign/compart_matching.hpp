#ifndef QCOMP_ALGO_ALIGN_SPLIGN_HPP
#define QCOMP_ALGO_ALIGN_SPLIGN_HPP

/* $Id$
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
* Author:  Yuri Kapustin
*
* Refactoring: Boris Kiryutin
*
* ===========================================================================
*/


#include <corelib/ncbiobj.hpp>
#include <corelib/ncbifile.hpp>
#include <algo/align/util/blast_tabular.hpp>
#include <algo/align/util/compartment_finder.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objmgr/seq_vector.hpp>
#include <objtools/blast/seqdb_reader/seqdb.hpp>



BEGIN_SCOPE(objects)
    class CSeq_id;
END_SCOPE(objects)


BEGIN_NCBI_SCOPE

USING_SCOPE(objects);


class NCBI_XALGOALIGN_EXPORT ISequenceSource
{
private:
    virtual const vector<CSeq_id_Handle>& GetIds(void) const = 0;
    virtual CBioseq_Handle GetSequence(const CSeq_id_Handle&) = 0;

//if the two methods above are implemented, you can leave the rest as is.
public:

    virtual Uint8 GetTotalLength(void) {
        Uint8 res = 0;
        for(int i = 0; i<GetNumSeqs(); ++i) {
            res += GetSeqLength(i);
        }
        return res;
    }
    virtual int GetNumSeqs(void) { return GetIds().size(); }
    //methods to iterate
    virtual void ResetIndex(void) { m_idx = -1; }
    virtual bool GetNext(void) {//returns true if more seqs left
        ++m_idx;
        return m_idx < (int)GetIds().size();
    }
    virtual int GetCurrentIndex() { return m_idx; } // index might not be sequential in some implementations
    virtual int GetSeqLength(int idx) { return (int)GetSequence(GetIds()[idx]).GetSeqVector(CBioseq_Handle::eCoding_Iupac).size(); }
    virtual int GetSeqLength(void) { return GetSeqLength(m_idx); }
//for stand alone splign  performance sake the following functions added for straightforward implementation based on CSeqDB 
    virtual void SetMemoryBound(Uint8 membound) {}
    virtual int GetSeq(const char ** buffer) { return GetSeq(m_idx, buffer); }
    virtual int GetSeq(int idx, const char ** buffer) {
        CSeqVector vec = GetSequence(GetIds()[idx]).GetSeqVector();
        vec.SetRandomizeAmbiguities();
        vec.SetCoding(CSeq_data::e_Ncbi2na);
        string seq;
        vec.GetPackedSeqData(seq);
        m_seq.resize(seq.size());
        copy(seq.begin(), seq.end(), m_seq.begin());
        *buffer = &m_seq[0];
        return (int) vec.size();
    }
    virtual void RetSequence(const char ** buffer) {}
    virtual CConstRef<CSeq_id> GetSeqID(int idx) { return GetIds()[idx].GetSeqId(); }
    virtual CConstRef<CSeq_id> GetSeqID() { return GetSeqID(m_idx); }


    virtual ~ISequenceSource(void) {}
protected:
    int m_idx;
    vector<char> m_seq;//4 letters per byte (ncbi2na)
};


class NCBI_XALGOALIGN_EXPORT CBlastSequenceSource : public ISequenceSource
{
private:
    virtual const vector<CSeq_id_Handle>& GetIds(void) const { return s_ids; } //never called
    virtual CBioseq_Handle GetSequence(const CSeq_id_Handle&) { return CBioseq_Handle(); } // never called
public:

    using ISequenceSource::GetSeq;
    using ISequenceSource::GetSeqLength;
    using ISequenceSource::GetSeqID;

    CBlastSequenceSource(const string& db) : m_seqdb( new CSeqDB(db, CSeqDB::eNucleotide) ) {}
    virtual ~CBlastSequenceSource(void) {}

    virtual Uint8 GetTotalLength(void) { return m_seqdb->GetTotalLength(); }
    virtual int GetNumSeqs(void) { return m_seqdb->GetNumSeqs(); }
    virtual bool GetNext(void) {//returns true if more seqs left
        ++m_idx;
        return m_seqdb->CheckOrFindOID(m_idx);
    }
    virtual int GetSeqLength(int idx) { return  m_seqdb->GetSeqLength(idx); }
    virtual void SetMemoryBound(Uint8 membound) { return m_seqdb->SetMemoryBound(membound); }
    virtual int GetSeq(int idx, const char ** buffer) {return m_seqdb->GetSequence(idx, buffer); }
    virtual void RetSequence(const char ** buffer) {  m_seqdb->RetSequence(buffer); }
    virtual CConstRef<CSeq_id> GetSeqID(int idx) { return m_seqdb->GetSeqIDs(idx).back(); }

protected:
    CRef<CSeqDB> m_seqdb;
    static vector<CSeq_id_Handle> s_ids;
};


class NCBI_XALGOALIGN_EXPORT CElementaryMatching: public CObject
{
public:
    typedef CBlastTabular        THit;
    typedef CRef<CBlastTabular>  THitRef;
    typedef vector<THitRef>      THitRefs;
    typedef CRef<objects::CSeq_align_set> TResults;

    CElementaryMatching(ISequenceSource *qsrc, const string & sdb, const string& FilePath = "."):
        m_FilePath(CDirEntry::DeleteTrailingPathSeparator(FilePath)), m_qsrc(qsrc), m_sdb(sdb), m_XDropOff(s_GetDefaultDropOff())
    {
        x_InitBasic();
    }

    virtual ~CElementaryMatching() {
        x_Cleanup();
    }

    /// Compartmentization parameters - see CCompartmentFinder for details

    /// if true, output goes to cout
    /// if false output is collected and may be returned with GetResults()
    void   SetOutputMethod(bool om) { m_OutputMethod = om; }

    /// returns compartments.
    /// SetOutputMethod must be set to false 
    /// SetHitsOnly must be set to false
    TResults GetResults(void) { return m_Results; }

    void   SetPenalty(const double & penalty) { m_Penalty = penalty; }
    double GetPenalty(void) const { return m_Penalty; }

    void   SetMinIdty(const double & min_idty) { m_MinCompartmentIdty = min_idty; }
    double GetMinIdty(void) const { return m_MinCompartmentIdty; }

    void   SetMinSingletonIdty(const double & idty) { m_MinSingletonIdty = idty; }
    double GetMinSingletonIdty(void) const { return m_MinSingletonIdty; }

    void SetMaxIntron(const size_t max_intron) { m_MaxIntron = max_intron; }
    size_t GetMaxIntron(void) const { return m_MaxIntron; }

    /// Set or clear the "hits only" mode.
    /// @param hits_only
    ///    When true, no compartmentization occurs and only hits are reported.

    void   SetHitsOnly(bool hits_only) { m_HitsOnly = hits_only; }

    /// Controls the max size of an index volume.
    /// Only decrease this if getting an out-ofspace error because of
    /// too many hits per volume.

    void   SetMaxVolSize(size_t max_vol_size) { m_MaxVolSize = max_vol_size; }

    /// Sets the length cut-off for cDNA (query) sequences.

    void   SetMinQueryLength(size_t min_qlen) { m_MinQueryLength = min_qlen; }

    /// In "hits only" mode, sets the min length of hits to report.
    /// No effect in "compartments" mode.

    void   SetMinHitLength(size_t min_hit_len) { m_MinHitLength = min_hit_len; }

    /// Controls the drop-off parameter used to extend hits over possible defects.

    void   SetDropOff(int dropoff) { m_XDropOff = dropoff; }
    int    GetDropOff(void) const { return m_XDropOff; }
    static int s_GetDefaultDropOff(void) { return 5; }

    void Run(void);
    
private:

    typedef vector<string>       TStrings;
    typedef THit::TId            TId;
    typedef vector<TId>          TSeqIds;
    
    struct SSeqInfo {

        SSeqInfo(void):
            m_Start(0),
            m_Length(0),
            m_Oid(-1)
        {}

        SSeqInfo(Uint4 start, Uint4 len, int id):
            m_Start(start),
            m_Length(len),
            m_Oid(id)
        {}
    
        Uint4 m_Start, m_Length;
        int   m_Oid;

        bool operator < (const SSeqInfo& rhs) const {
            return m_Start < rhs.m_Start;
        }

        friend ostream& operator << (ostream& ostr, const SSeqInfo& rhs)
        {
            return  ostr << rhs.m_Oid << '\t' 
                         << rhs.m_Start << '\t' 
                         << rhs.m_Length;
        }
    };

    typedef vector<SSeqInfo> TSeqInfos;

    struct SHitIndexEntry {

        Uint4 m_QueryOfs;
        Uint4 m_SubjOfs;
        Uint2 m_QueryCount;
        Uint2 m_SubjCount;

        friend ostream& operator << (ostream& ostr, const SHitIndexEntry& hie) {

            ostr << hie.m_QueryOfs << '\t' << size_t(hie.m_QueryCount) << '\t'
                 << hie.m_SubjOfs << '\t' << size_t(hie.m_SubjCount) << endl;

            return ostr;
        }
    };

    typedef vector<SHitIndexEntry> THitIndex;

    // A helper class to use in place of vector<bool> or a bitset
    class CBoolVector {
 
    public:
 
        CBoolVector (void): m_pBuffer(0) {}
 
        CBoolVector (Uint8 dim_bits, bool init_value): m_pBuffer(0) {
            Init(dim_bits, init_value);
        }
 
        CBoolVector (Uint8 dim_bits, const Uint8* src): m_pBuffer(0) {
            Init(dim_bits, src);
        }
 
        ~CBoolVector() {
            Clear();
        }
 
        void Init(Uint8 dim_bits, bool init_value) {
 
            if(dim_bits % 64 > 0) {
                NCBI_THROW(CException, eUnknown,
                           "CBoolVector: bit dim not multiple of 64");
            }
 
            const Uint8 X (init_value? numeric_limits<Uint8>::max(): 0);
            Clear();
            m_pBuffer = new TBuffer;
            m_pBuffer->assign(dim_bits / 64, X);
        }
 
        void Init(Uint8 dim_bits, const Uint8* src) {
 
            if(dim_bits % 64 > 0) {
                NCBI_THROW(CException, eUnknown,
                           "CBoolVector: bit dim not multiple of 64");
            }

            Clear();
            const Uint8 dimw (dim_bits / 64);
            m_pBuffer = new TBuffer (dimw);
            copy(src, src + dimw, &(m_pBuffer->front()));
        }
 
        void Clear(void) {
            delete m_pBuffer;
            m_pBuffer = 0;
        }
 
        bool get_at (const Uint8 idx) const {
            return 1 & ((*m_pBuffer)[idx >> 6] >> (idx & 0x3F));
        }
 
        void set_at (const Uint8 idx) {
            (*m_pBuffer)[idx >> 6] |= (Uint8(1) << (idx & 0x3F));
        }
 
        void reset_at (const Uint8 idx) {
            (*m_pBuffer)[idx >> 6] &= ~(Uint8(1) << (idx & 0x3F));
        }
 
        const Uint8 * GetBuffer (void) const {
            return & (m_pBuffer->front());
        }
 
        Uint8 GetSetCount(void) const {
            Uint8 rv (0);
            ITERATE(TBuffer, ii, (*m_pBuffer)) {
                Uint8 w (*ii);
                for(size_t i(0); i < 64; ++i) {
                    if(w & 1) ++rv;
                    w >>= 1;
                }
            }
            return rv;
        }
 
    private:
     
        typedef vector<Uint8> TBuffer;
        TBuffer * m_pBuffer;
    };


    enum EIndexMode {
        eIM_Genomic,
        eIM_cDNA
    };

    double                    m_MinCompartmentIdty, m_Penalty, m_MinSingletonIdty;

    size_t                    m_MaxIntron;

    string                    m_lbn_q;
    string                    m_lbn_s;
    string                    m_FilePath; //folder path for index and temporary files

    TSeqIds                   m_GenomicSeqIds;
    TSeqIds                   m_cDNASeqIds;

    TSeqInfos                 m_SeqInfos_Genomic;
    TSeqInfos                 m_SeqInfos_cdna;

    TSeqInfos::const_iterator m_ii_cdna;
    TSeqInfos::const_iterator m_ii_genomic;

    const char *              m_CurSeq_cDNA;
    const char *              m_CurSeq_Genomic;

    bool                      m_HitsOnly;
    bool                      m_CurGenomicStrand;

    size_t                    m_MaxVolSize;
    size_t                    m_MinQueryLength;
    size_t                    m_MaxQueryLength;
    size_t                    m_MinHitLength;

    ISequenceSource *         m_qsrc;
    string                    m_sdb;

    int                       m_XDropOff;

    CBoolVector               m_Mers;

    bool m_OutputMethod;
    TResults m_Results;

    // explicit default construction not supported
    CElementaryMatching(void) {}

    void   x_InitBasic(void);
    void   x_Cleanup(void);

    TId    x_GetId(EIndexMode mode, Uint4 coord) const;

    void   x_InitFilteringVector (const string& sdb, bool strand);
    void   x_InitParticipationVector(bool strand);

    void   x_CreateIndex (const string& db, EIndexMode index_more, bool strand);
    void   x_CreateIndex (ISequenceSource *m_qsrc, EIndexMode index_more, bool strand);

    size_t x_WriteIndexFile(size_t volume,
                            EIndexMode mode,
                            bool strand, 
                            vector<Uint8>& MersAndCoords);

    void   x_CleanVolumes  (const string& lbn, const TStrings& vol_extensions);

    void   x_Search(bool strand);

    void   x_CreateRemapData(const string& db, EIndexMode mode);
    void   x_CreateRemapData(ISequenceSource *m_qsrc, EIndexMode mode);
    void   x_LoadRemapData  (ISequenceSource *m_qsrc, const string& sdb);

    void   x_CompartVolume (vector<Uint8>* phits);

    void   x_CompartPair   (vector<Uint8>* pvol,
                            TSeqInfos::const_iterator ii_cdna,
                            TSeqInfos::const_iterator ii_genomic,
                            size_t  idx_start, 
                            size_t  idx_stop,
                            size_t* pidx_compacted);

    bool   x_IsMatch(Uint4 q, Uint4 s) const;

    Int8   x_ExtendHit(const Int8 & left_limit,
                       const Int8 & right_limit,
                       THitRef hitref);
    
    static bool s_IsLowComplexity(Uint4 key);
};


END_NCBI_SCOPE

#endif
