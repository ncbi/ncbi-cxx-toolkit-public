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


#include <ncbi_pch.hpp>

#include <math.h>

#include <corelib/ncbi_system.hpp>
#include <corelib/ncbifile.hpp>
#include <util/random_gen.hpp>
#include <algo/align/util/compartment_finder.hpp>
#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include <algo/align/splign/compart_matching.hpp>

BEGIN_NCBI_SCOPE

vector<CSeq_id_Handle> CBlastSequenceSource::s_ids;


namespace {

    // N-mer size for repeat filtering
    const Uint4        kNr                (14);

    // total number of Nr-mers for repeat filtering
    const size_t       kNrMersTotal       (1 << (kNr * 2));

    // percentile used for repeat filtering
    const double       kRepeatsPercentile (0.995);

    // N-mer size for matching
    const Uint4        kN                 (16);

    // total number of N-mers for indexing
    const Uint8        kNMersTotal        (Uint8(1) << (kN * 2));

    // extension to indicate repeat filtering table
    const string kFileExt_Masked          (".rep");

    // extension to indicate ID and coordinate remapping table
    const string kFileExt_Remap           (".idc");
    
    const string kFileExt_Offsets         (".ofs");
    const string kFileExt_Positions       (".pos");

    const Uint8  kUI8_LoWord              (0xFFFFFFFF);
    const Uint8  kUI8_LoByte              (kUI8_LoWord >> 24);
    const Uint8  kUI8_MidWord             (kUI8_LoWord << 16);
    const Uint8  kUI8_HiWord              (kUI8_LoWord << 32);
    const Uint8  kUI8_LoFive              (kUI8_LoWord | (kUI8_LoWord << 8));

    const Uint8  kUI8_LoHalfWordEachByte  ((Uint8(0x0F0F0F0F) << 32) | 0x0F0F0F0F);

    const Uint4  kUI4_Lo28                (0xFFFFFFFF >> 4);

    const Uint8  kUI8_SeqDb_lo            (0x03030303);
    const Uint8  kUI8_SeqDb               ((kUI8_SeqDb_lo << 32) | kUI8_SeqDb_lo);

    const Int8   kDiagMax                 (numeric_limits<Int8>::max());

    const Uint8  kSeqDbMemBound           (512 * 1024 * 1024);
    const size_t kMapGran                 (512 * 1024 * 1024);
}


char DecodeSeqDbChar(Uint1 c)
{
    char rv ('*'); 
    switch(c) {
    case 0: rv = 'A'; break;
    case 1: rv = 'C'; break;
    case 2: rv = 'G'; break;
    case 3: rv = 'T'; break;
    }
    return rv;
}

//#define ALGO_ALIGN_SPLIGN_QCOMP_DEBUG
#ifdef ALGO_ALIGN_SPLIGN_QCOMP_DEBUG

template<typename T>
string GetBinString(T v)
{
    vector<bool> vb;

    const size_t bitdim (sizeof(T) * 8);
    for(size_t i(0); i < bitdim; ++i) {
        vb.push_back(v & 1);
        v >>= 1;
    }

    CNcbiOstrstream ostr;
    for(size_t i (bitdim); i > 0; --i) {
        if(i < bitdim && i % 8 == 0) {
            ostr << ' ';
        }
        ostr << vb[i-1];
    }

    const string rv = CNcbiOstrstreamToString(ostr);
    return rv;
}

#endif

void CheckWrittenFile(const string& filename, const Uint8& len_bytes)
{
    Int8 reported_len (-1);
    for(size_t attempt(0); attempt < 1; ++attempt) {
        reported_len = CFile(filename).GetLength();
        if(reported_len >= 0 && Uint8(reported_len) == len_bytes) {
            return;
        }
        else {
            SleepSec(1);
        }
    }

    CNcbiOstrstream ostr;

    if(reported_len < 0) {
        ostr << "Cannot write " << filename 
             << " (error code = " << reported_len << "). ";
    }
    else {
        ostr << "The size of " << filename << " (" << reported_len << ')'
             << " is different from the expected " << len_bytes << ". ";
    }
    ostr << "Please make sure there is enough disk space.";

    const string errmsg = CNcbiOstrstreamToString(ostr);
    NCBI_THROW(CException, eUnknown, errmsg);
}


//
// Brute-force reversal & complementation

template<typename T>
T ReverseAndComplement(T v)
{
    T rv (0);
    for(size_t i (0), imax (4*sizeof(T)); i < imax; ++i, v >>= 2) {
        rv = (rv << 2) | (v & 3);
    }
    rv = ~rv;

    return rv;
}


//
// Table-based bit reversal & complementation

template<typename T>
class CReverseAndComplement {

public:

    CReverseAndComplement(void) {

        m_Table.resize(256);

        for(Uint1 i(1); i < 0xFF; ++i) {
            m_Table[i] = ReverseAndComplement(i);
        }

        m_Table[0]     = 0xFF;
        m_Table[0xFF]  = 0;
    }

    T operator() (T v) const {

        T rv (0);
        for(size_t i(0), imax(sizeof(T)); i < imax; ++i)  {
            rv <<= 8;
            rv |= m_Table[v & 0xFF];
            v >>= 8;
        }
        return rv;
    }

private:

    vector<Uint1> m_Table;
};


namespace {
    CReverseAndComplement<Uint4> g_RC;
}


bool CElementaryMatching::s_IsLowComplexity(Uint4 key)
{
    // evaluate the lower 28 bits (14 residues) only

    const Uint4 kMaxTwoBaseContent (14);

    vector<Uint4> counts(4, 0);

    for(Uint4 k = 0; k < 14; ++k) {
        ++counts[0x00000003 & key];
        key >>= 2;
    }

    const Uint4 ag (counts[0] + counts[1]);
    const Uint4 at (counts[0] + counts[2]);
    const Uint4 ac (counts[0] + counts[3]);
    const Uint4 gt (counts[1] + counts[2]);
    const Uint4 gc (counts[1] + counts[3]);
    const Uint4 tc (counts[2] + counts[3]);

    return
        ag >= kMaxTwoBaseContent || at >= kMaxTwoBaseContent ||
        ac >= kMaxTwoBaseContent || gt >= kMaxTwoBaseContent ||
        gc >= kMaxTwoBaseContent || tc >= kMaxTwoBaseContent;
}


string GetLocalBaseName(const string& extended_name, const string & sfx)
{
    string dir, base, ext;
    CDirEntry::SplitPath(extended_name, &dir, &base, &ext);
    string rv (base);
    if(!ext.empty()) {
        rv += ext;
    }
    rv += "." + sfx;
    return rv;
}

string ReplaceExt(const string& extended_name, const string & new_ext)
{
    string dir, base, ext;
    CDirEntry::SplitPath(extended_name, &dir, &base, &ext);
    string rv;
    if(!dir.empty()) {
        rv = dir; // + CDirEntry::GetPathSeparator();
    }
    if(!base.empty()) {
        rv += base;
    }
    rv += new_ext;

    return rv;
}


//#define COMPART2_TEMPFILE_USE_MEM_FILE

template <typename VectorT>
string g_SaveToTemp(const VectorT& v)
{
    typedef typename VectorT::value_type TElem;

    const string filename(CFile::GetTmpNameEx(".", "splqcomp_"));
    const Uint8 len_bytes (v.size() * sizeof(TElem));

#ifndef COMPART2_TEMPFILE_USE_MEM_FILE
    {
        CNcbiOfstream tempcgrfile (filename.data(), IOS_BASE::binary);
        tempcgrfile.write((const char* ) & v.front(), len_bytes);
        tempcgrfile.close();
    }
#else
    {
        CMemoryFile mf (filename, CMemoryFile::eMMP_Write,
                        CMemoryFile::eMMS_Shared,
                        0, 0, CMemoryFile::eCreate, len_bytes);
        void * pvoid (mf.Map());
        
        if(v.size() > 0) {
            TElem * dest (reinterpret_cast<TElem*>(pvoid));
            const TElem * src (& v.front());
            copy(src, src + len_bytes / sizeof(TElem), dest);
        }
    }
#endif

    CheckWrittenFile(filename, len_bytes);

    return filename;
}


template <typename VectorT>
void g_RestoreFromTemp(const string& filename, VectorT* pvd)
{
    typedef typename  VectorT::value_type TElem;

    VectorT& v = *pvd;

    const Uint8 dim (CFile(filename).GetLength() / sizeof(TElem));

#ifndef COMPART2_TEMPFILE_USE_MEM_FILE
    CNcbiIfstream tempcgrfile (filename.data(), IOS_BASE::binary);
    tempcgrfile.read((char* ) & v.front(), dim * sizeof(TElem));
#else
    CMemoryFile mf (filename);
    const TElem * src (reinterpret_cast<TElem*>(mf.Map()));
    copy(src, src + dim, &v.front());
#endif

    CFile(filename).Remove();
}


void CElementaryMatching::x_InitParticipationVector(bool strand)
{
    // Skim over the query offset volumes and set the bitmask

    m_Mers.Init(kNMersTotal, false);

    CDir dir (CDir::GetCwd());
    const string sfx (string(strand?".p": ".m") + ".v*");
    const string mask_ofs_q (m_lbn_q + sfx + kFileExt_Offsets);
    CDir::TEntries vols_ofs_q (dir.GetEntries(mask_ofs_q));
    ITERATE(CDir::TEntries, ii, vols_ofs_q) {

        const string  filename ((*ii)->GetPath());
        const Int8    vol_length (CFile(filename).GetLength());

        CMemoryFile   mf (filename);
        for(const Uint8 * p8 (reinterpret_cast<const Uint8 *> (mf.Map())),
                * p8e (p8 + vol_length / 8);  p8 != p8e;
            m_Mers.set_at(*p8++ & kUI8_LoWord));
        mf.Unmap();
    }

    m_Mers.reset_at(0);
}


