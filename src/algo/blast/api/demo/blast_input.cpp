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
#include <algo/blast/blast_input.hpp>
#include <algo/blast/blast_aux.hpp>

USING_NCBI_SCOPE;
BEGIN_NCBI_SCOPE

vector< CConstRef<CSeq_loc> >
BLASTGetSeqLocFromStream(CNcbiIstream& in, CRef<CScope>& scope, 
    ENa_strand strand, int from, int to, int *counter, 
    BlastMask** lcase_mask)
{
    _ASSERT(scope);
    vector< CConstRef<CSeq_loc> > retval;
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
        CSeq_loc* sl = new CSeq_loc();
        sl->SetWhole(*(const_cast<CSeq_id*>(&*itr->GetId().front())));
        CConstRef<CSeq_loc> seqlocref(sl);
        retval.push_back(seqlocref);
        ++num_queries;
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
