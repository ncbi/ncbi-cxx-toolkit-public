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
 *       Subclass of CCdCored for use by CDTree
 *		 As compared to CCdCore in CDTree
 *			Remove TaxServer -related methods
 *			Remove any methods using BLASTMatrix
 *
 * ===========================================================================
 */


#include <ncbi_pch.hpp>

#include <algorithm>
#include <algo/structure/cd_utils/cuSort.hpp>
#include <algo/structure/cd_utils/cuCdCore.hpp>
#include <algo/structure/cd_utils/cuAlign.hpp>
#include <algo/structure/cd_utils/cuCD.hpp>  
#include <algo/structure/cd_utils/cuSequence.hpp>
#include <algo/structure/cd_utils/cuUtils.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)


//const int    CCdCore::INVALID_MAPPED_POSITION = INVALID_POSITION; 

//const ncbi::ENull  CCdCore::NULL_CREF = ncbi::null;
const CRef<CSeq_align> EMPTY_CREF_SEQALIGN;
const CRef<CSeq_annot> EMPTY_CREF_SEQANNOT;


    // destructor
CCdCore::~CCdCore(void) {
}

// constructor
CCdCore::CCdCore(void)
{
}

/* ======================= */
/*  CD identifier methods  */
/* ======================= */

string CCdCore::GetAccession() const {
    int Dummy;
    return(GetAccession(Dummy));
}

string CCdCore::GetAccession(int& Version) const {
//-------------------------------------------------------------------------
// get accession name of CD
//-------------------------------------------------------------------------
  CCdd_id_set::Tdata::const_iterator  i;
  string  Str;

  for (i=GetId().Get().begin(); i!=GetId().Get().end(); i++) {
    if ((*i)->IsGid()) {
      if ((*i)->GetGid().IsSetVersion()) {
        Version = (*i)->GetGid().GetVersion();
      }
      else {
        Version = 1;
      }
      return((*i)->GetGid().GetAccession());
    }
  }
  return(Str);
}

void CCdCore::SetAccession(string Accession) {
    SetAccession(Accession, 1);
}

void CCdCore::SetAccession(string Accession, int Version) {
//-------------------------------------------------------------------------
// set accession name of CD
//-------------------------------------------------------------------------
  CCdd_id_set::Tdata::iterator  i;

  for (i=SetId().Set().begin(); i!=SetId().Set().end(); i++) {
    if ((*i)->IsGid()) {
      (*i)->SetGid().SetAccession(Accession);
      (*i)->SetGid().SetVersion(Version);
    }
  }
}

void CCdCore::EraseUID() {
//-------------------------------------------------------------------------
// erase CD's uid
//-------------------------------------------------------------------------
  CCdd_id_set::Tdata::iterator  i;

  for (i=SetId().Set().begin(); i!=SetId().Set().end(); i++) {
    if ((*i)->IsUid()) {
      SetId().Set().erase(i);
      return;
    }
  }
}


/* ADDED */
// is 'id' an identifier for this CD?
bool CCdCore::HasCddId(const CCdd_id& id) const{
//formerly ... bool isIdInSet(const CRef<CCdCore>& cd, const CCdd_id& id)

    bool isHere = false;
    CCdd_id_set::Tdata idSet = GetId().Get();  // list<CRef<CCdd_id>>
    for (CCdd_id_set::Tdata::const_iterator ci=idSet.begin(); ci!=idSet.end(); ++ci) {
        if ((**ci).Equals(id)) {
            isHere = true;
            break;
        }
    }
    return isHere;
}


/* ============================ */
/*  Basic information about CD  */
/* ============================ */

string CCdCore::GetLongDescription() {
//-------------------------------------------------------------------------
// get descriptive comment about CD
//-------------------------------------------------------------------------
  CCdd_descr_set::Tdata::const_iterator  i;
  string  Str;

  if (IsSetDescription()) {
    for (i=GetDescription().Get().begin(); i!=GetDescription().Get().end(); i++) {
      if ((*i)->IsComment()) {
        return((*i)->GetComment());
      }
    }
  }
  return(Str);
}

string CCdCore::GetUpdateDate() {
//-------------------------------------------------------------------------
// get string indicating date of last change to CD
//-------------------------------------------------------------------------
  CCdd_descr_set::Tdata::const_iterator i;
  string  Str;

  if (IsSetDescription()) {
    for (i=GetDescription().Get().begin(); i!=GetDescription().Get().end(); i++) {
      if ((*i)->IsUpdate_date()) {
        (*i)->GetUpdate_date().GetDate(&Str);
        return(Str);
      }
    }
  }
  return(Str);
}


int CCdCore::GetNumRows() const {
//-------------------------------------------------------------------------
// get number of rows in CD
//-------------------------------------------------------------------------

    const CRef< CSeq_annot >& alignment = GetAlignment();
    if (alignment.NotEmpty() && alignment->GetData().IsAlign()) {
        // number pairs + 1 == num rows
        return(alignment->GetData().GetAlign().size()+1);
    }
    return(0);
}

// number of rows of alignment with valid sequence indices
int CCdCore::GetNumRowsWithSequences() const {

    int count = 0, seqIndex = -1;
    int nrows = GetNumRows();
    for (int i = 0; i < nrows; ++i) {
        seqIndex = GetSeqIndexForRowIndex(i);
        if (seqIndex >= 0) {
            ++count;
        }
    }
    return count;
}

int CCdCore::GetNumSequences() const{
//-------------------------------------------------------------------------
// get number of sequences in CD
//-------------------------------------------------------------------------
  if (IsSetSequences()) {
    if (GetSequences().IsSet()) {
      return(GetSequences().GetSet().GetSeq_set().size());
    }
  }
  return(0);
}


int CCdCore::GetAlignmentLength() const{
//-------------------------------------------------------------------------
// get total number of aligned residues
// TDendiag = list< CRef< CDense_diag > >
//-------------------------------------------------------------------------
    const CRef< CSeq_align >& seqAlign = GetSeqAlign(0);
    if (seqAlign.NotEmpty()) {
        return GetNumAlignedResidues(seqAlign);
    }
    return 0;

}


int CCdCore::GetPSSMLength() const{
//-------------------------------------------------------------------------
// get number of residues in the master sequence, from the first
// aligned residue to the last aligned residue
//-------------------------------------------------------------------------
  return(GetUpperBound(0) - GetLowerBound(0) + 1);
}


/*  ADDED  */
// return number of blocks in alignment (0 if no alignment, or not a Dense_diag)
int CCdCore::GetNumBlocks() const {
    if (IsSeqAligns()) {
        const CRef< CSeq_align >& seqAlign = GetSeqAlign(0);
        if (seqAlign.NotEmpty()) {
            return GetBlockCount(seqAlign);
        }
    }
    return 0;
}

/* ADDED */ 
bool   CCdCore::GetCDBlockLengths(vector<int>& lengths) const {
    if (IsSeqAligns()) {
        const CRef< CSeq_align >& seqAlign = GetSeqAlign(0);
        if (seqAlign.NotEmpty()) {
            return (GetBlockLengths(seqAlign, lengths) != 0);
        }
    }
    return false;
}

/* ADDED */ 
bool   CCdCore::GetBlockStartsForRow(int rowIndex, vector<int>& starts) const {
    bool onMaster = (rowIndex) ? false : true;
    bool result = false;
    if (IsSeqAligns() && rowIndex >= 0) {
        const CRef< CSeq_align >& seqAlign = GetSeqAlign(0);
        if (seqAlign.NotEmpty()) {
            result = (GetBlockStarts(seqAlign, starts, onMaster) != 0);
            sort(starts.begin(), starts.end());
        }
    }
    return result;
}


/* ============================================ */
/*  Find/convert sequence list and row indices  */
/* ============================================ */

int CCdCore::GetSeqIndexForRowIndex(int rowIndex) const {
    int seqIndex = -1;
    CRef< CSeq_id > seqId;

    if (rowIndex < 0 || rowIndex > GetNumRows()) {
        return seqIndex;
    }

    if (GetSeqIDFromAlignment(rowIndex, seqId)) {
        seqIndex = GetSeqIndex(seqId);
    }
    return seqIndex;
}

int CCdCore::GetMasterSeqIndex() const{
    return GetSeqIndexForRowIndex(0);
}

int CCdCore::GetSeqIndex(const CRef< CSeq_id >& SeqID) const{
//-------------------------------------------------------------------------
//  get the sequence index with given SeqID
//-------------------------------------------------------------------------
  int  i, NumSequences = GetNumSequences();
  CRef< CSeq_id > TrialSeqID;

  if (SeqID.NotEmpty()) {
      for (i=0; i<NumSequences; i++) {
        if (GetSeqIDForIndex(i, TrialSeqID) && SeqIdsMatch(SeqID, TrialSeqID)) {
            return(i);
        }
    }
  }
  return(-1);
}

int CCdCore::GetNthMatchFor(CRef< CSeq_id >& ID, int N) {
//-------------------------------------------------------------------------
// get the row index for the Nth match of ID.
// N is 1-based.
// return -1 if no match is found.
//-------------------------------------------------------------------------
  int  k, Count, NumRows=GetNumRows();
  CRef< CSeq_id > TestID;

  Count = 0;
  for (k=0; k<NumRows; k++) {
    GetSeqIDFromAlignment(k, TestID);
    if (SeqIdsMatch(ID, TestID)) {
      Count++;
      if (Count == N) {
        return(k);
      }
    }
  }
  return(-1);
}

