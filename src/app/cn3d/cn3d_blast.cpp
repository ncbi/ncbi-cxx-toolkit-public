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
* Authors:  Paul Thiessen
*
* File Description:
*      module for aligning with BLAST and related algorithms
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2001/09/18 03:10:45  thiessen
* add preliminary sequence import pipeline
*
* ===========================================================================
*/

#include <wx/string.h> // kludge for now to fix weird namespace conflict
#include <corelib/ncbistd.hpp>

#if defined(__WXMSW__)
#include <wx/msw/winundef.h>
#endif
#include <wx/wx.h>

#include "cn3d/cn3d_blast.hpp"
#include "cn3d/structure_set.hpp"
#include "cn3d/sequence_set.hpp"
#include "cn3d/block_multiple_alignment.hpp"
#include "cn3d/cn3d_tools.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

void CreateNewPairwiseAlignments(const Sequence *master,
    const SequenceList& newSequences, AlignmentList *newAlignments)
{
    SequenceList::const_iterator s, se = newSequences.end();
    for (s=newSequences.begin(); s!=se; s++) {

        // create new alignment
        BlockMultipleAlignment::SequenceList *seqs = new BlockMultipleAlignment::SequenceList(2);
        (*seqs)[0] = master;
        (*seqs)[1] = *s;
        BlockMultipleAlignment *newAlignment =
            new BlockMultipleAlignment(seqs, master->parentSet->alignmentManager);

        // fill out with blocks from BLAST alignment

        // finalize and and add new alignment to list
        if (newAlignment->AddUnalignedBlocks() && newAlignment->UpdateBlockMapAndColors()) {
            newAlignments->push_back(newAlignment);
        } else {
            ERR_POST(Error << "CreateNewPairwiseAlignments() - error finalizing alignment");
            delete newAlignment;
        }
    }
}

END_SCOPE(Cn3D)

