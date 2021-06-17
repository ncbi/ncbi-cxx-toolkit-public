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

struct LessBySeqId
{
	bool operator()(const CRef<CSeq_id>& left, const CRef<CSeq_id>& right) const
	{
		if (left->IsPdb() && right->IsPdb())
		{
			const CPDB_seq_id& leftPdb = left->GetPdb();
			const CPDB_seq_id& rightPdb = right->GetPdb();
			int comp = leftPdb.GetMol().Get().compare(rightPdb.GetMol().Get());
			if (comp == 0)
			{
				int compChain = leftPdb.GetChain() - rightPdb.GetChain();
				return compChain < 0;
			}
			else
				return comp < 0;
		}
		else
			return (*left) < (*right);
	}
};

class NCBI_CDUTILS_EXPORT SequenceTable
{
public:
	SequenceTable();

	void addSequences(vector< CRef<CBioseq> > & bioseqVec, bool grouped=false);
	void addSequence(CRef< CBioseq > bioseq);
	void addSequences(CSeq_entry& seqEntry);
	void addSequences(const SequenceTable& seqTable);
	unsigned findSequencesInTheGroup(CRef< CSeq_id > seqId, vector< CRef< CBioseq > >& bioseqVec)const;
	bool findSequence(CRef< CSeq_id > seqId, CRef< CBioseq >& bioseq)const;
	bool findSequence(CRef< CSeq_id > seqId, CRef< CSeq_entry >& seqEntry)const;
	void dump(string filename); //for debug
    void reset() { m_table.clear(); }
private:
	typedef multimap<CRef< CSeq_id >, CRef< CBioseq >, LessBySeqId> SeqidToBioseqMap;
	SeqidToBioseqMap m_table;

};

END_SCOPE(cd_utils)
END_NCBI_SCOPE

#endif
