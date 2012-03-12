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
 * Author:  Adapted from CDTree-1 code by Chris Lanczycki
 *
 * File Description:
 *
 *       High-level algorithmic operations on one or more CCd objects.
 *       (methods that only traverse the cdd ASN.1 data structure are in
 *        placed in the CCd class itself)
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <objects/cdd/Align_annot.hpp>
#include <objects/cdd/Align_annot_set.hpp>
#include <objects/cdd/Cdd.hpp>
#include <objects/cdd/Cdd_id.hpp>
#include <objects/cdd/Cdd_descr_set.hpp>
#include <objects/ncbimime/Ncbi_mime_asn1.hpp>
#include <objects/ncbimime/Biostruc_seqs_aligns_cdd.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seqalign/Dense_diag.hpp>
#include <objects/seqalign/Seq_align.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include <objects/seqloc/PDB_mol_id.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/taxon1/taxon1.hpp>
#include <objects/taxon1/Taxon2_data.hpp>
#include "corelib/ncbitime.hpp"
#include <objects/general/Date.hpp>
#include <algo/structure/cd_utils/cuCdCore.hpp>
#include <algo/structure/cd_utils/cuCdReadWriteASN.hpp>
#include <algo/structure/cd_utils/cuAlign.hpp>
#include <algo/structure/cd_utils/cuSequence.hpp>
#include <algo/structure/cd_utils/cuCD.hpp>
#include <algo/structure/cd_utils/cuBlockIntersector.hpp>
#include <algo/structure/cd_utils/cuAlignmentCollection.hpp>
#include <math.h>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)


int GetReMasterFailureCode(const CCdCore* cd) {
//-------------------------------------------------------------------------
// return an error code indicating which, if any, checks went wrong.
// return value of 0 indicates all checks were ok.
//-------------------------------------------------------------------------
  int RetVal = 0;

  if (!cd->AlignAnnotsValid()) {
    RetVal = RetVal | ALIGN_ANNOTS_VALID_FAILURE;
  }
  return(RetVal);
}




void ResetFields(CCdCore* cd) {
    if (cd) {
        cd->ResetProfile_range();
        cd->ResetTrunc_master();
        cd->ResetPosfreq();
        cd->ResetScoremat();
        cd->ResetDistance();
        cd->ResetFeatures();
        cd->EraseUID();
		list< CRef< CCdd_descr > >& cdDescrList = cd->SetDescription().Set();
		list< CRef< CCdd_descr > >::iterator lit = cdDescrList.begin();
		while (lit != cdDescrList.end())
		{
			if ((*lit)->IsRepeats())
				lit = cdDescrList.erase(lit);
			else
				lit++;
		}
    }
}



bool Reorder(CCdCore* pCD, const vector<int> positions)
{
	if (!pCD->IsSeqAligns() || positions.size() == 0)
		return false;

//	list< CRef< CSeq_align > >&	alignments = (*(pCD->SetSeqannot().begin()))->SetData().SetAlign();
	list< CRef< CSeq_align > >&	alignments = pCD->GetSeqAligns();
	if (alignments.size() != positions.size())
		return false;

    vector<CRef< CSeq_align > > temp(alignments.size());
	list<CRef< CSeq_align > >::iterator lit = alignments.begin();
	int row = 0;
	//copy aligment rows to the vector in order
	for(;lit != alignments.end(); lit++)
	{
		//sanity check
		if (positions[row] >= (int) temp.size())
			return false;
		temp[positions[row]] = *lit;
		row++;
	}

    //  Before moving the alignments around, reorder the structural alignments to reflect
    //  the new ordering of the rows.  If there was a problem, the structural alignments
    //  are left in their original order.
    ReorderStructureAlignments(pCD, positions);

    alignments.clear();
	for (unsigned int i = 0; i < temp.size(); i++)
	{
		alignments.push_back(temp[i]);
	}
	return true;
}