/* ADDED */
// convenience method to return a vector vs. a list
// find all row indices for a seqID, irrespective of the footprint (return # found)
int CCdCore::GetAllRowIndicesForSeqId(const CRef<CSeq_id>& SeqID, vector<int>& rows) const
{
    int numMatches = 0;
    list<int> lint;
    list<int>::iterator lintIt;

    rows.clear();
    numMatches = GetAllRowIndicesForSeqId(SeqID, lint);
    if (numMatches > 0) {
        for (lintIt = lint.begin(); lintIt != lint.end(); ++lintIt) {
            rows.push_back(*lintIt);
        }
    }
    return numMatches;
}

/* ADDED */
// find all row indices for a seqID, irrespective of the footprint (return # found)
int CCdCore::GetAllRowIndicesForSeqId(const CRef<CSeq_id>& SeqID, list<int>& rows) const
{
    // List size is returned.
    // Place all row indices for the seq_id into the list 'rows'.  Use a new or
    // cleared list; do not pass a list with other data as this function clears it!!
    // If Seq_id is not found, return empty list.

    CRef<CSeq_id> testID;
    CRef<CSeq_id> findID = SeqID;
    int i, nrow = GetNumRows();

    rows.empty();
    for (i=0; i<nrow; i++) {
        if (GetSeqIDFromAlignment(i, testID)) {  // tests only for gi or pdb IDs
            if (findID->Match(*testID)) { // match
                rows.push_back(i);
            }
        }
    }
    return rows.size();
}


/* ========================================== */
/*  Access CD info via alignment row number   */
/* ========================================== */

bool CCdCore::GetGI(int Row, int& GI, bool ignorePDBs) {
//-------------------------------------------------------------------------
// get the GI for Row
//-------------------------------------------------------------------------
  CRef< CSeq_id >  SeqID;
  int  Pair, DenDiagRow;

  Pair = (Row <= 1) ? 0 : Row-1;
  DenDiagRow = (Row == 0) ? 0 : 1;
  GetSeqIDForRow(Pair, DenDiagRow, SeqID);
  if (SeqID->IsGi()) {
    GI = SeqID->GetGi();
    return(true);
  } else if (SeqID->IsPdb() && !ignorePDBs) {   //  cjl; 1/09/04; to match AlignmentCollection
    GI = GetGIFromSequenceList(GetSeqIndex(SeqID));
    return true;
  }
  return(false);
}


bool CCdCore::GetPDB(int Row, const CPDB_seq_id*& pPDB) {
//-------------------------------------------------------------------------
// get the PDB ID for Row
//-------------------------------------------------------------------------
  CRef< CSeq_id >  SeqID;
  int  Pair, DenDiagRow;

  Pair = (Row <= 1) ? 0 : Row-1;
  DenDiagRow = (Row == 0) ? 0 : 1;
  GetSeqIDForRow(Pair, DenDiagRow, SeqID);
  if (SeqID->IsPdb()) {
    pPDB = &(SeqID->GetPdb());
    return(true);
  }
  return(false);
}

int CCdCore::GetLowerBound(int Row) const {
//-------------------------------------------------------------------------
// get the lower alignment boundary for Row
// return INVALID_MAPPED_POSITION on failure
//-------------------------------------------------------------------------
  CRef< CDense_diag > DenDiag;
  CDense_diag::TStarts::const_iterator  i;

  const CRef< CSeq_align >& seqAlign = GetSeqAlign(Row);
  if (seqAlign.NotEmpty() && GetFirstOrLastDenDiag(seqAlign, true, DenDiag)) {
      i = DenDiag->GetStarts().begin();
      if (Row != 0) {
          i++;
      }
      return(*i);
  }
  return INVALID_POSITION;
}

int CCdCore::GetUpperBound(int Row) const {
//-------------------------------------------------------------------------
// get the upper alignment boundary for Row
// return INVALID_MAPPED_POSITION on failure
//-------------------------------------------------------------------------
  CRef< CDense_diag > DenDiag;
  CDense_diag::TStarts::const_iterator  i;

  const CRef< CSeq_align >& seqAlign = GetSeqAlign(Row);
  if (seqAlign.NotEmpty() && GetFirstOrLastDenDiag(seqAlign, false, DenDiag)) {
    i = DenDiag->GetStarts().begin();
    if (Row != 0) {
        i++;
    }
    return((*i + DenDiag->GetLen()) - 1);
  }
  return INVALID_POSITION;
}

    
bool CCdCore::Get_GI_or_PDB_String_FromAlignment(int RowIndex, std::string& Str, bool Pad, int Len) const {
//-------------------------------------------------------------------
// get seq-id string for RowIndex of alignment
//-------------------------------------------------------------------
  int  Pair = (RowIndex <= 1) ? 0 : RowIndex-1;
  int  DenDiagRow = (RowIndex == 0) ? 0 : 1;
  CRef< CSeq_id > SeqID;

  GetSeqIDForRow(Pair, DenDiagRow, SeqID);
  Make_GI_or_PDB_String(SeqID, Str, Pad, Len);
  return(true);
}


bool CCdCore::GetSeqEntryForRow(int rowId, CRef< CSeq_entry >& seqEntry) const {

	bool result = false;
	CRef< CSeq_id > seqID;
	list< CRef< CSeq_id > >::const_iterator sici;
	list< CRef< CSeq_entry > >::const_iterator seci, seci_start, seci_end;

	if (GetSeqIDFromAlignment(rowId, seqID)) {
		if (IsSetSequences()) {
			if (GetSequences().IsSet()) {
				seci_start = GetSequences().GetSet().GetSeq_set().begin();
				seci_end   = GetSequences().GetSet().GetSeq_set().end();
				for (seci = seci_start; seci != seci_end && result == false; ++seci) {
					if ((*seci)->IsSeq()) {
						for (sici  = (*seci)->GetSeq().GetId().begin();
							 sici != (*seci)->GetSeq().GetId().end() && result == false; ++sici) {
							if (seqID->Match(**sici)) {
								result = true;
								seqEntry = *seci;
							}
						}
					}
				}
			}
		}
	}

	return result;
}



/* ADDED */
//  get the bioseq for the designated alignment row (for editing)
bool   CCdCore::GetBioseqForRow(int rowId, CRef< CBioseq >& bioseq)  {

    int seqIndex = GetSeqIndexForRowIndex(rowId);
    return GetBioseqForIndex(seqIndex, bioseq);
}

/* ADDED */
// find the species string for alignment row
string CCdCore::GetSpeciesForRow(int Row) {
    CRef< CBioseq > bioseq;  // = GetBioseqForRow(Row);
    if (GetBioseqForRow(Row, bioseq) && !bioseq.IsNull()) {
        return GetSpeciesFromBioseq(*bioseq);//  rework GetSpecies to use other stuff...
    }
    return kEmptyStr;
}


/* ADDED */
// get the sequence for specified row
string CCdCore::GetSequenceStringByRow(int rowId) {
    int seqIndex = GetSeqIndexForRowIndex(rowId);
    return GetSequenceStringByIndex(seqIndex);
}


/* ========================================== */
/*  Access CD info via a sequence list index  */
/* ========================================== */

int CCdCore::GetGIFromSequenceList(int SeqIndex) const {
//-------------------------------------------------------------------------
// get GI from the list of sequences.
// return -1 if no GI is found.
//-------------------------------------------------------------------------
  list< CRef< CSeq_entry > >::const_iterator  i;
  list< CRef< CSeq_id > >::const_iterator  j;
  int  SeqCount, IDCount;

  if (IsSetSequences()) {
    if (GetSequences().IsSet()) {
      // count to the SeqIndex sequence
      SeqCount = 0;
      for (i=GetSequences().GetSet().GetSeq_set().begin();
           i!=GetSequences().GetSet().GetSeq_set().end(); i++) {
        if (SeqCount == SeqIndex) {
          if ((*i)->IsSeq()) {
            // look through IDs for a gi
            IDCount = 0;
            for (j = (*i)->GetSeq().GetId().begin();
                 j != (*i)->GetSeq().GetId().end(); j++) {
              if ((*j)->IsGi()) {
                return((*j)->GetGi());
              }
              IDCount++;
            }
          }
        }
        SeqCount++;
        if (SeqCount > SeqIndex) break;
      }
    }
  }
  return(-1);
}
  

string CCdCore::GetDefline(int SeqIndex) {
//-------------------------------------------------------------------------
// get a description for the SeqIndex sequence
//-------------------------------------------------------------------------
  list< CRef< CSeq_entry > >::const_iterator  i;
  list< CRef< CSeqdesc > >::const_iterator  j;
  int  SeqCount;
  string  Description;

  if (IsSetSequences()) {
    if (GetSequences().IsSet()) {
      // count to the SeqIndex sequence
      SeqCount = 0;
      for (i=GetSequences().GetSet().GetSeq_set().begin();
           i!=GetSequences().GetSet().GetSeq_set().end(); i++) {
        if (SeqCount == SeqIndex) {
          if ((*i)->IsSeq()) {
            if ((*i)->GetSeq().IsSetDescr()) {
              // look through the sequence descriptions
              for (j=(*i)->GetSeq().GetDescr().Get().begin();
                   j!=(*i)->GetSeq().GetDescr().Get().end(); j++) {
                // if there's a title, return that description
                if ((*j)->IsTitle()) {
                  return((*j)->GetTitle());
                }
                // if there's a pdb description, return it
                if ((*j)->IsPdb()) {
                  if ((*j)->GetPdb().GetCompound().size() > 0) {
                    return((*j)->GetPdb().GetCompound().front());
                  }
                }
              }
            }
          }
        }
        SeqCount++;
        if (SeqCount > SeqIndex) break;
      }
    }
  }
  return(Description);
}