void CElementaryMatching::x_InitFilteringVector(const string& sdb, bool strand)
{
    // for plus strand create using the actual sequence data;
    // use the plus strand table to init the minus strand table

    if(strand) {

        // create repeat filtering table (genome);
        CRef<CSeqDB> subjdb (new CSeqDB(sdb, CSeqDB::eNucleotide));
        subjdb->SetMemoryBound(kSeqDbMemBound);
        if(subjdb->GetTotalLength() > numeric_limits<Uint4>::max())
        {
            CNcbiOstrstream ostr;
            ostr << "Sequence volumes with total length exceeding " 
                 << numeric_limits<Uint4>::max()
                 << " are not yet supported. Please split your FASTA file and re-run "
                 << " formatdb.";
            const string err = CNcbiOstrstreamToString(ostr);
            NCBI_THROW(CException, eUnknown, err);
        }

        Uint4 current_offset (0);

        auto_ptr<vector<Uint4> > pNrCounts (new vector<Uint4> (kNrMersTotal, 0));
        vector<Uint4> & NrCounts (*pNrCounts);

        cerr << " Scanning " << subjdb->GetNumSeqs() << " genomic sequences ... ";
        for(int oid (0); subjdb->CheckOrFindOID(oid); ++oid)
        {
            const char * pcb (0);
            const Uint4 bases (subjdb->GetSequence(oid, &pcb));
            const char * pcbe (pcb + bases / 4);
            uintptr_t npcb (reinterpret_cast<uintptr_t>(pcb)), npcb0 (npcb);
            npcb -= npcb % 8;
            if(npcb < npcb0) npcb += 8;
            const size_t gcbase (4*(npcb - npcb0));
            const Uint8 * pui8 (reinterpret_cast<const Uint8*>(npcb));
            const Uint8 * pui8_e (reinterpret_cast<const Uint8*>(pcbe));

            for(size_t gccur (current_offset + gcbase); pui8 < pui8_e; ++pui8) {

                Uint8 ui8 (*pui8);

                // counting the hi 28 bits correspond to 
                // a subsequence at positions 1,2,5-16
#define QCOMP_COUNT_NrMERS  {{ \
                if(gccur + 16 >= current_offset + bases) { \
                    break; \
                } \
                const Uint4 mer4 (ui8 & kUI8_LoWord); \
                ++NrCounts[mer4 >> 4];           \
                gccur += 4; \
                ui8 >>= 8; \
                }}

                QCOMP_COUNT_NrMERS;
                QCOMP_COUNT_NrMERS;
                QCOMP_COUNT_NrMERS;
                QCOMP_COUNT_NrMERS;

                if(pui8 + 1 < pui8_e) {

                    ui8 |= (*(pui8 + 1) << 32);

                    QCOMP_COUNT_NrMERS;
                    QCOMP_COUNT_NrMERS;
                    QCOMP_COUNT_NrMERS;
                    QCOMP_COUNT_NrMERS;
                }

#undef QCOMP_COUNT_NMERS
            }

            subjdb->RetSequence(&pcb);

            current_offset += bases;
        } // OIDs

        subjdb.Reset(0);

        cerr << "Ok" << endl;
        cerr << " Constructing FV ... ";

        string filename_temp_01;
        auto_ptr<vector<Uint4> > pNrCounts2 (new vector<Uint4>);
        vector<Uint4> & NrCounts2 (* pNrCounts2);
        try {
            NrCounts2 = NrCounts;
        }
        catch(...) {
            filename_temp_01 = g_SaveToTemp(NrCounts);
            pNrCounts2.reset(0);
        }

        // find the percentile
        size_t total_mers (0);
        ITERATE(vector<Uint4>, ii, NrCounts) {
            if(*ii > 0) ++total_mers;
        }
        const size_t percentile ((size_t)(kNrMersTotal - 
                                          total_mers * (1 - kRepeatsPercentile)));
        nth_element(NrCounts.begin(), NrCounts.begin() + percentile, NrCounts.end());
        const Uint4 max_repeat_count (NrCounts[percentile]);

        if(filename_temp_01.empty()) {
            NrCounts = NrCounts2;
        }
        else {
            g_RestoreFromTemp(filename_temp_01, &NrCounts);
        }
        pNrCounts2.reset(0);

        m_Mers.Init(kNrMersTotal, false);
        for(size_t i (0); i < kNrMersTotal; ++i) {
            const bool v (NrCounts[i] <= max_repeat_count && !s_IsLowComplexity(i));
            if(v) {
                m_Mers.set_at(i);
            }
        }
        pNrCounts.reset(0);

        // serialize
        const string masked_filename (m_lbn_s + kFileExt_Masked);
        const Uint8 len_bytes (kNrMersTotal / 8);
        {{
            CMemoryFile mf (masked_filename, CMemoryFile::eMMP_Write,
                            CMemoryFile::eMMS_Shared, 0, kNrMersTotal / 8,
                            CMemoryFile::eCreate,
                            len_bytes);
            Uint8* dest (reinterpret_cast<Uint8*>(mf.Map()));

            const Uint8 * src (m_Mers.GetBuffer());
            copy(src, src + kNrMersTotal / 64, dest);
        }}

        CheckWrittenFile(masked_filename, len_bytes);
    }
    else {

        cerr << " Reading/transforming FV ... ";

        // read the plus strand vector and trasnform
        const string masked_filename (m_lbn_s + kFileExt_Masked);

        CMemoryFile mf (masked_filename);
        const Uint8 * p8 (reinterpret_cast<Uint8*>(mf.Map()));
        CBoolVector nrmers_plus (kNrMersTotal, p8);
        mf.Unmap();

        m_Mers.Init(kNrMersTotal, false);

        for(size_t i(0); i < kNrMersTotal; ++i) {
            if(nrmers_plus.get_at(i)) {
                // todo: optimize using tables
                const size_t irc (g_RC(Uint4(i) << 4) & kUI4_Lo28);
                m_Mers.set_at(irc);
            }
        }
    }

    cerr << "Ok" << endl;
}


void CElementaryMatching::x_CreateRemapData(const string& db, EIndexMode mode)
{
    // create ID and coordinate remapping tables

    CSeqDB blastdb (db, CSeqDB::eNucleotide);

    TSeqInfos seq_infos;
    const size_t num_seqs (blastdb.GetNumSeqs());
    seq_infos.reserve(num_seqs);

    Uint4 current_offset (0);
    for(int oid (0); blastdb.CheckOrFindOID(oid); ++oid) {

        const int seqlen (blastdb.GetSeqLength(oid));
        if(seqlen <= 0 || size_t(seqlen) >= 0xFFFFFFFF) {
            CNcbiOstrstream ostr;
            ostr << "Cannot create remap data for:\t" <<
                blastdb.GetSeqIDs(oid).back()->GetSeqIdString(true);
            const string err = CNcbiOstrstreamToString(ostr);
            NCBI_THROW(CException, eUnknown, err);
        }

        const Uint4 bases (seqlen);
        seq_infos.push_back(SSeqInfo(current_offset, bases, oid));
        current_offset += bases;
    }

    const string remap_filename ((mode == eIM_Genomic? m_lbn_s: m_lbn_q) +
                                 kFileExt_Remap);

    CNcbiOfstream ofstr_remap (remap_filename.c_str(), IOS_BASE::binary);
    const Uint8 len_bytes (seq_infos.size() * sizeof(SSeqInfo));
    ofstr_remap.write((const char*) &seq_infos.front(), len_bytes);
    ofstr_remap.close();

    CheckWrittenFile(remap_filename, len_bytes);

    cerr << " Remap data created for " << db << "; max offset = "
         << current_offset << endl;
}

void CElementaryMatching::x_CreateRemapData(ISequenceSource *m_qsrc, EIndexMode mode)
{
    // create ID and coordinate remapping tables

    TSeqInfos seq_infos;
    const size_t num_seqs (m_qsrc->GetNumSeqs());
    seq_infos.reserve(num_seqs);

    Uint4 current_offset (0);
    for(m_qsrc->ResetIndex(); m_qsrc->GetNext(); ) {

        const int seqlen (m_qsrc->GetSeqLength());
        if(seqlen <= 0 || size_t(seqlen) >= 0xFFFFFFFF) {
            CNcbiOstrstream ostr;
            ostr << "Cannot create remap data for:\t" <<
                m_qsrc->GetSeqID()->GetSeqIdString(true);
            const string err = CNcbiOstrstreamToString(ostr);
            NCBI_THROW(CException, eUnknown, err);
        }

        const Uint4 bases (seqlen);
        seq_infos.push_back(SSeqInfo(current_offset, bases, m_qsrc->GetCurrentIndex()));
        current_offset += bases;
    }

    const string remap_filename ((mode == eIM_Genomic? m_lbn_s: m_lbn_q) +
                                 kFileExt_Remap);

    CNcbiOfstream ofstr_remap (remap_filename.c_str(), IOS_BASE::binary);
    const Uint8 len_bytes (seq_infos.size() * sizeof(SSeqInfo));
    ofstr_remap.write((const char*) &seq_infos.front(), len_bytes);
    ofstr_remap.close();

    CheckWrittenFile(remap_filename, len_bytes);

    cerr << " Remap data created for sequences; max offset = "
         << current_offset << endl;
}