bool ReorderStructureAlignments(CCdCore* pCD, const vector<int>& positions)
{
    bool result = false;
    if (!pCD || !pCD->IsSeqAligns() || positions.size() == 0 || !pCD->Has3DMaster())
		return result;

    typedef CBiostruc_feature_set::TFeatures TStructureAlignments;
    TStructureAlignments::iterator saListIt, saListEnd;

    typedef map<unsigned int, CRef< CBiostruc_feature > > TSAMap;
    TSAMap saMapTmp;
    TSAMap::iterator saMapIt, saMapEnd;

    const CPDB_seq_id* pdbId = NULL;

    unsigned int i, nRows = (unsigned) pCD->GetNumRows();
    int n3DAlign = pCD->Num3DAlignments(), nPDBs = 0;
	if (nRows-1 != positions.size())
		return result;

    if (!pCD->IsSetFeatures() || pCD->GetFeatures().GetFeatures().size() == 0) {
		return result;
	}

    TStructureAlignments& saList = pCD->SetFeatures().SetFeatures().front()->SetFeatures();
    saListIt = saList.begin();
    saListEnd = saList.end();

    //  For each non-master row, check if it's a structure.  If so, add the corresponding
    //  structure alignment to the temporary map indexed by the *new* position.  Recall that
    //  positions does not include an entry for the master and positions[0] is the new position
    //  of row 1.
    int si;
    for (i = 0; i < (nRows - 1) && saListIt != saListEnd; ++i) {
        si = (int) i;
        if (pCD->GetPDB(si+1, pdbId)) {
            saMapTmp.insert(TSAMap::value_type(positions[si], *saListIt));
            ++saListIt;
            ++nPDBs;
        }
    }

    //  The number of non-master PDBs founds should be the same as the number of
    //  structural alignments.  If not, then there's no way to know how to reorder
    //  them --> leave them in the original state.
    if (nPDBs == n3DAlign) {
        saList.clear();
        saMapIt  = saMapTmp.begin();
        saMapEnd = saMapTmp.end();
        for (; saMapIt != saMapEnd; ++saMapIt) {
            saList.push_back(saMapIt->second);
        }
        result = true;
    }
    saMapTmp.clear();

    return result;
}

bool SetCreationDate(CCdCore* cd) {
    bool result = true;
    if (cd) {
	    //add creation date
	    list< CRef< CCdd_descr > > & descList = cd->SetDescription().Set();
	    list< CRef< CCdd_descr > >::iterator it = descList.begin();
	    for(; it != descList.end(); it++)
	    {
		    if((*it)->IsCreate_date())
		    {
			    descList.erase(it);
			    break;
		    }
	    }
	    CTime cur(CTime::eCurrent);
		CDate* curDate = new CDate(cur, CDate::ePrecision_day);
        if (curDate) {
	        CRef< CCdd_descr > dateDesc(new CCdd_descr);
	        dateDesc->SetCreate_date(*curDate);
	        descList.push_back(dateDesc);
        } else {
            result = false;
        }
    }
    return result;
}

bool SetUpdateDate(CCdCore* cd) {
    bool result = true;
    if (cd) {
	    //add creation date
	    list< CRef< CCdd_descr > > & descList = cd->SetDescription().Set();
	    list< CRef< CCdd_descr > >::iterator it = descList.begin();
	    for(; it != descList.end(); it++)
	    {
		    if((*it)->IsUpdate_date())
		    {
			    descList.erase(it);
			    break;
		    }
	    }
	    CTime cur(CTime::eCurrent);
	    CDate* curDate = new CDate(cur, CDate::ePrecision_day);
        if (curDate) {
	        CRef< CCdd_descr > dateDesc(new CCdd_descr);
	        dateDesc->SetUpdate_date(*curDate);
	        descList.push_back(dateDesc);
        } else {
            result = false;
        }
    }
    return result;
}

//   If ncbiMime contains a CD, return it.  Otherwise return NULL.
CCdCore* ExtractCDFromMime(CNcbi_mime_asn1* ncbiMime) {

    CCdCore* pCD = NULL;

    //  Only mime type defined w/ a CD in it is 'general' == Biostruc-seqs-aligns-cdd,
    //  whose 'seq-align-data' choice object must have a cd.  Ignore the additional
    //  'structures' and 'structure-type' fields.
    if (ncbiMime && ncbiMime->IsGeneral() && ncbiMime->GetGeneral().GetSeq_align_data().IsCdd()) {
        const CCdd* tmpCCdd = &ncbiMime->GetGeneral().GetSeq_align_data().GetCdd();
        pCD = CopyCD((const CCdCore*) tmpCCdd);
    }

    return pCD;
}

//  from cdt_frame.cpp
//  Wraps a call to CopyASNObject
//  If copyNonASNMembers = true, all statistics, sequence strings and aligned residues
//  data structures are copied.  The CD's read status is *always* copied.
CCdCore* CopyCD(const CCdCore* cd) {

  string  err;

  if (cd != NULL) {
      CCdCore* newCD = CopyASNObject(*cd, &err);
      return newCD;
      //return CopyASNObject(*cd, &err);
  }
  return NULL;
}

