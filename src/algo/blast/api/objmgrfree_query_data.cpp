#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

/* ===========================================================================
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
 * Author:  Christiam Camacho, Kevin Bealer
 *
 */

/** @file objmgrfree_query_data.cpp
 * NOTE: This file contains work in progress and the APIs are likely to change,
 * please do not rely on them until this notice is removed.
 */

#include <ncbi_pch.hpp>
#include <algo/blast/api/objmgrfree_query_data.hpp>
#include <algo/blast/api/blast_options.hpp>
#include <algo/blast/api/blast_exception.hpp>

#include <serial/iterator.hpp>
#include <serial/enumvalues.hpp>

#include <objects/seq/Seq_data.hpp>
#include <util/sequtil/sequtil.hpp>
#include <util/sequtil/sequtil_convert.hpp>
#include <util/sequtil/sequtil_manip.hpp>

#include "blast_setup.hpp"

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

/// FIXME: these should go into some private header...
extern BlastQueryInfo*
s_SafeSetupQueryInfo(const IBlastQuerySource& queries,
                     const CBlastOptions* options);
extern BLAST_SequenceBlk*
s_SafeSetupQueries(const IBlastQuerySource& queries,
                   const CBlastOptions* options,
                   const BlastQueryInfo* query_info);

/////////////////////////////////////////////////////////////////////////////
//
// CBlastSeqVectorFromCSeq_data
//
/////////////////////////////////////////////////////////////////////////////

class CBlastSeqVectorFromCSeq_data : public IBlastSeqVector
{
public:
    CBlastSeqVectorFromCSeq_data(const CSeq_data& seq_data, TSeqPos length);
    void SetCoding(CSeq_data::E_Choice c);
    Uint1 operator[] (TSeqPos pos) const;
    SBlastSequence GetCompressedPlusStrand();

protected:
    TSeqPos x_Size() const;
    void x_SetPlusStrand();
    void x_SetMinusStrand();

private:
    vector<char> m_SequenceData;
    CSeqUtil::ECoding m_Encoding;

    void x_ComplementData();
    CSeqUtil::ECoding x_Encoding_CSeq_data2CSeqUtil(CSeq_data::E_Choice c);
};

CBlastSeqVectorFromCSeq_data::CBlastSeqVectorFromCSeq_data
    (const CSeq_data& seq_data, TSeqPos length)
{
    m_SequenceData.reserve(length);
    m_Strand = eNa_strand_plus;

    switch (seq_data.Which()) {
    // Nucleotide encodings
    case CSeq_data::e_Ncbi2na: 
        CSeqConvert::Convert(seq_data.GetNcbi2na().Get(),
                             CSeqUtil::e_Ncbi2na, 0, length,
                             m_SequenceData, CSeqUtil::e_Ncbi2na_expand);
        m_Encoding = CSeqUtil::e_Ncbi2na_expand;
        break;
    case CSeq_data::e_Ncbi4na: 
        CSeqConvert::Convert(seq_data.GetNcbi4na().Get(),
                             CSeqUtil::e_Ncbi4na, 0, length,
                             m_SequenceData, CSeqUtil::e_Ncbi4na_expand);
        m_Encoding = CSeqUtil::e_Ncbi4na_expand;
        break;
    case CSeq_data::e_Iupacna: 
        CSeqConvert::Convert(seq_data.GetIupacna().Get(),
                             CSeqUtil::e_Iupacna, 0, length,
                             m_SequenceData, CSeqUtil::e_Ncbi4na_expand);
        m_Encoding = CSeqUtil::e_Ncbi4na_expand;
        break;

    // Protein encodings
    case CSeq_data::e_Ncbistdaa: 
        m_SequenceData = const_cast< vector<char>& >
            (seq_data.GetNcbistdaa().Get());
        m_Encoding = CSeqUtil::e_Ncbistdaa;
        break;
    case CSeq_data::e_Ncbieaa: 
        CSeqConvert::Convert(seq_data.GetNcbieaa().Get(),
                             CSeqUtil::e_Ncbieaa, 0, length,
                             m_SequenceData, CSeqUtil::e_Ncbistdaa);
        m_Encoding = CSeqUtil::e_Ncbistdaa;
        break;
    case CSeq_data::e_Iupacaa:
        CSeqConvert::Convert(seq_data.GetIupacaa().Get(),
                             CSeqUtil::e_Iupacaa, 0, length,
                             m_SequenceData, CSeqUtil::e_Ncbistdaa);
        m_Encoding = CSeqUtil::e_Ncbistdaa;
        break;
    default:
        NCBI_THROW(CBlastException, eNotSupported, "Encoding not handled in " +
                   string(NCBI_CURRENT_FUNCTION) + " " +
                   NStr::IntToString((int) seq_data.Which()));
    }
}