void CElementaryMatching::x_CreateIndex(const string& db, EIndexMode mode, bool strand)
{
    // sort all adjacent genomic N-mers and their positions
    // except for those marked in the Nr-mer bit vector

    cerr << " Scanning " << db << " for N-mers and their positions." << endl;

    if(m_Mers.get_at(0)) {
        NCBI_THROW(CException, eUnknown, "NULL mer not excluded!");
    }

    const size_t mcidx_max (m_MaxVolSize / 8);
    vector<Uint8> MersAndCoords (mcidx_max, 0);
    size_t mcidx (0);
    size_t current_offset (0);

    CRef<CSeqDB> blastdb (new CSeqDB(db, CSeqDB::eNucleotide));
    blastdb->SetMemoryBound(kSeqDbMemBound);

    const Uint8 blastdb_total_length (blastdb->GetTotalLength());
    if((mode == eIM_Genomic && blastdb_total_length / kN > numeric_limits<Uint4>::max())
       || (mode == eIM_cDNA && blastdb_total_length > numeric_limits<Uint4>::max()))
    {
        CNcbiOstrstream ostr;
        ostr << "Sequence volumes with total length exceeding " 
             << numeric_limits<Uint4>::max()
             << " are not yet supported. Please split your FASTA file and re-run "
             << " formatdb.";
        const string err = CNcbiOstrstreamToString(ostr);
        NCBI_THROW(CException, eUnknown, err);
    }

    size_t volume(0);
    for(int oid (0); blastdb->CheckOrFindOID(oid); ++oid) {

        const char * pcb (0);
        const Uint4 bases (blastdb->GetSequence(oid, &pcb));
        const char * pce (pcb + bases/4);
        uintptr_t npcb (reinterpret_cast<uintptr_t>(pcb)), npcb0 (npcb);
        npcb -= npcb % 8;
        if(npcb < npcb0) npcb += 8;
        const Uint4  gcbase (4*(npcb - npcb0));
        const Uint8* pui8_start (reinterpret_cast<const Uint8*>(npcb));
        const Uint8* pui8 (pui8_start);

        // It helps not to break volumes in the middle of a sequence.
        // We explicitly check here if the volume is close to its limit.

        const size_t max_new_elems (mode == eIM_Genomic? size_t(8.0 * bases / kN):
                                    bases);

        if(mcidx > 1000 && mcidx + max_new_elems >= mcidx_max) {
            MersAndCoords.resize(mcidx);
            x_WriteIndexFile(++volume, mode, strand, MersAndCoords);
            MersAndCoords.assign(mcidx_max, 0);
            mcidx = 0;
        }

        if(mode == eIM_Genomic) {

            // index every other position
            for(size_t gccur (current_offset + gcbase); 
                gccur + 16 < current_offset + bases && mcidx < mcidx_max;
                ++pui8)
            {
                Uint8 w8 (*pui8);
                
#define QCOMP_PREPARE_SHIFTED_GENOMIC_IDX \
                size_t gccur2 (gccur + 2); \
                const Uint8 ui8_2930 (w8 >> 60); \
                Uint8 ui8_ls4 (w8 << 4); \
                const Uint8 ui8_mask (ui8_ls4 & kUI8_LoHalfWordEachByte); \
                ui8_ls4 &= kUI8_LoHalfWordEachByte << 4; \
                ui8_ls4 |= (ui8_mask >> 16) | (ui8_2930 << 48);

                QCOMP_PREPARE_SHIFTED_GENOMIC_IDX;

#define QCOMP_CREATE_GENOMIC_IDX(w8,gccur) \
                { \
                    if(gccur + 16 >= current_offset + bases) { \
                        break; \
                    } \
                    const Uint8 mer (w8 & kUI8_LoWord); \
                    if(strand) { \
                        if(m_Mers.get_at(mer)) { \
                            MersAndCoords[mcidx++] = (mer << 32) | gccur; \
                        } \
                    } \
                    else { \
                        const Uint4 rc (g_RC(Uint4(mer))); \
                        if(m_Mers.get_at(rc)) { \
                            MersAndCoords[mcidx++] = (Uint8(rc) << 32) | \
                                ((current_offset + bases - gccur - 16) \
                                 + current_offset); \
                        } \
                    } \
                    gccur += 4; \
                    w8 >>= 8; \
                }

                QCOMP_CREATE_GENOMIC_IDX(w8,gccur);
                QCOMP_CREATE_GENOMIC_IDX(w8,gccur);
                QCOMP_CREATE_GENOMIC_IDX(w8,gccur);
                QCOMP_CREATE_GENOMIC_IDX(w8,gccur);

                QCOMP_CREATE_GENOMIC_IDX(ui8_ls4,gccur2);
                QCOMP_CREATE_GENOMIC_IDX(ui8_ls4,gccur2);
                QCOMP_CREATE_GENOMIC_IDX(ui8_ls4,gccur2);
                QCOMP_CREATE_GENOMIC_IDX(ui8_ls4,gccur2);

                if(gccur + 32 >= current_offset + bases) {
                    break;
                }
                else {

                    w8 |= ((*(pui8 + 1)) << 32);

                    QCOMP_PREPARE_SHIFTED_GENOMIC_IDX;

                    QCOMP_CREATE_GENOMIC_IDX(w8,gccur);
                    QCOMP_CREATE_GENOMIC_IDX(w8,gccur);
                    QCOMP_CREATE_GENOMIC_IDX(w8,gccur);
                    QCOMP_CREATE_GENOMIC_IDX(w8,gccur);

                    QCOMP_CREATE_GENOMIC_IDX(ui8_ls4,gccur2);
                    QCOMP_CREATE_GENOMIC_IDX(ui8_ls4,gccur2);
                    QCOMP_CREATE_GENOMIC_IDX(ui8_ls4,gccur2);
                    QCOMP_CREATE_GENOMIC_IDX(ui8_ls4,gccur2);
                }
#undef QCOMP_PREPARE_SHIFTED_GENOMIC_IDX
#undef QCOMP_CREATE_GENOMIC_IDX
            }
        }
        else {  // cDNA mode

            if(bases >= m_MinQueryLength && bases <= m_MaxQueryLength) {
            
                // head
                Uint8 fivebytes (0);
                for(const char * p (pcb), 
                    *pche ( (reinterpret_cast<const char*>(pui8)) + 5 );
                    p < pche; ++p)
                {
                    const Uint8 c8 (*p & kUI8_LoByte);
                    const Uint8 ui8curbyte (c8);
                    if(pcb + 5 > p) {
                        fivebytes = (fivebytes >> 8) | (ui8curbyte << 32);
                    }
                    else {

                        for(Uint4 k(0); k < 4; ++k) {
                        
                            const Uint4 mer (fivebytes & kUI8_LoWord);
                            if(m_Mers.get_at(strand? (mer >> 4): (mer & kUI4_Lo28))) {
                                const Uint8 ui8   (mer);
                                const Uint4 coord (current_offset + 
                                                   4*(p - pcb - 5) + k);
                                MersAndCoords[mcidx++] = (ui8 << 32) | coord;
                            }

                            fivebytes &= kUI8_LoFive;
                            fivebytes <<= 2;
                            const Uint8 ui8m ((fivebytes & kUI8_SeqDb) >> 16);
                            fivebytes &= ~kUI8_SeqDb;
                            fivebytes |= ui8m;
                        }                   
                        fivebytes |= ui8curbyte << 32;
                    }
                }
            
                // body
                Uint8 ui8 (0);
                Uint4 gccur (current_offset + gcbase);
                for(; gccur + 32 < current_offset + bases; ++pui8)
                {
                    ui8 = *pui8;

                    for(Uint4 gccur_hi (gccur + 16); gccur < gccur_hi; ++gccur) {

                        const Uint8 loword = ui8 & kUI8_LoWord;
                        if(m_Mers.get_at(strand? (loword >> 4):
                                         (loword & kUI4_Lo28)))
                        {
                            MersAndCoords[mcidx++] = (loword << 32) | gccur;
                        }
                    
                        const Uint8 ui8hi2 ((ui8 >> 62) << 48);
                        ui8 <<= 2;
                        const Uint8 ui8m ((ui8 & kUI8_SeqDb) >> 16);
                        ui8 &= ~kUI8_SeqDb;
                        ui8 |= (ui8m | ui8hi2);
                    }

                    if(gccur + 32 < current_offset + bases) {

                        // pre-read next 16 residues
                        const uintptr_t n (reinterpret_cast<uintptr_t>(pui8 + 1));
                        const Uint4 * pui4 (reinterpret_cast<const Uint4*>(n));
                        Uint4 ui4 (*pui4);
                        Uint8 ui8_4 (ui4);
                        ui8 |= (ui8_4 << 32);

                        for(Uint4 gccur_hi (gccur + 16); gccur < gccur_hi; ++gccur) {

                            const Uint8 loword = ui8 & kUI8_LoWord;
                            if(m_Mers.get_at(strand? (loword >> 4):
                                             (loword & kUI4_Lo28)))
                            {
                                MersAndCoords[mcidx++] = (loword << 32) | gccur;
                            }
                        
                            const Uint8 ui8hi2 ((ui8 >> 62) << 48);
                            ui8 <<= 2;
                            const Uint8 ui8m ((ui8 & kUI8_SeqDb) >> 16);
                            ui8 &= ~kUI8_SeqDb;
                            ui8 |= (ui8m | ui8hi2);
                        }
                    }
                }
                
                // tail
                if(gccur + 16 <= current_offset + bases) {

                    fivebytes = (gccur == current_offset + gcbase)? fivebytes:
                        (ui8 & kUI8_LoWord);
                    const char * p (reinterpret_cast<const char*>(pui8_start) + 4 
                                    + (gccur - current_offset - gcbase) / 4);
                    size_t k (0);
                    do {
                        const Uint8 loword = fivebytes & kUI8_LoWord;

                        if(m_Mers.get_at(strand? (loword >> 4):
                                         (loword & kUI4_Lo28)))
                        {
                            MersAndCoords[mcidx++] = (loword << 32) | gccur;
                        }

                        if(k % 4 == 0) {
                            if(p < pce) {
                                const Uint8 ui8 (*p++ & kUI8_LoByte);
                                fivebytes |= (ui8 << 32);
                            }
                            else {
                                break;
                            }
                        }
                        
                        fivebytes &= kUI8_LoFive;
                        fivebytes <<= 2;
                        const Uint8 ui8m ((fivebytes & kUI8_SeqDb) >> 16);
                        fivebytes &= ~kUI8_SeqDb;
                        fivebytes |= ui8m;

                        ++k;
                        ++gccur;
                    } while (true);
                }

            } // min cDNA length check

        } // genomic or cDNA

        blastdb->RetSequence(&pcb);

        current_offset += bases;

        if(mcidx >= mcidx_max) {
            CNcbiOstrstream ostr;
            ostr << "Selected max volume size is too small: "
                 << "it must be large enough to fit the index for the " 
                 << "longest input sequence.";
            const string err = CNcbiOstrstreamToString(ostr);
            NCBI_THROW(CException, eUnknown, err);
        }
    } // seqdb iteration

    blastdb.Reset(0);

    if(mcidx > 0) {
        MersAndCoords.resize(mcidx);
        x_WriteIndexFile(++volume, mode, strand, MersAndCoords);
        mcidx = 0;
    }

    m_Mers.Clear();
}

