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
* Authors:  Chris Lanczycki
*
* File Description:
*          Some utility functions on BlockMultipleAlignment objects.
*
* ===========================================================================
*/

#ifndef AR_BMA_UTILS__HPP
#define AR_BMA_UTILS__HPP

#include <vector>
#include <corelib/ncbistd.hpp>

#include <algo/structure/struct_util/su_block_multiple_alignment.hpp>
#include <algo/structure/struct_util/struct_util.hpp>

#include <algo/structure/bma_refine/diagnosticDefs.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(struct_util);

typedef struct_util::BlockMultipleAlignment BMA;

BEGIN_SCOPE(align_refine)


enum AlignmentCharacterType {
    eAlignedResidues = 0, //  residues in an aligned block (such blocks never contain gaps)
    eUnalignedCharacters, //  residues and gaps in all columns of an unaligned block
    eUnalignedInPssm,     //  columns in an unaligned block but in columns covered by PSSM
    eInPssm,              //  residues in columns covered by the PSSM (eAlignedResidues + eUnalignedInPssm)
    eNotInPssm,           //  residues/gaps in columns *NOT* covered by the PSSM (eUnalignedCharacters - eUnalignedResidues)
    eAllCharacters
};

class NCBI_BMAREFINE_EXPORT BMAUtils {

public:

    //  These need to work for both aligned columns and non-aligned columns in the PSSM.
    //  Returns true if there is a valid residue at the position; if a gap '-', seqIndex is eUndefined
    //  and the function returns false.
    static bool GetCharacterAndIndexForColumn(const BMA& bma, unsigned alignmentIndex, unsigned row, char* residue, unsigned int* seqIndex);
    static bool GetCharacterForColumn(const BMA& bma, unsigned alignmentIndex, unsigned row, char* residue);

    //  return kMin_Int if problem obtaining the PSSM (ignores the -32768 entries)
    static int GetSmallestValueInPssm(const BMA& bma);

    //   Where can't get a row's character in 'alignmentIndex' column, return '-' 
    //   in its place.  Needs to work for both aligned columns and non-aligned 
    //   columns in the PSSM.
    static void GetResiduesForColumn(const BMA& bma, unsigned alignmentIndex, vector< char >& residues);

    //   For the specified column, return the PSSM score for each row in the alignment.
    //   Where can't get a row's character in 'alignmentIndex' column, return score for '-' 
    //   in its place.  Also return the residues in the specified column if requested.
    //   Needs to work for both aligned columns and non-aligned columns in the PSSM.
    static void GetPSSMScoresForColumn(const BMA& bma, unsigned alignmentIndex, vector< int >& scores, vector< char >* residues = NULL);

    //  If 'block' is given, assume this is the block containing position column;
    //  return false if column is not of 'type' or is not in the specified block.
    //  Return whether or not column is in the PSSM in the 'isInPssm' argument.
    static bool IsColumnOfType(const BMA& bma, unsigned int column, AlignmentCharacterType ctype, bool& isInPssm, const Block* block = NULL);

    //  Return true if column is in PSSM (i.e., has a valid residue on each row).
    static bool IsColumnInPSSM(const BMA& bma, unsigned int column);

    //  Return true if the specified block could contain residues of type 'ctype'.
    static bool IsBlockConsistentWithType(const Block* block, AlignmentCharacterType ctype);

    static void MapAlignmentIndexToSeqIndex(const BMA& bma, unsigned int row, map<unsigned int, unsigned int>& aimap);
    //  Return the Pssm column for the specified residue, or eUndefined if not in the pssm.
//    static unsigned int GetPssmColumnForResidue(const BMA& bma, unsigned int row, unsigned int residueId);


    //  Print the PSSM (row major by default).  All indices in output are ONE-based.
    //  Output is to the diagnostic stream defined at invokation, unless stringOutput is 
    //  non-NULL in which case it's directed to the string.
    static void PrintPSSM(const BMA& bma, bool rowMajor = true, string* stringOutput = NULL);

    //  If dumpRawMatrix = true, print the raw matrix in addition to other output (can use to
    //  ensure indices/values in proper correspondence and to help debugging).
    //  If viewColumn = true, for each aligned residue print the score of every possible residue
    //  at that position.
    //  All indices in output are ONE-based.
    //  Output is in row-major format to the diagnostic stream defined at invokation.
    static void PrintPSSMByRow(const BMA& bma, bool dumpRawMatrix, bool viewColumn = false, AlignmentCharacterType ctype = eAlignedResidues);
    static void PrintPSSMForRow(const BMA& bma, unsigned int row, bool viewColumn = false, AlignmentCharacterType ctype = eAlignedResidues);

    //  Same as above except output is in column-major format.  Column # = BMA alignment index
    static void PrintPSSMByColumn(const BMA& bma, bool dumpRawMatrix, bool viewColumn = false, AlignmentCharacterType ctype = eAlignedResidues);
    static void PrintPSSMForColumn(const BMA& bma, unsigned int column, bool viewColumn = false, AlignmentCharacterType ctype = eAlignedResidues);

    //  For non-aligned columns, print in column-major format the characters at 
    //  each row.  If 'allowGapsInColumn' is false, filter for only those columns for which
    //  no gaps exist.  Always prints the full matrix (to allow doublechecking...).
    //  'viewColumn' has same meaning as above.
    static void PrintUnalignedBlocksByColumn(const BMA& bma, bool allowGapsInColumn = false, bool viewColumn = false);

};

END_SCOPE(align_refine)

#endif // AR_BMA_UTILS__HPP