void 
CBlastSeqVectorFromCSeq_data::SetCoding(CSeq_data::E_Choice c)
{
    if (c != CSeq_data::e_Ncbi2na && c != CSeq_data::e_Ncbi4na && 
        c != CSeq_data::e_Ncbistdaa) {
        NCBI_THROW(CBlastException, eInvalidArgument, 
                   "Requesting invalid encoding, only Ncbistdaa, Ncbi4na, "
                   "and Ncbi2na are supported");
    }

    if (m_Encoding != x_Encoding_CSeq_data2CSeqUtil(c)) {
        // FIXME: are ambiguities randomized if the encoding requested is 
        // ncbi2na?
        vector<char> tmp;
        TSeqPos nconv = CSeqConvert::Convert(m_SequenceData, m_Encoding,
                                             0, size(),
                                             tmp,
                                             x_Encoding_CSeq_data2CSeqUtil(c));
        ASSERT(nconv == tmp.size());
        m_Encoding = x_Encoding_CSeq_data2CSeqUtil(c);
        m_SequenceData = tmp;
    }
}

inline TSeqPos
CBlastSeqVectorFromCSeq_data::x_Size() const
{
    return m_SequenceData.size();
}

inline Uint1 
CBlastSeqVectorFromCSeq_data::operator[] (TSeqPos pos) const 
{
    return m_SequenceData.at(pos);
}

SBlastSequence 
CBlastSeqVectorFromCSeq_data::GetCompressedPlusStrand() 
{
    SetCoding(CSeq_data::e_Ncbi2na);
    SBlastSequence retval(size());
    int i = 0;
    ITERATE(vector<char>, itr, m_SequenceData) {
        retval.data.get()[i++] = *itr;
    }
    return retval;
}

void 
CBlastSeqVectorFromCSeq_data::x_SetPlusStrand() 
{
    if (GetStrand() != eNa_strand_plus) {
        x_ComplementData();
    }
}

void 
CBlastSeqVectorFromCSeq_data::x_SetMinusStrand() 
{
    if (GetStrand() != eNa_strand_minus) {
        x_ComplementData();
    }
}

inline void 
CBlastSeqVectorFromCSeq_data::x_ComplementData()
{
    TSeqPos nconv = CSeqManip::ReverseComplement(m_SequenceData,
                                                 m_Encoding, 0, size());
    ASSERT(nconv == size());
}

inline CSeqUtil::ECoding 
CBlastSeqVectorFromCSeq_data::x_Encoding_CSeq_data2CSeqUtil
(CSeq_data::E_Choice c)
{
    switch (c) {
    case CSeq_data::e_Ncbi2na: return CSeqUtil::e_Ncbi2na_expand;
    case CSeq_data::e_Ncbi4na: return CSeqUtil::e_Ncbi4na_expand;
    case CSeq_data::e_Ncbistdaa: return CSeqUtil::e_Ncbistdaa;
    default: NCBI_THROW(CBlastException, eNotSupported,
                   "Encoding not handled in " +
                   string(NCBI_CURRENT_FUNCTION));

    }
}

/////////////////////////////////////////////////////////////////////////////
//
// CBlastQuerySourceBioseqSet
//
/////////////////////////////////////////////////////////////////////////////