//  For a specified row in cd1, find all potential matches in cd2.
//  If cd1 == cd2, returns row1 plus any other matches (should be exactly one in a valid CD)
//  If cd1AsChild == true,  mapping assumes cd2 is parent of cd1.
//  If cd1AsChild == false, mapping assumes cd1 is parent of cd2.
//  In 'overlapMode', returns the row index of cd2 for *any* overlap with row1.
//  Return number of rows found.
//  (except for interface, almost the same as the CFootprint::isMemberOf methods;
//   CFootprint is streamlined by factoring out the check on Seq_id's)
int GetMappedRowIds(CCdCore* cd1, int row1, CCdCore* cd2, vector<int>& rows2, bool cd1AsChild, bool overlapMode) {

    int   Lower, Upper, Lower2, Upper2;
    int   NumMatches;
    vector<int> matches;
    CRef<CSeq_id>  SeqId;

    //  Each CD must be valid, and row1 must be valid
    if (cd1 == NULL || cd2 == NULL || !cd1->GetSeqIDFromAlignment(row1, SeqId)) {
        rows2.clear();
        return 0;
    }

    // get its alignment range
    Lower = cd1->GetLowerBound(row1);
    Upper = cd1->GetUpperBound(row1);

    matches.clear();
    NumMatches = cd2->GetAllRowIndicesForSeqId(SeqId, matches);  // find all row indices for a seqID (return # found)
    if (NumMatches) {

        // for each of the matches
        for (int j=0; j<NumMatches; j++) {
            Lower2 = cd2->GetLowerBound(matches[j]);
            Upper2 = cd2->GetUpperBound(matches[j]);
            if (overlapMode) {         // if the alignment ranges overlap
                if (((Lower2 >= Lower) && (Lower2 <= Upper)) ||
                    ((Upper2 >= Lower) && (Upper2 <= Upper)) ||
                    ((Lower2 <= Lower) && (Upper2 >= Upper)) ||
                    ((Lower2 >= Lower) && (Upper2 <= Upper))) {
                    rows2.push_back(matches[j]);
                }
            } else if (cd1AsChild) {   //  cd1 == child; cd2 == parent
                if ((Lower2 >= Lower) && (Upper2 <= Upper)) {
                    rows2.push_back(matches[j]);
                }
            } else {                   //  cd2 == child; cd1 == parent
                if ((Lower2 <= Lower) && (Upper2 >= Upper)) {
                    rows2.push_back(matches[j]);
                }
            }
        }
    }
    return rows2.size();
}

//  from cdt_summary_box.cpp

//  Get overlaps of cd1 with cd2 (no parent/child relationship between CDs is assumed).
//  If cd2 == NULL and cd1 is valid, looks for overlaps in that single CD.
//  If cd1 == cd2, also looks for overlaps in a single CD.
int GetOverlappedRows(CCdCore* cd1, CCdCore* cd2, vector<int>& rowsOfCD1, vector<int>& rowsOfCD2) {
//------------------------------------------------------------------------
// get the accessions of all newest-version CDs that have aligned residues
// that overlap aligned residues from this CD
//------------------------------------------------------------------------

  int   RowIndex1, NumRows = cd1->GetNumRows();
  int   NumMatches = 0;
  vector<int> matches;
  CRef<CSeq_id>  SeqId;

  bool  sameCD = (cd1 == cd2  || !cd2) ? true : false;

//  don't clear rows in case want to accummulate all overlaps in one container
//  rowsOfCD1.clear();
//  rowsOfCD2.clear();
  if (cd1 == NULL && cd2 == NULL) return 0;
  if (sameCD && !cd2) {
      cd2 = cd1;
  }

  // for each row of CD1
  for (RowIndex1=0; RowIndex1<NumRows; RowIndex1++) {
      matches.clear();
      NumMatches = GetMappedRowIds(cd1, RowIndex1, cd2, matches, true, true);
      if (NumMatches) {
        //  when looking in a single CD, expect to find exactly one.
        if (sameCD && NumMatches == 1) {
            continue;
        }

        // for each of the matches
        for (int j=0; j<NumMatches; j++) {
            //  when looking in a single CD, skip the self-match
            if (sameCD && matches[j] == RowIndex1) {
                continue;
            }
            // add overlap
            rowsOfCD1.push_back(RowIndex1);
            rowsOfCD2.push_back(matches[j]);
        }
      }
  }
  return rowsOfCD1.size();
}




//  Just check if have overlapped rows; returns the number of overlap.
//  Ignore any self-overlaps when cd1==cd2.
int NumberOfOverlappedRows(CCdCore* cd1, CCdCore* cd2) {
    vector<int> rows1, rows2;
    return GetOverlappedRows(cd1, cd2, rows1, rows2);
}

void SetConvertedSequencesForCD(CCdCore* cd, vector<string>& convertedSequences, bool forceRecompute) {

    if (!cd || (convertedSequences.size() > 0 && !forceRecompute)) {
        return;
    }

    int numSeq = cd->GetNumSequences();

    convertedSequences.clear();
    for (int i = 0; i < numSeq; ++i) {
        convertedSequences.push_back(cd->GetSequenceStringByIndex(i));
    }
}

