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
 * Authors:  Christiam Camacho
 *
 */

/** @file blast_input.cpp
 * Reading FASTA from an input file
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

#include <objmgr/scope.hpp>

#include "blast_input.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

TSeqLocVector
BLASTGetSeqLocFromStream(CNcbiIstream& in, CObjectManager& objmgr, 
                         ENa_strand strand, TSeqPos from, TSeqPos to, 
                         int *counter, bool get_lcase_mask)
{
    TSeqLocVector retval;
    CRef<CSeq_entry> seq_entry;
    vector<CConstRef<CSeq_loc> > lcase_mask;
    CRef<CScope> scope(new CScope(objmgr));
    scope->AddDefaults();

    if (get_lcase_mask) {
        if ( !(seq_entry = ReadFasta(in, fReadFasta_AllSeqIds, counter, 
                                     &lcase_mask)))
            NCBI_THROW(CObjReaderException, eInvalid, 
                       "Could not retrieve seq entry");
    } else {
        if ( !(seq_entry = ReadFasta(in, fReadFasta_AllSeqIds, counter)))
            NCBI_THROW(CObjReaderException, eInvalid, 
                       "Could not retrieve seq entry");
    }

    int index = 0;
    scope->AddTopLevelSeqEntry(*seq_entry);

    from = MAX(from - 1, 0);
    to = MAX(to - 1, 0);

    for (CTypeConstIterator<CBioseq> itr(ConstBegin(*seq_entry)); itr; ++itr) {

        CRef<CSeq_loc> seqloc(new CSeq_loc());
        TSeqPos seq_length = sequence::GetLength(*itr->GetId().front(), 
                                                scope) - 1;

        if (to > 0 && to < seq_length)
            seqloc->SetInt().SetTo(to);
        else
            seqloc->SetInt().SetTo(seq_length);

        if (from > 0 && from < seq_length && from < to)
            seqloc->SetInt().SetFrom(from);
        else
            seqloc->SetInt().SetFrom(0);

        seqloc->SetInt().SetStrand(strand);
        seqloc->SetInt().SetId().Assign(*itr->GetId().front());

        //CRef<CScope> s(scope);
        SSeqLoc sl(seqloc, scope);

        if (get_lcase_mask) {
            sl.mask.Reset(lcase_mask[index++]);
        }
        retval.push_back(sl);
    }

    return retval;
}

END_SCOPE(blast)
END_NCBI_SCOPE
