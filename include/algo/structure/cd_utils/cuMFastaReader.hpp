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
 *       Read a mFasta file into SeqEntry and SeqAlignSet,  or a CCd 
 *
 * ===========================================================================
 */
#ifndef CU_MFASTA_READER_HPP
#define CU_MFASTA_READER_HPP

#include <corelib/ncbistre.hpp>
#include <objtools/readers/fasta.hpp>
#include <algo/structure/cd_utils/cuBlock.hpp>
#include <algo/structure/cd_utils/cuCdCore.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

class MFastaReader
{
public:
	MFastaReader(CNcbiIstream& input);
	~MFastaReader();

	bool read(string& error);
	CRef<CSeq_entry> getSequences();
	int getAlignment(vector< CRef<CSeq_align> >& seqAlginVec);
	CCdCore* getCd();
	string getError();

private:
	bool m_read;
	CNcbiIstream& m_stream;
	CRef<CSeq_entry> m_masterSeqEntry; //master
	CRef<CSeq_id> m_seqId; //master seqId
	CRef<CSeq_entry> m_seqEntry;  //all seq_entries
	vector< CRef<CSeq_align> > m_seqAlignVec;
	string m_error;
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