void SetAlignedResiduesForCD(CCdCore* cd, char** & ppAlignedResidues, bool forceRecompute) {
//-------------------------------------------------------------------------
// allocate space for, and make, an array of all aligned residues
//-------------------------------------------------------------------------
    bool isMaster;

    string s;
    int numRows = cd->GetNumRows();
    int numAligned = cd->GetAlignmentLength();

    CRef< CSeq_align > seqAlign;

    // if space isn't allocated yet, allocate space for array of aligned residues
    if (ppAlignedResidues == NULL) {
        ppAlignedResidues = new char*[numRows];
        for (int i=0; i<numRows; i++) {
            ppAlignedResidues[i] = new char[numAligned];
        }
    // if space is already allocated then safe to assume array's been filled in
//    } else {
//        return;
    } else if (!forceRecompute) {
        return;
    }

    for (int i = 0; i < numRows; i++) {
        s = cd->GetSequenceStringByRow(i);
        if (s.size() > 0) {
            if (cd->GetSeqAlign(i, seqAlign)) {
                isMaster = (i == 0);
                SetAlignedResiduesOnSequence(seqAlign, s, ppAlignedResidues[i], isMaster);
            }
        }
    }
}

void GetAlignmentColumnsForCD(CCdCore* cd, map<unsigned int, string>& columns, unsigned int referenceRow)
{
    bool isOK = true, useRefRow = true;
    int j;
    unsigned int i, col, row, pos, mapIndex, nRows, nCols, nBlocks;
    char** alignedResidues = NULL;
    string rowString, colString;
    //  Map column number to position on the selected reference row.
    map<unsigned int, unsigned int> colToPos;
    map<unsigned int, string> rowStrings;
    vector<int> starts, lengths;
    CRef< CSeq_align > seqAlign;

    //  Empty the columns map first, as this is used as a way to flag problems.
    columns.clear();

    if (!cd) return;

    //  Check if the block structure is consistent.
    try {
        MultipleAlignment* ma = new MultipleAlignment(cd);
        if (!ma) {
            ERR_POST("Creation of MultipleAlignment object failed for CD " << cd->GetAccession() << ".");
            return;
        } else if (! ma->isBlockAligned()) {
            delete ma;
            ERR_POST("CD " << cd->GetAccession() << " must have a consistent block structure for column extraction.");
            return;
        }
        delete ma;
        ma = NULL;
    } catch (...) {
        ERR_POST("Could not extract columns for CD " << cd->GetAccession());
    }

    nCols = cd->GetAlignmentLength();
    nRows = cd->GetNumRows();

    //  Get a reference seq-align for mapping between alignment rows.
    //  If the columns map index will simply be the column count, use the master, row 0.
    if (referenceRow >= nRows) {
        useRefRow = false;
        referenceRow = 0;
    } 
    if (! cd->GetSeqAlign(referenceRow, seqAlign)) {
        isOK = false;
    }

    //  Initialize the column # -> reference row position mapping.
    //  If useRefRow is true, use the indicated row's coordinates as the position.
    //  Otherwise, use the column number as the position.
    if (isOK && GetBlockStarts(seqAlign, starts, (referenceRow == 0)) > 0 && GetBlockLengths(seqAlign, lengths) > 0) {
        nBlocks = starts.size();
        if (nBlocks == lengths.size()) {
            for (i = 0, col = 0; i < nBlocks; ++i) {
                pos = (useRefRow) ? starts[i] : col;
                for (j = 0; j < lengths[i]; ++j, ++col, ++pos) {
                    //  Not explicitly checking if 'pos' is aligned since above 
                    //  we confirmed the CD has a valid block model.
                    colToPos[col] = pos;
                }
            }
        } else {
            isOK = false;
        }
    } else {
        isOK = false;
    }

    SetAlignedResiduesForCD(cd, alignedResidues, true);

    //  Construct the columns as string objects.
    if (isOK && alignedResidues) {
        for (col = 0; col < nCols; ++col) {
            colString.erase();
            for (row = 0; row < nRows; ++row) {
                colString += alignedResidues[row][col];
            }
            mapIndex = colToPos[col];
            columns[mapIndex] = colString;
        }
    }

    //  Clean up array of characters.
    if (alignedResidues) {
        for (row = 0; row < nRows; ++row) {
            delete [] alignedResidues[row];
        }
        delete [] alignedResidues;
    }

}

string GetVerboseNameStr(const CCdCore* cd) {
    return ((cd) ? cd->GetAccession() + " (" + cd->GetName() + ")" : "");
}