/* ADDED */
// find the species string for sequence list index
string CCdCore::GetSpeciesForIndex(int SeqIndex) {
    CRef< CBioseq > bioseq;  // = GetBioseqForIndex(SeqIndex);
    if (GetBioseqForIndex(SeqIndex, bioseq) && !bioseq.IsNull()) {
        return GetSpeciesFromBioseq(*bioseq);
    }
    return kEmptyStr;
}



/*  MOVED  */   //  was in cdt_manipcd as CD_GetSeq
bool CCdCore::GetSeqEntryForIndex(int seqIndex, CRef< CSeq_entry > & seqEntry) const
{
    list< CRef< CSeq_entry > >::const_iterator i, iend;

    int SeqCount = 0;
    if (seqIndex >= 0 && seqIndex < GetNumSequences() && IsSetSequences()) {
        if (GetSequences().IsSet()) {
            iend = GetSequences().GetSet().GetSeq_set().end();
            for (i =  GetSequences().GetSet().GetSeq_set().begin(); i != iend; ++i) {
                if (SeqCount == seqIndex) {
                    seqEntry = (*i);
                    return true;
                }
                SeqCount++;
            }
        }
    }
    seqEntry.Reset();
    return false;

}

/* ADDED */
//  get the bioseq for the designated sequence index
bool CCdCore::GetBioseqForIndex(int seqIndex, CRef< CBioseq >& bioseq) {
    list< CRef< CSeq_entry > >::iterator i, iend;

    int SeqCount = 0;
    if (seqIndex >= 0 && seqIndex < GetNumSequences() && IsSetSequences()) {
        if (SetSequences().IsSet()) {
            iend = SetSequences().SetSet().SetSeq_set().end();
            for (i =  SetSequences().SetSet().SetSeq_set().begin(); i != iend; ++i) {
                if (SeqCount == seqIndex && (*i)->IsSeq()) {
                    bioseq.Reset(&(*i)->SetSeq());
                    return true;
                }
                SeqCount++;
            }
        }
    }
    bioseq.Reset();
    return false;
}


/* ADDED */
// get the sequence for specified sequence index
string CCdCore::GetSequenceStringByIndex(int seqIndex) {
    string s = kEmptyStr;
    CRef< CBioseq > bioseq;
    if (GetBioseqForIndex(seqIndex, bioseq) && GetNcbieaaString(*bioseq, s)) {
        return s;
    }
    return s;
}


/* =========================================== */
/*  Examine alignment for a SeqId or footprint */
/* =========================================== */

bool CCdCore::HasSeqId(const CRef< CSeq_id >& ID) const {
//-------------------------------------------------------------------------
// look through each row of the alignment & pending list for a matching ID
//-------------------------------------------------------------------------
  int  Dummy;
  return(HasSeqId(ID, Dummy));
}


bool CCdCore::HasSeqId(const CRef< CSeq_id >& ID, int& RowIndex) const {
//-------------------------------------------------------------------------------
// look through each row of the alignment for a matching ID
// also look through each row of the pending list for a matching ID -- dih & vvs
//-------------------------------------------------------------------------------
  int  k, Pair, DenDiagRow, NumRows=GetNumRows();
  CRef< CSeq_id >  TestID;

  for (k=0; k<NumRows; k++) {
    Pair = (k <= 1) ? 0 : k-1;
    DenDiagRow = (k == 0) ? 0 : 1;
    GetSeqIDForRow(Pair, DenDiagRow, TestID);
    if (SeqIdsMatch(ID, TestID)) {
      RowIndex = k;
      return(true);
    }
  }

  k = 0;
  list <CRef <CUpdate_align> > ::const_iterator pPen;
  for(pPen=GetPending().begin();pPen!=GetPending().end();pPen++){
    const CSeq_align * pAl = *((*pPen)->GetSeqannot().GetData().GetAlign().begin());
    const CDense_diag * pDDPen=*(pAl->GetSegs().GetDendiag().begin());
    vector < CRef< CSeq_id > >::const_iterator pid=pDDPen->GetIds().begin();
    TestID=*(++pid);
    if (SeqIdsMatch(ID, TestID)) {
      RowIndex = k;
      return(true);
    }
    k++;
  }
  return(false);
}


int CCdCore::GetNumMatches(const CRef< CSeq_id >& ID) const {
//-------------------------------------------------------------------------
// count the number of rows that have a matching ID
//-------------------------------------------------------------------------
  int  k, Count, NumRows=GetNumRows();
  CRef< CSeq_id > TestID;

  Count = 0;
  for (k=0; k<NumRows; k++) {
    GetSeqIDFromAlignment(k, TestID);
    if (SeqIdsMatch(ID, TestID)) {
      Count++;
    }
  }
  return(Count);
}

/* ====================================== */
/*  SeqID getters ... from alignment info */
/* ====================================== */


bool CCdCore::GetSeqIDForRow(int Pair, int DenDiagRow, CRef< CSeq_id >& SeqID) const {
//-------------------------------------------------------------------------
// get a SeqID.
// first get the Pair'th DenDiag, then the DenDiagRow'th SeqID.
//-------------------------------------------------------------------------
  CRef< CDense_diag > DenDiag;
  CDense_diag::TIds IdsSet;
  CDense_diag::TIds::iterator i;
  int  Row;

  Row = (Pair == 0) ? DenDiagRow : Pair+1;
//  GetDenDiag(Row, true, DenDiag);
  const CRef< CSeq_align >& seqAlign = GetSeqAlign(Row);
  if (seqAlign.NotEmpty() && GetFirstOrLastDenDiag(seqAlign, true, DenDiag)) {
    IdsSet = DenDiag->GetIds();
    // for Row=0, get the first id, otherwise get the 2nd id
    i = IdsSet.begin();
    if (DenDiagRow != 0) {
        i++;
    }
    SeqID = (*i);
    return(SeqID.NotEmpty());
  }
  return(false);
}


//  Returns false if an empty SeqID is found
bool   CCdCore::GetSeqIDFromAlignment(int RowIndex, CRef<CSeq_id>& SeqID) const { // get SeqID from alignment
    if (RowIndex < 0) {
        return false;
    }
    int  Pair = (RowIndex <= 1) ? 0 : RowIndex-1;
    int  DenDiagRow = (RowIndex == 0) ? 0 : 1;
    return(GetSeqIDForRow(Pair, DenDiagRow, SeqID));
}


/* ====================================== */
/*  SeqID getters ... from sequence list  */
/* ====================================== */

bool CCdCore::GetSeqIDForIndex(int SeqIndex, CRef< CSeq_id >& SeqID) const {
//-------------------------------------------------------------------------
// get a SeqID from a list of sequences.
// each sequence can have multiple id's.
// if there's a pdb-id, return it.
// otherwise, if there's a gi, return it.
// otherwise, return false.
// return false if the SeqID is empty
//-------------------------------------------------------------------------
  list< CRef< CSeq_entry > >::const_iterator  i;
  list< CRef< CSeq_id > >::const_iterator  j;
  int  SeqCount, IDCount, NumIDs;

  if (IsSetSequences()) {
    if (GetSequences().IsSet()) {
      // count to the SeqIndex sequence
      SeqCount = 0;
      for (i=GetSequences().GetSet().GetSeq_set().begin();
           i!=GetSequences().GetSet().GetSeq_set().end(); i++) {
        if (SeqCount == SeqIndex) {
          if ((*i)->IsSeq()) {
            // look through the IDs for a PDB id
            NumIDs = (*i)->GetSeq().GetId().size();
            IDCount = 0;
            for (j = (*i)->GetSeq().GetId().begin();
                 j != (*i)->GetSeq().GetId().end(); j++) {
              if ((*j)->IsPdb()) {
                SeqID = (*j);
                return(SeqID.NotEmpty());
//                return(true);
              }
              IDCount++;
            }
            // look through IDs again for a gi
            IDCount = 0;
            for (j = (*i)->GetSeq().GetId().begin();
                 j != (*i)->GetSeq().GetId().end(); j++) {
              if ((*j)->IsGi()) {
                SeqID = (*j);
                return(SeqID.NotEmpty());
//                return(true);
              }
              IDCount++;
            }
            // look through IDs again for a Other
            IDCount = 0;
            for (j = (*i)->GetSeq().GetId().begin();
                 j != (*i)->GetSeq().GetId().end(); j++) {
              if ((*j)->IsOther()) {
                SeqID = (*j);
                return(SeqID.NotEmpty());
//                return(true);
              }
              IDCount++;
            }
          }
          return(false);
        }
        SeqCount++;
      }
    }
  }
  return(false);
}


bool CCdCore::GetSeqIDs(int SeqIndex, list< CRef< CSeq_id > >& SeqIDs) {
//-------------------------------------------------------------------------
// get the list of SeqIDs for a sequence
//-------------------------------------------------------------------------
  list< CRef< CSeq_entry > >::const_iterator  i;
  int  SeqCount;

  if (IsSetSequences()) {
    if (GetSequences().IsSet()) {
      // count to the SeqIndex sequence
      SeqCount = 0;
      for (i=GetSequences().GetSet().GetSeq_set().begin();
           i!=GetSequences().GetSet().GetSeq_set().end(); i++) {
        if (SeqCount == SeqIndex) {
          if ((*i)->IsSeq()) {
            // return its set of ids
            SeqIDs = (*i)->GetSeq().GetId();
            return(true);
          }
        }
        SeqCount++;
        if (SeqCount > SeqIndex) break;
      }
    }
  }
  return(false);
}

