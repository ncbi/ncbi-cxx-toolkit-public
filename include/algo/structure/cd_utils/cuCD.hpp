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
 * Author:  Chris Lanczycki
 *
 * File Description:
 *
 *       High-level algorithmic operations on one or more CCdCore objects.
 *       (methods that only traverse the cdd ASN.1 data structure are in 
 *        placed in the CCdCore class itself)
 *
 * ===========================================================================
 */

#ifndef CU_CD_HPP
#define CU_CD_HPP

#include <corelib/ncbistd.hpp>
#include <objects/ncbimime/Ncbi_mime_asn1.hpp>
#include <algo/structure/cd_utils/cuCdCore.hpp>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(cd_utils) // namespace ncbi::objects::

//class CCdCore;
//class CNcbi_mime_asn1;

const int ALIGN_ANNOTS_VALID_FAILURE = 1;

NCBI_CDUTILS_EXPORT 
int  GetReMasterFailureCode(const CCdCore* cd);

NCBI_CDUTILS_EXPORT 
bool Reorder(CCdCore* pCD, const vector<int> positions);

//   Structure alignments in the features are required to be listed the order in
//   which the PDB identifiers appear in the alignment.
//   'positions' contains the new order of all NON-MASTER rows, as in Reorder
NCBI_CDUTILS_EXPORT 
bool ReorderStructureAlignments(CCdCore* pCD, const vector<int>& positions);

//  Assumes cd has been remastered and the alignannot field still
    //  is indexed to the oldMasterRow's sequence.  Does nothing if oldMasterRow = 0,
    //  or out of range, and returns false.  Returns false if seqId of oldMasterRow
    //  does not match the seqId of every annotation in the alignannot, or if any
    //  mapping of from/to has an error.
NCBI_CDUTILS_EXPORT 
bool remasterAlignannot(CCdCore& cd, unsigned int oldMasterRow = 1);

//   Resets a variety of fields that need to be wiped out on remastering, or
//   when removing a consensus.
NCBI_CDUTILS_EXPORT 
void ResetFields(CCdCore* pCD);

//   If ncbiMime contains a CD, return it.  Otherwise return NULL.
NCBI_CDUTILS_EXPORT 
CCdCore* ExtractCDFromMime(CNcbi_mime_asn1* ncbiMime);

//   Remove consensus sequence from alignment and sequence list.
//   If the master was a consensus sequence, remaster to the 2nd alignment row first.
//int  PurgeConsensusSequences(CCdCore* pCD, bool resetFields = true);

//  If copyNonASNMembers = true, all statistics, sequence strings and aligned residues 
//  data structures are copied.  The CD's read status is *always* copied.
NCBI_CDUTILS_EXPORT 
CCdCore* CopyCD(const CCdCore* cd);
/* replaced CdAlignmentAdapter::CreateCD
CCdCore* CreateChildCD(const CCdCore* origCD, const vector<int> selectedRows, string newAccession, string shortName);
*/

//   Set creation date of CD w/ the current time.  Removes existing creation date.
NCBI_CDUTILS_EXPORT 
bool SetCreationDate(CCdCore* cd);
NCBI_CDUTILS_EXPORT 
bool SetUpdateDate(CCdCore* cd);
//  When only the first pointer is passed, checks for overlaps among that CD's rows.
//  Otherwise, it reports on overlaps between two distinct CDs.
NCBI_CDUTILS_EXPORT 
int NumberOfOverlappedRows(CCdCore* cd1, CCdCore* cd2 = NULL);
NCBI_CDUTILS_EXPORT 
int GetOverlappedRows(CCdCore* cd1, CCdCore* cd2, vector<int>& rowsOfCD1, vector<int>& rowsOfCD2);

//  For a specified row in cd1, find all potential matches in cd2.
//  If cd1 == cd2, returns row1 plus any other matches (should be no such overlaps in a valid CD)
//  If cd1AsChild == true,  mapping assumes cd2 is parent of cd1.
//  If cd1AsChild == false, mapping assumes cd1 is parent of cd2.
//  In 'overlapMode', returns the row index of cd2 for *any* overlap with row1, not just
//  those overlaps which obey the specified parent/child relationship between.
//  Return number of rows found.
NCBI_CDUTILS_EXPORT 
int GetMappedRowIds(CCdCore* cd1, int row1, CCdCore* cd2, vector<int>& rows2, bool cd1AsChild, bool overlapMode = false);