class CBlastQuerySourceBioseqSet : public IBlastQuerySource
{
public:
    CBlastQuerySourceBioseqSet(CConstRef<CBioseq_set> bs, bool is_prot);
    ENa_strand GetStrand(int index) const;
    TSeqPos Size() const;
    CConstRef<CSeq_loc> GetMask(int index) const;
    CConstRef<CSeq_loc> GetSeqLoc(int index) const;
    SBlastSequence
    GetBlastSequence(int index, EBlastEncoding encoding, ENa_strand strand,
                     ESentinelType sentinel, string* warnings = 0) const;
    TSeqPos GetLength(int index) const;

private:
    bool m_IsProt;
    vector< CConstRef<CBioseq> > m_Bioseqs;

    void x_BioseqSanityCheck(const CBioseq& bs);
};

CBlastQuerySourceBioseqSet::CBlastQuerySourceBioseqSet
    (CConstRef<CBioseq_set> bs, bool is_prot) 
    : m_IsProt(is_prot)
{
    // sacrifice speed for protection against infinite loops
    CTypeConstIterator<CBioseq> itr(ConstBegin(*bs, eDetectLoops)); 
    for (; itr; ++itr) {
        x_BioseqSanityCheck(*itr);
        m_Bioseqs.push_back(CConstRef<CBioseq>(&*itr));
    }
}

inline ENa_strand 
CBlastQuerySourceBioseqSet::GetStrand(int /*index*/) const 
{
    // Although the strand represented in the Bioseq is always the plus
    // strand, the default for searching BLAST is both strands in the
    // query, unless specified otherwise in the BLAST options
    return m_IsProt ? eNa_strand_unknown : eNa_strand_both;
}

inline TSeqPos 
CBlastQuerySourceBioseqSet::Size() const 
{ 
    return m_Bioseqs.size(); 
}

inline CConstRef<CSeq_loc> 
CBlastQuerySourceBioseqSet::GetMask(int /*index*/) const 
{
    return CConstRef<CSeq_loc>(0);
}

inline CConstRef<CSeq_loc> 
CBlastQuerySourceBioseqSet::GetSeqLoc(int index) const 
{ 
    CRef<CSeq_loc> retval(new CSeq_loc);
    retval->SetWhole().Assign(*m_Bioseqs[index]->GetFirstId());
    // FIXME: make sure this works (perhaps we need to build our own
    // Seq-interval
    return retval;
}

SBlastSequence
CBlastQuerySourceBioseqSet::GetBlastSequence(int index, 
                                             EBlastEncoding encoding, 
                                             ENa_strand strand, 
                                             ESentinelType sentinel, 
                                             string* warnings) const 
{
    const CSeq_inst& inst = m_Bioseqs[index]->GetInst();
    if ( !inst.CanGetLength()) {
        NCBI_THROW(CBlastException, eInvalidArgument, 
                   "Cannot get sequence length");
    }
    if ( !inst.CanGetSeq_data() ) {
        NCBI_THROW(CBlastException, eInvalidArgument, 
                   "Cannot get sequence data");
    }

    CBlastSeqVectorFromCSeq_data seq_data(inst.GetSeq_data(), inst.GetLength());
    return GetSequence_OMF(seq_data, encoding, strand, sentinel, warnings);
}

TSeqPos 
CBlastQuerySourceBioseqSet::GetLength(int index) const 
{
    if ( !m_Bioseqs[index]->GetInst().CanGetLength() ) {
        NCBI_THROW(CBlastException, eInvalidArgument, 
                   "Bioseq " + NStr::IntToString(index) + " does not " 
                   "have its length field set");
    }
    return m_Bioseqs[index]->GetInst().GetLength();
}

