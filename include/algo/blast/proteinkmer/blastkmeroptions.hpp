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
 *   Options for BLAST-kmer searches
 *
 */

#ifndef BLAST_KMER_OPTIONS_HPP
#define BLAST_KMER_OPTIONS_HPP

#include <corelib/ncbistd.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include <algo/blast/core/blast_export.h>

BEGIN_NCBI_SCOPE
USING_SCOPE(blast);


/// Class of optiosn for the KMEr search.
class NCBI_XBLAST_EXPORT CBlastKmerOptions : public CObject
{
public:
	/// Constructor
	CBlastKmerOptions();

	/// Destructor 
	~CBlastKmerOptions() {}

	/// Checks that options are valid
	bool Validate() const;
	
	/// Get the threshold
	/// @return double between 0 and 1.
	double GetThresh() const {return m_Thresh;}

	/// Set the threshold. 1 means (nearly) identical matches.
	/// the closer to zero, the lower the identity of the matches
	/// is likely to be.
	// @thresh double 0 < thresh <= 1 [in]
	void SetThresh(double thresh) {m_Thresh = thresh;}

	/// Get the number of LSH hits to initiate the 
	/// calculation of the Jaccard distance.
	/// Returning zero means CBlastKmer chooses a value based upon the alphabet.
	/// @return integer 
	int GetMinHits() const {return m_MinHits;}

	/// Set the minimum number of LSH hits to initiate
	/// a calculation of the Jaccard distance.
	/// Setting to zero permits CBlastKmer to choose a value based upon the alphabet.
	// @param minhits integer 
	void SetMinHits(int minhits) {m_MinHits = minhits;}

	/// Gets the number of matches (subject sequences)
	/// to return 
	/// @return integer
	int GetNumTargetSeqs() const {return m_NumTargetSeqs;}

	/// Sets the number of matches (subject sequences)
	/// to return.  Zero indicates all.
	/// @param num integer 
	void SetNumTargetSeqs(int matches) {m_NumTargetSeqs = matches;}

private:

	/// value of the threshold 
	double m_Thresh; 

	/// Value of number of LSH hits to initiate the 
	/// calculation of the Jaccard distance.
	int m_MinHits;

	/// Number of matches to return 
	int m_NumTargetSeqs;
};


END_NCBI_SCOPE

#endif /* BLAST_KMER_OPTIONS_HPP */
