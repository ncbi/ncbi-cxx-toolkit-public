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
 * Author:  Jason Papadopoulos
 *
 */

/** @file blast_fasta_input.cpp
 * Convert FASTA-formatted files into blast sequence input
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <serial/iterator.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/readers/reader_exception.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>

#include <algo/blast/blastinput/blast_fasta_input.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
USING_SCOPE(objects);


CBlastFastaInputSource::CBlastFastaInputSource(objects::CObjectManager& objmgr,
                                               CNcbiIstream& infile,
                                               objects::ENa_strand strand,
                                               bool lowercase,
                                               bool believe_defline,
                                               TSeqRange range)
    : CBlastInputSource(objmgr),
      m_Config(strand, lowercase, believe_defline, range),
      m_LineReader(infile),
      m_IdGenerator()
{
}


bool
CBlastFastaInputSource::End()
{
    return m_LineReader.AtEOF();
}


CRef<CSeq_loc>
CBlastFastaInputSource::x_FastaToSeqLoc(
                         CRef<objects::CSeq_loc>& lcase_mask,
                         bool *query_is_protein)
{
    bool seq_is_protein;
    static const TSeqRange kEmptyRange(TSeqRange::GetEmpty());

    const CFastaReader::TFlags flags = m_Config.GetBelieveDeflines() ? 
                                    CFastaReader::fAllSeqIds :
                                    (CFastaReader::fNoParseID |
                                     CFastaReader::fDLOptional);

    CRef<CSeq_entry> seq_entry;
    CFastaReader fasta_reader(m_LineReader, flags);

    fasta_reader.SetIDGenerator(m_IdGenerator);

    if (m_Config.GetLowercaseMask())
        lcase_mask = fasta_reader.SaveMask();

    if ( !(seq_entry = fasta_reader.ReadOneSeq())) {
        NCBI_THROW(CBlastException, eInvalidArgument, 
                   "Could not retrieve seq entry");
    }

    m_Scope->AddTopLevelSeqEntry(*seq_entry);

    CTypeConstIterator<CBioseq> itr(ConstBegin(*seq_entry));
    CRef<CSeq_loc> retval(new CSeq_loc());

    seq_is_protein = itr->IsAa();

    // set strand
    if (m_Config.GetStrand() == eNa_strand_other ||
        m_Config.GetStrand() == eNa_strand_unknown) {
        if (seq_is_protein)
            retval->SetInt().SetStrand(eNa_strand_unknown);
        else
            retval->SetInt().SetStrand(eNa_strand_both);
    } else {
        if (seq_is_protein) {
            NCBI_THROW(CBlastException, eInvalidArgument, 
                       "Cannot assign nucleotide strand to protein sequence");
        }
        retval->SetInt().SetStrand(m_Config.GetStrand());
    }

    // sanity checks for the range
    const TSeqPos from = m_Config.GetRange().GetFrom() == kEmptyRange.GetFrom()
        ? 0 : m_Config.GetRange().GetFrom();
    const TSeqPos to = m_Config.GetRange().GetTo() == kEmptyRange.GetTo()
        ? 0 : m_Config.GetRange().GetTo();
    const TSeqPos seq_length = sequence::GetLength(*itr->GetId().front(), 
                                                   m_Scope) - 1;
    if (to > seq_length || from > seq_length || (to > 0 && to < from)) {
        NCBI_THROW(CBlastException, eInvalidArgument, 
                   "Invalid sequence range");
    }
    // set sequence range
    retval->SetInt().SetFrom(from);
    retval->SetInt().SetTo(to > 0 ? to : seq_length);

    // set ID
    retval->SetInt().SetId().Assign(*itr->GetId().front());

    // remember the type of sequence
    if (query_is_protein)
        *query_is_protein = seq_is_protein;

    return retval;
}


SSeqLoc
CBlastFastaInputSource::GetNextSSeqLoc()
{
    CRef<CSeq_loc> lcase_mask;
    CRef<CSeq_loc> seqloc(x_FastaToSeqLoc(lcase_mask));

    SSeqLoc retval(seqloc, m_Scope);
    if (m_Config.GetLowercaseMask())
        retval.mask = lcase_mask;

    return retval;
}


CRef<CBlastSearchQuery>
CBlastFastaInputSource::GetNextSequence()
{
    bool query_is_protein = false;
    CRef<CSeq_loc> lcase_mask;
    CRef<CSeq_loc> seqloc(x_FastaToSeqLoc(lcase_mask, &query_is_protein));

    TMaskedQueryRegions masks_in_query;
    if (m_Config.GetLowercaseMask()) {
        const EBlastProgramType program = query_is_protein ? 
                                eBlastTypeBlastp : eBlastTypeBlastn;
        const bool apply_mask_to_both_strands = true;
        masks_in_query = 
            PackedSeqLocToMaskedQueryRegions(
                                static_cast<CConstRef<CSeq_loc> >(lcase_mask),
                                program, apply_mask_to_both_strands);
    }
    return CRef<CBlastSearchQuery>
        (new CBlastSearchQuery(*seqloc, *m_Scope, masks_in_query));
}

END_SCOPE(blast)
END_NCBI_SCOPE
