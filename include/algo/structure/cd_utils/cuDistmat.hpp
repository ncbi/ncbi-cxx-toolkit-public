#ifndef CU_DISTMAT__HPP
#define CU_DISTMAT__HPP

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
* Author:  Chris Lanczycki
*
* File Description:  cdt_distmat.hpp
*
*      Representation of a distance matrix for phylogenetic calculations.
*      Note that the base class AMatrix is explicitly a matrix of doubles;
*      templatize if becomes necessary.
*
*/

#include <algo/structure/cd_utils/cuMatrix.hpp>
#include <algo/structure/cd_utils/cuAlignmentCollection.hpp>
#include <algo/structure/cd_utils/cuScoringMatrix.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)
// pProgressFunction is called by ComputeMatrix to update the progress bar
typedef void (* pProgressFunction) (int Num, int Total);

const int NUMBER_OF_DISTANCE_METHODS = 6;
const string DISTANCE_METHOD_NAMES[] = {
		"",
		"Percent Identity (Aligned Residues)",
		"Kimura-Corrected % Identity (Aligned Residues)",
		"Score of Aligned Residues",
		"Score of Optimally-Extended Blocks",
		"Blast Score (Footprint)",
		"Blast Score (Full Sequence)"
};
enum EDistMethod {
	eNoDistMethod     = 0,
	ePercentIdentity,
	ePercIdWithKimura,
	eScoreAligned,
	eScoreAlignedOptimal,
	eScoreBlastFoot,
	eScoreBlastFull
};
const EDistMethod GLOBAL_DEFAULT_DIST_METHOD = ePercentIdentity;
	

//  Subclass this class inorder to provide a specific distance matrix
//  calculation algorithm.  

//  Since this is a distance matrix, diagonal entries should be zero.

class DistanceMatrix : public AMatrix {

	static const double TINY_DISTANCE;
	static const double HUGE_DISTANCE;

public:
    static const bool USE_ALIGNED_DEFAULT;
    static const int  NO_EXTENSION;
	static string GetDistMethodName(EDistMethod method);
	static bool DistMethodUsesScoringMatrix(EDistMethod method);
	static bool ExtensionsAllowed(EDistMethod method);
	static bool RequireAlignedBlocks(EDistMethod method);

    typedef double TMatType;   //  type-def for values stored in matrix

    DistanceMatrix() : AMatrix() { 
        initialize();
    }

//	DistanceMatrix(TCdd& cddref) : AMatrix() {
	/*
	DistanceMatrix(const CCd* cddref) : AMatrix() {
        initialize();
        SetCDD(cddref);
	}*/

    DistanceMatrix(const int nrows) : AMatrix(nrows, nrows) {
        initialize();
    }
  
    bool UseAll() const     { return !m_useAligned;} 
    bool UseAligned() const { return m_useAligned;} 
    void SetUseAligned(bool useAligned) { m_useAligned = useAligned;} 

    double** GetMatrix() { return m_Array;}

	//  GetMax/Min ignore the diagonal entries.
	double   GetMaxEntry();
	double   GetMinEntry();

    // force the matrix to be symmetric by averaging d_ij and d_ji
    void EnforceSymmetry();  

	// zero distances in off-diagonals can confuse tree algorithms later;
	// replace with a tiny number
	void ReplaceZeroWithTinyValue(const double tiny = TINY_DISTANCE);
	void SetData(AlignmentCollection* aligns);

    //  distance computation method methods
	string GetDistMethodName() {return DISTANCE_METHOD_NAMES[m_dMethod];}
	EDistMethod GetDistMethod() {return m_dMethod;}

    //  scoring matrix methods
    bool ResetMatrixType(EScoreMatrixType newType);    //  discards old matrix even if of the same type
    string GetMatrixName();
    EScoreMatrixType GetMatrixType();
    
    //  methods to manage extensions
    void SetNTermExt(int ext);
    void SetCTermExt(int ext);
    int  GetNTermExt();
    int  GetCTermExt();

    // pass ComputeMatrix a pointer-to-function, so ComputeMatrix can call fn with meter updates
    virtual bool ComputeMatrix(pProgressFunction pFunc) = 0;

    virtual ~DistanceMatrix();
    static void readMat(ifstream& ifs, DistanceMatrix& dm, bool triangular);
    static void writeMat(ofstream& ofs, const DistanceMatrix& dm, bool triangular);
    void printMat(bool triangular=true);

protected:
    static const int  OUTPUT_PRECISION;
	static const int  INITIAL_SCORE_BOUND;
    typedef char*  CharPtr;

    std::vector< std::string >  m_ConvertedSequences;

    ScoreMatrix* m_scoreMatrix;
	EDistMethod m_dMethod;
    bool  m_useAligned;
	AlignmentCollection* m_aligns;

    int m_nTermExt;
    int m_cTermExt;

    virtual void initialize();
    void writeMat(ostream& os, bool triangular=true) const;
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