void CElementaryMatching::x_CreateIndex(ISequenceSource *m_qsrc, EIndexMode mode, bool strand)
{
    // sort all adjacent genomic N-mers and their positions
    // except for those marked in the Nr-mer bit vector

    cerr << " Scanning sequences for N-mers and their positions." << endl;

    if(m_Mers.get_at(0)) {
        NCBI_THROW(CException, eUnknown, "NULL mer not excluded!");
    }

    const size_t mcidx_max (m_MaxVolSize / 8);
    vector<Uint8> MersAndCoords (mcidx_max, 0);
    size_t mcidx (0);
    size_t current_offset (0);

    m_qsrc->SetMemoryBound(kSeqDbMemBound);

    const Uint8 blastdb_total_length (m_qsrc->GetTotalLength());
    if((mode == eIM_Genomic && blastdb_total_length / kN > numeric_limits<Uint4>::max())
       || (mode == eIM_cDNA && blastdb_total_length > numeric_limits<Uint4>::max()))
    {
        CNcbiOstrstream ostr;
        ostr << "Sequence volumes with total length exceeding " 
             << numeric_limits<Uint4>::max()
             << " are not yet supported. Please split your FASTA file and re-run "
             << " formatdb.";
        const string err = CNcbiOstrstreamToString(ostr);
        NCBI_THROW(CException, eUnknown, err);
    }

    size_t volume(0);
    for(m_qsrc->ResetIndex(); m_qsrc->GetNext(); ) {

        const char * pcb (0);
        const Uint4 bases (m_qsrc->GetSequence(&pcb));
        const char * pce (pcb + bases/4);
        uintptr_t npcb (reinterpret_cast<uintptr_t>(pcb)), npcb0 (npcb);
        npcb -= npcb % 8;
        if(npcb < npcb0) npcb += 8;
        const Uint4  gcbase (4*(npcb - npcb0));
        const Uint8* pui8_start (reinterpret_cast<const Uint8*>(npcb));
        const Uint8* pui8 (pui8_start);

        // It helps not to break volumes in the middle of a sequence.
        // We explicitly check here if the volume is close to its limit.

        const size_t max_new_elems (mode == eIM_Genomic? size_t(8.0 * bases / kN):
                                    bases);

        if(mcidx > 1000 && mcidx + max_new_elems >= mcidx_max) {
            MersAndCoords.resize(mcidx);
            x_WriteIndexFile(++volume, mode, strand, MersAndCoords);
            MersAndCoords.assign(mcidx_max, 0);
            mcidx = 0;
        }

        if(mode == eIM_Genomic) {

            // index every other position
            for(size_t gccur (current_offset + gcbase); 
                gccur + 16 < current_offset + bases && mcidx < mcidx_max;
                ++pui8)
            {
                Uint8 w8 (*pui8);
                
#define QCOMP_PREPARE_SHIFTED_GENOMIC_IDX \
                size_t gccur2 (gccur + 2); \
                const Uint8 ui8_2930 (w8 >> 60); \
                Uint8 ui8_ls4 (w8 << 4); \
                const Uint8 ui8_mask (ui8_ls4 & kUI8_LoHalfWordEachByte); \
                ui8_ls4 &= kUI8_LoHalfWordEachByte << 4; \
                ui8_ls4 |= (ui8_mask >> 16) | (ui8_2930 << 48);

                QCOMP_PREPARE_SHIFTED_GENOMIC_IDX;

#define QCOMP_CREATE_GENOMIC_IDX(w8,gccur) \
                { \
                    if(gccur + 16 >= current_offset + bases) { \
                        break; \
                    } \
                    const Uint8 mer (w8 & kUI8_LoWord); \
                    if(strand) { \
                        if(m_Mers.get_at(mer)) { \
                            MersAndCoords[mcidx++] = (mer << 32) | gccur; \
                        } \
                    } \
                    else { \
                        const Uint4 rc (g_RC(Uint4(mer))); \
                        if(m_Mers.get_at(rc)) { \
                            MersAndCoords[mcidx++] = (Uint8(rc) << 32) | \
                                ((current_offset + bases - gccur - 16) \
                                 + current_offset); \
                        } \
                    } \
                    gccur += 4; \
                    w8 >>= 8; \
                }

                QCOMP_CREATE_GENOMIC_IDX(w8,gccur);
                QCOMP_CREATE_GENOMIC_IDX(w8,gccur);
                QCOMP_CREATE_GENOMIC_IDX(w8,gccur);
                QCOMP_CREATE_GENOMIC_IDX(w8,gccur);

                QCOMP_CREATE_GENOMIC_IDX(ui8_ls4,gccur2);
                QCOMP_CREATE_GENOMIC_IDX(ui8_ls4,gccur2);
                QCOMP_CREATE_GENOMIC_IDX(ui8_ls4,gccur2);
                QCOMP_CREATE_GENOMIC_IDX(ui8_ls4,gccur2);

                if(gccur + 32 >= current_offset + bases) {
                    break;
                }
                else {

                    w8 |= ((*(pui8 + 1)) << 32);

                    QCOMP_PREPARE_SHIFTED_GENOMIC_IDX;

                    QCOMP_CREATE_GENOMIC_IDX(w8,gccur);
                    QCOMP_CREATE_GENOMIC_IDX(w8,gccur);
                    QCOMP_CREATE_GENOMIC_IDX(w8,gccur);
                    QCOMP_CREATE_GENOMIC_IDX(w8,gccur);

                    QCOMP_CREATE_GENOMIC_IDX(ui8_ls4,gccur2);
                    QCOMP_CREATE_GENOMIC_IDX(ui8_ls4,gccur2);
                    QCOMP_CREATE_GENOMIC_IDX(ui8_ls4,gccur2);
                    QCOMP_CREATE_GENOMIC_IDX(ui8_ls4,gccur2);
                }
#undef QCOMP_PREPARE_SHIFTED_GENOMIC_IDX
#undef QCOMP_CREATE_GENOMIC_IDX
            }
        }
        else {  // cDNA mode

            if(bases >= m_MinQueryLength && bases <= m_MaxQueryLength) {
            
                // head
                Uint8 fivebytes (0);
                for(const char * p (pcb), 
                    *pche ( (reinterpret_cast<const char*>(pui8)) + 5 );
                    p < pche; ++p)
                {
                    const Uint8 c8 (*p & kUI8_LoByte);
                    const Uint8 ui8curbyte (c8);
                    if(pcb + 5 > p) {
                        fivebytes = (fivebytes >> 8) | (ui8curbyte << 32);
                    }
                    else {

                        for(Uint4 k(0); k < 4; ++k) {
                        
                            const Uint4 mer (fivebytes & kUI8_LoWord);
                            if(m_Mers.get_at(strand? (mer >> 4): (mer & kUI4_Lo28))) {
                                const Uint8 ui8   (mer);
                                const Uint4 coord (current_offset + 
                                                   4*(p - pcb - 5) + k);
                                MersAndCoords[mcidx++] = (ui8 << 32) | coord;
                            }

                            fivebytes &= kUI8_LoFive;
                            fivebytes <<= 2;
                            const Uint8 ui8m ((fivebytes & kUI8_SeqDb) >> 16);
                            fivebytes &= ~kUI8_SeqDb;
                            fivebytes |= ui8m;
                        }                   
                        fivebytes |= ui8curbyte << 32;
                    }
                }
            
                // body
                Uint8 ui8 (0);
                Uint4 gccur (current_offset + gcbase);
                for(; gccur + 32 < current_offset + bases; ++pui8)
                {
                    ui8 = *pui8;

                    for(Uint4 gccur_hi (gccur + 16); gccur < gccur_hi; ++gccur) {

                        const Uint8 loword = ui8 & kUI8_LoWord;
                        if(m_Mers.get_at(strand? (loword >> 4):
                                         (loword & kUI4_Lo28)))
                        {
                            MersAndCoords[mcidx++] = (loword << 32) | gccur;
                        }
                    
                        const Uint8 ui8hi2 ((ui8 >> 62) << 48);
                        ui8 <<= 2;
                        const Uint8 ui8m ((ui8 & kUI8_SeqDb) >> 16);
                        ui8 &= ~kUI8_SeqDb;
                        ui8 |= (ui8m | ui8hi2);
                    }

                    if(gccur + 32 < current_offset + bases) {

                        // pre-read next 16 residues
                        const uintptr_t n (reinterpret_cast<uintptr_t>(pui8 + 1));
                        const Uint4 * pui4 (reinterpret_cast<const Uint4*>(n));
                        Uint4 ui4 (*pui4);
                        Uint8 ui8_4 (ui4);
                        ui8 |= (ui8_4 << 32);

                        for(Uint4 gccur_hi (gccur + 16); gccur < gccur_hi; ++gccur) {

                            const Uint8 loword = ui8 & kUI8_LoWord;
                            if(m_Mers.get_at(strand? (loword >> 4):
                                             (loword & kUI4_Lo28)))
                            {
                                MersAndCoords[mcidx++] = (loword << 32) | gccur;
                            }
                        
                            const Uint8 ui8hi2 ((ui8 >> 62) << 48);
                            ui8 <<= 2;
                            const Uint8 ui8m ((ui8 & kUI8_SeqDb) >> 16);
                            ui8 &= ~kUI8_SeqDb;
                            ui8 |= (ui8m | ui8hi2);
                        }
                    }
                }
                
                // tail
                if(gccur + 16 <= current_offset + bases) {

                    fivebytes = (gccur == current_offset + gcbase)? fivebytes:
                        (ui8 & kUI8_LoWord);
                    const char * p (reinterpret_cast<const char*>(pui8_start) + 4 
                                    + (gccur - current_offset - gcbase) / 4);
                    size_t k (0);
                    do {
                        const Uint8 loword = fivebytes & kUI8_LoWord;

                        if(m_Mers.get_at(strand? (loword >> 4):
                                         (loword & kUI4_Lo28)))
                        {
                            MersAndCoords[mcidx++] = (loword << 32) | gccur;
                        }

                        if(k % 4 == 0) {
                            if(p < pce) {
                                const Uint8 ui8 (*p++ & kUI8_LoByte);
                                fivebytes |= (ui8 << 32);
                            }
                            else {
                                break;
                            }
                        }
                        
                        fivebytes &= kUI8_LoFive;
                        fivebytes <<= 2;
                        const Uint8 ui8m ((fivebytes & kUI8_SeqDb) >> 16);
                        fivebytes &= ~kUI8_SeqDb;
                        fivebytes |= ui8m;

                        ++k;
                        ++gccur;
                    } while (true);
                }

            } // min cDNA length check

        } // genomic or cDNA

        m_qsrc->RetSequence(&pcb);

        current_offset += bases;

        if(mcidx >= mcidx_max) {
            CNcbiOstrstream ostr;
            ostr << "Selected max volume size is too small: "
                 << "it must be large enough to fit the index for the " 
                 << "longest input sequence.";
            const string err = CNcbiOstrstreamToString(ostr);
            NCBI_THROW(CException, eUnknown, err);
        }
    } // seqdb iteration

    if(mcidx > 0) {
        MersAndCoords.resize(mcidx);
        x_WriteIndexFile(++volume, mode, strand, MersAndCoords);
        mcidx = 0;
    }

    m_Mers.Clear();
}


