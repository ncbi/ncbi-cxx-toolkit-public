
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
 * Authors:  Justin Foley
 *
 * File Description:  Write alignment
 *
 */

#include <ncbi_pch.hpp>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Sparse_align.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Prot_pos.hpp>
#include <objects/general/Object_id.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_vector.hpp>

#include <objtools/writers/writer_exception.hpp>
#include <objtools/writers/write_util.hpp>
#include <objtools/writers/aln_writer.hpp>

#include <util/sequtil/sequtil_manip.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ----------------------------------------------------------------------------
CAlnWriter::CAlnWriter(
    CScope& scope,
    CNcbiOstream& ostr,
    unsigned int uFlags) :
    CWriterBase(ostr, uFlags) 
{
    m_pScope.Reset(&scope);
};


//  ----------------------------------------------------------------------------

CAlnWriter::CAlnWriter(
    CNcbiOstream& ostr,
    unsigned int uFlags) :
    CWriterBase(ostr, uFlags)
{
    m_pScope.Reset(new CScope(*CObjectManager::GetInstance()));
};


//  ----------------------------------------------------------------------------
bool CAlnWriter::WriteAlign(
    const CSeq_align& align,
    const string& name,
    const string& descr) 
{

    switch (align.GetSegs().Which()) {
    case CSeq_align::C_Segs::e_Denseg:
        return xWriteAlignDenseSeg(align.GetSegs().GetDenseg());
    case CSeq_align::C_Segs::e_Spliced:
        return xWriteAlignSplicedSeg(align.GetSegs().GetSpliced());
    case CSeq_align::C_Segs::e_Sparse:
        break;
    case CSeq_align::C_Segs::e_Std:
        break;
    default:
        break;
    }



    return false;
}
//  ----------------------------------------------------------------------------

bool s_TryFindRange(const CObject_id& local_id, 
        CRef<CSeq_id>& seq_id,
        CRange<TSeqPos>& range)
{
    if (local_id.IsStr()) {

        string id_string = local_id.GetStr();
        string true_id;
        string range_string;
        if (!NStr::SplitInTwo(id_string, ":", true_id, range_string)) {
            return false;
        }

        string start_pos, end_pos;
        if (!NStr::SplitInTwo(range_string, "-", start_pos, end_pos)) {
            return false;
        }


        try {
            TSeqPos start_index = NStr::StringToNumeric<TSeqPos>(start_pos);
            TSeqPos end_index = NStr::StringToNumeric<TSeqPos>(end_pos);

            list<CRef<CSeq_id>> id_list;
            CSeq_id::ParseIDs(id_list, true_id);

            seq_id = id_list.front();
            range.SetFrom(start_index);
            range.SetTo(end_index);
        }
        catch (...) {
            return false;
        }
    }
    return true;
}

// -----------------------------------------------------------------------------

void CAlnWriter::xProcessSeqId(const CSeq_id& id, CBioseq_Handle& bsh, CRange<TSeqPos>& range) 
{   
    if (m_pScope) {
        if (id.IsLocal()) {
            CRef<CSeq_id> pTrueId;
            if (s_TryFindRange(id.GetLocal(), pTrueId, range)) {
                bsh = m_pScope->GetBioseqHandle(*pTrueId);
            }
        }
        else 
        {
            bsh = m_pScope->GetBioseqHandle(id);
            range.SetFrom(CRange<TSeqPos>::GetPositionMin());
            range.SetToOpen(CRange<TSeqPos>::GetPositionMax());
        }
    }
}


// -----------------------------------------------------------------------------

bool CAlnWriter::xGetSeqString(CBioseq_Handle bsh, 
    const CRange<TSeqPos>& range,
    ENa_strand strand, 
    string& seq)
{
    if (!bsh) {
        return false;
    }
    CSeqVector seq_vec = bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac, strand);
    if (range.IsWhole()) {
        seq_vec.GetSeqData(0, bsh.GetBioseqLength(), seq);
    }
    else {
        seq_vec.GetSeqData(range.GetFrom(), range.GetTo(), seq);
    }

    return true;
}