void 
CBlastQuerySourceBioseqSet::x_BioseqSanityCheck(const CBioseq& bs) 
{
    // Verify that the correct representation is used
    switch (CSeq_inst::ERepr repr = bs.GetInst().GetRepr()) {
    case CSeq_inst::eRepr_raw: break;
    default:
        {
            const CEnumeratedTypeValues* p =
                CSeq_inst::ENUM_METHOD_NAME(ERepr)();
            string msg = p->FindName(repr, false) + " is not supported for "
                "BLAST query sequence data - Use object manager "
                "interface or provide " +
                p->FindName(CSeq_inst::eRepr_raw, false) + 
                " representation";
            NCBI_THROW(CBlastException, eNotSupported, msg);
        }
    }

    // Verify that the molecule of the data is the same as the one
    // specified by the program requested

    if ( bs.GetInst().IsAa() && !m_IsProt ) {
        NCBI_THROW(CBlastException, eInvalidArgument,
           "Protein Bioseq specified in program which expects "
           "nucleotide query");
    }

    if ( bs.GetInst().IsNa() && m_IsProt ) {
        NCBI_THROW(CBlastException, eInvalidArgument,
           "Nucleotide Bioseq specified in program which expects "
           "protein query");
    }
}


static CRef<CBioseq_set>
s_ConstBioseqSetToBioseqSet(CConstRef<CBioseq_set> bioseq_set)
{
    // KB
    throw runtime_error(string("KB will implement ") + NCBI_CURRENT_FUNCTION);
    return CRef<CBioseq_set>(NULL);
}
static IRemoteQueryData::TSeqLocs
s_ConstBioseqSetToSeqLocs(CConstRef<CBioseq_set> bioseq_set)
{
    // KB
    IRemoteQueryData::TSeqLocs retval;
    throw runtime_error(string("KB will implement ") + NCBI_CURRENT_FUNCTION);
    return retval;
}

/////////////////////////////////////////////////////////////////////////////
//
// CObjMgrFree_LocalQueryData
//
/////////////////////////////////////////////////////////////////////////////

class CObjMgrFree_LocalQueryData : public ILocalQueryData
{
public:
    CObjMgrFree_LocalQueryData(CConstRef<objects::CBioseq_set> bioseq_set,
                               const CBlastOptions* options);
    
    BLAST_SequenceBlk* GetSequenceBlk();
    BlastQueryInfo* GetQueryInfo();
    
    /// Get the number of queries.
    virtual int GetNumQueries()
    {
        return m_QuerySource->Size();
    }
    
    /// Get the Seq_loc for the sequence indicated by index.
    virtual CConstRef<CSeq_loc> GetSeq_loc(int index)
    {
        return m_QuerySource->GetSeqLoc(index);
    }
    
    /// Get the length of the sequence indicated by index.
    virtual int GetSeqLength(int index)
    {
        return m_QuerySource->GetLength(index);
    }
    
private:
    const CBlastOptions* m_Options;
    CConstRef<objects::CBioseq_set> m_BioseqSet;
    
    AutoPtr<IBlastQuerySource> m_QuerySource;
};

CObjMgrFree_LocalQueryData::CObjMgrFree_LocalQueryData
    (CConstRef<CBioseq_set> bioseq_set, const CBlastOptions* options)
    : m_Options(options), m_BioseqSet(bioseq_set)
{
    bool is_prot = Blast_QueryIsProtein(options->GetProgramType());
    m_QuerySource.reset(new CBlastQuerySourceBioseqSet(bioseq_set, is_prot));
}

BLAST_SequenceBlk*
CObjMgrFree_LocalQueryData::GetSequenceBlk()
{
    if (m_SeqBlk.Get() == NULL) {
        if (m_BioseqSet.NotEmpty()) {
            m_SeqBlk.Reset(s_SafeSetupQueries(*m_QuerySource,
                                              m_Options,
                                              GetQueryInfo()));
        } else {
            NCBI_THROW(CBlastException, eInvalidArgument,
                       "Missing source data in " +
                       string(NCBI_CURRENT_FUNCTION));
        }
    }
    return m_SeqBlk;
}

BlastQueryInfo*
CObjMgrFree_LocalQueryData::GetQueryInfo()
{
    if (m_QueryInfo.Get() == NULL) {
        if (m_BioseqSet.NotEmpty()) {
            m_QueryInfo.Reset(s_SafeSetupQueryInfo(*m_QuerySource, m_Options));
        } else {
            NCBI_THROW(CBlastException, eInvalidArgument,
                       "Missing source data in " +
                       string(NCBI_CURRENT_FUNCTION));
        }
    }
    return m_QueryInfo;
}

