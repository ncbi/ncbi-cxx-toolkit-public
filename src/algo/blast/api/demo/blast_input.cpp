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
#include <objtools/readers/fasta.hpp>
#include <algo/blast/api/blast_input.hpp>
#include <algo/blast/api/blast_aux.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seq/Bioseq.hpp>
#include <serial/iterator.hpp>

BEGIN_NCBI_SCOPE

CBl2Seq::TSeqLocVector
BLASTGetSeqLocFromStream(CNcbiIstream& in, CRef<CScope>& scope, 
    ENa_strand strand, int from, int to, int *counter, 
    BlastMask** lcase_mask)
{
    _ASSERT(scope);
    CBl2Seq::TSeqLocVector retval;
    CRef<CSeq_entry> seq_entry;
    vector <CConstRef<CSeq_loc> > mask_loc;
    BlastMask* last_mask, *new_mask;
    int index = 0, num_queries = 0;

    if (lcase_mask) {
        if ( !(seq_entry = ReadFasta(in, fReadFasta_AllSeqIds, counter, &mask_loc)))
            throw runtime_error("Could not retrieve seq entry");
    } else {
        if ( !(seq_entry = ReadFasta(in, fReadFasta_AllSeqIds, counter)))
            throw runtime_error("Could not retrieve seq entry");
    }

    for (CTypeConstIterator<CBioseq> itr(ConstBegin(*seq_entry)); itr; ++itr) {

        CSeq_loc* seqloc = new CSeq_loc();
        seqloc->SetWhole(*(const_cast<CSeq_id*>(&*itr->GetId().front())));
        retval.push_back(make_pair(seqloc, &*scope));
        ++num_queries;

        // Check if this seqentry has been added to the scope already
        CBioseq_Handle bh = scope->GetBioseqHandle(*seqloc);
        if (!bh)
            scope->AddTopLevelSeqEntry(*seq_entry);
    }

    if (lcase_mask) {
        *lcase_mask = NULL;
        for (index = 0; index < num_queries; ++index) {
            new_mask = CSeqLoc2BlastMask(mask_loc[index], index);
            if (!last_mask)
                *lcase_mask = last_mask = new_mask;
            else {
                last_mask->next = new_mask;
                last_mask = last_mask->next;
            }
        }
    }

    return retval;
}

END_NCBI_SCOPE