//  Assumes proper index is in range and CD in proper format for retrieval; no checks.
const list< CRef< CSeq_id > >& CCdCore::GetSeqIDs(int SeqIndex) const{
    list< CRef< CSeq_entry > >::const_iterator  i=GetSequences().GetSet().GetSeq_set().begin();
    int  SeqCount = 0;
    
    while (i != GetSequences().GetSet().GetSeq_set().end()) {
        if (SeqCount == SeqIndex) {
            break;
        }
        ++SeqCount;
        ++i;
    }
    // return its set of ids
    return (*i)->GetSeq().GetId();
}

/* ============= */
/*  Row removal  */
/* ============= */


int intSortRowsFunction(void * pVal,int i, int j)
{
	vector<int> * iVal=(vector<int> *)pVal;
	if ((*iVal)[i]>(*iVal)[j])return 1;
	else if ((*iVal)[i]<(*iVal)[j])return -1;
	else return 0;
}


bool CCdCore::EraseTheseRows(const std::vector<int>& TossRows) {
//-------------------------------------------------------------------------
// erase rows from the alignment.
// EraseRows is a list of rows that are deleted from this CD.
// EraseRows won't contain the master (0 index) row.
//-------------------------------------------------------------------------
  int  i;

  int Count = 0;
  int Test = TossRows.size();

  CRef< CSeq_annot > alignment;
  if (!GetAlignment(alignment)) {
      return false;
  }

  int * ind=new int[3*TossRows.size()];
  algSortQuickCallbackIndex((void * )&TossRows,TossRows.size(),ind+TossRows.size(),ind,intSortRowsFunction);
  /*
  for(int i=0;i<TossRows.size(); i++){
	  int iii=TossRows[ind[i]];
	  iii++;
  }*/
  
  for (i=TossRows.size()-1; i>=0; i--) {
    if (TossRows[ind[i]] == 0) return(false);  // return false if master row is to be deleted
    if (!EraseRow(alignment, TossRows[ind[i]])) return(false);  // return false if problem deleting a row
    Count++;
  }

  delete ind;

  return(true);
}


bool CCdCore::EraseOtherRows(const std::vector<int>& KeepRows) {
//-------------------------------------------------------------------------
// erase rows from the alignment.
// KeepRows is a list of rows that are NOT deleted from this CD.
// KeepRows won't contain the master (0 index) row.
// return of true means successful completion.
//-------------------------------------------------------------------------
//  list< CRef< CSeq_annot > >::iterator i;
    int   j, k, NumRows;
    bool  FoundIt;

    CRef< CSeq_annot > alignment;
    if (!GetAlignment(alignment)) {
        return false;
    }

    NumRows = alignment->SetData().SetAlign().size() + 1;
    for (j=NumRows-1; j>0; j--) {
      // see if row is in KeepRows
        FoundIt = false;
        for (k=0; k<KeepRows.size(); k++) {
            if (KeepRows[k] == j) {
                FoundIt = true;
            break;
            }
        }
        // if row is not in KeepRows, then erase it
        if (!FoundIt) {
            if (!EraseRow(alignment, j)) {
                return(false);
            }
        }
    }
//  }
    return(true);
}


void CCdCore::EraseSequences() {
//-------------------------------------------------------------------------
// erase sequences not in alignment
//-------------------------------------------------------------------------
  int  i, NumSequences;
  CRef< CSeq_id >  ID;

  NumSequences = GetNumSequences();
  for (i=NumSequences-1; i>=0; i--) {
    GetSeqIDForIndex(i, ID);
    if (!HasSeqId(ID)) {
      EraseSequence(i);
    }
  }
  // testing
  NumSequences = GetNumSequences();
}


void CCdCore::EraseSequence(int SeqIndex) {
//-------------------------------------------------------------------------
// erase a sequence from the set of sequences
//-------------------------------------------------------------------------
  list< CRef< CSeq_entry > >::iterator  i;
  int  SeqCount;

  if (IsSetSequences()) {
    if (GetSequences().IsSet()) {
      SeqCount = 0;
      for (i=SetSequences().SetSet().SetSeq_set().begin();
           i!=SetSequences().SetSet().SetSeq_set().end(); i++) {
        if (SeqCount == SeqIndex) {
          SetSequences().SetSet().SetSeq_set().erase(i);
          return;
        }
        SeqCount++;
        if (SeqCount > SeqIndex) break;
      }
    }
  }
}

/* ================================================================ */
/*  Methods for adding alignment or sequence to CD */
/* ================================================================ */

bool CCdCore::AddSeqAlign(CRef< CSeq_align > seqAlign)
{
	CRef< CSeq_align > sa(new CSeq_align());
	sa->Assign(*seqAlign);
	(*(SetSeqannot().begin()))->SetData().SetAlign().push_back(sa);
	return true;
}

bool CCdCore::AddPendingSeqAlign(CRef< CSeq_align > seqAlign)
{
	CRef< CSeq_align > sa(new CSeq_align());
	sa->Assign(*seqAlign);
	CRef< CUpdate_align > newPend ( new CUpdate_align);
    CRef < CUpdate_comment > com (new CUpdate_comment); 
   
    newPend->SetSeqannot().SetData().SetAlign().push_back(sa); // copy the alignment to new pending alignmnet 
    com->SetComment ("Sequence aligns to the CD partially.");
    newPend->SetDescription().push_back(com);
    newPend->SetType(CUpdate_align::eType_other);
    SetPending().push_back(newPend);
	return true;
}

void CCdCore::ErasePendingRows(set<int>& rows)
{
	for (set<int>::reverse_iterator sit = rows.rbegin(); sit != rows.rend(); sit++)
	{
		ErasePendingRow(*sit);
	}
	EraseSequences();
}

void CCdCore::ErasePendingRow(int row)
{
	list< CRef< CUpdate_align > >& pendingList = SetPending();
	list< CRef< CUpdate_align > >::iterator lit = pendingList.begin();
	int order = 0;
	for(; lit != pendingList.end(); ++lit)
	{
		if (order== row)
		{
			pendingList.erase(lit);
			break;
		}
		else
			order++;
	}
}

bool CCdCore::AddSequence(CRef< CSeq_entry > srcSeq)
{
	CRef< CSeq_entry > newSeq(new CSeq_entry());
	newSeq->Assign(*srcSeq);
	SetSequences().SetSet().SetSeq_set().push_back(newSeq);
	return true;
}
void CCdCore::Clear()
{
	(*(SetSeqannot().begin()))->SetData().SetAlign().clear();
	SetPending().clear();
	SetSequences().SetSet().SetSeq_set().clear();
}
/* ================================================================ */
/*  Methods for structures, structure alignments, MMDB identifiers  */
/* ================================================================ */



bool CCdCore::Has3DMaster() const {
//-------------------------------------------------------------------------
// confirm if this CD has a structure as its master
// this must be true for all Seq_aligns in the Cdd object
//-------------------------------------------------------------------------

	bool result = true;
	bool tmp_result = false;
	CRef< CSeq_align > salist;
	TDendiag ddlist;
	list< CRef< CSeq_annot > >::const_iterator sanci;

	if (IsSetSeqannot()) {
		for (sanci = GetSeqannot().begin(); sanci != GetSeqannot().end(); ++sanci) {
			tmp_result = false;
			if ((*sanci)->GetData().IsAlign()) {
				salist = (*sanci)->GetData().GetAlign().front();
				if (salist->GetSegs().IsDendiag()) {
					ddlist = salist->GetSegs().GetDendiag();
					if (ddlist.front()->GetIds().front()->IsPdb()) {
						tmp_result = true;
					}
				}
			}
			result = result & tmp_result;
		}
	}
    return result;
}

int CCdCore::Num3DAlignments() const {
//-------------------------------------------------------------------------
//  return the number of structure-related alignments in a CD,
//  ignoring the alignment to a consensus sequence if present.
//-------------------------------------------------------------------------

	int count = 0;

	TDendiag ddlist;
	list< CRef< CSeq_annot > >::const_iterator sanci;
	list< CRef< CSeq_align > >::const_iterator saci;

	bool usesConsensus = UsesConsensusSequenceAsMaster();
	bool structMaster  = Has3DMaster();

	if (!usesConsensus && !structMaster) {
		return count;
	}
	if (IsSetSeqannot()) {
		for (sanci = GetSeqannot().begin(); sanci != GetSeqannot().end(); ++sanci) {
			if ((*sanci)->GetData().IsAlign()) {
				for (saci = (*sanci)->GetData().GetAlign().begin(); \
					saci != (*sanci)->GetData().GetAlign().end(); ++saci) {
					if ((*saci)->GetSegs().IsDendiag()) {
						ddlist=(*saci)->GetSegs().GetDendiag();
						if (ddlist.front()->GetIds().back()->IsPdb()) {
							++count;
						}
					}
				}
			}
		}
	}
	if (count > 0 && usesConsensus) {
		--count;
	}

	return count;
}


