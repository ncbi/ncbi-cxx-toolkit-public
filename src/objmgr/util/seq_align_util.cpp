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
* Author:  Aleksey Grichenko
*
* File Description:
*   Seq-align utilities
*/

#include <ncbi_pch.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Spliced_exon_chunk.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/Object_id.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/seq_align_util.hpp>
#include <util/sequtil/sequtil_manip.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(sequence)


CRef<CSeq_align> RemapAlignToLoc(const CSeq_align& align,
                                 CSeq_align::TDim  row,
                                 const CSeq_loc&   loc,
                                 CScope*           scope)
{
    if ( loc.IsWhole() ) {
        CRef<CSeq_align> copy(new CSeq_align);
        copy->Assign(align);
        return copy;
    }
    const CSeq_id* orig_id = loc.GetId();
    if ( !orig_id ) {
        NCBI_THROW(CAnnotMapperException, eBadLocation,
                   "Location with multiple ids can not be used to "
                   "remap seq-aligns.");
    }
    CRef<CSeq_id> id(new CSeq_id);
    id->Assign(*orig_id);

    // Create source seq-loc
    CSeq_loc src_loc(*id, 0, GetLength(loc, scope) - 1);
    ENa_strand strand = loc.GetStrand();
    if (strand != eNa_strand_unknown) {
        src_loc.SetStrand(strand);
    }
    CSeq_loc_Mapper mapper(src_loc, loc, scope);
    return mapper.Map(align, row);
}


class CProductStringBuilder
{
public:
    CProductStringBuilder(const CSeq_align& align, CScope& scope);
    const string& GetProductString(void);

private:
    bool x_AddExon(const CSpliced_exon& ex);
    bool x_AddExonPart(const CSpliced_exon_chunk& ch, TSeqPos& gen_offset);
    void x_Match(TSeqPos gen_from, TSeqPos gen_to_open);
    bool x_Mismatch(TSeqPos mismatch_len);

    const CSeq_align& m_Align;
    CScope& m_Scope;
    string m_MismatchedBases;
    bool m_GenRev = false;
    bool m_ProdRev = false;
    CSeqVector m_GenVector;
    string m_ExonData;
    string m_Result;
    TSeqPos m_ProdPos = 0;
    size_t m_MismatchPos = 0;
};


CProductStringBuilder::CProductStringBuilder(const CSeq_align& align, CScope& scope)
    : m_Align(align), m_Scope(scope)
{
}


const string& CProductStringBuilder::GetProductString(void)
{
    m_Result.clear();
    // Only spliced-segs are supported.
    if (!m_Align.GetSegs().IsSpliced()) {
        NCBI_THROW(CObjmgrUtilException, eBadAlignment,
            "Only splised-seg alignments are supported");
    }

    const CSpliced_seg& spliced_seg = m_Align.GetSegs().GetSpliced();
    // Only genomic alignments support MismatchedBases.
    if (spliced_seg.GetProduct_type() != CSpliced_seg::eProduct_type_transcript) {
        // ERROR: Non-transcript alignment.
        NCBI_THROW(CObjmgrUtilException, eBadAlignment,
            "Only transcript spliced-segs are supported");
    }

    const CSeq_id& gen_id = m_Align.GetSeq_id(1);

    CBioseq_Handle gen_handle = m_Scope.GetBioseqHandle(gen_id);
    if ( !gen_handle ) {
        NCBI_THROW(CObjmgrUtilException, eBadAlignment,
            "Failed to fetch genomic sequence data");
    }

    m_GenVector = CSeqVector(gen_handle, CBioseq_Handle::eCoding_Iupac);

    if ( spliced_seg.IsSetProduct_length() ) {
        m_Result.reserve(spliced_seg.GetProduct_length());
    }
    m_GenRev = IsReverse(m_Align.GetSeqStrand(1));
    m_ProdRev = IsReverse(m_Align.GetSeqStrand(0));

    // NOTE: Even if ext is not set or does not contain MismatchedBases entry it may
    // still be possible to generate product sequence if the alignment is a perfect
    // match (no indels, mismatches or unaligned ranges on product).

    if ( m_Align.IsSetExt() ) {
        // Find MismatchedBases entry in ext. If several entries are present, use
        // the first one.
        ITERATE(CSeq_align::TExt, ext_it, m_Align.GetExt()) {
            const CUser_object& obj = **ext_it;
            if (obj.GetType().IsStr() && obj.GetType().GetStr() == "MismatchedBases") {
                ITERATE(CUser_object::TData, data_it, obj.GetData()) {
                    const CUser_field& field = **data_it;
                    if (field.GetLabel().IsStr() && field.GetLabel().GetStr() == "Bases"  &&
                        field.GetData().IsStr()) {
                        m_MismatchedBases = field.GetData().GetStr();
                        break;
                    }
                }
                if ( !m_MismatchedBases.empty() ) break;
            }
        }
    }

    if ((m_GenRev != m_ProdRev)  &&  !m_MismatchedBases.empty()) {
        CSeqManip::ReverseComplement(m_MismatchedBases, CSeqUtil::e_Iupacna, 0, (TSeqPos)m_MismatchedBases.size());
    }

    const CSpliced_seg::TExons& exons = spliced_seg.GetExons();

    if ( m_ProdRev ) {
        REVERSE_ITERATE(CSpliced_seg::TExons, ex_it, exons) {
            if ( !x_AddExon(**ex_it) ) return kEmptyStr;
        }
    }
    else {
        ITERATE(CSpliced_seg::TExons, ex_it, exons) {
            if ( !x_AddExon(**ex_it) ) return kEmptyStr;
        }
    }
    if (m_MismatchPos < m_MismatchedBases.size()) {
        x_Mismatch(TSeqPos(m_MismatchedBases.size() - m_MismatchPos));
    }

    return m_Result;
}