// -----------------------------------------------------------------------------

bool CAlnWriter::xWriteAlignDenseSeg(
    const CDense_seg& denseg)
{
    if (!denseg.CanGetDim() ||
        !denseg.CanGetNumseg() ||
        !denseg.CanGetIds() ||
        !denseg.CanGetStarts() ||
        !denseg.CanGetLens()) 
    {
        return false;
    }

    const auto num_rows = denseg.GetDim();
    const auto num_segs = denseg.GetNumseg();


    for (int row=0; row<num_rows; ++row) 
    {
        const CSeq_id& id = denseg.GetSeq_id(row);
        CRange<TSeqPos> range;

        CBioseq_Handle bsh;
        xProcessSeqId(id, bsh, range);
        if (!bsh) {
            continue;
        }

        string seq_plus;
        if (!xGetSeqString(bsh, range, eNa_strand_plus, seq_plus)) {
            // Throw an exception
        }

        const CSeqUtil::ECoding coding = 
            (bsh.IsNucleotide()) ?
            CSeqUtil::e_Iupacna :
            CSeqUtil::e_Iupacaa;

        string seqdata = "";
        for (int seg=0; seg<num_segs; ++seg)
        {
            const auto start = denseg.GetStarts()[seg*num_rows + row];
            const auto len   = denseg.GetLens()[seg];
            const ENa_strand strand = (denseg.IsSetStrands()) ?
                denseg.GetStrands()[seg*num_rows + row] :
                eNa_strand_plus;
            seqdata += xGetSegString(seq_plus, coding, strand, start, len);
        }
        m_Os << ">" + id.AsFastaString() << "\n";
        size_t pos=0;
        size_t width = 60;
        while (pos < seqdata.size()) {
            m_Os << seqdata.substr(pos, width) << "\n";
            pos += width;
        }
    }

    return true;
}

// -----------------------------------------------------------------------------

bool CAlnWriter::xWriteAlignSplicedSeg(
    const CSpliced_seg& spliced_seg)
{
    if (!spliced_seg.IsSetExons()) {
        return false;
    }
    
    string genomic_id;
    if (spliced_seg.IsSetGenomic_id()) {
        genomic_id = spliced_seg.GetGenomic_id().AsFastaString();
    }


    string product_id;
    if (spliced_seg.IsSetGenomic_id()) {
        product_id = spliced_seg.GetProduct_id().AsFastaString();
    }


    ENa_strand genomic_strand = 
        spliced_seg.IsSetGenomic_strand() ?
        spliced_seg.GetGenomic_strand() :
        eNa_strand_plus;


    ENa_strand product_strand = 
        spliced_seg.IsSetProduct_strand() ?
        spliced_seg.GetProduct_strand() :
        eNa_strand_plus;


    
    return false;
}

// -----------------------------------------------------------------------------
bool CAlnWriter::xWriteSplicedExons(const list<CRef<CSpliced_exon>>& exons,   
    CSpliced_seg::TProduct_type product_type,
    CRef<CSeq_id> default_genomic_id,
    ENa_strand default_genomic_strand,
    CRef<CSeq_id> default_product_id,
    ENa_strand default_product_strand) 
{

    string prev_genomic_id;
    string prev_product_id;
    for (const CRef<CSpliced_exon>& exon : exons) {
        
        const CSeq_id& genomic_id = 
            exon->IsSetGenomic_id() ? 
            exon->GetGenomic_id() :
            *default_genomic_id;

        const CSeq_id& product_id = 
            exon->IsSetProduct_id() ?
            exon->GetProduct_id() :
            *default_product_id;

        // Should check to see that the ids are not empty
   
        const ENa_strand genomic_strand = 
            exon->IsSetGenomic_strand() ?
            exon->GetGenomic_strand() :
            default_genomic_strand;

        const ENa_strand product_strand = 
            exon->IsSetProduct_strand() ?
            exon->GetProduct_strand() :
            default_product_strand;

        const auto genomic_start = exon->GetGenomic_start();
        const auto genomic_end = exon->GetGenomic_end();

        if (genomic_end < genomic_start) {
            // Throw an exception
        }
        const int genomic_length = genomic_end - genomic_start;

        const CProduct_pos& product_start = exon->GetProduct_start();
        const CProduct_pos& product_end = exon->GetProduct_end();
/*
        if (product_end < product_start) {
            // Throw an exception
        }
        const int product_length = product_end - product_start;
*/
        // if genomic_length/tgtwidth != product_length, we need to look at 
        // parts. These appear in biological order.
        //
    }

    return false;
}



// -----------------------------------------------------------------------------

string CAlnWriter::xGetSegString(const string& seq_plus, 
    CSeqUtil::ECoding coding,
    const ENa_strand strand,
    const int start,
    const size_t len)
{
    if (start >= 0) {
        if (start >= seq_plus.size()) {
            // Throw an exception
        }
        if (strand == eNa_strand_plus) {
            return seq_plus.substr(start, len);
        }
        // else
        string seq_minus;
        CSeqManip::ReverseComplement(seq_plus, coding, start, len, seq_minus);
        return seq_minus;
    }

    return string(len, '-');
}


// -----------------------------------------------------------------------------

bool CAlnWriter::xWriteSparseAlign(const CSparse_align& sparse_align)
{
    const auto num_segs = sparse_align.GetNumseg();
    

    { // First row
        const CSeq_id& first_id  = sparse_align.GetFirst_id();
        CBioseq_Handle bsh;
        CRange<TSeqPos> range;
        xProcessSeqId(first_id, bsh, range);
        if (!bsh) {
            // Throw an exception
        }
        const CSeqUtil::ECoding coding = 
            (bsh.IsNucleotide())?
            CSeqUtil::e_Iupacna :
            CSeqUtil::e_Iupacaa;

        string seq_plus;
        if (!xGetSeqString(bsh, range, eNa_strand_plus, seq_plus)) {
            // Throw an exception
        }
    
        string seqdata = "";
        for (int seg=0; seg<num_segs; ++seg) {
            const auto start = sparse_align.GetFirst_starts()[seg];
            const auto len = sparse_align.GetLens()[seg];
            seqdata += xGetSegString(seq_plus, coding, eNa_strand_plus, start, len);
        }
    }

    { // Second row
        const CSeq_id& second_id  = sparse_align.GetSecond_id();
        CBioseq_Handle bsh;
        CRange<TSeqPos> range;
        xProcessSeqId(second_id, bsh, range);
        if (!bsh) {
            // Throw an exception
        }
        const CSeqUtil::ECoding coding = 
            (bsh.IsNucleotide())?
            CSeqUtil::e_Iupacna :
            CSeqUtil::e_Iupacaa;

        string seq_plus;
        if (!xGetSeqString(bsh, range, eNa_strand_plus, seq_plus)) {
            // Throw an exception
        }

        string seqdata = "";
        const vector<ENa_strand>& strands = sparse_align.IsSetSecond_strands() ?
            sparse_align.GetSecond_strands() : vector<ENa_strand>(num_segs, eNa_strand_plus);
        for (int seg=0; seg<num_segs; ++seg) {
            const auto start = sparse_align.GetFirst_starts()[seg];
            const auto len = sparse_align.GetLens()[seg];
            seqdata += xGetSegString(seq_plus, coding, eNa_strand_plus, start, len);
        }
    }


    return true;
}

// -----------------------------------------------------------------------------

//string CAlnWriter::xGetSparseExonString(const CSeq_id& product_id,
//        const CSeq_id& genomic_id 


END_NCBI_SCOPE