bool CCdCore::Has3DSuperpos(list<int>& MMDBId_vec) const {
//-------------------------------------------------------------------------
//  confirm that all bioseqs with MMDB ids have a structural alignment
//-------------------------------------------------------------------------
	int nstruct = 0;
	int mmdbid;

	int featBiostrucId;
	//CChem_graph_alignment* cgap;
	list< CRef< CBiostruc_feature > >     biostrucFeatList;
	list< CRef< CBiostruc_feature_set > > biostrucFeatSetList;
	list< CRef< CBiostruc_id > >::const_iterator bidit;
	list<int>::iterator iit;

	list< CRef< CBiostruc_feature > >::const_iterator bfcit;
	list< CRef< CBiostruc_feature_set > >::const_iterator bfscit;
	list< CRef< CSeq_annot > >::const_iterator sannotcit;
	list< CRef< CSeq_align > >::const_iterator saligncit;
	CRef< CDense_diag > dd;
	CRef< CSeq_id > sid1, sid2;
	const CBioseq* bs = NULL;
	const CBioseq_set* bsSet = NULL;

	if (!IsSetFeatures()) {
		return false;
	}
	biostrucFeatSetList = GetFeatures().GetFeatures();

	if (IsSetSequences()) {
		if (GetSequences().IsSet()) {
			bsSet = &(GetSequences().GetSet());
		}
	}
	if (!bsSet) {
		return false;
	}

	MMDBId_vec.clear();
	if (IsSetSeqannot()) {
		for (sannotcit=GetSeqannot().begin(); sannotcit != GetSeqannot().end(); ++sannotcit) {
			if ((*sannotcit)->GetData().IsAlign()) {
				for (saligncit = (*sannotcit)->GetData().GetAlign().begin(); \
					saligncit != (*sannotcit)->GetData().GetAlign().end(); ++saligncit) {
                    if ((*saligncit)->GetSegs().IsDendiag()) {
                        dd = (*saligncit)->GetSegs().GetDendiag().front();
                        bs = NULL;
                        sid1 = dd->GetIds().front();
                        sid2 = dd->GetIds().back();
                        if (nstruct == 0 && sid1->IsPdb()) {
                            if (GetBioseqWithSeqid(*sid1, (*bsSet).GetSeq_set(), bs)) {
                                mmdbid = GetMMDBId(*bs);
                                MMDBId_vec.push_back(mmdbid);
                                nstruct++;
                            }
                        }
                        if (sid2->IsPdb()) {
                            if (GetBioseqWithSeqid(*sid2, (*bsSet).GetSeq_set(), bs)) {
                                mmdbid = GetMMDBId(*bs);
                                MMDBId_vec.push_back(mmdbid);
                                nstruct++;
                            }
                        }
                    }
				}
			}
		}
	}  //  end if (IsSetSeqannot())

	//check features now ... mark any mmdb_id found.  if any leftover --> error.
	//if an MMDBId is in the list more than once the extra copies are also removed.

	for (bfscit = biostrucFeatSetList.begin(); bfscit != biostrucFeatSetList.end(); ++bfscit) {
		biostrucFeatList = (*bfscit)->GetFeatures();
		for (bfcit = biostrucFeatList.begin(); bfcit != biostrucFeatList.end(); ++bfcit) {
			if ((*bfcit)->IsSetLocation() && (*bfcit)->GetLocation().IsAlignment()) {
				const CChem_graph_alignment& cgap = (*bfcit)->GetLocation().GetAlignment();
				for (bidit = cgap.GetBiostruc_ids().begin(); bidit != cgap.GetBiostruc_ids().end(); ++bidit) {
					if ((*bidit)->IsMmdb_id()) {
						featBiostrucId = (*bidit)->GetMmdb_id().Get();
						//  don't remove the master MMDB_id
						iit = MMDBId_vec.begin();
						while(iit != MMDBId_vec.end()) {
							if (*iit == featBiostrucId && iit != MMDBId_vec.begin()) {
								*iit = 0;
							}
							++iit;
						}
					}
				}
			}
		}
	}
	MMDBId_vec.remove(0);
	if (MMDBId_vec.size() > 1) {
		return false;
	} else {
		return true;
	}

	return true;
}

bool CCdCore::GetRowsForMmdbId(int mmdbId, list<int>& rows) const {

	int rowMmdbId= -1;
	int seqIndex = -1;

	rows.clear();
	if (mmdbId < 0) {
		return false;
	}

	for (int rowIndex=0; rowIndex<GetNumRows(); rowIndex++) {
		rowMmdbId = -1;
        seqIndex = GetSeqIndexForRowIndex(rowIndex);
        if (seqIndex > 0) {
			if (GetMmdbId(seqIndex, rowMmdbId) && (rowMmdbId == mmdbId)) {
				rows.push_back(rowIndex);
			}
		}
	}
	if (rows.size() > 0) {
		return true;
	}
	return false;
}

bool CCdCore::GetRowsWithMmdbId(vector<int>& rows) const {

	int rowMmdbId= -1;
	int seqIndex = -1;

	//rows.clear();
	for (int rowIndex=0; rowIndex<GetNumRows(); rowIndex++) {
		rowMmdbId = -1;
        seqIndex = GetSeqIndexForRowIndex(rowIndex);
        if (seqIndex >= 0) {
			if (GetMmdbId(seqIndex, rowMmdbId)) {
				rows.push_back(rowIndex);
			}
		}
	}
	if (rows.size() > 0) {
		return true;
	}
	return false;
}

int	CCdCore::GetStructuralRowsWithEvidence(vector<int>& rows)
{
	set<int> mmdbIds;
	GetMmdbIdWithEvidence(mmdbIds);
	for (set<int>::iterator sit = mmdbIds.begin(); sit != mmdbIds.end(); sit++)
	{
		list<int> mRows;
		GetRowsForMmdbId(*sit, mRows);
		for (list<int>::iterator lit = mRows.begin();
			lit != mRows.end(); lit++)
		{
			rows.push_back(*lit);
		}
	}
	return rows.size();
}

bool CCdCore::GetMmdbId(int SeqIndex, int& id) const{
//-------------------------------------------------------------------------
// get mmdb-id from sequence list
//-------------------------------------------------------------------------
  list< CRef< CSeq_entry > >::const_iterator  i;

  int  SeqCount;

  if (SeqIndex < 0) {
	  return false;
  }

  if (IsSetSequences()) {
    if (GetSequences().IsSet()) {
      SeqCount = 0;
      // look through each sequence in set for SeqIndex sequence
      for (i=GetSequences().GetSet().GetSeq_set().begin();
           i!=GetSequences().GetSet().GetSeq_set().end(); i++) {
        if (SeqCount == SeqIndex) {
          if ((*i)->IsSeq()) {
            id = GetMMDBId((*i)->GetSeq());  //  library call
            if (id > 0) {
              return(true);
            }
          }
        }
        SeqCount++;
        if (SeqCount > SeqIndex) break;
      }
    }
  }
  return(false);
}

/* ====================== */
/*  CD alignment methods  */
/* ====================== */




//  Return the first seqAnnot that is of type 'align'
const CRef< CSeq_annot >& CCdCore::GetAlignment() const {
	list< CRef< CSeq_annot > >::const_iterator sancit;
    if (IsSetSeqannot()) {
        for (sancit = GetSeqannot().begin(); sancit != GetSeqannot().end(); ++sancit) {
            if ((*sancit)->GetData().IsAlign()) {
                return *sancit;
            }
        }
    }
    return EMPTY_CREF_SEQANNOT;
}

//  Return the first seqAnnot that is of type 'align'
bool CCdCore::GetAlignment(CRef< CSeq_annot >& seqAnnot) {
	list< CRef< CSeq_annot > >::iterator sanit;
    int count = 0;

    seqAnnot = null;
    if (IsSetSeqannot()) {
        for (sanit = SetSeqannot().begin(); count == 0 && sanit != SetSeqannot().end(); ++sanit) {
            if ((*sanit)->SetData().IsAlign()) {
                ++count;
                seqAnnot = (*sanit);
            }
        }
    }
    return (count == 1);
}


bool CCdCore::IsSeqAligns() const {
//-------------------------------------------------------------------------
// check if there are seq-aligns
//-------------------------------------------------------------------------
  list< CRef< CSeq_annot > >::const_iterator i;

  if (IsSetSeqannot()) {
    i = GetSeqannot().begin();
    if ((*i)->GetData().IsAlign()) {
      return(true);
    }
  }
  return(false);
}



const list< CRef< CSeq_align > >& CCdCore::GetSeqAligns() const {
//-------------------------------------------------------------------------
// get the seq-aligns.  Must know they're present (call IsSeqAligns() first)
// Assumes the first seq_annot is the alignment.
//-------------------------------------------------------------------------
  list< CRef< CSeq_annot > >::const_iterator i;

  i = GetSeqannot().begin();
  return((*i)->GetData().GetAlign());
}


/*  ADDED  */
// get the list of Seq-aligns for editing
list< CRef< CSeq_align > >& CCdCore::GetSeqAligns() {
//-------------------------------------------------------------------------
// get the seq-aligns.  Must know they're present (call IsSeqAligns() first)
// Assumes the first seq_annot is the alignment.
//-------------------------------------------------------------------------
    return SetSeqannot().front()->SetData().SetAlign();
}

/*  ADDED  */
// get the Row-th Seq-align
bool CCdCore::GetSeqAlign(int Row, CRef< CSeq_align >& seqAlign) {

  list< CRef< CSeq_align > >::iterator j;

  if (IsSeqAligns() && Row >= 0) {
      list< CRef< CSeq_align > > lsa = GetSeqAligns();
      // figure out which seq-align to get (based on Row)
      if (Row == 0) {
          seqAlign = lsa.front();
          return true;
      } else {
          int Count = 0;
          for (j = lsa.begin(); j != lsa.end(); j++) {
              if (++Count == Row) {
                  seqAlign = *j;
                  return true;
              }
          }
      }
  }
  return false;
}