//  return a vector containing (in order) the full sequence in NCBIeaa
//  format for every Seq_entry in the sequence list
NCBI_CDUTILS_EXPORT 
void SetConvertedSequencesForCD(CCdCore* cd, vector<string>& convertedSequences, bool forceRecompute = false);

//  for each row, return a char* containing all residues aligned in the cd
NCBI_CDUTILS_EXPORT 
void SetAlignedResiduesForCD(CCdCore* cd, char** & ppAlignedResidues, bool forceRecompute = false);

//  Return strings containing the residues in each alignment column, in row order; pending rows are ignored.
//  The index into the map is the zero-based position on the sequence corresponding to 'referenceRow'.
//  If 'referenceRow' is not provided, or is out of range, the index will simply be the column number, 
//  starting from zero.
//  Assumes that the CCdCore object has the same block model on each row.  If that is not true,
//  or other problems arise, 'columns' will be returned as an empty map.
NCBI_CDUTILS_EXPORT 
void GetAlignmentColumnsForCD(CCdCore* cd, map<unsigned int, string>& columns, unsigned int referenceRow = kMax_UInt);

//  Returns '<cd->GetAccession> (<cd->GetName>)' w/o the angle brackets; 
//  format used by the validator. 
NCBI_CDUTILS_EXPORT 
string GetVerboseNameStr(const CCdCore* cd);

// for getting the bioseq and seq-loc for rows of a CD.
NCBI_CDUTILS_EXPORT 
CRef< CBioseq > GetMasterBioseqWithFootprintOld(CCdCore* cd);  // deprecated
NCBI_CDUTILS_EXPORT 
CRef< CBioseq > GetMasterBioseqWithFootprint(CCdCore* cd);
NCBI_CDUTILS_EXPORT 
CRef< CBioseq > GetBioseqWithFootprintForNthRow(CCdCore* cd, int N, string& errstr);
NCBI_CDUTILS_EXPORT 
bool GetBioseqWithFootprintForNRows(CCdCore* cd, int N, vector< CRef< CBioseq > >& bioseqs, string& errstr);

//  Sequences reporting no taxonomy info are ignored when finding the common tax node.
//  Specify how to handle the case when *no* sequences in 'cd' have taxonomy info (e.g., all local sequences):  
//  useRootWhenNoTaxInfo = true means to return the root tax node
//  useRootWhenNoTaxInfo = false means to return an empty CRef (i.e., report nothing)
NCBI_CDUTILS_EXPORT 
CRef< COrg_ref > GetCommonTax(CCdCore* cd, bool useRootWhenNoTaxInfo = true);
NCBI_CDUTILS_EXPORT 
bool obeysParentTypeConstraints(const CCdCore* pCD);


//   Remove consensus sequence from alignment and sequence list.
//   If the master was a consensus sequence, remaster to the 2nd alignment row first.
NCBI_CDUTILS_EXPORT 
int  PurgeConsensusSequences(CCdCore* pCD, bool resetFields = true);

NCBI_CDUTILS_EXPORT
bool RemasterWithStructure(CCdCore* cd, string* msg = NULL);

NCBI_CDUTILS_EXPORT 
bool ReMasterCdWithoutUnifiedBlocks(CCdCore* cd, int Row, bool resetFields = true);

//   Return +ve (equal to # of block in IBM CD) if the block structure was modified successfully. 
//   Return 0 if no action taken.
//   Return -ve if run IBM and it found no intersection or otherwise failed.
//  'rowFraction' specifies the minimum fraction of rows in the alignment that
//  must have an aligned residue at a position for that position to be part of
//  the intersected alignment.  If 'rowFraction' <= 0 or > 1.0, rowFraction is
//  reset to 1.0 (i.e., only columns with an aligned residue on all rows appear
//  in the interested alignment).
//   NOTE:  Only modifying the alignment data; no other coordinate-dependent data in
//          'ccd' are altered due to modification of alignment blocks caused by IBM.
NCBI_CDUTILS_EXPORT 
int IntersectByMaster(CCdCore* ccd, double rowFraction = 1.0);

//return the number of PDBs fixed
NCBI_CDUTILS_EXPORT
int FixPDBDefline(CCdCore* cd);

END_SCOPE(cd_utils)
END_NCBI_SCOPE


#endif // ALGCD_HPP