// deprecated by dih, written by charlie.
// gets a bioseq and seqloc for master row.  also puts in a seq-id which identifies cd of origin.
CRef< CBioseq > GetMasterBioseqWithFootprintOld(CCdCore* cd)
{
	//get master bioseq
	CRef< CBioseq > masterBioseq(new CBioseq);
	CRef< CBioseq > bioseq;
	cd->GetBioseqForRow(0, bioseq);
	masterBioseq->Assign(*bioseq);
	//add local seq-id of cd accession
	list< CRef< CSeq_id > >& idList = masterBioseq->SetId();
	CRef< CSeq_id >  seqIdRef(new CSeq_id(CSeq_id::e_Local, cd->GetAccession(), ""));
	idList.push_back(seqIdRef);
	//add seq-loc of footprint
	list< CRef< CSeq_annot > >&  seqAnnots = masterBioseq->SetAnnot();
	CRef< CSeq_annot > seqAnnot(new CSeq_annot);
	list< CRef< CSeq_loc > >& seqlocs = seqAnnot->SetData().SetLocs();
	CRef< CSeq_loc > seqLoc(new CSeq_loc((**idList.begin()), cd->GetLowerBound(0), cd->GetUpperBound(0)));
	seqlocs.push_back(seqLoc);
	seqAnnots.push_back(seqAnnot);
	return masterBioseq;
}


CRef< CBioseq > GetMasterBioseqWithFootprint(CCdCore* cd) {
//-------------------------------------------------------------------------
// same as GetMasterBioseqWithFootprintOld but uses
// GetBioseqWithFootprintForNthRow.
//-------------------------------------------------------------------------
    string errstr;
    return(GetBioseqWithFootprintForNthRow(cd, 0, errstr));
}


CRef< CBioseq > GetBioseqWithFootprintForNthRow(CCdCore* cd, int N, string& errstr) {
//-------------------------------------------------------------------------
// copied from Charlie's function GetMasterBioseqWithFootprintOld.
// gets bioseq and seqloc for any row, not just master
//-------------------------------------------------------------------------
	CRef< CBioseq > BioseqForNthRow(new CBioseq);
	CRef< CBioseq > bioseq;

    errstr.erase();
    if (N >= cd->GetNumRows()) {
        errstr = "can't return bioseq for " + NStr::IntToString(N)
            + "th row, because CD has only "
            + NStr::IntToString(cd->GetNumRows()) + "rows.\n";
        BioseqForNthRow->Assign(*bioseq);
        return(BioseqForNthRow);
    }
	//get bioseq for Nth row.
	cd->GetBioseqForRow(N, bioseq);
	BioseqForNthRow->Assign(*bioseq);
	//add local seq-id of cd accession
	list< CRef< CSeq_id > >& idList = BioseqForNthRow->SetId();
	CRef< CSeq_id >  seqIdRef(new CSeq_id(CSeq_id::e_Local, cd->GetAccession(), ""));
	idList.push_back(seqIdRef);
	//add seq-loc of footprint
	list< CRef< CSeq_annot > >&  seqAnnots = BioseqForNthRow->SetAnnot();
	CRef< CSeq_annot > seqAnnot(new CSeq_annot);
	list< CRef< CSeq_loc > >& seqlocs = seqAnnot->SetData().SetLocs();
	CRef< CSeq_loc > seqLoc(new CSeq_loc((**idList.begin()), cd->GetLowerBound(N), cd->GetUpperBound(N)));
	seqlocs.push_back(seqLoc);
	seqAnnots.push_back(seqAnnot);
	return BioseqForNthRow;
}


bool GetBioseqWithFootprintForNRows(CCdCore* cd, int N, vector< CRef< CBioseq > >& bioseqs, string& errstr) {
//-------------------------------------------------------------------------
// get bioseqs and seq-locs for the first N rows of cd.
// if cd has less than N rows, return bioseqs and seq-locs for all rows.
//-------------------------------------------------------------------------
    N = N <= cd->GetNumRows() ? N : cd->GetNumRows();
    bioseqs.clear();
    for (int i=0; i<N; i++) {
        bioseqs.push_back(GetBioseqWithFootprintForNthRow(cd, i, errstr));
        if (!errstr.empty()) return(false);
    }
    return(true);
}


CRef< COrg_ref > GetCommonTax(CCdCore* cd, bool useRootWhenNoTaxInfo)
{
	int comTax = 0;
	CRef< COrg_ref > orgRef;
	CTaxon1 taxServer;
	if (!taxServer.Init())
		return orgRef;

	bool is_species;
	bool is_uncultured;
	string blast_name;
	int num = cd->GetNumRows();

	for (int i = 0; i < num; i++)
	{
		int gi = -1;
		cd->GetGI(i,gi,false);
		int taxid = 0;
		if (gi > 0)
			taxServer.GetTaxId4GI(gi, taxid);
		if (taxid == 0)
		{
			CRef< CBioseq > bioseq;
			if (cd->GetBioseqForRow(i, bioseq))
			{
				taxid = GetTaxIdInBioseq(*bioseq);
			}
		}

		if (taxid > 0)
		{
			if (comTax == 0)
				comTax = taxid;
			else
			{
				int joined = taxServer.Join(comTax, taxid);
				if (joined == 0)
				{
					LOG_POST("Failed to join two taxids:"<<comTax<<" and "<<taxid<<". The latter one is ignored.");
				}
				else
					comTax = joined;
			}
		}
		if (comTax == 1) //reach root
			break;
	}

    //  The condition 'comTax == 0' is true only if no row satisfied (taxid > 0) above.
    //  Use root tax node as common tax node unless told not to.
	if (comTax == 0 && useRootWhenNoTaxInfo)
		comTax = 1;

	orgRef = new COrg_ref;
    if (comTax > 0) {
        orgRef->Assign(*taxServer.GetOrgRef(comTax, is_species, is_uncultured, blast_name));
    } else {
        orgRef.Reset();
    }
	return orgRef;
}