size_t CElementaryMatching::x_WriteIndexFile(
          size_t volume,
          EIndexMode mode, 
          bool   strand,
          vector<Uint8>& MersAndCoords)
{
    const size_t t0 (time(0));

    string basename;
    if(mode == eIM_Genomic) {
        basename = m_lbn_s;
    }
    else {
        basename = m_lbn_q;
    }
    basename += string(strand? ".p": ".m") + ".v" + NStr::IntToString(volume);

    const string filename_offs (basename + kFileExt_Offsets);
    CNcbiOfstream ofstr_offs   (filename_offs.data(), IOS_BASE::binary);
    Uint8 bytes_offs (0);
   
    const string filename_positions (basename + kFileExt_Positions);
    CNcbiOfstream ofstr_positions (filename_positions.data(), IOS_BASE::binary);
    Uint8 bytes_positions (0);    

    cerr << " Generating index volume: " << basename << " ... ";

    Uint4 curmer (numeric_limits<Uint4>::max());
    Uint4 curofs (0);

    sort(MersAndCoords.begin(), MersAndCoords.end());
    
    ITERATE(vector<Uint8>, ii, MersAndCoords) {

        const Uint4 mer ((*ii & kUI8_HiWord) >> 32);
        if(mer != curmer) {

            ofstr_offs.write((const char*) &mer,    sizeof mer);
            ofstr_offs.write((const char*) &curofs, sizeof curofs);
            curmer = mer;
            bytes_offs += sizeof(mer) + sizeof(curofs);
        }
        const Uint4 pos (*ii & kUI8_LoWord);

        ofstr_positions.write((const char*) &pos, sizeof pos);
        bytes_positions += sizeof(pos);
        ++curofs;
    }

    // termination zero mer
    const Uint4 mer (0);
    ofstr_offs.write((const char*)&mer, sizeof mer);
    ofstr_offs.write((const char*)&curofs, sizeof curofs);
    bytes_offs += sizeof(mer) + sizeof(curofs);

    ofstr_offs.close();
    ofstr_positions.close();

    CheckWrittenFile(filename_offs,      bytes_offs);
    CheckWrittenFile(filename_positions, bytes_positions);
    
    cerr << "Ok" << endl;

    return time(0) - t0;
}

#define CHECK_MEMMAP(mm,ofs,len) \
{{ \
    const size_t ofs1 (mm.GetOffset());     \
    if(ofs1 != ofs) {                                                        \
        cerr << "Real offset " << ofs1 << " different from " << ofs << endl; \
    }                                                                        \
    const size_t len1 (mm.GetSize());                                    \
    if(len1 != len) {                                                        \
        cerr << "Real length " << len1 << " different from " << len << endl; \
    } \
}}


void CElementaryMatching::x_Search(bool strand)
{
    m_Mers.Clear();

    cerr << " Matching (strand = " << (strand? "plus": "minus") << ") ... ";
    m_CurGenomicStrand = strand;

    CDir dir (CDir::GetCwd());

    const string sfx (string(strand?".p": ".m") + ".v*");

    const string mask_ofs_q   (m_lbn_q + sfx + kFileExt_Offsets);
    const string mask_ofs_s   (m_lbn_s + sfx + kFileExt_Offsets);
    CDir::TEntries vols_ofs_q (dir.GetEntries(mask_ofs_q));
    CDir::TEntries vols_ofs_s (dir.GetEntries(mask_ofs_s));

    Uint8 elem_hits_total (0);

    ITERATE(CDir::TEntries, ii_s, vols_ofs_s) {

        const string filename_ofs_s ((*ii_s)->GetPath());
        const string filename_pos_s (ReplaceExt(filename_ofs_s, kFileExt_Positions));
        const CFile  file_subj (filename_ofs_s);
        const size_t dim_ofs_s (file_subj.GetLength() / 8);
            
        ITERATE(CDir::TEntries, ii_q, vols_ofs_q) {

            const string filename_ofs_q ((*ii_q)->GetPath());
            const string filename_pos_q (ReplaceExt(filename_ofs_q,
                                                    kFileExt_Positions));

            Uint8 hit_index_dim (0), elem_hits_this_pair (0);
            string filename_hit_index;

            {{

            // map offset files
            CMemoryFile mf_ofs_s ((*ii_s)->GetPath());
            const Uint8 * ofs_s (reinterpret_cast<const Uint8*>
                                               (mf_ofs_s.Map()));

            const CFile  file_query ((*ii_q)->GetPath());
            const size_t dim_ofs_q (file_query.GetLength() / 8);

            CMemoryFile mf_ofs_q ((*ii_q)->GetPath());
            const Uint8 * ofs_q (reinterpret_cast<const Uint8*> 
                                               (mf_ofs_q.Map()));

            // build hit index
            THitIndex hit_index;
            hit_index.reserve(min(dim_ofs_q, dim_ofs_s));

            while((*ofs_q & kUI8_LoWord) && (*ofs_s & kUI8_LoWord)) {

                while( (*ofs_q & kUI8_LoWord) 
                       && ((*ofs_q & kUI8_LoWord) < (*ofs_s & kUI8_LoWord)))
                {
                    ++ofs_q;
                }
                if((*ofs_q & kUI8_LoWord) == 0) break;

                while( (*ofs_s & kUI8_LoWord)
                       && ((*ofs_s & kUI8_LoWord) < (*ofs_q & kUI8_LoWord)))
                {
                    ++ofs_s;
                }
                if((*ofs_s & kUI8_LoWord) == 0) break;

                if((*ofs_s & kUI8_LoWord) == (*ofs_q & kUI8_LoWord)) {
                    
                    hit_index.push_back(SHitIndexEntry());
                    SHitIndexEntry& elem (hit_index.back());

                    elem.m_QueryOfs = (*ofs_q) >> 32;
                    size_t count ((*(ofs_q + 1) >> 32) - elem.m_QueryOfs);
                    elem.m_QueryCount = (count > 0xFFFF)? 0xFFFF: count;

                    elem.m_SubjOfs = (*ofs_s) >> 32;
                    count = ((*(ofs_s + 1) >> 32) - elem.m_SubjOfs);
                    elem.m_SubjCount = (count > 0xFFFF)? 0xFFFF: count;

                    ++ofs_s;
                    ++ofs_q;

                    elem_hits_this_pair += elem.m_QueryCount * elem.m_SubjCount;
                }
            }

            // unload offset files; save hit index
            filename_hit_index = g_SaveToTemp(hit_index);
            hit_index_dim = (hit_index.size());
            elem_hits_total += elem_hits_this_pair;
            }}

            // load position files
            
            const CFile  file_pos_s (filename_pos_s);
            const size_t dim_pos_s (file_pos_s.GetLength() / 4);
 
            CMemoryFile mf_pos_s (filename_pos_s);
            size_t map_offset_s (0);
            size_t map_length_s (min(kMapGran, 4*dim_pos_s - map_offset_s));
            const Uint4 * pos_s (reinterpret_cast<const Uint4*>(
                                           mf_pos_s.Map(map_offset_s, map_length_s)));
 
            const CFile  file_pos_q (filename_pos_q);
            const size_t dim_pos_q (file_pos_q.GetLength() / 4);
 
            CMemoryFile mf_pos_q (filename_pos_q);
            size_t map_offset_q (0);
            size_t map_length_q (min(kMapGran, 4*dim_pos_q - map_offset_q));
            const Uint4 * pos_q (reinterpret_cast<const Uint4*>(
                                           mf_pos_q.Map(map_offset_q, map_length_q)));

            // scan hit index file and build hits
            vector<Uint8> hits (elem_hits_this_pair);
            size_t hidx (0);

            CNcbiIfstream ifstr (filename_hit_index.c_str(), IOS_BASE::binary);
            for(size_t cnt(0); cnt < hit_index_dim; ++cnt) {
 
                SHitIndexEntry hie;
                ifstr.read((char*) &hie, sizeof(hie));
 
                const size_t idx_s_max (hie.m_SubjOfs + hie.m_SubjCount);
                const size_t idx_q_max (hie.m_QueryOfs + hie.m_QueryCount);
                if(idx_s_max > dim_pos_s || idx_q_max > dim_pos_q) {
                    NCBI_THROW(CException, eUnknown, "Coordinate out of scope");
                }
 
                if(4*idx_s_max >= map_offset_s + map_length_s) {

                    map_offset_s = 4 * hie.m_SubjOfs;
                    map_length_s = min(kMapGran, 4*dim_pos_s - map_offset_s);
                    mf_pos_s.Unmap();
                    pos_s = (reinterpret_cast<const Uint4*>
                             (mf_pos_s.Map(map_offset_s, map_length_s)));
                }

                if(4*idx_q_max >= map_offset_q + map_length_q) {

                    map_offset_q = 4 * hie.m_QueryOfs;
                    map_length_q = min(kMapGran, 4*dim_pos_q - map_offset_q);
                    mf_pos_q.Unmap();
                    pos_q = (reinterpret_cast<const Uint4*>
                             (mf_pos_q.Map(map_offset_q, map_length_q)));
                }

                for(size_t idx_s (hie.m_SubjOfs); idx_s < idx_s_max; ++idx_s) {

                    Uint8 hiword = pos_s[idx_s - map_offset_s/4];
                    hiword <<= 32;
                    for(size_t idx_q (hie.m_QueryOfs); idx_q < idx_q_max; ++idx_q) {

                        const Uint8 loword = pos_q[idx_q - map_offset_q/4];
                        hits[hidx++] = (hiword | loword);
                    }
                }
            }

            if(hidx != elem_hits_this_pair) {
                CNcbiOstrstream ostr;
                ostr << "The number of hits found (" << hidx 
                     << ") does not match the expected " << elem_hits_this_pair;
                const string str = CNcbiOstrstreamToString(ostr);
                NCBI_THROW(CException, eUnknown, str);
            }

            // remove the hit index file
            CFile(filename_hit_index).Remove();

            // detect compartments
            x_CompartVolume (&hits);

        } // query vols

    } // subj vols

    cerr << "Ok" << endl;
}

