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
 * Author: Tom Madden
 *
 * File Description:
 *   Build index for BLAST-kmer searches
 *
 */

#ifndef BLAST_KMER_INDEX_HPP
#define BLAST_KMER_INDEX_HPP


#include <corelib/ncbistd.hpp>
#include <corelib/ncbifile.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include <algo/blast/api/query_data.hpp>
#include <algo/blast/api/sseqloc.hpp>
#include <objtools/blast/seqdb_reader/seqdb.hpp>

#include "blastkmerutils.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
USING_SCOPE(objects);


class NCBI_XBLAST_EXPORT CBlastKmerBuildIndex : public CObject
{
public:
	/// Constructor

	CBlastKmerBuildIndex(CRef<CSeqDB> seqdb,
		int kmerSize=5,
		int numHashFct=32, 
		int samples=0,
		int compress=2,
		int alphabet=0,
		int version=0,
		int chunkSize=150);

	/// Destructor 
	~CBlastKmerBuildIndex() {}

	///  Build the index
	void Build(int numThreads=1);

private:
	/// Writes out the data file
	void x_WriteDataFile(vector < vector < vector < uint32_t > > > & seq_hash, int num_seqs, CNcbiOfstream& data_file);
	/// BUild index for an individual BLAST volume.
	void x_BuildIndex(string& name, int start=0, int number=0);

	int m_NumHashFct; /// Number of hash functions.

	int m_NumBands; /// Number of LSH bands.

	int m_RowsPerBand; /// Number of rows per band

	int m_KmerSize;	/// Residues in kmer

	CRef<CSeqDB> m_SeqDB;  /// BLAST database.

	bool m_DoSeg;  /// Should Seg be run on sequences.

	int m_Samples;  /// Number of samples (Buhler only)

	int m_Compress; /// Compress the arrays for Jaccard matches.

	int m_Alphabet; /// 0 for 15 letters, 1 for 10 letters.

	int m_Version; /// version of index file 

	int m_ChunkSize; /// # residues in a chunk.
};


END_SCOPE(blast)
END_NCBI_SCOPE

#endif /* BLAST_KMER_KMER_HPP */
