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
 *		 Subclass of CCdd object.
 *       Originally forked from the CDTree3 CCd class.
 *
 * ===========================================================================
 */


#include <ncbi_pch.hpp>

#include <algorithm>
#include <objects/biblio/PubMedId.hpp>
#include <objects/pub/Pub.hpp>
#include <algo/structure/cd_utils/cuSort.hpp>
#include <algo/structure/cd_utils/cuCdCore.hpp>
#include <algo/structure/cd_utils/cuAlign.hpp>
#include <algo/structure/cd_utils/cuCD.hpp>  
#include <algo/structure/cd_utils/cuSequence.hpp>
#include <algo/structure/cd_utils/cuUtils.hpp>

#include <stdio.h>

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
    bool hasGid = false;
  CCdd_id_set::Tdata::iterator  i;

  for (i=SetId().Set().begin(); i!=SetId().Set().end(); i++) {
    if ((*i)->IsGid()) {
      (*i)->SetGid().SetAccession(Accession);
      (*i)->SetGid().SetVersion(Version);
      hasGid = true;
    }
  }

  //  If there was no Gid (or SetId().Set() is empty), create and add one.
  if (!hasGid) {
        CRef< CCdd_id > cdId(new CCdd_id());
        CRef< CGlobal_id > global(new CGlobal_id());
        global->SetAccession(Accession);
        global->SetVersion(Version);
        cdId->SetGid(*global);
        SetId().Set().push_back(cdId);
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

int CCdCore::GetUID() const
{
    int uid = 0;
    const CCdd_id_set::Tdata ids = GetId().Get();
    CCdd_id_set::Tdata::const_iterator idCit = ids.begin(), idEnd = ids.end();
    for (; idCit != idEnd; ++idCit) {
        if ((*idCit)->IsUid()) {
            uid = (*idCit)->GetUid();
            break;
        }
    }
    return uid;
}


bool CCdCore::HasCddId(const CCdd_id& id) const{

    bool isHere = false;
    CCdd_id_set::Tdata idSet = GetId().Get();
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

    //  Be sure GetAlign() is not an empty container -> no such thing as a CD blob with one row.
    if (alignment.NotEmpty() && alignment->GetData().IsAlign() && alignment->GetData().GetAlign().size() > 0) {
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

bool   CCdCore::GetCDBlockLengths(vector<int>& lengths) const {
    if (IsSeqAligns()) {
        const CRef< CSeq_align >& seqAlign = GetSeqAlign(0);
        if (seqAlign.NotEmpty()) {
            return (GetBlockLengths(seqAlign, lengths) != 0);
        }
    }
    return false;
}

bool   CCdCore::GetBlockStartsForRow(int rowIndex, vector<int>& starts) const {
    bool onMaster = (rowIndex) ? false : true;
    bool result = false;
    if (IsSeqAligns() && rowIndex >= 0) {
        const CRef< CSeq_align >& seqAlign = GetSeqAlign(rowIndex);
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

    //  Sanity check that the ASN.1 has the assumed format.
    if (!IsSetSequences() || !GetSequences().IsSet() || GetSequences().GetSet().GetSeq_set().size() == 0) {
        return(-1);
    }

    int  i, NumSequences = GetNumSequences();
    CRef< CSeq_id > TrialSeqID;

    CBioseq_set::TSeq_set::const_iterator seCit = GetSequences().GetSet().GetSeq_set().begin();
    CBioseq_set::TSeq_set::const_iterator seCend = GetSequences().GetSet().GetSeq_set().end();

    if (SeqID.NotEmpty()) {
        for (i=0; seCit != seCend && i<NumSequences; ++seCit, i++) {
            //  Stopped using GetSeqIDForIndex (or variants) as it selected only one of the possible Seq_ids
            //  -- potentially not the desired one for a given use case.
            if ((*seCit)->IsSeq() && SeqIdHasMatchInBioseq(SeqID, (*seCit)->GetSeq())) {
                return i;
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

    rows.clear();
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

bool CCdCore::GetGI(int Row, TGi& GI, bool ignorePDBs) {
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
  } else if (SeqID->IsPdb() && !ignorePDBs) {   //  to match AlignmentCollection behavior
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
  if (SeqID->IsGi() || SeqID->IsPdb()) {
    Str += Make_SeqID_String(SeqID, Pad, Len);
  } else {
    Str += "<Non-gi/pdb Sequence Types Unsupported>";
  }

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


//  get the bioseq for the designated alignment row (for editing)
bool   CCdCore::GetBioseqForRow(int rowId, CRef< CBioseq >& bioseq)  {

    int seqIndex = GetSeqIndexForRowIndex(rowId);
    return GetBioseqForIndex(seqIndex, bioseq);
}

// find the species string for alignment row
string CCdCore::GetSpeciesForRow(int Row) {
    CRef< CBioseq > bioseq;  // = GetBioseqForRow(Row);
    if (GetBioseqForRow(Row, bioseq) && !bioseq.IsNull()) {
        return GetSpeciesFromBioseq(*bioseq);//  rework GetSpecies to use other stuff...
    }
    return kEmptyStr;
}


// get the sequence for specified row
string CCdCore::GetSequenceStringByRow(int rowId) {
    int seqIndex = GetSeqIndexForRowIndex(rowId);
    return GetSequenceStringByIndex(seqIndex);
}


/* ========================================== */
/*  Access CD info via a sequence list index  */
/* ========================================== */

TGi CCdCore::GetGIFromSequenceList(int SeqIndex) const {
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
  return INVALID_GI;
}
  

string CCdCore::GetDefline(int SeqIndex) const {
//-------------------------------------------------------------------------
// get a description for the SeqIndex sequence
//-------------------------------------------------------------------------
  list< CRef< CSeq_entry > >::const_iterator  i;
  list< CRef< CSeqdesc > >::const_iterator  j;
  int  SeqCount;
  string  Description = kEmptyStr;

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


// find the species string for sequence list index
string CCdCore::GetSpeciesForIndex(int SeqIndex) {
    CRef< CBioseq > bioseq;  // = GetBioseqForIndex(SeqIndex);
    if (GetBioseqForIndex(SeqIndex, bioseq) && !bioseq.IsNull()) {
        return GetSpeciesFromBioseq(*bioseq);
    }
    return kEmptyStr;
}


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
			if (NumIDs > 0)
			{
				SeqID = *((*i)->GetSeq().GetId().begin());
				return (SeqID.NotEmpty());
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

  CRef< CSeq_annot > alignment;
  if (!GetAlignment(alignment)) {
      return false;
  }

  int * ind=new int[3*TossRows.size()];
  algSortQuickCallbackIndex((void * )&TossRows,TossRows.size(),ind+TossRows.size(),ind,intSortRowsFunction);
  
  for (i=TossRows.size()-1; i>=0; i--) {
      if (TossRows[ind[i]] == 0) {
          delete [] ind;
          return(false);  // return false if master row is to be deleted
      }
      if (!EraseRow(alignment, TossRows[ind[i]])) {
          delete [] ind;
          return(false);  // return false if problem deleting a row
      }
      Count++;
  }

  delete [] ind;

  return(true);
}


bool CCdCore::EraseOtherRows(const std::vector<int>& KeepRows) {
//-------------------------------------------------------------------------
// erase rows from the alignment.
// KeepRows is a list of rows that are NOT deleted from this CD.
// KeepRows won't contain the master (0 index) row.
// return of true means successful completion.
//-------------------------------------------------------------------------

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
        for (k=0; k<(int)KeepRows.size(); k++) {
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

    return(true);
}


void CCdCore::EraseSequences() {
//-------------------------------------------------------------------------
// erase sequences not in alignment
//-------------------------------------------------------------------------
    bool hasId;
    int  i;
    int NumSequences = GetNumSequences();
    set<int> indicesToErase;
    set<int>::reverse_iterator rit, ritEnd;

    CBioseq::TId::const_iterator idCit, idCend;
    CBioseq_set::TSeq_set::const_iterator seCit = GetSequences().GetSet().GetSeq_set().begin();
    CBioseq_set::TSeq_set::const_iterator seCend = GetSequences().GetSet().GetSeq_set().end();

    //  Note:  GetSeqIDForIndex only checks one of the possible IDs against those in the alignment; 
    //         look through all possible IDs them before erasing a sequence.

    for (i=0; seCit != seCend && i<NumSequences; ++seCit, i++) {
        hasId = false;
        if ((*seCit)->IsSeq()) {
            const CBioseq::TId& ids = (*seCit)->GetSeq().GetId();
            idCend = ids.end();
            for (idCit = ids.begin(); idCit != idCend; ++idCit) {
                if (HasSeqId(*idCit)) {
                    hasId = true;
                    break;
                }
            }
            if (!hasId) {
                indicesToErase.insert(i);
            }
        }
    }

    //  Erase in reverse order so iterators don't get invalidated.
    if (indicesToErase.size() > 0) {
        ritEnd = indicesToErase.rend();
        for (rit = indicesToErase.rbegin(); rit != ritEnd; ++rit) {
            EraseSequence(*rit);
        }
    }

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

bool CCdCore::SynchronizeMaster3D(bool checkRow1WhenConsensusMaster)
{
    bool result = false;
    CRef< CSeq_id > masterPdbId(new CSeq_id);

    ResetMaster3d();
    if (Has3DMaster()) {

        //  this should *always* be true but just in case...) {
        if (GetSeqIDForRow(0, 0, masterPdbId) && masterPdbId->IsPdb()) {
            SetMaster3d().push_back(masterPdbId);
            result = true;
        }

    } else if (checkRow1WhenConsensusMaster && UsesConsensusSequenceAsMaster()) {

        //  If the first row is a structure, then this will be the master3d entry
        //  after the consensus has been removed.
        if (GetSeqIDForRow(0, 1, masterPdbId) && masterPdbId->IsPdb()) {
            SetMaster3d().push_back(masterPdbId);
            result = true;
        }
    }

    return result;
}

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

//  Return the first seqAnnot of type 'align'
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

//  Return the first seqAnnot of type 'align'
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


list< CRef< CSeq_align > >& CCdCore::GetSeqAligns() {
//-------------------------------------------------------------------------
// get the list of Seq-aligns for editing
// Empty list returned when none are present.
//-------------------------------------------------------------------------
    return SetSeqannot().front()->SetData().SetAlign();
}

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
            NewTo   = INVALID_POSITION;
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
              NewTo   = INVALID_POSITION;
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

bool  CCdCore::CopyBioseqForSeqId(const CRef< CSeq_id>& seqId, CRef< CBioseq >& bioseq) const
{
    if (!IsSetSequences() || !GetSequences().IsSet() || !GetSequences().GetSet().IsSetSeq_set()) {
        bioseq.Reset();
        return false;
    }

    const CBioseq_set::TSeq_set& seqEntryList = GetSequences().GetSet().GetSeq_set();
    CBioseq_set::TSeq_set::const_iterator seListIt = seqEntryList.begin(), seListEnd = seqEntryList.end();
    list< CRef< CSeq_id > >::const_iterator lsii;

    for (; seListIt != seListEnd; ++seListIt) {
        if ((*seListIt)->IsSeq()) {
			const list< CRef< CSeq_id > > seqIdList = (*seListIt)->GetSeq().GetId();
			for (lsii = seqIdList.begin(); lsii != seqIdList.end(); ++lsii) {
				if (seqId->Match(**lsii)) {
					bioseq->Assign((*seListIt)->GetSeq());
					return true;
				}
			}
        }
	}
    return false;
}

//  Recursively look for a bioseq with the given seqid; return the first instance found.
bool  CCdCore::GetBioseqWithSeqId(const CRef< CSeq_id>& seqId, const CBioseq*& bioseq) const
{
    if (!IsSetSequences() || !GetSequences().IsSet() || !GetSequences().GetSet().IsSetSeq_set()) {
        return false;
    }

    const CBioseq_set::TSeq_set& seqEntryList = GetSequences().GetSet().GetSeq_set();
    return GetBioseqWithSeqid(seqId, seqEntryList, bioseq);
}

//  Recursively look for a bioseq with the given seqid in seqEntryList; return the first instance found.
bool CCdCore::GetBioseqWithSeqid(const CRef< CSeq_id>& seqid, const list< CRef< CSeq_entry > >& seqEntryList, const CBioseq*& bioseq) {

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
				if (seqid->Match(**lsii)) {
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

bool CCdCore::AddComment(const string& comment)
{
    bool result = (comment.length() > 0);

    //  Don't add an identical comment.
    if (result && IsSetDescription()) {
        for (TDescription::Tdata::const_iterator cit = GetDescription().Get().begin(); result && cit != GetDescription().Get().end(); ++cit) {
            if ((*cit)->IsComment() && (*cit)->GetComment() == comment) {
                result = false;
            }
        }
    }

    if (result) {
        CRef<CCdd_descr> descr(new CCdd_descr);
        descr->SetComment(comment);
        result = AddCddDescr(descr);
    }
    return result;
}

bool CCdCore::AddOthername(const string& othername)
{
    bool result = (othername.length() > 0);

    //  Don't add an identical othername.
    if (result && IsSetDescription()) {
        for (TDescription::Tdata::const_iterator cit = GetDescription().Get().begin(); result && cit != GetDescription().Get().end(); ++cit) {
            if ((*cit)->IsOthername() && (*cit)->GetOthername() == othername) {
                result = false;
            }
        }
    }

    if (result) {
        CRef<CCdd_descr> descr(new CCdd_descr);
        descr->SetOthername(othername);
        result = AddCddDescr(descr);
    }
    return result;
}

bool CCdCore::AddTitle(const string& title)
{
    bool result = (title.length() > 0);

    //  Don't add an identical title.
    if (result && IsSetDescription()) {
        for (TDescription::Tdata::const_iterator cit = GetDescription().Get().begin(); result && cit != GetDescription().Get().end(); ++cit) {
            if ((*cit)->IsTitle() && (*cit)->GetTitle() == title) {
                result = false;
            }
        }
    }

    if (result) {
        CRef<CCdd_descr> descr(new CCdd_descr);
        descr->SetTitle(title);
        result = AddCddDescr(descr);
    }
    return result;
}

string CCdCore::GetTitle() const
{
    string result = kEmptyStr;

    if (IsSetDescription()) {
        TDescription::Tdata::const_iterator cit = GetDescription().Get().begin();
        TDescription::Tdata::const_iterator cend = GetDescription().Get().end();
        while (cit != cend) {
            if ((*cit)->IsTitle()) { 
                result = (*cit)->GetTitle();
                break;
            }
            ++cit;
        }
    }

    return result;
}

unsigned int CCdCore::GetTitles(vector<string>& titles) const
{
    string result = kEmptyStr;
    
    titles.clear();
    if (IsSetDescription()) {
        TDescription::Tdata::const_iterator cit = GetDescription().Get().begin();
        TDescription::Tdata::const_iterator cend = GetDescription().Get().end();
        while (cit != cend) {
            if ((*cit)->IsTitle()) { 
                result = (*cit)->GetTitle();
                titles.push_back(result);
            }
            ++cit;
        }
    }

    return titles.size();
}

bool CCdCore::AddPmidReference(TEntrezId pmid)
{
    //  Don't add a duplicate PMID.
    if (IsSetDescription()) {
        for (TDescription::Tdata::const_iterator cit = GetDescription().Get().begin(); cit != GetDescription().Get().end(); ++cit) {
            if ((*cit)->IsReference() && (*cit)->GetReference().IsPmid()) {
                if (pmid == (*cit)->GetReference().GetPmid()) {
                    return false;
                }
            }
        }
    }

    //  validate the pmid???
    CRef<CPub> pub(new CPub);
    pub->SetPmid(CPub::TPmid(pmid));

    CRef<CCdd_descr> descr(new CCdd_descr);
    descr->SetReference(*pub);
    return AddCddDescr(descr);
}

bool CCdCore::AddSource(const string& source, bool removeExisting)
{
    bool result = (source.length() > 0);

    if (result) {
        if (removeExisting)
            RemoveCddDescrsOfType(CCdd_descr::e_Source);

        CRef<CCdd_descr> descr(new CCdd_descr);
        descr->SetSource(source);
        result = AddCddDescr(descr);
    }
    return result;
}

bool CCdCore::AddCreateDate()  
{
    return SetCreationDate(this);
}

bool CCdCore::AddCddDescr(CRef< CCdd_descr >& descr)
{
    if (!IsSetDescription()) {
        CCdd_descr_set* newDescrSet = new CCdd_descr_set();
        if (newDescrSet) 
            SetDescription(*newDescrSet);
        else 
            return false;
    }

    if (descr.NotEmpty()) {
        SetDescription().Set().push_back(descr);
        return true;
    }
    return false;
}

bool CCdCore::RemoveCddDescrsOfType(int cddDescrChoice)
{
    if (cddDescrChoice <= CCdd_descr::e_not_set || cddDescrChoice >= CCdd_descr::e_MaxChoice) return false;

    unsigned int count = 0;
    bool reachedEnd = false;
    CCdd_descr_set::Tdata::iterator i, iEnd;
    if (IsSetDescription()) {
        while (!reachedEnd) {
            i = SetDescription().Set().begin();
            iEnd = SetDescription().Set().end();
            for (; i != iEnd; i++) {
                if ((*i)->Which() == cddDescrChoice) {
                    ++count;
                    SetDescription().Set().erase(i);
                    break;
                }
            }
            reachedEnd = (i == iEnd);
        }
    }
    return (count > 0);
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE
