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
*       alignment utility algorithms
*
* ===========================================================================
*/

#ifndef STRUCT_UTIL__HPP
#define STRUCT_UTIL__HPP

#include <corelib/ncbistd.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seq_annot.hpp>


BEGIN_SCOPE(struct_util)

class SequenceSet;
class AlignmentSet;
class BlockMultipleAlignment;

class AlignmentUtility
{
public:
    typedef std::list < ncbi::CRef < ncbi::objects::CSeq_entry > > SeqEntryList;
    typedef std::list< ncbi::CRef< ncbi::objects::CSeq_annot > > SeqAnnotList;

    AlignmentUtility(const ncbi::objects::CSeq_entry& seqEntry, const SeqAnnotList& seqAnnots);
    AlignmentUtility(const SeqEntryList& seqEntries, const SeqAnnotList& seqAnnots);
    ~AlignmentUtility();

    bool Okay(void) const { return m_okay; }

    const SeqEntryList& GetSeqEntries(void) const { return m_seqEntries; }
    const SeqAnnotList& GetSeqAnnots(void) const { return m_seqAnnots; }

    bool DoIBM(void);

private:
    SeqEntryList m_seqEntries;
    SequenceSet *m_sequenceSet;
    SeqAnnotList m_seqAnnots;
    AlignmentSet *m_alignmentSet;

    void Init(void);
    bool m_okay;

    BlockMultipleAlignment *m_currentMultiple;
};

END_SCOPE(struct_util)

#endif // STRUCT_UTIL__HPP

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2004/05/25 15:50:46  thiessen
* add BlockMultipleAlignment, IBM algorithm
*
* Revision 1.1  2004/05/24 23:05:10  thiessen
* initial checkin
*
*/