#undef CHECK_MEMMAP


bool PLoWord(const Uint8& lhs, const Uint8& rhs)
{
    return (lhs & kUI8_LoWord) < (rhs & kUI8_LoWord);
}


bool PDiag(const Uint8& lhs, const Uint8& rhs)
{
    const double lhs_q (double(lhs & kUI8_LoWord) / 2);
    const double lhs_s (double(lhs >> 32) / 2);
    const double rhs_q (double(rhs & kUI8_LoWord) / 2);
    const double rhs_s (double(rhs >> 32) / 2);

    const double lhs_q1 (lhs_q + lhs_s);
    const double lhs_s1 (lhs_s - lhs_q);
    const double rhs_q1 (rhs_q + rhs_s);
    const double rhs_s1 (rhs_s - rhs_q);

    if(lhs_s1 == rhs_s1) {
        return lhs_q1 < rhs_q1;
    }
    else {
        return lhs_s1 < rhs_s1;
    }
}


void CElementaryMatching::x_CleanVolumes(const string& lbn,
                                       const TStrings& vol_extensions)
{
    // make sure there are no offset or position files left before
    // we generate the new ones

    CDir dir (CDir::GetCwd());

    CFileDeleteList fdl;
    ITERATE(TStrings, ii, vol_extensions) {

        const string mask (lbn + "*" + *ii);
        CDir::TEntries dir_entries (dir.GetEntries(mask));
        ITERATE(CDir::TEntries, jj, dir_entries) {
            fdl.Add((*jj)->GetPath());
        }
    }
}


void CElementaryMatching::x_CompartVolume(vector<Uint8>* phits)
{
    if(phits->size() == 0) {
        return;
    }

    // sort by the global genomic corrdinate
    sort(phits->begin(), phits->end());

    TSeqInfos::const_iterator ii_genomic_b (m_SeqInfos_Genomic.begin());
    TSeqInfos::const_iterator ii_genomic_e (m_SeqInfos_Genomic.end());
    TSeqInfos::const_iterator ii_genomic   (ii_genomic_b);
    TSeqInfos::const_iterator ii_cdna_b    (m_SeqInfos_cdna.begin());
    TSeqInfos::const_iterator ii_cdna_e    (m_SeqInfos_cdna.end());

    CSeqDB seqdb_genomic (m_sdb, CSeqDB::eNucleotide);

    m_CurSeq_cDNA = m_CurSeq_Genomic = 0;

    size_t idx_compacted (0);
    for(size_t idx (0), idx_hi (phits->size()); idx < idx_hi; ) {

        Uint4 gc_s (((*phits)[idx]) >> 32);

        // find the relevant genomic record
        ii_genomic = lower_bound(ii_genomic + 1, ii_genomic_e, SSeqInfo(gc_s, 0, 0));
        if(ii_genomic == ii_genomic_e || ii_genomic->m_Start > gc_s) {
            --ii_genomic;
        }

        if(gc_s < ii_genomic->m_Start || 
           ii_genomic->m_Start + ii_genomic->m_Length <= gc_s)
        {
            CNcbiOstrstream ostr;
            ostr << "Global genomic coordinate " 
                 << gc_s << " out of range: ["
                 << ii_genomic->m_Start << ", "
                 << (ii_genomic->m_Start + ii_genomic->m_Length) << "), "
                 << (m_GenomicSeqIds[ii_genomic->m_Oid]->GetSeqIdString(true));
            const string str = CNcbiOstrstreamToString(ostr);
            NCBI_THROW(CException, eUnknown, str);
        }

        const Uint4 gc_s_max (ii_genomic->m_Start + ii_genomic->m_Length);

        // preload genomic sequence
        seqdb_genomic.GetSequence(ii_genomic->m_Oid, &m_CurSeq_Genomic);

        // find all hits that belong to the current genomic record
        size_t idx0 (idx);
        while(idx < idx_hi && (((*phits)[idx]) >> 32) < gc_s_max) {
            ++idx;
        }

        // sort by the global cDNA coordinate
        sort(phits->begin() + idx0, phits->begin() + idx, PLoWord);

        // find the relevant cDNA record
        TSeqInfos::const_iterator ii_cdna (ii_cdna_b);
        Uint4 gc_q_min (ii_cdna->m_Start);
        Uint4 gc_q_max (ii_cdna->m_Start + ii_cdna->m_Length);
        size_t jdx (idx0), jdx0 (jdx);
        for(; jdx < idx; ++jdx) {
            
            Uint4 gc_q (((*phits)[jdx]) & kUI8_LoWord);
            
            if(gc_q < gc_q_min || gc_q >= gc_q_max) {
                
                const THit::TCoord min_matches1 = THit::TCoord(
                      round(m_MinCompartmentIdty * ii_cdna->m_Length));

                const THit::TCoord min_matches2 = THit::TCoord(
                      round(m_MinSingletonIdty * ii_cdna->m_Length));

                const THit::TCoord min_matches (min(min_matches1,min_matches2));

                if(kN * (jdx - jdx0) >= min_matches / 2) {

                    // preload genomic sequence
                    m_qsrc->GetSequence(ii_cdna->m_Oid, &m_CurSeq_cDNA);
                    x_CompartPair(phits, ii_cdna, ii_genomic,
                                  jdx0, jdx, &idx_compacted);
                    m_qsrc->RetSequence(&m_CurSeq_cDNA);
                    m_CurSeq_cDNA = 0;
                }
                
                ii_cdna = lower_bound(ii_cdna + 1, ii_cdna_e, SSeqInfo(gc_q, 0, 0));

                if(ii_cdna == ii_cdna_e || ii_cdna->m_Start > gc_q) {
                    --ii_cdna;
                }

                if(gc_q < ii_cdna->m_Start || 
                   ii_cdna->m_Start + ii_cdna->m_Length <= gc_q)
                {
                    CNcbiOstrstream ostr;
                    ostr << "Global cDNA coordinate " 
                         << gc_q << " out of range: ["
                         << ii_cdna->m_Start << ", "
                         << (ii_cdna->m_Start + ii_cdna->m_Length) << "), "
                         << (m_cDNASeqIds[ii_cdna->m_Oid]->GetSeqIdString(true));
                    const string str = CNcbiOstrstreamToString(ostr);
                    NCBI_THROW(CException, eUnknown, str);
                }

                gc_q_min  = ii_cdna->m_Start;
                gc_q_max  = ii_cdna->m_Start + ii_cdna->m_Length;
                jdx0 = jdx;
            }
        }
        
        const THit::TCoord min_matches1 = THit::TCoord(
                      round(m_MinCompartmentIdty * ii_cdna->m_Length));

        const THit::TCoord min_matches2 = THit::TCoord(
                      round(m_MinSingletonIdty * ii_cdna->m_Length));

        const THit::TCoord min_matches (min(min_matches1,min_matches2));

        if(kN * (jdx - jdx0) >= min_matches / 2) {

            m_qsrc->GetSequence(ii_cdna->m_Oid, &m_CurSeq_cDNA);

            x_CompartPair(phits, ii_cdna, ii_genomic,
                          jdx0, jdx, &idx_compacted);
            m_qsrc->RetSequence(&m_CurSeq_cDNA);
            m_CurSeq_cDNA = 0;
        }

        seqdb_genomic.RetSequence(&m_CurSeq_Genomic);
        m_CurSeq_Genomic = 0;
    }
}