/////////////////////////////////////////////////////////////////////////////
//
// CObjMgrFree_RemoteQueryData
//
/////////////////////////////////////////////////////////////////////////////

class CObjMgrFree_RemoteQueryData : public IRemoteQueryData
{
public:
    CObjMgrFree_RemoteQueryData(CConstRef<objects::CBioseq_set> bioseq_set);

    CRef<objects::CBioseq_set> GetBioseqSet();
    TSeqLocs GetSeqLocs();

private:
    CConstRef<objects::CBioseq_set> m_ClientBioseqSet;
};

CObjMgrFree_RemoteQueryData::CObjMgrFree_RemoteQueryData
    (CConstRef<CBioseq_set> bioseq_set)
    : m_ClientBioseqSet(bioseq_set)
{}

CRef<CBioseq_set>
CObjMgrFree_RemoteQueryData::GetBioseqSet()
{
    if (m_Bioseqs.Empty()) {
        if (m_ClientBioseqSet.NotEmpty()) {
            m_Bioseqs.Reset(s_ConstBioseqSetToBioseqSet(m_ClientBioseqSet));
        } else {
            NCBI_THROW(CBlastException, eInvalidArgument,
                       "Missing source data in " +
                       string(NCBI_CURRENT_FUNCTION));
        }
    }
    return m_Bioseqs;
}

IRemoteQueryData::TSeqLocs
CObjMgrFree_RemoteQueryData::GetSeqLocs()
{
    if (m_SeqLocs.empty()) {
        if (m_ClientBioseqSet.NotEmpty()) {
            m_SeqLocs = s_ConstBioseqSetToSeqLocs(m_ClientBioseqSet);
        } else {
            NCBI_THROW(CBlastException, eInvalidArgument,
                       "Missing source data in " +
                       string(NCBI_CURRENT_FUNCTION));
        }
    }
    return m_SeqLocs;
}

/////////////////////////////////////////////////////////////////////////////
//
// CObjMgrFree_QueryFactory
//
/////////////////////////////////////////////////////////////////////////////

CObjMgrFree_QueryFactory::CObjMgrFree_QueryFactory(CConstRef<CBioseq> b)
    : m_Bioseqs(0)
{
    CRef<CSeq_entry> seq_entry(new CSeq_entry);
    seq_entry->SetSeq(const_cast<CBioseq&>(*b));
    CRef<CBioseq_set> bioseq_set(new CBioseq_set);
    bioseq_set->SetSeq_set().push_back(seq_entry);
    m_Bioseqs = bioseq_set;
}

CObjMgrFree_QueryFactory::CObjMgrFree_QueryFactory(CConstRef<CBioseq_set> b)
    : m_Bioseqs(b)
{}

CRef<ILocalQueryData>
CObjMgrFree_QueryFactory::x_MakeLocalQueryData(const CBlastOptions* opts)
{
    CRef<ILocalQueryData> retval;
    
    if (m_Bioseqs.NotEmpty()) {
        retval.Reset(new CObjMgrFree_LocalQueryData(m_Bioseqs, opts));
    } else {
        NCBI_THROW(CBlastException, eInvalidArgument,
                   "Missing source data in " +
                   string(NCBI_CURRENT_FUNCTION));
    }
    
    return retval;
}

CRef<IRemoteQueryData>
CObjMgrFree_QueryFactory::x_MakeRemoteQueryData()
{
    CRef<IRemoteQueryData> retval;

    if (m_Bioseqs.NotEmpty()) {
        retval.Reset(new CObjMgrFree_RemoteQueryData(m_Bioseqs));
    } else {
        NCBI_THROW(CBlastException, eInvalidArgument,
                   "Missing source data in " +
                   string(NCBI_CURRENT_FUNCTION));
    }

    return retval;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