// get the Row-th Seq-align
// Burden is placed on caller to ensure Row is not too large.
const CRef< CSeq_align >& CCdCore::GetSeqAlign(int Row) const {
    
    int Count = 0;
    
    if (IsSeqAligns() && Row >= 0) {
        if (Row == 0) {
            return (GetSeqAligns().front());
        } else {
            list< CRef< CSeq_align > >::const_iterator j, jend = GetSeqAligns().end();
            for (j = GetSeqAligns().begin(); j != jend; j++) {
                if (++Count == Row) {
                    return (*j);             
                }
            }
        }
        
    }
    return EMPTY_CREF_SEQALIGN;
}


/*  ADDED  */
//  Returns coordinate on 'otherRow' that is mapped to 'thisPos' on 'thisRow'.
//  Returns INVALID_MAPPED_POSITION on failure.
int    CCdCore::MapPositionToOtherRow(int thisRow, int thisPos, int otherRow) const {

    int masterPos, otherPos = INVALID_POSITION;
    if (thisPos < 0 || thisRow < 0 || otherRow < 0) {
        return otherPos;
    } else if (thisRow == otherRow) {
        return thisPos;
    }

    if (thisRow == 0) {  // direct master->child mapping
        const CRef< CSeq_align >& seqalign = GetSeqAlign(otherRow);
        if (seqalign.NotEmpty()) {
            otherPos = MapPositionToChild(thisPos, *seqalign);
        }
    } else {
        const CRef< CSeq_align >& seqalign = GetSeqAlign(thisRow);
        if (seqalign.NotEmpty()) {
            masterPos = MapPositionToMaster(thisPos, *seqalign);
            if (otherRow != 0) {   //  child->child mapping
                const CRef< CSeq_align >& otherSeqalign = GetSeqAlign(otherRow);
                if (seqalign.NotEmpty()) {
                    otherPos = MapPositionToChild(masterPos, *otherSeqalign);
                }
            } else {               //  child->master mapping
                otherPos = masterPos;
            }
        }
    }
    return otherPos;
}

/*  RENAMING  */  //  (was GetSeqPosition)
//  mapDir controls direction of mapping  
int    CCdCore::MapPositionToOtherRow(const CRef< CSeq_align >& seqAlign, int thisPos, CoordMapDir mapDir) const {
    int otherPos = INVALID_POSITION;
    if (thisPos >= 0) {
        otherPos = (mapDir == CHILD_TO_MASTER) ? MapPositionToMaster(thisPos, *seqAlign)
                                               : MapPositionToChild(thisPos, *seqAlign);
    }
    return otherPos;
}


bool CCdCore::HasConsensusSequence() const {
//-------------------------------------------------------------------------
// check if this CD has a consensus in the alignment or sequence list
//-------------------------------------------------------------------------
  bool result = false;
  int nrows = GetNumRows();

  CRef< CSeq_id >  SeqID;
  
  for (int i = 0; i < nrows; ++i) {
      if (GetSeqIDFromAlignment(i, SeqID)) {
          if (IsConsensus(SeqID)) {
              result = true;
              break;
          }
      }
  }

  //  If no consensus in the alignment, check the sequence list.
  if (!result) {
      result = FindConsensusInSequenceList();
  }

  return result;
}


bool CCdCore::FindConsensusInSequenceList(vector<int>* indices) const {
    bool result = false;
    int nseqs = GetNumSequences();

  //  Make sure a consensus is not lurking in the sequence list...
  for (int i = 0; i < nseqs; ++i) {
      const list< CRef< CSeq_id > >& ids = GetSeqIDs(i);
      for (list<CRef< CSeq_id > >::const_iterator lit = ids.begin(); lit != ids.end(); ++lit) {
          if (IsConsensus(*lit)) {
              result = true;
              if (indices == NULL) {
                  return result;
              }
              indices->push_back(i);
              break;
          }
      }
  }

  return(result);
}

bool CCdCore::UsesConsensusSequenceAsMaster() const {
//-------------------------------------------------------------------------
// check if this CD uses consensus sequence for master
//-------------------------------------------------------------------------
  CRef< CSeq_id >  SeqID;

  if (GetSeqIDFromAlignment(0, SeqID)) {
      if (IsConsensus(SeqID)) {
        return(true);
      }
  }
  return(false);
}

int  CCdCore::GetRowsWithConsensus(vector<int>& consensusRows) const {
//-------------------------------------------------------------------------
// look for rows where the seq_id refers to a consensus sequence
//-------------------------------------------------------------------------

    int nrows = GetNumRows();
    CRef< CSeq_id >  SeqID;
    
    consensusRows.clear();
    for (int i = 0; i < nrows; ++i) {    
        if (GetSeqIDFromAlignment(i, SeqID)) {
            if (IsConsensus(SeqID)) {
                consensusRows.push_back(i);
            }
        }
    }
    return consensusRows.size();
}


bool CCdCore::IsInPendingList(const CRef< CSeq_id >& ID, vector<int>& listIndex) const {
//-------------------------------------------------------------------------
// look through each row of the pending list for a matching ID; return list Index if found
//-------------------------------------------------------------------------
    int  foundIndex = 0;
    list< CRef< CUpdate_align > >::const_iterator luaci;
    list< CRef< CSeq_align> >::const_iterator lsaci;
    vector< CRef< CSeq_id> >::const_iterator vsici;

    listIndex.clear();
    if ((IsSetPending() && GetPending().size() == 0) || !IsSetPending()) {
        return false;
    }

    for (luaci = GetPending().begin(); luaci != GetPending().end(); ++luaci) {
        if ((*luaci)->IsSetSeqannot()) {
            if ((*luaci)->GetSeqannot().GetData().IsAlign()) {
                for (lsaci = (*luaci)->GetSeqannot().GetData().GetAlign().begin();
                     lsaci!= (*luaci)->GetSeqannot().GetData().GetAlign().end(); ++lsaci) {
                         if ((*lsaci)->GetSegs().Which() == CSeq_align::C_Segs::e_Dendiag) {
                            const CRef< CDense_diag> denDiag = (*lsaci)->GetSegs().GetDendiag().front();
                            for (vsici = denDiag->GetIds().begin(); vsici != denDiag->GetIds().end(); ++vsici) {
                                if (ID->Match(**vsici)) {
                                    listIndex.push_back(foundIndex);
//                                    return true;
                                }
                            }
                         }
                     }
            }
        }
        ++foundIndex;
    }
    return (listIndex.size() != 0);
}

void CCdCore::SetComment(CCdd_descr::TComment oldComment, CCdd_descr::TComment newComment) {
//-------------------------------------------------------------------------
// set comment of CD
//-------------------------------------------------------------------------
  CCdd_descr_set::Tdata::iterator i;

  if (IsSetDescription()) {
    // if comment is set, reset it
    for (i=SetDescription().Set().begin(); i!=SetDescription().Set().end(); i++) {
      if ((*i)->IsComment() && (*i)->GetComment() == oldComment) {
        (*i)->SetComment(newComment);
        return;
      }
    }
    // otherwise add another description with comment
    CRef < CCdd_descr > Comment(new CCdd_descr());
    Comment->SetComment(newComment);
    SetDescription().Set().push_back(Comment);
  }
}

/* ================== */
/*  Old root methods  */
/* ================== */

bool CCdCore::IsOldRoot() {
//-------------------------------------------------------------------------
// indicate if old-root field is populated
//-------------------------------------------------------------------------
  CCdd_descr_set::Tdata::const_iterator i;

  if (IsSetDescription()) {
    for (i=GetDescription().Get().begin(); i!=GetDescription().Get().end(); i++) {
      if ((*i)->IsOld_root()) {
        return(true);
      }
    }
  }
  return(false);
}


void CCdCore::SetOldRoot(string Accession, int Version) {
//-------------------------------------------------------------------------
// set accession and version of old-root
//-------------------------------------------------------------------------
  CCdd_descr_set::Tdata::iterator i;

  // make a new old-root
  CRef< CCdd_id > ID(new CCdd_id);
  CRef< CGlobal_id > GID(new CGlobal_id);
  GID->SetAccession(Accession);
  GID->SetVersion(Version);
  ID->SetGid(*GID);

  // look through the descriptions
  if (IsSetDescription()) {
    // if there is an old-root
    for (i=SetDescription().Set().begin(); i!=SetDescription().Set().end(); i++) {
      if ((*i)->IsOld_root()) {
        // reset it, and set the new one
        (*i)->SetOld_root().Reset();
        (*i)->SetOld_root().Set().push_back(ID);
        return;
      }
    }
    // otherwise add another description, this one with a new old-root
    CRef < CCdd_descr > Description(new CCdd_descr);
    CRef < CCdd_id_set> SetOfIds(new CCdd_id_set);
    SetOfIds->Set().push_back(ID);
    Description->SetOld_root(*SetOfIds);
    SetDescription().Set().push_back(Description);
  }
}