bool CElementaryMatching::x_IsMatch(Uint4 q, Uint4 s) const
{
    const Uint1 cq (((m_CurSeq_cDNA[q / 4]) >> ((3 - (q % 4))*2)) & 0x03);
    const Uint1 cs (((m_CurSeq_Genomic[s / 4]) >> ((3 - (s % 4))*2)) & 0x03);
    const bool rv (m_CurGenomicStrand? (cq == cs): ((cq^cs) == 3));
    return rv;
}


Int8 CElementaryMatching::x_ExtendHit(const Int8 & left_limit0, 
                                      const Int8 & right_limit0,
                                      THitRef hitref)
{
    const int Wm (1), Wms (-2);
    int score (int(hitref->GetLength()) * Wm 
               + int(hitref->GetMismatches()) * (Wms - Wm));
    int score_max (score);

    const int overrun (6); // enables connecting hits over a mismatch
    const Int8 left_limit (left_limit0 >= overrun? left_limit0 - overrun: 0);
    const Int8 right_limit (right_limit0 >= kDiagMax - overrun?
                            kDiagMax: right_limit0 + overrun);

    // extend left
    Int8 q0 (hitref->GetQueryStart()), s0 (hitref->GetSubjStart());
    size_t mm (0), mm0 (0);
    bool no_overrun_yet (true);
    for(Int8 q (q0 - 1), s (s0 - 1);
        (q + s > left_limit && score + m_XDropOff >= score_max 
         && q >= m_ii_cdna->m_Start && s >= m_ii_genomic->m_Start);
        --q, --s)
    {

        if(q + s == left_limit0) {
            no_overrun_yet = false;
            mm0 += mm;
        }

        Uint4 qq (q - m_ii_cdna->m_Start);
        Uint4 ss (s - m_ii_genomic->m_Start);
        if(m_CurGenomicStrand == false) {
            ss = m_ii_genomic->m_Length - ss - 1;
        }

        if(x_IsMatch(qq, ss)) {

            score += Wm;
            if(score > score_max) {
                q0 = q;
                s0 = s;
                score_max = score;
                if(no_overrun_yet) {
                    mm0 += mm;
                    mm = 0;
                }
            }
        }
        else {
            score += Wms;
            ++mm;
        }
    }

    while(q0 + s0 <= left_limit0) {++q0; ++s0;}

    bool extended_left (false);
    if(q0 < hitref->GetQueryStart()) {
        hitref->SetQueryStart(q0);
        hitref->SetSubjStart(s0);
        extended_left = true;
    }

    // extend right
    q0 = (hitref->GetQueryStop());
    s0 = (hitref->GetSubjStop());
    score = score_max;
    mm = mm0;

    no_overrun_yet = true;

    for(Int8 q (q0 + 1), s (s0 + 1);
        (q + s < right_limit && score + m_XDropOff >= score_max
         && q < m_ii_cdna->m_Start + m_ii_cdna->m_Length 
         && s < m_ii_genomic->m_Start + m_ii_genomic->m_Length);
        ++q, ++s)
    {
        if(q + s == right_limit0) {
            no_overrun_yet = false;
            mm0 += mm;
        }

        Uint4 qq (q - m_ii_cdna->m_Start);
        Uint4 ss (s - m_ii_genomic->m_Start);
        if(m_CurGenomicStrand == false) {
            ss = m_ii_genomic->m_Length - ss - 1;
        }

        if(x_IsMatch(qq, ss)) {

            score += Wm;
            if(score > score_max) {
                q0 = q;
                s0 = s;
                score_max = score;
                if(no_overrun_yet) {
                    mm0 += mm;
                    mm = 0;
                }
            }
        }
        else {
            score += Wms;
            ++mm;
        }
    }

    while(q0 + s0 >= right_limit0) {--q0; --s0; }
    
    bool extended_right (false);
    if(q0 > hitref->GetQueryStop()) {
        hitref->SetQueryStop(q0);
        hitref->SetSubjStop(s0);
        extended_right = true;
    }

    if(extended_left || extended_right) {

        hitref->SetMismatches(mm0);
        const THit::TCoord len (hitref->GetQueryStop() - hitref->GetQueryStart() + 1);
        hitref->SetLength(len);
        hitref->SetIdentity((len - mm0) / double(len));
        hitref->SetScore(2 * len);
    }

    return q0 + s0;
}