bool obeysParentTypeConstraints(const CCdCore* pCD) {
    bool isAncestors;
    bool result = false;
    int  nAncestors, nClassical = 0;

    if (pCD) {
        //  Check that one or neither of 'ancestors' or 'parent' is set.
        //  If the later is set, automatically means a single classical parent.
        isAncestors = pCD->IsSetAncestors();
        result = (isAncestors) ? !pCD->IsSetParent() : true;
        if (result && isAncestors) {
            list< CRef< CDomain_parent > >::const_iterator pit = pCD->GetAncestors().begin(), pit_end = pCD->GetAncestors().end();
            while (pit != pit_end) {
                if ((*pit)->GetParent_type() == CDomain_parent::eParent_type_classical) {
                    ++nClassical;
                }
                ++pit;
            }
            nAncestors = pCD->GetAncestors().size();
            result = ((nClassical == 1 && nAncestors == 1) ||
                      (nClassical == 0 && nAncestors  > 0));
        }
    }
    return result;
}


bool RemasterWithStructure(CCdCore* cd, string* msg)
{
    static const string msgHeader = "Remastering CD to ";

	CRef< CSeq_id > seqId;
	cd->GetSeqIDForRow(0,0,seqId);
	if (seqId->IsPdb())
		return false;
	AlignmentCollection ac(cd,CCdCore::USE_NORMAL_ALIGNMENT);
	int nrows = ac.GetNumRows();
	int i = 1;
	for (; i < nrows; i++)
	{
		ac.GetSeqIDForRow(i,seqId);
		if (seqId->IsPdb())
			break;
	}
	if ( i < nrows)
	{
		ReMasterCdWithoutUnifiedBlocks(cd, i, true);
        if (msg) {
            *msg = msgHeader + seqId->AsFastaString();
        }
		return true;
	}
	else
		return false;
}

//return the number of PDBs fixed
int FixPDBDefline(CCdCore* cd)
{
	CRef< CSeq_id > seqId;
	AlignmentCollection ac(cd,CCdCore::USE_NORMAL_ALIGNMENT);
	int nrows = ac.GetNumRows();
	int numFixed = 0;
	for (int i = 0; i < nrows; i++)
	{
		ac.GetSeqIDForRow(i,seqId);
		if (seqId->IsPdb())
		{
			CRef< CBioseq > bioseq;
			ac.GetBioseqForRow(i, bioseq);
			if (checkAndFixPdbBioseq(bioseq))
				numFixed++;
		}
	}
	return numFixed;
}

bool ReMasterCdWithoutUnifiedBlocks(CCdCore* cd, int Row, bool resetFields)
{
	if (Row == 0)
		return true;
	list<CRef< CSeq_align > >& seqAlignList = cd->GetSeqAligns();
	const CRef< CSeq_align >& guide = cd->GetSeqAlign(Row);
	BlockModelPair guideBmp(guide);
	int i = 1;
	list<CRef< CSeq_align > >::iterator lit = seqAlignList.begin();
	list<CRef< CSeq_align > >::iterator guideIt;
	for (; lit != seqAlignList.end(); lit++)
	{
		if ( i != Row)
		{
			BlockModelPair bmp (*lit);
			bmp.remaster(guideBmp);
			*lit = bmp.toSeqAlign();
		}
		else
			guideIt = lit;
		i++;
	}
	guideBmp.reverse();
	*guideIt = guideBmp.toSeqAlign();
	// if there's a master3d
	if (cd->IsSetMaster3d() && resetFields) {
	 // get rid of it
		cd->SetMaster3d().clear();
	}
	// if the new master has a pdb-id
	CRef< CSeq_id >  SeqID(new CSeq_id);
	if (cd->GetSeqIDForRow(0, 0, SeqID)) {
		if (SeqID->IsPdb()) {
		// make it the master3d
		// (the ref-counter for SeqID should have been incremented in GetSeqID)
		cd->SetMaster3d().clear();
		cd->SetMaster3d().push_back(SeqID);
		}
	}
	if (resetFields) 
	{
		ResetFields(cd);
    }
	return remasterAlignannot(*cd, Row);
}