bool CCdCore::GetOldRoot(int Index, string& Accession, int& Version) {
//-------------------------------------------------------------------------
// get accession and version of Index-th old-root
//-------------------------------------------------------------------------
  CCdd_descr_set::Tdata::const_iterator i;
  CCdd_id_set::Tdata  SetOfIds;
  CCdd_id_set::Tdata::const_iterator j;
  int IdIndex=0;

  // look through the descriptions
  if (IsSetDescription()) {
    // if there is an old-root
    for (i=GetDescription().Get().begin(); i!=GetDescription().Get().end(); i++) {
      if ((*i)->IsOld_root()) {
        SetOfIds = (*i)->GetOld_root().Get();
        for (j=SetOfIds.begin(); j!=SetOfIds.end(); j++) {
          if (IdIndex == Index) {
            Accession = (*j)->GetGid().GetAccession();
            Version = (*j)->GetGid().GetVersion();
            return(true);
          }
        }
      }
    }
  }
  return(false);
}

int CCdCore::GetNumIdsInOldRoot() {
//-------------------------------------------------------------------------
// get size of list of ids
//-------------------------------------------------------------------------
  CCdd_descr_set::Tdata::const_iterator i;
  CCdd_id_set::Tdata  SetOfIds;

  // look through the descriptions
  if (IsSetDescription()) {
    // if there is an old-root
    for (i=GetDescription().Get().begin(); i!=GetDescription().Get().end(); i++) {
      if ((*i)->IsOld_root()) {
        return((*i)->GetOld_root().Get().size());
      }
    }
  }
  return(0);
}

/* ========================================== */
/*  Alignment & structure annotation methods  */
/* ========================================== */

bool CCdCore::AllResiduesInRangeAligned(int rowId, int from, int to) const {

    int  i = 0, tmp, nBlocks, nextStart;
    bool toFound = true, onMaster = (rowId == 0) ? true : false;
    vector<int> blockStarts, blockLen;

    if (from > to) {
        tmp  = to;
        to   = from;
        from = tmp;
    }

    const CRef< CSeq_align >& seqAlign = GetSeqAlign(rowId);
    i = GetBlockNumberForResidue(from, seqAlign, onMaster, &blockStarts, &blockLen);
    if (i >= 0) {
        nBlocks = blockStarts.size();
        while (i < nBlocks && toFound) {
            if (to >= blockStarts[i] + blockLen[i]) {  // 'to' beyond block i
                nextStart = (i == nBlocks - 1) ? 1000000000 : blockStarts[i+1];
                if (to < nextStart) {  // 'to' is in between blocks
                    toFound = false;
                } else if (nextStart != blockStarts[i] + blockLen[i]) {  // non-adjacent blocks
                    toFound = false;
                }
                ++i;
            } else {
                i = nBlocks;
            }
        }
    }

    return (i >= 0 && toFound);
}

bool CCdCore::AlignAnnotsValid(string* err) const{
//-------------------------------------------------------------------------
// check if the alignannot's are covered by aligned blocks
//-------------------------------------------------------------------------
    bool result = true;
    int intNumber;
  int  From, To, NewFrom, NewTo;

//  const TDendiag*  pDenDiagSet;
  list< CRef< CAlign_annot > >::const_iterator  m;
  list< CRef< CSeq_interval > >::const_iterator  n;

  if (err) {
      err->erase();
  }

  // if there's an align-annot set
  const CRef< CSeq_align >& masterSeqAlign = GetSeqAlign(0);
  if (masterSeqAlign.NotEmpty() && IsSetAlignannot()) {

    // for each alignannot
    for (m=GetAlignannot().Get().begin(); m!=GetAlignannot().Get().end(); m++) {
      // if it's a from-to
      if ((*m)->GetLocation().IsInt()) {
        // All coordinates of alignannots are given for the master.
        // If the end coordinates do not map to valid blocks in the
        // slave/child row, there is a problem.
        From = (*m)->GetLocation().GetInt().GetFrom();
        To = (*m)->GetLocation().GetInt().GetTo();

        //  The annotation entry should be confined to a block,
        //  or if it spans blocks, there can be no unaligned residues
        //  between the blocks.
        if (AllResiduesInRangeAligned(0, From, To)) {
            NewFrom = MapPositionToOtherRow(masterSeqAlign, From, MASTER_TO_CHILD);
            NewTo   = MapPositionToOtherRow(masterSeqAlign, To,   MASTER_TO_CHILD);
        } else {
            NewFrom = INVALID_POSITION;
        }
        if ((NewFrom == INVALID_POSITION) || (NewTo == INVALID_POSITION)) {
            result = false;
            if (err) {
            	char s[1024];
                string d = ((*m)->IsSetDescription()) ? (*m)->GetDescription() : "<unnamed>";
	            sprintf(s,"   ==> Annotation '%s' at [%d, %d]\n", d.c_str(), From+1, To+1);
                err->append(s);
            }
        }
      }
      // if it's a set of from-to's
      else if ((*m)->GetLocation().IsPacked_int()) {
        // for each from-to
        intNumber = 0;
        for (n=(*m)->GetLocation().GetPacked_int().Get().begin();
             n!=(*m)->GetLocation().GetPacked_int().Get().end(); n++) {
          // update from and to with new master
          From = (*n)->GetFrom();
          To = (*n)->GetTo();
          if (AllResiduesInRangeAligned(0, From, To)) {
              NewFrom = MapPositionToOtherRow(masterSeqAlign, From, MASTER_TO_CHILD);
              NewTo   = MapPositionToOtherRow(masterSeqAlign, To,   MASTER_TO_CHILD);
          } else {
              NewFrom = INVALID_POSITION;
          }
          if ((NewFrom == INVALID_POSITION) || (NewTo == INVALID_POSITION)) {
                result = false;
                if (err) {
            	    char s[1024];
                    string d = ((*m)->IsSetDescription()) ? (*m)->GetDescription() : "<unnamed>";
	                sprintf(s,"   ==> Annotation '%s' at segment %d in range [%d, %d]\n", 
                              d.c_str(), intNumber+1, From+1, To+1);
                    err->append(s);
                }
          }
          ++intNumber;
        }
      }
    }
  }
  return result;
}

int CCdCore::GetNumAlignmentAnnotations() {
//-------------------------------------------------------------------------
// return the number of alignment annotations
//-------------------------------------------------------------------------
  if (IsSetAlignannot()) {
    return(SetAlignannot().Set().size());
  }
  return(0);
}


string CCdCore::GetAlignmentAnnotationDescription(int Index) {
//-------------------------------------------------------------------------
// return the description for the Index alignment annotation
//-------------------------------------------------------------------------
  list< CRef< CAlign_annot > >::iterator  i;

  int Count=0;
  for (i=SetAlignannot().Set().begin(); i!=SetAlignannot().Set().end(); i++) {
    if (Count == Index) {
      if ((*i)->IsSetDescription()) {
        return((*i)->GetDescription());
      }
      else {
        return("");
      }
    }
    Count++;
  }
  return("");
}


bool CCdCore::DeleteAlignAnnot(int Index) {
//-------------------------------------------------------------------------
// delete the Index alignment annotation
//-------------------------------------------------------------------------
  list< CRef< CAlign_annot > >::iterator  i;

  int Count=0;
  for (i=SetAlignannot().Set().begin(); i!=SetAlignannot().Set().end(); i++) {
    if (Count == Index) {
      SetAlignannot().Set().erase(i);
      return(true);
    }
    Count++;
  }
  return(false);
}


void CCdCore::EraseStructureEvidence() {
//-------------------------------------------------------------------------
// erase structure evidence for sequences that are no longer present
//-------------------------------------------------------------------------
  list< CRef< CFeature_evidence > >::iterator  FeatureIterator;

  // make list of current mmdb id's
  int  id;
  list<int> MmdbIds;
  for (int SeqIndex=0; SeqIndex<GetNumSequences(); SeqIndex++) {
    if (GetMmdbId(SeqIndex, id)) {
      MmdbIds.push_back(id);
    }
  }

  // delete structure evidence for mmdb-id's not in the list
  while (IsNoEvidenceFor(MmdbIds, FeatureIterator)) {
    GetFeatureSet(MmdbIds).erase(FeatureIterator);
  }
}


