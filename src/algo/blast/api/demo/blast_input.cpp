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
 * File Description:
 *   Reading FASTA from an input file
 *
 */
#include <serial/iterator.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/readers/fasta.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>

#include <objmgr/scope.hpp>

#include <algo/blast/api/blast_aux.hpp>
#include "blast_input.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)


TSeqLocVector
BLASTGetSeqLocFromStream(CNcbiIstream& in, CScope* scope, 
    ENa_strand strand, TSeqPos from, TSeqPos to, int *counter, 
    bool get_lcase_mask)
{
    _ASSERT(scope);
    TSeqLocVector retval;
    CRef<CSeq_entry> seq_entry;
    vector<CConstRef<CSeq_loc> > lcase_mask;

    if (get_lcase_mask) {
        if ( !(seq_entry = ReadFasta(in, fReadFasta_AllSeqIds, counter, 
                                     &lcase_mask)))
            throw runtime_error("Could not retrieve seq entry");
    } else {
        if ( !(seq_entry = ReadFasta(in, fReadFasta_AllSeqIds, counter)))
            throw runtime_error("Could not retrieve seq entry");
    }

    int index = 0;
    scope->AddTopLevelSeqEntry(*seq_entry);

    for (CTypeConstIterator<CBioseq> itr(ConstBegin(*seq_entry)); itr; ++itr) {

        CRef<CSeq_loc> seqloc(new CSeq_loc());
        if (strand == eNa_strand_plus || strand == eNa_strand_minus || 
            from > 0 || 
            (to > 0 && to < sequence::GetLength(*itr->GetId().front(), scope)-1))
        {
            seqloc->SetInt().SetFrom(from);
            seqloc->SetInt().SetTo(to);
            seqloc->SetInt().SetStrand(strand);
            seqloc->SetInt().SetId(*(const_cast<CSeq_id*>(&*itr->GetId().front())));
        } else {
            seqloc->SetWhole(*(const_cast<CSeq_id*>(&*itr->GetId().front())));
        }
        CRef<CScope> s(scope);
        SSeqLoc *slp;

        if (get_lcase_mask) {
            slp = new SSeqLoc(seqloc, s, lcase_mask[index]);
            ++index;
        } else {
            slp = new SSeqLoc (seqloc, s);
        }
        retval.push_back(*slp);
    }

    return retval;
}

END_SCOPE(blast)
END_NCBI_SCOPE
