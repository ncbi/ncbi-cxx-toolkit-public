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
#include <objtools/readers/fasta.hpp>
#include <objtools/readers/reader_exception.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>

#include <algo/blast/blastinput/blast_fasta_input.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
USING_SCOPE(objects);


CBlastFastaInputSource::CBlastFastaInputSource(CObjectManager& objmgr,
                                               CNcbiIstream& infile,
                                               ENa_strand strand,
                                               bool lowercase,
                                               bool believe_defline,
                                               TSeqPos from,
                                               TSeqPos to)
    : CBlastInputSource(objmgr),
      m_Config(strand, lowercase, believe_defline, from, to),
      m_InputFile(infile),
      m_Counter(0)
{
}


bool
CBlastFastaInputSource::End()
{
    return m_InputFile.eof();
}


CRef<CSeq_loc>
CBlastFastaInputSource::x_FastaToSeqLoc(
                         vector<CConstRef<CSeq_loc> > *lcase_mask)
{
    CRef<CSeq_entry> seq_entry;
    
    int flags;
    TSeqPos from = m_Config.GetFrom();
    TSeqPos to = m_Config.GetTo();

    flags = fReadFasta_OneSeq;
    if (m_Config.GetBelieveDeflines() == true)
        flags |= fReadFasta_AllSeqIds;
    else
        flags |= fReadFasta_NoParseID;

    if ( !(seq_entry = ReadFasta(m_InputFile, flags, 
                                     &m_Counter, lcase_mask))) {
        NCBI_THROW(CObjReaderException, eInvalid, 
                       "Could not retrieve seq entry");
    }

    m_Scope->AddTopLevelSeqEntry(*seq_entry);

    CTypeConstIterator<CBioseq> itr(ConstBegin(*seq_entry));
    CRef<CSeq_loc> seqloc(new CSeq_loc());
    TSeqPos seq_length = sequence::GetLength(*itr->GetId().front(), 
                                             m_Scope) - 1;

    if (to > seq_length || from > seq_length ||
        (to > 0 && to < from)) {
        NCBI_THROW(CObjReaderException, eInvalid, 
                  "Invalid sequence range");
    }

    // set sequence range
    if (to > 0)
        seqloc->SetInt().SetTo(to);
    else
        seqloc->SetInt().SetTo(seq_length);
    seqloc->SetInt().SetFrom(from);

    // set strand
    if (m_Config.GetStrand() == eNa_strand_other) {
        if (itr->IsAa())
            seqloc->SetInt().SetStrand(eNa_strand_unknown);
        else
            seqloc->SetInt().SetStrand(eNa_strand_both);
    }
    else if (m_Config.GetStrand() == eNa_strand_unknown) {
        if (itr->IsNa()) {
            NCBI_THROW(CObjReaderException, eInvalid, 
                       "Cannot assign unknown strand to nucleotide sequence");
        }
        seqloc->SetInt().SetStrand(eNa_strand_other);
    }
    else {
        if (itr->IsAa()) {
            NCBI_THROW(CObjReaderException, eInvalid, 
                       "Cannot assign nucleotide strand to protein sequence");
        }
        seqloc->SetInt().SetStrand(m_Config.GetStrand());
    }

    seqloc->SetInt().SetId().Assign(*itr->GetId().front());
    return seqloc;
}


SSeqLoc
CBlastFastaInputSource::GetNextSSeqLoc()
{
    bool get_lcase_mask = m_Config.GetLowercaseMask();
    vector<CConstRef<CSeq_loc> > lcase_mask;

    vector<CConstRef<CSeq_loc> > *mask_ptr = 0;
    if (get_lcase_mask)
        mask_ptr = &lcase_mask;
    
    CRef<CSeq_loc> seqloc(x_FastaToSeqLoc(mask_ptr));

    SSeqLoc retval(seqloc, m_Scope);
    if (get_lcase_mask)
        retval.mask.Reset(lcase_mask[0]);

    return retval;
}


CRef<CBlastSearchQuery>
CBlastFastaInputSource::GetNextSequence()
{
    bool get_lcase_mask = m_Config.GetLowercaseMask();
    vector<CConstRef<CSeq_loc> > lcase_mask;

    vector<CConstRef<CSeq_loc> > *mask_ptr = 0;
    if (get_lcase_mask)
        mask_ptr = &lcase_mask;
    
    CRef<CSeq_loc> seqloc(x_FastaToSeqLoc(mask_ptr));

    CRef<CBlastSearchQuery> retval(new CBlastSearchQuery(
                               *seqloc, *m_Scope, TMaskedQueryRegions()));
    if (get_lcase_mask) {
        const CSeq_loc& maskloc = *lcase_mask[0];
        if (maskloc.IsPacked_int()) {
            const CPacked_seqint::Tdata& intervals = 
                                maskloc.GetPacked_int().Get();
            TMaskedQueryRegions new_regions;

            ITERATE(CPacked_seqint::Tdata, itr, intervals) {
                const CSeq_interval *intptr = &**itr;
                CRef<CSeqLocInfo> sli(new CSeqLocInfo(
                               const_cast<CSeq_interval *>(intptr),
                               CSeqLocInfo::eFrameNotSet));
                retval->AddMask(sli);
            }
        } else if (!maskloc.IsNull()) {
            NCBI_THROW(CBlastException, eInvalidArgument, 
                        "unexpected mask location type from ReadFasta");
        }
    }

    return retval;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/*---------------------------------------------------------------------
 * $Log$
 * Revision 1.5  2006/10/30 20:27:27  papadopo
 * correct sanity checks for sequence ranges
 *
 * Revision 1.4  2006/09/26 21:44:12  papadopo
 * add to blast scope; add CVS log
 *
 *-------------------------------------------------------------------*/