// private
bool CCdCore::IsNoEvidenceFor(list<int>& MmdbIds,
                           list< CRef< CFeature_evidence > >::iterator& FeatureIterator) {
//------------------------------------------------------------------------------------
// look through the feature-evidence of each align-annot-set.
// return true if there's an id that needs to be deleted from a structure-evidence.
//------------------------------------------------------------------------------------
  list< CRef< CAlign_annot > >::iterator  i;
  list< CRef< CFeature_evidence > >::iterator j;
  list< CRef< CBiostruc_id > >::iterator k;
  list<int>::iterator  m;
  bool  FoundIt;

  // look through each align-annot in the align-annot-set
  if (IsSetAlignannot()) {
    for (i=SetAlignannot().Set().begin(); i!=SetAlignannot().Set().end(); i++) {
      // look through each feature-evidence in the feature-evidence-set
      if ((*i)->IsSetEvidence()) {
        for (j=(*i)->SetEvidence().begin(); j!=(*i)->SetEvidence().end(); j++) {
          // look through each biostruc-id in the biostruc-id-set
          if ((*j)->IsBsannot()) {
            if ((*j)->SetBsannot().IsSetId()) {
              for (k=(*j)->SetBsannot().SetId().begin(); k!=(*j)->SetBsannot().SetId().end(); k++) {
                if ((*k)->IsMmdb_id()) {
                  // if biostruc id is NOT in the list return true
                  FoundIt = false;
                  for (m=MmdbIds.begin(); m!=MmdbIds.end(); m++) {
                    if ((*m) == (*k)->GetMmdb_id().Get()) {
                      FoundIt = true;
                    }
                  }
                  if (!FoundIt) {
                    FeatureIterator = j;
                    return(true);
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  return(false);
}


// private
list< CRef< CFeature_evidence > >& CCdCore::GetFeatureSet(list<int>& MmdbIds) {
//-------------------------------------------------------------------------
// this function mirrors IsNoEvidenceFor.  It returns the list of features
// that is modified.
//-------------------------------------------------------------------------
  list< CRef< CAlign_annot > >::iterator  i;
  list< CRef< CFeature_evidence > >::iterator j;
  list< CRef< CBiostruc_id > >::iterator k;
  list< int >::iterator  m;
  bool  FoundIt;

  // look through each align-annot in the align-annot-set
  if (IsSetAlignannot()) {
    for (i=SetAlignannot().Set().begin(); i!=SetAlignannot().Set().end(); i++) {
      // look through each feature-evidence in the feature-evidence-set
      if ((*i)->IsSetEvidence()) {
        for (j=(*i)->SetEvidence().begin(); j!=(*i)->SetEvidence().end(); j++) {
          // look through each biostruc-id in the biostruc-id-set
          if ((*j)->IsBsannot()) {
            if ((*j)->SetBsannot().IsSetId()) {
              for (k=(*j)->SetBsannot().SetId().begin(); k!=(*j)->SetBsannot().SetId().end(); k++) {
                if ((*k)->IsMmdb_id()) {
                  // if biostruc id is NOT in the list return list to delete id from
                  FoundIt = false;
                  for (m=MmdbIds.begin(); m!=MmdbIds.end(); m++) {
                    if ((*m) == (*k)->GetMmdb_id().Get()) {
                      FoundIt = true;
                    }
                  }
                  if (!FoundIt) {
                    return((*i)->SetEvidence());
                  }
                }
              }
            }
          }
        }
      }
    }
  }
//  assert(false);   // should never get here
  return((*i)->SetEvidence());
}

int CCdCore::GetMmdbIdWithEvidence(set<int>& MmdbIds) {
//-------------------------------------------------------------------------
// this function mirrors IsNoEvidenceFor.  It returns the list of features
// that is modified.
//-------------------------------------------------------------------------
  list< CRef< CAlign_annot > >::iterator  i;
  list< CRef< CFeature_evidence > >::iterator j;
  list< CRef< CBiostruc_id > >::iterator k;

  // look through each align-annot in the align-annot-set
  if (IsSetAlignannot()) {
    for (i=SetAlignannot().Set().begin(); i!=SetAlignannot().Set().end(); i++) {
      // look through each feature-evidence in the feature-evidence-set
      if ((*i)->IsSetEvidence()) {
        for (j=(*i)->SetEvidence().begin(); j!=(*i)->SetEvidence().end(); j++) {
          // look through each biostruc-id in the biostruc-id-set
          if ((*j)->IsBsannot()) {
            if ((*j)->SetBsannot().IsSetId()) {
              for (k=(*j)->SetBsannot().SetId().begin(); k!=(*j)->SetBsannot().SetId().end(); k++) {
                if ((*k)->IsMmdb_id()) {
                   MmdbIds.insert((*k)->GetMmdb_id().Get()); 
                }
              }
            }
          }
        }
      }
    }
  }
  return MmdbIds.size();
}


//  Recursively look for a bioseq with the given seqid; return the first instance found.
bool CCdCore::GetBioseqWithSeqid(CSeq_id& seqid, const list< CRef< CSeq_entry > >& seqEntryList, const CBioseq*& bioseq) const{

	bool result = false;

	list< CRef< CSeq_entry > >::const_iterator lsei;

//	const list< CRef< CSeq_id > > seqIdList;
	list< CRef< CSeq_id > >::const_iterator lsii;

	for (lsei = seqEntryList.begin(); lsei != seqEntryList.end(); ++lsei) {
		if ((*lsei)->IsSet()) {
			result = GetBioseqWithSeqid(seqid, (*lsei)->GetSet().GetSeq_set(), bioseq);  //  RECURSIVE!!
			if (result) {
				return result;
			}
		} else if ((*lsei)->IsSeq()) {
			const list< CRef< CSeq_id > > seqIdList = (*lsei)->GetSeq().GetId();
			for (lsii = seqIdList.begin(); lsii != seqIdList.end(); ++lsii) {
				if (seqid.Match(**lsii)) {
					bioseq = &(*lsei)->GetSeq();
					return true;
				}
			}
		}

	}

	return false;
}

/* ============================== */
/*  Parent CD identifier methods  */
/* ============================== */

bool CCdCore::HasParentType(EClassicalOrComponent parentType) const {
    bool result = false;
    bool hasClassicalParent = HasParentType(CDomain_parent::eParent_type_classical);
    
    if (parentType == eClassicalParent) {
        result = hasClassicalParent;
    } else if (parentType == eComponentParent && !hasClassicalParent) {
        
        //  Once know constraints are satisfied and there are no classical parents,
        //  make sure every ancestor is not of type eParent_type_other.
        if (obeysParentTypeConstraints(this)) {
            if (IsSetAncestors()) {
                list< CRef< CDomain_parent > >::const_iterator pit, pit_end = GetAncestors().end();
                for (pit = GetAncestors().begin(); pit != pit_end && !result; ++pit) {
                    if ((*pit)->GetParent_type() != CDomain_parent::eParent_type_other) {
                        result = true;
                    }
                }
            }
        }
    }
    return result;
}

bool CCdCore::HasParentType(CDomain_parent::EParent_type parentType) const {
    bool result = obeysParentTypeConstraints(this);

    //  Once know constraints are satisfied, just look for the type.
    if (result) {
        // 'ancestors' field set
        if (IsSetAncestors()) {
            list< CRef< CDomain_parent > >::const_iterator pit, pit_end = GetAncestors().end();
			result=false;
            for (pit = GetAncestors().begin(); (pit != pit_end) && !result; ++pit) {
                if ((*pit)->GetParent_type() == parentType) {
                    result = true;
                }
            }
        // 'parent' field set
        } else if (IsSetParent()) {
            result = (parentType == CDomain_parent::eParent_type_classical);
        // neither 'ancestors' nor 'parent' set
        } else {
            result = false;
        }
    }
    return result;
}

bool CCdCore::GetClassicalParentId(const CCdd_id*& parentId) const {
    bool result = HasParentType(eClassicalParent);
    if (result) {
        if (IsSetAncestors()) {
            parentId = &(*(GetAncestors().begin()))->GetParentid();
        } else {
            parentId = &GetParent();
        }
    }
	return result;
}

bool CCdCore::GetComponentParentIds(vector< const CCdd_id* >& parentIds) const {
    bool result = HasParentType(eComponentParent);

    parentIds.clear();
    if (result) {
        if (IsSetAncestors()) {
			const list< CRef< CDomain_parent > >& parents = GetAncestors();
            list< CRef< CDomain_parent > >::const_iterator pit, pit_end = parents.end();
            for (pit = parents.begin(); pit != pit_end; ++pit) {
                parentIds.push_back(&(*pit)->GetParentid());
            }
        }
    }
	return result;
}

string CCdCore::GetClassicalParentAccession() const {
    int Dummy;
    return(GetClassicalParentAccession(Dummy));
}

string CCdCore::GetClassicalParentAccession(int& Version) const{
//-------------------------------------------------------------------------
// get accession name and version of parent
//-------------------------------------------------------------------------
  string  Str;
  const CCdd_id* parentId;

  if (GetClassicalParentId(parentId)) {
      Str = parentId->GetGid().GetAccession();

      if (parentId->IsGid()) {
          if (parentId->GetGid().IsSetVersion()) {
              Version = parentId->GetGid().GetVersion();
          }
          else {
              Version = 1;
          }
      }
  }
  return(Str);
}


bool CCdCore::SetClassicalParentAccessionNew(string Parent, int Version) {
//-------------------------------------------------------------------------
// set accession name and version of parent
//-------------------------------------------------------------------------
    bool result = !HasParentType(eComponentParent);  //  allow there to be *no* parent defined!

    if (result) {
        ResetParent();  //  just in case its set...
    
        CCdd_id* pID = new CCdd_id;
        CGlobal_id* pGID = new CGlobal_id;
        pGID->SetAccession(Parent);
        pGID->SetVersion(Version);
        pID->SetGid(*pGID);

        CRef< CDomain_parent > classicalParent(new CDomain_parent());
        if (classicalParent.NotEmpty()) {
            classicalParent->SetParentid(*pID);
            classicalParent->SetParent_type(CDomain_parent::eParent_type_classical);
            list< CRef< CDomain_parent > >& fus = SetAncestors();
	        fus.push_back(classicalParent);
        } else {
            result = false;
        }
    }
    return result;
}

void CCdCore::SetClassicalParentAccession(string Parent, int Version) {
//-------------------------------------------------------------------------
// set accession name and version of parent
//-------------------------------------------------------------------------
    SetClassicalParentAccessionNew(Parent, Version);

/*
    ResetParent();

    CCdd_id* pID = new CCdd_id;
    CGlobal_id* pGID = new CGlobal_id;
    pGID->SetAccession(Parent);
    pGID->SetVersion(Version);
    pID->SetGid(*pGID);
    SetParent(*pID);
    */
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/04/19 14:27:18  lanczyck
 * initial version under algo/structure
 *
 * ===========================================================================
 */