bool remasterAlignannot(CCdCore& cd, unsigned int oldMasterRow)
{
    //  Exit if invalid row number or the old master is same as current master.
    if ((int)oldMasterRow >= cd.GetNumRows() || oldMasterRow == 0) return false;

    bool ok = true;
    int From, To, NewFrom, NewTo;
    CAlign_annot_set::Tdata::iterator  alignAnnotIt, alignAnnotEnd;
    CPacked_seqint::Tdata::iterator intervalIt, intervalEnd;
    BlockModelPair guideAlignment(cd.GetSeqAlign(oldMasterRow));
    CRef< CSeq_id > seqIdMaster(guideAlignment.getMaster().getSeqId());
    CRef< CSeq_id > seqIdOldMaster(guideAlignment.getSlave().getSeqId());

    // if there's an align-annot set
    if (cd.IsSetAlignannot()) {
        // for each alignannot
        alignAnnotEnd = cd.SetAlignannot().Set().end();
        for (alignAnnotIt = cd.SetAlignannot().Set().begin(); alignAnnotIt != alignAnnotEnd; alignAnnotIt++) {
            // if it's a from-to; make sure current seq id matches
            if ((*alignAnnotIt)->SetLocation().IsInt()) {
                // update from and to with new master
                From = (int) (*alignAnnotIt)->SetLocation().GetInt().GetFrom();
                To = (int) (*alignAnnotIt)->SetLocation().GetInt().GetTo();
                NewFrom = guideAlignment.mapToMaster(From);
                NewTo   = guideAlignment.mapToMaster(To);

                if (!seqIdOldMaster->Match((*alignAnnotIt)->SetLocation().SetInt().GetId()) || NewFrom < 0 || NewTo < 0)
                {
                    //  If somehow already have alignannot mapped to the current master, don't flag an error.
                    //  Second condition is to deal with case of old/new masters being different footprints on
                    //  the same sequence ==> would only be here if there was a problem in NewFrom or NewTo
                    //  in that case if seqIdMaster == seqIdOldMaster and first condition passed.
                    if (seqIdMaster->Match((*alignAnnotIt)->SetLocation().SetInt().GetId()) && !seqIdMaster->Match(*seqIdOldMaster)) {
                        continue;
                    }
                    ok = false;
                    continue;
                }
                (*alignAnnotIt)->SetLocation().SetInt().SetFrom(NewFrom);
                (*alignAnnotIt)->SetLocation().SetInt().SetTo(NewTo);
                (*alignAnnotIt)->SetLocation().SetInt().SetId(*seqIdMaster);
            }
            // if it's a set of from-to's
            else if ((*alignAnnotIt)->SetLocation().IsPacked_int()) {
                // for each from-to
                intervalIt = (*alignAnnotIt)->SetLocation().SetPacked_int().Set().begin();
                intervalEnd = (*alignAnnotIt)->SetLocation().SetPacked_int().Set().end();
                for (; intervalIt != intervalEnd; ++intervalIt) {
                    // update from and to with new master
                    From = (int) (*intervalIt)->GetFrom();
                    To = (int) (*intervalIt)->GetTo();
                    NewFrom = guideAlignment.mapToMaster(From);
                    NewTo   = guideAlignment.mapToMaster(To);
                    if (!seqIdOldMaster->Match((*intervalIt)->GetId()) || NewFrom < 0 || NewTo < 0)
                    {
                        //  If somehow already have alignannot mapped to the current master, don't flag an error.
                        //  Second condition is to deal with case of old/new masters being different footprints on
                        //  the same sequence ==> would only be here if there was a problem in NewFrom or NewTo
                        //  in that case if seqIdMaster == seqIdOldMaster and first condition passed.
                        if (seqIdMaster->Match((*intervalIt)->GetId()) && !seqIdMaster->Match(*seqIdOldMaster)) {
                            continue;
                        }
                        ok = false;
                        continue;
                    }
                    (*intervalIt)->SetFrom(NewFrom);
                    (*intervalIt)->SetTo(NewTo);
                    (*intervalIt)->SetId(*seqIdMaster);
                }
            }
        }
    }
    return ok;
}

int  PurgeConsensusSequences(CCdCore* pCD, bool resetFields)
{
    int nPurged = 0;
    vector<int> consensusRows, consensusSeqListIds;

    if (pCD) {
        //  First see if the master is a consensus; if so, remaster to 2nd row
        if (pCD->UsesConsensusSequenceAsMaster()) {
            ReMasterCdWithoutUnifiedBlocks(pCD, 1,true);
        }

        //  Next, find all rows using 'consensus' as a seq_id and remove them.

        nPurged = pCD->GetRowsWithConsensus(consensusRows);
        if (nPurged) {
            pCD->EraseTheseRows(consensusRows);
            if (pCD->FindConsensusInSequenceList(&consensusSeqListIds)) {
                for (int i = consensusSeqListIds.size() - 1; i >= 0 ; --i) {
                    pCD->EraseSequence(consensusSeqListIds[i]);
                }
            }
        }
    }

    return nPurged;
}


