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
*      AlignmentUtility class
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <algo/structure/struct_util/struct_util.hpp>
#include "su_private.hpp"
#include "su_sequence_set.hpp"
#include "su_alignment_set.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(struct_util)

AlignmentUtility::AlignmentUtility(const CSeq_entry& seqEntry, const SeqAnnotList& seqAnnots)
{
    CRef < CSeq_entry > seqEntryCopy(new CSeq_entry());
    seqEntryCopy->Assign(seqEntry);
    m_seqEntries.push_back(seqEntryCopy);

    SeqAnnotList::const_iterator a, ae = seqAnnots.end();
    for (a=seqAnnots.begin(); a!=ae; ++a) {
        CRef < CSeq_annot > seqAnnotCopy(new CSeq_annot());
        seqAnnotCopy->Assign(**a);
        m_seqAnnots.push_back(seqAnnotCopy);
    }

    Init();
}

AlignmentUtility::AlignmentUtility(const SeqEntryList& seqEntries, const SeqAnnotList& seqAnnots)
{
    SeqEntryList::const_iterator s, se = seqEntries.end();
    for (s=seqEntries.begin(); s!=se; s++) {
        CRef < CSeq_entry > seqEntryCopy(new CSeq_entry());
        seqEntryCopy->Assign(**s);
        m_seqEntries.push_back(seqEntryCopy);
    }

    SeqAnnotList::const_iterator a, ae = seqAnnots.end();
    for (a=seqAnnots.begin(); a!=ae; ++a) {
        CRef < CSeq_annot > seqAnnotCopy(new CSeq_annot());
        seqAnnotCopy->Assign(**a);
        m_seqAnnots.push_back(seqAnnotCopy);
    }

    Init();
}

const Sequence * DetermineMaster(const AlignmentUtility::SeqAnnotList& seqAnnots, const SequenceSet& sequenceSet)
{
    return NULL;
}

void AlignmentUtility::Init(void)
{
    m_sequenceSet = NULL;
    m_alignmentSet = NULL;
    m_okay = true;

    bool prevState = CException::EnableBackgroundReporting(false);

    try {
        m_sequenceSet = new SequenceSet(m_seqEntries);
        const Sequence *master = DetermineMaster(m_seqAnnots, *m_sequenceSet);
        m_alignmentSet = new AlignmentSet(m_seqAnnots, master, *m_sequenceSet);
    } catch (CException& e) {
        ERROR_MESSAGE("exception during AlignmentUtility initialization: " << e.GetMsg());
        m_okay = false;
    }

    CException::EnableBackgroundReporting(prevState);
}

AlignmentUtility::~AlignmentUtility()
{
    if (m_sequenceSet)
        delete m_sequenceSet;
    if (m_alignmentSet)
        delete m_alignmentSet;
}

END_SCOPE(struct_util)

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2004/05/24 23:04:05  thiessen
* initial checkin
*
*/

