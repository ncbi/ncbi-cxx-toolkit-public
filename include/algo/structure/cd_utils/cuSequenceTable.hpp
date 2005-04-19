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
 * Author:  Charlie Liu
 *
 * File Description:
 *
 *       Bioseq table indexed by seq-id
 *
 * ===========================================================================
 */
#ifndef CU_SEQUENCE_TABLE_HPP
#define CU_SEQUENCE_TABLE_HPP

#include <algo/structure/cd_utils/cuCppNCBI.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

struct LessBySeqId : public binary_function <CRef<CSeq_id>, CRef<CSeq_id>, bool> 
{
	bool operator()(const CRef<CSeq_id>& left, const CRef<CSeq_id>& right) const
	{
		return (left->AsFastaString()) < (right->AsFastaString());
	}
};

class SequenceTable
{
public:
	SequenceTable();
	void addSequences(vector< CRef<CBioseq> > & bioseqVec);
	void addSequence(CRef< CBioseq > bioseqVec);
	bool findSequence(CRef< CSeq_id > seqId, CRef< CBioseq >& bioseq);
	bool findSequence(CRef< CSeq_id > seqId, CRef< CSeq_entry >& seqEntry);
	void dump(string filename); //for debug
private:
	typedef map<CRef< CSeq_id >, CRef< CBioseq >, LessBySeqId> SeqidToBioseqMap;
	SeqidToBioseqMap m_table;
};

END_SCOPE(cd_utils)
END_NCBI_SCOPE

#endif

/* 
 * ===========================================================================
 *
 * $Log$
 * Revision 1.1  2005/04/19 14:28:01  lanczyck
 * initial version under algo/structure
 *
 *
 * ===========================================================================
 */