int IntersectByMaster(CCdCore* ccd, double rowFraction) {

    int result = -1;
    unsigned int masterLen = (ccd) ? ccd->GetSequenceStringByRow(0).length() : 0;
    if (masterLen == 0) return result;

    int slaveStart;
    int nAlignedIBM = 0;
    unsigned int i, j, nBlocks;
    unsigned int nRows = ccd->GetNumRows();

    //  If there is already a consistent block model, do nothing.
    MultipleAlignment* ma = new MultipleAlignment(ccd);
    if (ma && ma->isBlockAligned()) {
        delete ma;
        return 0;
    }
    delete ma;


    BlockIntersector blockIntersector(masterLen);
    BlockModel* intersectedBlockModel;
    //BlockModel* simpleIntersectedBlockModel;
    BlockModelPair* bmp;
    vector<BlockModelPair*> blockModelPairs;
    set<int> forcedCTerminiInIntersection;

    list< CRef< CSeq_align > >& cdSeqAligns = ccd->GetSeqAligns();
    list< CRef< CSeq_align > >::iterator cdSeqAlignIt = cdSeqAligns.begin(), cdSeqAlignEnd = cdSeqAligns.end();

    for (i = 0; cdSeqAlignIt != cdSeqAlignEnd; ++cdSeqAlignIt, ++i) {
        bmp = new BlockModelPair(*cdSeqAlignIt);

        //  We assume # of blocks and all block lengths are same on master and slave.
        if (bmp && bmp->isValid()) {

            blockModelPairs.push_back(bmp);
            blockIntersector.addOneAlignment(bmp->getMaster());

            //  Find places the intersection can't merge blocks (i.e., where there are
            //  gaps in the slave across a block boundary, but not in the master).
            BlockModel& slave = bmp->getSlave();
            nBlocks = slave.getBlocks().size();
            for (j = 0; j < nBlocks - 1; ++j) {  //  '-1' as I don't care about end of the C-terminal block
                if (slave.getGapToCTerminal(j) > 0 && bmp->getMaster().getGapToCTerminal(j) == 0) {
                    forcedCTerminiInIntersection.insert(bmp->getMaster().getBlock(j).getEnd());
                }
            }
        }
    }

    //  There was a problem creating one of the BlockModelPair objects from a seq_align,
    //  or one or more seq_align was invalid.
    if (blockModelPairs.size() != cdSeqAligns.size()) {
        return result;
    }

    //simpleIntersectedBlockModel = blockIntersector.getIntersectedAlignment(forcedCTerminiInIntersection);
    intersectedBlockModel = blockIntersector.getIntersectedAlignment(forcedCTerminiInIntersection, rowFraction);
    nAlignedIBM = (intersectedBlockModel) ? intersectedBlockModel->getTotalBlockLength() : 0;
    if (nAlignedIBM == 0) {
        return result;
    }

/*
    string testStr, testStr2;
    string sint = intersectedBlockModel->toString();
    string sintsimple = simpleIntersectedBlockModel->toString();
    delete simpleIntersectedBlockModel;
    cout << "rowFraction = 1:\n" << sintsimple << endl;
    cout << "rowFraction = " << rowFraction << ":\n" << sint << endl;
*/

    //  As we have case where every block model isn't identical,
    //  change each seq-align to reflect the common set of aligned columns.
    nBlocks = intersectedBlockModel->getBlocks().size();
    for (i = 0, cdSeqAlignIt = cdSeqAligns.begin(); i < nRows - 1 ; ++i, ++cdSeqAlignIt) {

        bmp = blockModelPairs[i];  //BlockModelPair seqAlignPair(*cdSeqAlignIt);
        BlockModel* intersectedSeqAlignSlave = new BlockModel(bmp->getSlave().getSeqId(), false);

        bmp->reverse();
        for (j = 0; j < nBlocks; ++j) {
            const Block& jthMasterBlock = intersectedBlockModel->getBlock(j);
            slaveStart = bmp->mapToMaster(jthMasterBlock.getStart());

            //  since we're dealing w/ an intersection, slaveStart should always be valid
            assert(slaveStart != -1);

            Block b(slaveStart, jthMasterBlock.getLen(), jthMasterBlock.getId());
            intersectedSeqAlignSlave->addBlock(b);
        }
        *cdSeqAlignIt = intersectedSeqAlignSlave->toSeqAlign(*intersectedBlockModel);
        //testStr = intersectedSeqAlignSlave->toString();
        //testStr2 = bmp->getMaster().toString();  // original *slave* alignment

        delete bmp;
    }
    blockModelPairs.clear();
    result = nBlocks;

    delete intersectedBlockModel;

    return result;
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE
