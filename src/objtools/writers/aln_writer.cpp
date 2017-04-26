
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
        CBioseq_Handle bsh = m_pScope->GetBioseqHandle(id);

        if (!bsh) {
            continue;
        }

        auto length = bsh.GetBioseqLength();
        CSeqVector vec_plus = bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac, eNa_strand_plus);
        string seq_plus;
        vec_plus.GetSeqData(0, length, seq_plus);

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
        m_Os << seqdata << "\n";
    }

    return true;
}


END_NCBI_SCOPE