bool CProductStringBuilder::x_AddExon(const CSpliced_exon& ex)
{
    TSeqPos gen_from = ex.GetGenomic_start();
    TSeqPos gen_to = ex.GetGenomic_end() + 1; // open range
    _ASSERT(ex.GetProduct_start().IsNucpos());
    _ASSERT(ex.GetProduct_end().IsNucpos());

    // The whole exon must be reverse-complemented.
    m_GenVector.GetSeqData(gen_from, gen_to, m_ExonData);
    if (m_GenRev != m_ProdRev) {
        CSeqManip::ReverseComplement(m_ExonData, CSeqUtil::e_Iupacna, 0, gen_to - gen_from);
    }

    TSeqPos prod_from = ex.GetProduct_start().GetNucpos();
    if (prod_from > m_ProdPos) {
        if ( !x_Mismatch(prod_from - m_ProdPos) ) return false;
    }
    _ASSERT(prod_from == m_ProdPos);

    if ( ex.IsSetParts() ) {
        // Iterate parts
        TSeqPos gen_offset = 0;
        if (m_ProdRev) {
            REVERSE_ITERATE(CSpliced_exon::TParts, part_it, ex.GetParts()) {
                if ( !x_AddExonPart(**part_it, gen_offset) ) return false;
            }
        }
        else {
            ITERATE(CSpliced_exon::TParts, part_it, ex.GetParts()) {
                if ( !x_AddExonPart(**part_it, gen_offset) ) return false;
            }
        }
    }
    else {
        // Use whole exon
        x_Match(0, gen_to - gen_from);
    }
    _ASSERT(m_ProdPos == ex.GetProduct_end().GetNucpos() + 1);
    return true;
}


bool CProductStringBuilder::x_AddExonPart(const CSpliced_exon_chunk& ch, TSeqPos& gen_offset)
{
    switch ( ch.Which() ) {
    case CSpliced_exon_chunk::e_Match:
        x_Match(gen_offset, gen_offset + ch.GetMatch());
        gen_offset += ch.GetMatch();
        break;
    case CSpliced_exon_chunk::e_Mismatch:
        if ( !x_Mismatch(ch.GetMismatch()) ) return false;
        gen_offset += ch.GetMismatch();
        break;
    case CSpliced_exon_chunk::e_Product_ins:
        x_Mismatch(ch.GetProduct_ins());
        break;
    case CSpliced_exon_chunk::e_Genomic_ins:
        gen_offset += ch.GetGenomic_ins();
        break;
    case CSpliced_exon_chunk::e_Diag:
        // ERROR: It's not clear if diag is a match or a mismatch.
    default:
        // ERROR: Unexpected chunk type.
        NCBI_THROW(CObjmgrUtilException, eBadAlignment,
            "Unsupported chunk type");
    }
    return true;
}


inline
void CProductStringBuilder::x_Match(TSeqPos gen_from, TSeqPos gen_to_open)
{
    m_Result.append(m_ExonData.substr(gen_from, gen_to_open - gen_from));
    m_ProdPos += gen_to_open - gen_from;
}


inline
bool CProductStringBuilder::x_Mismatch(TSeqPos mismatch_len)
{
    if (m_MismatchedBases.size() < mismatch_len) return false;
    m_Result.append(m_MismatchedBases.substr(m_MismatchPos, mismatch_len));
    m_MismatchPos += mismatch_len;
    m_ProdPos += mismatch_len;
    return true;
}


string GetProductString(const CSeq_align& align, CScope& scope)
{
    CProductStringBuilder builder(align, scope);
    return builder.GetProductString();
}


END_SCOPE(sequence)
END_SCOPE(objects)
END_NCBI_SCOPE
