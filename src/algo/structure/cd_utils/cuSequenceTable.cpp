/* $Id$
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
 * Author:  Charlie
 *
 * File Description:
 *
 *       Bioseq table indexed by seq-id
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <algo/structure/cd_utils/cuSequenceTable.hpp>
#include <algo/structure/cd_utils/cuCdReadWriteASN.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

SequenceTable::SequenceTable()
{
}

void SequenceTable::addSequences(vector< CRef< CBioseq > >& bioseqVec)
{
	for (int i = 0; i < bioseqVec.size(); i++)
	{
		addSequence(bioseqVec[i]);
	}
}

void SequenceTable::addSequence(CRef< CBioseq > bioseq)
{
	list< CRef< CSeq_id > >& seqIds = bioseq->SetId();
	for (list< CRef< CSeq_id > >::iterator it= seqIds.begin(); it != seqIds.end(); it++)
	{
		m_table.insert(SeqidToBioseqMap::value_type(*it, bioseq));
	}
}

bool SequenceTable::findSequence(CRef< CSeq_id > seqId, CRef< CBioseq >& bioseq)
{
	SeqidToBioseqMap::iterator sit = m_table.find(seqId);
	if (sit == m_table.end())
		return false;
	else
	{
		bioseq = sit->second;
		return true;
	}
}

bool SequenceTable::findSequence(CRef< CSeq_id > seqId, CRef< CSeq_entry >& seqEntry)
{
	CRef< CBioseq > bioseq;
	if (findSequence(seqId, bioseq))
	{
		seqEntry = new CSeq_entry;
		seqEntry->SetSeq(*bioseq);
		return true;
	}
	else
		return false;
}

void SequenceTable::dump(string filename)
{
	CNcbiOfstream outStream(filename.c_str(), IOS_BASE::out | IOS_BASE::binary);
	string err;
    if (!outStream) {
        err = "Cannot open file for writing";
    }
	SeqidToBioseqMap::iterator sit = m_table.begin();
	for (; sit != m_table.end(); sit++)
	{
		if (!WriteASNToStream(outStream, *(sit->first), false,&err))
			LOG_POST("Failed to write to "<<filename<<" because of "<<err);
		if (!WriteASNToStream(outStream, *(sit->second), false,&err))
			LOG_POST("Failed to write to "<<filename<< "because of "<<err);
	}
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE


/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.1  2005/04/19 14:27:18  lanczyck
 * initial version under algo/structure
 *
 *
 * ===========================================================================
 */
























