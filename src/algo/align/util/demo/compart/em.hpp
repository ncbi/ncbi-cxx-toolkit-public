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
* ===========================================================================
*/


#include <corelib/ncbiobj.hpp>
#include <algo/align/util/blast_tabular.hpp>
#include <objects/seqloc/Seq_id.hpp>


BEGIN_SCOPE(objects)
    class CSeq_id;
END_SCOPE(objects)


BEGIN_NCBI_SCOPE

class CElementaryMatching: public CObject {

public:

    CElementaryMatching(const string& qdb, const string & sdb):
        m_qdb(qdb), m_sdb (sdb)
    {
        x_InitBasic();
    }

    virtual ~CElementaryMatching() {
        x_Cleanup();
    }

    void   SetPenalty(const double & penalty) { m_Penalty = penalty; }
    double GetPenalty(void) const { return m_Penalty; }

    void   SetMinIdty(const double & min_idty) { m_MinCompartmentIdty = min_idty; }
    double GetMinIdty(void) const { return m_MinCompartmentIdty; }

    void   SetMinSingletonIdty(const double & idty) { m_MinSingletonIdty = idty; }
    double GetMinSingletonIdty(void) const { return m_MinSingletonIdty; }

    void   SetHitsOnly(bool ho) { m_HitsOnly = ho; }

    void   SetMaxVolSize(size_t max_vol_size) { m_MaxVolSize = max_vol_size; }
    
    void Run(void);
    
private:

    typedef CBlastTabular        THit;
    typedef CRef<CBlastTabular>  THitRef;
    typedef vector<THitRef>      THitRefs;
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

    string                    m_lbn_q;
    string                    m_lbn_s;

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

    string                    m_qdb, m_sdb;

    CBoolVector               m_Mers;


    void   x_InitBasic(void);
    void   x_Cleanup(void);

    TId    x_GetId(EIndexMode mode, Uint4 coord) const;

    void   x_InitFilteringVector (const string& sdb, bool strand);
    void   x_InitParticipationVector(bool strand);

    void   x_CreateIndex (const string& db, EIndexMode index_more, bool strand);

    size_t x_WriteIndexFile(size_t volume,
                            EIndexMode mode,
                            bool strand, 
                            vector<Uint8>& MersAndCoords);

    void   x_CleanVolumes  (const string& lbn, const TStrings& vol_extensions);

    void   x_Search(bool strand);

    void   x_CreateRemapData(const string& db, EIndexMode mode);
    void   x_LoadRemapData  (const string& qdb, const string& sdb);

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