void CElementaryMatching::x_CompartPair(vector<Uint8>* pvol,
                                        TSeqInfos::const_iterator ii_cdna,
                                        TSeqInfos::const_iterator ii_genomic,
                                        size_t  idx_start, 
                                        size_t  idx_stop,
                                        size_t* pidx_compacted)
{
    if(idx_start >= idx_stop) {
        return;
    }

    // init "global" members
    m_ii_cdna    = ii_cdna;
    m_ii_genomic = ii_genomic;

    // re-sort for better compactization
    sort(pvol->begin() + idx_start, pvol->begin() + idx_stop, PDiag);

    const size_t idx_compacted_start (*pidx_compacted);

    vector<Uint4> lens;
    lens.reserve(pvol->size() / 2);

    Uint8 qs0 ((*pvol)[idx_start]);
    Uint4 q0  (qs0 & kUI8_LoWord);
    Uint4 s0  (qs0 >> 32);
    (*pvol)[(*pidx_compacted)++] = qs0;
    lens.push_back(16);

    for(size_t idx (idx_start + 1); idx < idx_stop; ++idx) {
        
        const Uint8 qs  ((*pvol)[idx]);
        const Uint4 q   (qs & kUI8_LoWord);
        const Uint4 s   (qs >> 32);

        if((q0 >= s0 && q0 - s0 == q - s && q <= q0 + 16) ||
           (q0 < s0 && s0 - q0 == s - q && q <= q0 + 16) )
        {
            lens.back() += q - q0;
        }
        else {
            (*pvol)[(*pidx_compacted)++] = qs;
            lens.push_back(16);
        }

        q0 = q;
        s0 = s;
    }

    THitRefs hitrefs (lens.size());

    for(size_t idx (idx_compacted_start); idx < *pidx_compacted; ++idx) {

        const Uint8 qs  ((*pvol)[idx]);
        const Uint4 q   (qs & kUI8_LoWord);
        const Uint4 s   (qs >> 32);

        THitRef hitref (new THit);
        hitref->SetQueryStart(q);
        hitref->SetSubjStart(s);
        const Uint4 len (lens[idx - idx_compacted_start]);
        hitref->SetQueryStop(q + len - 1);
        hitref->SetSubjStop(s + len - 1);
        hitref->SetMismatches(0);
        hitref->SetGaps(0);
        hitref->SetEValue(0);
        hitref->SetIdentity(1.0);
        hitref->SetLength(len);
        hitref->SetScore(2*len);
        hitrefs[idx - idx_compacted_start] = hitref;
    }

#define EXTEND_USING_SEQUENCE_CHARS
#ifdef  EXTEND_USING_SEQUENCE_CHARS

    // try to extend even further over possible mismatches

    Int8 left_limit (0);         // diag extension left limit
    Int8 right_limit (kDiagMax); // diag ext right limit
    Int8 s_prime_cur (0);

    const int kn (hitrefs.size());
    for(int k(0); k < kn; ++k) {
        
        // diag coords: q_prime = q + s; s_prime = s - q;
        const THit::TCoord * box (hitrefs[k]->GetBox());

        if(Int8(box[2]) - Int8(box[0]) != s_prime_cur) {
            left_limit = box[0];
            s_prime_cur = Int8(box[2]) - Int8(box[0]);
        }

        right_limit = kDiagMax;
        if(k + 1 < kn) {
            const THit::TCoord * boX (hitrefs[k+1]->GetBox());
            if(Int8(boX[2]) - Int8(boX[0]) == s_prime_cur) {
                right_limit = boX[0] + boX[2];
            }
        }

        left_limit = x_ExtendHit(left_limit, right_limit, hitrefs[k]);
    }

    // merge adjacent hits
    int d (-1);
    Int8 rlimit (0), spc (0);
    for(int k (0); k < kn; ++k) {

        const THit::TCoord cur_diag_stop (hitrefs[k]->GetQueryStop() 
                                          + hitrefs[k]->GetSubjStop());

        const Int8 s_prime (-Int8(hitrefs[k]->GetQueryStart()) 
                            + Int8(hitrefs[k]->GetSubjStart()));
        if(k == 0 || spc != s_prime) {

            hitrefs[++d] = hitrefs[k];
            rlimit = cur_diag_stop;
            spc = s_prime;
        }
        else {

            const THit::TCoord cur_diag_start (hitrefs[k]->GetQueryStart() 
                                               + hitrefs[k]->GetSubjStart());

            THitRef & hrd (hitrefs[d]);

            if(rlimit + 2 == cur_diag_start) {

                rlimit = cur_diag_stop;
                hrd->SetQueryStop(hitrefs[k]->GetQueryStop());
                hrd->SetSubjStop(hitrefs[k]->GetSubjStop());
                hrd->SetMismatches(hrd->GetMismatches() 
                                   + hitrefs[k]->GetMismatches());
                const THit::TCoord len (hrd->GetQueryStop() 
                                        - hrd->GetQueryStart() + 1);
                hrd->SetLength(len);
                hrd->SetIdentity(double(len - hrd->GetMismatches()) / len);
                hrd->SetScore(2 * len);
            }
            else if (rlimit + 1 < cur_diag_start) {

                hitrefs[++d] = hitrefs[k];
                rlimit = cur_diag_stop;
            }
            else {
                NCBI_THROW(CException, eUnknown,
                           "x_CompartPair(): Unexpected alignment overlap");
            }
        }
    }
    hitrefs.resize(min(d + 1, kn));

#undef EXTEND_USING_SEQUENCE_CHARS
#endif
    
    // make sure nothing is stretching out
    const Uint4 qmax (m_ii_cdna->m_Start + m_ii_cdna->m_Length);
    NON_CONST_ITERATE(THitRefs, ii, hitrefs) {
        if((*ii)->GetQueryStop() >= qmax) {
            (*ii)->Modify(1, qmax - 1);
        }
    }

    if(m_HitsOnly) {

        NON_CONST_ITERATE(THitRefs, ii, hitrefs) {

            THit& h (**ii);

            if(h.GetLength() < m_MinHitLength) continue;

            h.SetQueryId (m_cDNASeqIds[m_ii_cdna->m_Oid]);
            h.SetSubjId  (m_GenomicSeqIds[m_ii_genomic->m_Oid]);
            
            const Uint4 * box0 (h.GetBox());
            Uint4 box [4] = {
                box0[0] - m_ii_cdna->m_Start,
                box0[1] - m_ii_cdna->m_Start,
                box0[2] - m_ii_genomic->m_Start,
                box0[3] - m_ii_genomic->m_Start
            };

            if(m_CurGenomicStrand == false) {
                box[2] = m_ii_genomic->m_Length - box[2] - 1;
                box[3] = m_ii_genomic->m_Length - box[3] - 1;
            }

            h.SetBox(box);

            cout << h << endl;
        }

    }
    else {

        // identify compartments
        const Uint4 qlen (m_ii_cdna->m_Length);

        const THit::TCoord min_matches1 = THit::TCoord(
                      round(m_MinCompartmentIdty * qlen));

        const THit::TCoord min_matches2 = THit::TCoord(
                      round(m_MinSingletonIdty * qlen));

        const THit::TCoord penalty = THit::TCoord(round(m_Penalty * qlen));

        CCompartmentAccessor<THit> ca (penalty,
                                       min_matches1,
                                       min_matches2,
                                       true);
        ca.SetMaxIntron(m_MaxIntron);
        ca.Run(hitrefs.begin(), hitrefs.end());

        // remap and print individual compartments
        THitRefs comp;
        for(bool b0 (ca.GetFirst(comp)); b0; b0 = ca.GetNext(comp)) {
        
            NON_CONST_ITERATE(THitRefs, ii, comp) {

                THit& h (**ii);
            
                h.SetQueryId (m_cDNASeqIds[m_ii_cdna->m_Oid]);
                h.SetSubjId  (m_GenomicSeqIds[m_ii_genomic->m_Oid]);
            
                const Uint4 * box0 (h.GetBox());
                Uint4 box [4] = {
                    box0[0] - m_ii_cdna->m_Start,
                    box0[1] - m_ii_cdna->m_Start,
                    box0[2] - m_ii_genomic->m_Start,
                    box0[3] - m_ii_genomic->m_Start
                };

                if(m_CurGenomicStrand == false) {
                    box[2] = m_ii_genomic->m_Length - box[2] - 1;
                    box[3] = m_ii_genomic->m_Length - box[3] - 1;
                }

                h.SetBox(box);

                if(m_OutputMethod) cout << h << endl;
            }

            // empty line to separate compartments
            if(m_OutputMethod) cout << endl;
            else m_Results.push_back(comp);
        }
    }
}


void CElementaryMatching::x_LoadRemapData(ISequenceSource *m_qsrc, const string& sdb)
{
    const string filename_genomic (m_lbn_s + kFileExt_Remap);
    const size_t elems_genomic (CFile(filename_genomic).GetLength()/sizeof(SSeqInfo));
    m_SeqInfos_Genomic.resize(elems_genomic);
    CNcbiIfstream ifstr_genomic (filename_genomic.c_str(), IOS_BASE::binary);
    ifstr_genomic.read((char *) &m_SeqInfos_Genomic.front(),
                       elems_genomic * sizeof (SSeqInfo));
    ifstr_genomic.close();

    const string filename_cdna (m_lbn_q + kFileExt_Remap);
    const size_t elems_cdna (CFile(filename_cdna).GetLength()/sizeof(SSeqInfo));
    m_SeqInfos_cdna.resize(elems_cdna);
    CNcbiIfstream ifstr_cdna (filename_cdna.c_str(), IOS_BASE::binary);
    ifstr_cdna.read((char *) &m_SeqInfos_cdna.front(),
                       elems_cdna * sizeof (SSeqInfo));
    ifstr_cdna.close();

    {{
        CSeqDB blastdb (sdb, CSeqDB::eNucleotide);
        m_GenomicSeqIds.clear();
        for(int oid (0); blastdb.CheckOrFindOID(oid); ++oid) {
            THit::TId id (blastdb.GetSeqIDs(oid).back());
            m_GenomicSeqIds.push_back(id);
        }
    }}

    {{
        m_cDNASeqIds.clear();
        for(m_qsrc->ResetIndex(); m_qsrc->GetNext(); ) {

            THit::TId id (m_qsrc->GetSeqID());
            m_cDNASeqIds.push_back(id);
        }
    }}
}


////////////////////////////////////////////////////////////////////////////////////
////


CRandom::TValue GenerateSeed(const string & str)
{
    CRandom::TValue rv (0);
    ITERATE(string, ii, str) {
        rv = (rv * 3 + (*ii)) % 3571;
    }
    return time(0) - 5000 + rv;
}


void CElementaryMatching::x_InitBasic(void)
{
    CRandom rand (GenerateSeed("qq" + m_sdb));
    const string base_sfx ( NStr::IntToString(rand.GetRand()));
    m_lbn_q = GetLocalBaseName("qq", base_sfx);
    m_lbn_s = GetLocalBaseName(m_sdb, base_sfx);

    m_OutputMethod = false;

    m_Penalty = 0.55;
    m_MinSingletonIdty = m_MinCompartmentIdty = .75;
    m_MaxIntron = CCompartmentFinder<THit>::s_GetDefaultMaxIntron();
    m_HitsOnly = false;
    m_MaxVolSize = 512 * 1024 * 1024;

    m_MinQueryLength = 50;
    m_MaxQueryLength = 100000;
    m_MinHitLength = 1;
}


void CElementaryMatching::Run(void)
{
    x_Cleanup();

    // create genomic ID and coordinate remapping tables.
    x_CreateRemapData(m_sdb, eIM_Genomic);

    // create the filtering (repeats + low complexity) table 
    // using the plus strand of the genomic sequence;
    x_InitFilteringVector(m_sdb, true);

    // create cDNA ID and coordinate remapping tables;
    x_CreateRemapData(m_qsrc, eIM_cDNA);

    // create cDNA index using the current filtering table;
    x_CreateIndex(m_qsrc, eIM_cDNA, true);

    // init the N-mer participation vector;
    x_InitParticipationVector(true);

    // create plus-strand genomic index using the participation vector
    x_CreateIndex(m_sdb, eIM_Genomic, true);

    // generate N-hits for both strands of the genome;
    // compact and identify compartments;
    // restore original coordinates and IDs in final hits;
    // print compartments
    x_LoadRemapData(m_qsrc, m_sdb);

    x_Search(true);

    TStrings vol_exts;
    vol_exts.push_back(kFileExt_Offsets);
    vol_exts.push_back(kFileExt_Positions);
    x_CleanVolumes(m_lbn_q, vol_exts);
    x_CleanVolumes(m_lbn_s, vol_exts);

    // create the filtering table for the minus genomic strand
    // from the plus strand filtering table
    x_InitFilteringVector(m_sdb, false);

    // repeat the steps to create cDNA and genomic indices
    x_CreateIndex(m_qsrc, eIM_cDNA, false);
    x_InitParticipationVector(false);
    x_CreateIndex(m_sdb, eIM_Genomic, false);

    x_Search(false);
}


void CElementaryMatching::x_Cleanup(void)
{
    m_Mers.Clear();
    TStrings vol_exts;
    vol_exts.push_back(kFileExt_Offsets);
    vol_exts.push_back(kFileExt_Positions);
    vol_exts.push_back(kFileExt_Masked);
    vol_exts.push_back(kFileExt_Remap);
    x_CleanVolumes(m_lbn_q, vol_exts);
    x_CleanVolumes(m_lbn_s, vol_exts);
    m_Results.clear();
}


END_NCBI_SCOPE
