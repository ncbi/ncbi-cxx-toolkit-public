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
 * Author:  Christiam Camacho
 *
 */

/** @file bioseq_extract_data_priv.cpp
 * Implementations of CBlastQuerySourceBioseqSet and
 * CBlastSeqVectorFromCSeq_data classes.
 */

#include <ncbi_pch.hpp>

// BLAST API includes
#include <algo/blast/api/blast_options.hpp>
#include <algo/blast/api/blast_exception.hpp>

// Sequence utilities includes
#include <util/sequtil/sequtil_convert.hpp>
#include <util/sequtil/sequtil_manip.hpp>

// Serial includes
#include <serial/iterator.hpp>
#include <serial/enumvalues.hpp>

// Object includes
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Seq_inst.hpp>

// Private BLAST API headers
#include "blast_setup.hpp"
#include "bioseq_extract_data_priv.hpp"

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

/////////////////////////////////////////////////////////////////////////////
//
// CBlastSeqVectorFromCSeq_data
//
/////////////////////////////////////////////////////////////////////////////

CBlastSeqVectorFromCSeq_data::CBlastSeqVectorFromCSeq_data
    (const objects::CSeq_data& seq_data, TSeqPos length)
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
CBlastSeqVectorFromCSeq_data::SetCoding(objects::CSeq_data::E_Choice c)
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
    // N.B.: we're not using the at() method for compatibility with GCC 2.95
    if (pos >= x_Size()) {
        NCBI_THROW(CCoreException, eInvalidArg,
                   "CBlastSeqVectorFromCSeq_data: position out of range");
    }
    return m_SequenceData[pos];
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

void 
CBlastSeqVectorFromCSeq_data::x_ComplementData()
{
    TSeqPos nconv = CSeqManip::ReverseComplement(m_SequenceData,
                                                 m_Encoding, 0, size());
    ASSERT(nconv == size());
    nconv += 0; // eliminate compiler warning
}

CSeqUtil::ECoding 
CBlastSeqVectorFromCSeq_data::x_Encoding_CSeq_data2CSeqUtil
(objects::CSeq_data::E_Choice c)
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

CBlastQuerySourceBioseqSet::CBlastQuerySourceBioseqSet
    (const objects::CBioseq_set& bss, bool is_prot) 
    : m_IsProt(is_prot)
{
    // sacrifice speed for protection against infinite loops
    CTypeConstIterator<objects::CBioseq> itr(ConstBegin(bss, eDetectLoops)); 
    for (; itr; ++itr) {
        x_BioseqSanityCheck(*itr);
        m_Bioseqs.push_back(CConstRef<objects::CBioseq>(&*itr));
    }
}

objects::ENa_strand 
CBlastQuerySourceBioseqSet::GetStrand(int /*index*/) const 
{
    // Although the strand represented in the Bioseq is always the plus
    // strand, the default for searching BLAST is both strands in the
    // query, unless specified otherwise in the BLAST options
    return m_IsProt ? objects::eNa_strand_unknown : objects::eNa_strand_both;
}

TSeqPos 
CBlastQuerySourceBioseqSet::Size() const 
{ 
    return m_Bioseqs.size(); 
}

CConstRef<objects::CSeq_loc> 
CBlastQuerySourceBioseqSet::GetMask(int /*index*/) const 
{
    return CConstRef<objects::CSeq_loc>(0);
}

CConstRef<objects::CSeq_loc> 
CBlastQuerySourceBioseqSet::GetSeqLoc(int index) const 
{ 
    CRef<objects::CSeq_loc> retval(new objects::CSeq_loc);
    retval->SetWhole().Assign(*m_Bioseqs[index]->GetFirstId());
    // FIXME: make sure this works (perhaps we need to build our own
    // Seq-interval
    return retval;
}

SBlastSequence
CBlastQuerySourceBioseqSet::GetBlastSequence(int index, 
                                             EBlastEncoding encoding, 
                                             objects::ENa_strand strand, 
                                             ESentinelType sentinel, 
                                             string* warnings) const 
{
    const objects::CSeq_inst& inst = m_Bioseqs[index]->GetInst();
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
CBlastQuerySourceBioseqSet::x_BioseqSanityCheck(const objects::CBioseq& bs) 
{
    // Verify that the correct representation is used
    switch (objects::CSeq_inst::ERepr repr = bs.GetInst().GetRepr()) {
    case objects::CSeq_inst::eRepr_raw: break;
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


END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
