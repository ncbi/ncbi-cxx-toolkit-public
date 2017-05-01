
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
#include <objects/general/Object_id.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_vector.hpp>

#include <objtools/writers/writer_exception.hpp>
#include <objtools/writers/write_util.hpp>
#include <objtools/writers/aln_writer.hpp>

//#include <util/sequtil/sequtil.hpp>
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

    if (align.GetSegs().Which() == CSeq_align::C_Segs::e_Denseg) {
        return xWriteAlignDenseg(align.GetSegs().GetDenseg());
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

bool CAlnWriter::xWriteAlignDenseg(
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
        CBioseq_Handle bsh;

        CRange<TSeqPos> range;
        if (id.IsLocal()) {
            CRef<CSeq_id> pAccession;
            if (s_TryFindRange(id.GetLocal(), pAccession, range)) {
                bsh = m_pScope->GetBioseqHandle(*pAccession);
            }
        }
        else 
        {
            bsh = m_pScope->GetBioseqHandle(id);
            auto length = bsh.GetBioseqLength();
            range.SetFrom(CRange<TSeqPos>::GetPositionMin());
            range.SetToOpen(CRange<TSeqPos>::GetPositionMax());
        }

        if (!bsh) {
            continue;
        }

        auto length = bsh.GetBioseqLength();
        CSeqVector vec_plus = bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac, eNa_strand_plus);
        string seq_plus;
        if (range.IsWhole()) {
            vec_plus.GetSeqData(0, length, seq_plus); 
        } 
        else {
            vec_plus.GetSeqData(range.GetFrom(), range.GetTo(), seq_plus);
        }

        string seqdata = "";
        for (int seg=0; seg<num_segs; ++seg)
        {
            const auto start = denseg.GetStarts()[seg*num_rows + row];
            const auto len   = denseg.GetLens()[seg];
            const ENa_strand strand = (denseg.IsSetStrands()) ?
                denseg.GetStrands()[seg*num_rows + row] :
                eNa_strand_plus;

            if (start >= 0) {
                if (start >= seq_plus.size()) {
                    // Throw an exception
                }
                if (strand == eNa_strand_plus) {
                    seqdata += seq_plus.substr(start, len);
                }
                else 
                {
                    CSeqUtil::ECoding coding = 
                        (bsh.IsNucleotide()) ?
                        CSeqUtil::e_Iupacna :
                        CSeqUtil::e_Iupacaa;
                    string seq_minus;
                    CSeqManip::ReverseComplement(seq_plus, coding, start, len, seq_minus);
                
                    seqdata += seq_minus;        
                }
            }  
            else  
            {
                seqdata += string(len, '-');
            }
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


END_NCBI_SCOPE

