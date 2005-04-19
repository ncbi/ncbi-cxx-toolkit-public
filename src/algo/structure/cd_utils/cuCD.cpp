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
//#include "cdAlignmentAdaptor.hpp"
#include <algo/structure/cd_utils/cuCdReadWriteASN.hpp>
//#include "cdWorkshop.hpp"

#include <algo/structure/cd_utils/cuAlign.hpp>
//#include "algComponent.hpp"
#include <algo/structure/cd_utils/cuCD.hpp>
#include <algo/structure/cd_utils/cuSeqTreeFactory.hpp>
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
		if (positions[row] >= temp.size())
			return false;
		temp[positions[row]] = *lit;
		row++;
	}

    //  Before moving the alignments around, reorder the structural alignments to reflect
    //  the new ordering of the rows.  If there was a problem, the structural alignments
    //  are left in their original order.
    bool saReorderOK = ReorderStructureAlignments(pCD, positions);    

    alignments.clear();
	for (int i = 0; i < temp.size(); i++)
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
    
    int   NumRows = cd1->GetNumRows();
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

    errstr.clear();
    if (N >= cd->GetNumRows()) {
        char buf[1024];
        sprintf(buf, "can't return bioseq for %dth row, because CD has only %d rows.\n", N, cd->GetNumRows());
        errstr = buf;
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


CRef< COrg_ref > GetCommonTax(CCdCore* cd)
{
	int comTax = 0;
	CRef< COrg_ref > orgRef;
	CTaxon1 taxServer;
	if (!taxServer.Init())
		return orgRef;
	int num = cd->GetNumSequences();
	for (int i = 0; i < num; i++)
	{
		int gi = cd->GetGIFromSequenceList(i);
		int taxid = 0;
		if (taxServer.GetTaxId4GI(gi, taxid) && (taxid > 0))
		{
			if (comTax == 0)
				comTax = taxid;
			else
				comTax = taxServer.Join(comTax, taxid);
		}
		if (comTax == 1) //reach root
			break;
	}
	orgRef = new COrg_ref;
	bool is_species;
	bool is_uncultured;
	string blast_name;
	orgRef->Assign(*taxServer.GetOrgRef(comTax, is_species, is_uncultured, blast_name));
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

void calcDiversityRanking(CCdCore* cd, list<int>& rankList)
{
	rankList.clear();
	MultipleAlignment ma(cd);
	TreeOptions treeOptions;
	SeqTree* seqTree = TreeFactory::makeTree(&ma, treeOptions);
	int colRow = ma.GetRowSourceTable().convertFromCDRow(cd, 0);
	seqTree->getDiversityRankToRow(colRow, rankList);
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE


/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.1  2005/04/19 14:27:18  lanczyck
 * initial version under algo/structure
 *
 * ---------------------------------------------------------------------------
 */
