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
#include <objects/seqalign/Sparse_seg.hpp>
#include <objects/seqalign/Sparse_align.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Prot_pos.hpp>
#include <objects/seqalign/Spliced_exon_chunk.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/seq_id_handle.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>

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
    m_Width = 60;
    CGenbankIdResolve::Get().SetLabelType(CSeq_id::eFasta);
};


//  ----------------------------------------------------------------------------

CAlnWriter::CAlnWriter(
    CNcbiOstream& ostr,
    unsigned int uFlags) :
    CAlnWriter(*(new CScope(*CObjectManager::GetInstance())), ostr, uFlags)
{
};


//  ----------------------------------------------------------------------------
bool CAlnWriter::WriteAlign(
    const CSeq_align& align,
    const string& name,
    const string& descr)
{

    switch (align.GetSegs().Which()) {
    case CSeq_align::C_Segs::e_Denseg:
        return WriteAlignDenseSeg(align.GetSegs().GetDenseg());
    case CSeq_align::C_Segs::e_Spliced:
        return WriteAlignSplicedSeg(align.GetSegs().GetSpliced());
    case CSeq_align::C_Segs::e_Sparse:
        return WriteAlignSparseSeg(align.GetSegs().GetSparse());
    case CSeq_align::C_Segs::e_Std:
        break;
    default:
        break;
    }

    return false;
}
//  ----------------------------------------------------------------------------

void CAlnWriter::ProcessSeqId(const CSeq_id& id, CBioseq_Handle& bsh, CRange<TSeqPos>& range)
{
    if (m_pScope) {

        bsh = m_pScope->GetBioseqHandle(id);
        range.SetFrom(CRange<TSeqPos>::GetPositionMin());
        range.SetToOpen(CRange<TSeqPos>::GetPositionMax());
    }
}


// -----------------------------------------------------------------------------

void CAlnWriter::GetSeqString(CBioseq_Handle bsh,
    const CRange<TSeqPos>& range,
    ENa_strand strand,
    string& seq)
{
    if (!bsh) {
        NCBI_THROW(CObjWriterException,
            eBadInput,
            "Empty bioseq handle");
    }

    CSeqVector seq_vec = bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac, strand);
    if (range.IsWhole()) {
        seq_vec.GetSeqData(0, bsh.GetBioseqLength(), seq);
    }
    else {
        seq_vec.GetSeqData(range.GetFrom(), range.GetTo(), seq);
    }

    if (NStr::IsBlank(seq)) {
        NCBI_THROW(CObjWriterException,
            eBadInput,
            "Empty sequence string");
    }
}

// -----------------------------------------------------------------------------

bool CAlnWriter::WriteAlignDenseSeg(
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
        ProcessSeqId(id, bsh, range);
        if (!bsh) {
            NCBI_THROW(CObjWriterException,
                eBadInput,
                "Unable to fetch Bioseq");
        }

        string seq_plus;
        GetSeqString(bsh, range, eNa_strand_plus, seq_plus);

        const CSeqUtil::ECoding coding =
            (bsh.IsNucleotide()) ?
            CSeqUtil::e_Iupacna :
            CSeqUtil::e_Iupacaa;

        string seqdata;
        for (int seg=0; seg<num_segs; ++seg)
        {
            const auto start = denseg.GetStarts()[seg*num_rows + row];
            const auto len   = denseg.GetLens()[seg];
            const ENa_strand strand = (denseg.IsSetStrands()) ?
                denseg.GetStrands()[seg*num_rows + row] :
                eNa_strand_plus;
            seqdata += GetSegString(seq_plus, coding, strand, start, len);
        }

        string best_id = GetBestId(id);
        string defline = ">" + best_id;
        WriteContiguous(defline, seqdata);
    }

    return true;
}

// -----------------------------------------------------------------------------

bool CAlnWriter::WriteAlignSplicedSeg(
    const CSpliced_seg& spliced_seg)
{
    if (!spliced_seg.IsSetExons()) {
        return false;
    }

    CRef<CSeq_id> genomic_id;
    if (spliced_seg.IsSetGenomic_id()) {
        genomic_id = Ref(new CSeq_id());
        genomic_id->Assign(spliced_seg.GetGenomic_id());
    }


    CRef<CSeq_id> product_id;
    if (spliced_seg.IsSetGenomic_id()) {
        product_id = Ref(new CSeq_id());
        product_id->Assign(spliced_seg.GetProduct_id());
    }


    ENa_strand genomic_strand =
        spliced_seg.IsSetGenomic_strand() ?
        spliced_seg.GetGenomic_strand() :
        eNa_strand_plus;


    ENa_strand product_strand =
        spliced_seg.IsSetProduct_strand() ?
        spliced_seg.GetProduct_strand() :
        eNa_strand_plus;

    return WriteSplicedExons(spliced_seg.GetExons(),
                              spliced_seg.GetProduct_type(),
                              genomic_id,
                              genomic_strand,
                              product_id,
                              product_strand);
}

unsigned int s_ProductLength(const CProduct_pos& start, const CProduct_pos& end)
{
    if (start.Which() != end.Which()) {
        NCBI_THROW(CObjWriterException,
            eBadInput,
            "Unable to determine product length");
    }

    if (start.Which() == CProduct_pos::e_not_set) {
        NCBI_THROW(CObjWriterException,
            eBadInput,
            "Unable to determine product length");
    }

    const int length = end.AsSeqPos() - start.AsSeqPos();

    return (length >= 0) ? length : -length;
}


// -----------------------------------------------------------------------------
bool CAlnWriter::WriteSplicedExons(const list<CRef<CSpliced_exon>>& exons,
    CSpliced_seg::TProduct_type product_type,
    CRef<CSeq_id> default_genomic_id, // May be NULL
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
            NCBI_THROW(CObjWriterException,
                eBadInput,
                "Bad genomic location: end < start");
        }
        const int genomic_length = genomic_end - genomic_start;

        const int product_start = exon->GetProduct_start().AsSeqPos();
        const int product_end = exon->GetProduct_end().AsSeqPos();


        if (product_end < product_start) {
            NCBI_THROW(CObjWriterException,
                eBadInput,
                "Bad product location: end < start");
        }
        // product_length is now given in nucleotide units
        const int product_length = product_end - product_start;

        CBioseq_Handle genomic_bsh;
        if (m_pScope) {
            genomic_bsh = m_pScope->GetBioseqHandle(genomic_id);
        }
        if (!genomic_bsh) {
            NCBI_THROW(CObjWriterException,
                eBadInput,
                "Unable to resolve genomic sequence");
        }
        CRange<TSeqPos> genomic_range(genomic_start, genomic_end+1);
        string genomic_seq;
        GetSeqString(genomic_bsh, genomic_range, genomic_strand, genomic_seq);

        CBioseq_Handle product_bsh;
        if (m_pScope) {
            product_bsh = m_pScope->GetBioseqHandle(product_id);
        }
        if (!product_bsh) {
            NCBI_THROW(CObjWriterException,
                eBadInput,
                "Unable to resolve product sequence");
        }
        CRange<TSeqPos> product_range(product_start, product_end+1);
        string product_seq;
        GetSeqString(product_bsh, product_range, product_strand, product_seq);

        if (exon->IsSetParts()) {
            AddGaps(product_type, exon->GetParts(), genomic_seq, product_seq);
        }
        else
        if (product_length != genomic_length) {
            NCBI_THROW(CObjWriterException,
                eBadInput,
                "Lengths of genomic and product sequences don't match");
        }

        WriteContiguous(">" + GetBestId(genomic_id), genomic_seq);
        WriteContiguous(">" + GetBestId(product_id), product_seq);
    }

    return true;
}

// -----------------------------------------------------------------------------

void CAlnWriter::AddGaps(
        CSpliced_seg::TProduct_type product_type,
        const CSpliced_exon::TParts& exon_chunks,
        string& genomic_seq,
        string& product_seq)
{

    if (exon_chunks.empty()) {
        return;
    }

    string genomic_string;
    string product_string;

    const unsigned int res_width =
        (product_type == CSpliced_seg::eProduct_type_transcript) ?
        1 : 3;


    // Check that match + mismatch + diag + genomic_ins = genomic_length
    int genomic_pos = 0;
    int product_pos = 0;
    unsigned int interval_width = 0;
    for (CRef<CSpliced_exon_chunk> exon_chunk : exon_chunks) {
        auto chunk_type = exon_chunk->Which();
        switch(chunk_type) {
        case CSpliced_exon_chunk::e_Match:
        case CSpliced_exon_chunk::e_Mismatch:
        case CSpliced_exon_chunk::e_Diag:
            switch(chunk_type) {
                default:
                    // compiler, shut up!
                    break;
                case CSpliced_exon_chunk::e_Match:
                    interval_width = exon_chunk->GetMatch();
                    break;
                case CSpliced_exon_chunk::e_Mismatch:
                    interval_width = exon_chunk->GetMismatch();
                    break;
                case CSpliced_exon_chunk::e_Diag:
                    interval_width = exon_chunk->GetDiag();
                    break;
            }
            genomic_string.append(genomic_seq, genomic_pos, interval_width);
            product_string.append(product_seq, product_pos, (interval_width + (res_width-1))/res_width);
            genomic_pos += interval_width;
            product_pos += interval_width/res_width;
            break;

        case CSpliced_exon_chunk::e_Genomic_ins:
            interval_width = exon_chunk->GetGenomic_ins();
            genomic_string.append(genomic_seq, genomic_pos, interval_width);
            product_string.append(interval_width/res_width, '-');
            genomic_pos += interval_width;
            break;

        case CSpliced_exon_chunk::e_Product_ins:
            interval_width = exon_chunk->GetProduct_ins();
            genomic_string.append(interval_width, '-');
            product_string.append(product_seq, product_pos, interval_width/res_width);
            product_pos += interval_width/res_width;
            break;
        default:
            break;
        }
    }
    genomic_seq = genomic_string;
    product_seq = product_string;
}


// -----------------------------------------------------------------------------

string CAlnWriter::GetSegString(const string& seq_plus,
    CSeqUtil::ECoding coding,
    const ENa_strand strand,
    const int start,
    const size_t len)
{
    if (start >= 0) {
        if (start >= seq_plus.size()) {
            NCBI_THROW(CObjWriterException,
                eBadInput,
                "Bad location: impossible start");
        }
        if (strand != eNa_strand_minus) {
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
bool CAlnWriter::WriteAlignSparseSeg(
    const CSparse_seg& sparse_seg)
{
    for (CRef<CSparse_align> align : sparse_seg.GetRows())
    {
        if (!WriteSparseAlign(*align)) {
            return false;
        }
    }
    return true;
}



// -----------------------------------------------------------------------------

bool CAlnWriter::WriteSparseAlign(const CSparse_align& sparse_align)
{
    const auto num_segs = sparse_align.GetNumseg();

    {
        const CSeq_id& first_id  = sparse_align.GetFirst_id();
        CBioseq_Handle bsh;
        CRange<TSeqPos> range;
        ProcessSeqId(first_id, bsh, range);
        if (!bsh) {
            NCBI_THROW(CObjWriterException,
                eBadInput,
                "Unable to resolve ID " + first_id.AsFastaString());
        }
        const CSeqUtil::ECoding coding =
            (bsh.IsNucleotide())?
            CSeqUtil::e_Iupacna :
            CSeqUtil::e_Iupacaa;

        string seq_plus;
        GetSeqString(bsh, range, eNa_strand_plus, seq_plus);

        string seqdata;
        for (int seg=0; seg<num_segs; ++seg) {
            const auto start = sparse_align.GetFirst_starts()[seg];
            const auto len = sparse_align.GetLens()[seg];
            seqdata += GetSegString(seq_plus, coding, eNa_strand_plus, start, len);
        }
        WriteContiguous(">" + GetBestId(first_id), seqdata);
    }

    { // Second row
        const CSeq_id& second_id  = sparse_align.GetSecond_id();
        CBioseq_Handle bsh;
        CRange<TSeqPos> range;
        ProcessSeqId(second_id, bsh, range);
        if (!bsh) {
            NCBI_THROW(CObjWriterException,
                eBadInput,
                "Unable to resolve ID " + second_id.AsFastaString());
        }
        const CSeqUtil::ECoding coding =
            (bsh.IsNucleotide())?
            CSeqUtil::e_Iupacna :
            CSeqUtil::e_Iupacaa;

        string seq_plus;
        GetSeqString(bsh, range, eNa_strand_plus, seq_plus);

        string seqdata;
        const vector<ENa_strand>& strands = sparse_align.IsSetSecond_strands() ?
            sparse_align.GetSecond_strands() : vector<ENa_strand>(num_segs, eNa_strand_plus);
        for (int seg=0; seg<num_segs; ++seg) {
            const auto start = sparse_align.GetFirst_starts()[seg];
            const auto len = sparse_align.GetLens()[seg];
            seqdata += GetSegString(seq_plus, coding, strands[seg], start, len);
        }

        WriteContiguous(">" + GetBestId(second_id), seqdata);
    }
    return true;
}

// -----------------------------------------------------------------------------

void CAlnWriter::WriteContiguous(const string& defline, const string& seqdata)
{
    if (defline.back() == '|' && defline.size() > 1)
    {
        const auto length = defline.size();
        m_Os << defline.substr(0,length-1) << "\n";
    }
    else {
        m_Os << defline << "\n";
    }
    size_t pos=0;
    while (pos < seqdata.size()) {

        if (IsCanceled()) {
            NCBI_THROW(
                CObjWriterException,
                eInterrupted,
                "Processing terminated by user");
        }

        m_Os << seqdata.substr(pos, m_Width) << "\n";
        pos += m_Width;
    }
}

// -----------------------------------------------------------------------------

string CAlnWriter::GetBestId(const CSeq_id& id)
{
    string best_id;
    CGenbankIdResolve::Get().GetBestId(
        CSeq_id_Handle::GetHandle(id),
        *m_pScope,
        best_id);
    return best_id;
}

// -----------------------------------------------------------------------------

END_NCBI_SCOPE

