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
 *       Subclass of CCdd for use by CDTree
 *
 * ===========================================================================
 */

#ifndef CU_CDCORE_HPP
#define CU_CDCORE_HPP

#pragma warning (disable: 4786)
#include <cassert>
#include <map>
#include <algo/structure/cd_utils/cuCppNCBI.hpp>
#include <algo/structure/cd_utils/cuGlobalDefs.hpp>
#include <algo/structure/cd_utils/cuMatrix.hpp>
//USING_NCBI_SCOPE;
BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

enum EClassicalOrComponent {
	eClassicalParent= 0,
	eComponentParent
};

const int PENDING_ROW_START = 1000000;

class CCdCore : public CCdd
{
public:
	enum AlignmentSrc {
		NORMAL_ALIGNMENT = 0,
		PENDING_ALIGNMENT
	};
	enum AlignmentUsage {
		USE_NORMAL_ALIGNMENT=0,
		USE_PENDING_ALIGNMENT,
		USE_ALL_ALIGNMENT
	};

    CCdCore(void);                                      // constructor
    ~CCdCore(void);                                     // destructor

    /*  CD identifier methods */
    string GetAccession(int& Version) const;              // get accession and version of CD
    string GetAccession() const;
    void   SetAccession(string Accession, int Version);   // set accession and version of CD
    void   SetAccession(string Accession);
    void   EraseUID();                                    // erase CD's uid
    /* ADDED */ 
    bool HasCddId(const CCdd_id& id) const;               // is 'id' an identifier for this CD

    /*  Basic information about CD  */
    string GetLongDescription();                 // long description of CD
    string GetUpdateDate();                      // last update date of CD
    int    GetNumRows() const;                   // number of rows in CD
    int    GetNumSequences() const;              // number of sequences in CD
    int    GetNumRowsWithSequences() const;      // number of rows with a valid sequence index
    int    GetAlignmentLength() const;           // total number aligned residues
    int    GetPSSMLength() const;                // number of residues in master, from first to last aligned residue

    /*  ADDED:  Block information  */
    int    GetNumBlocks() const;                            // return number of blocks in alignment (0 if no alignment)
    bool   GetCDBlockLengths(vector<int>& lengths) const;   
    bool   GetBlockStartsForRow(int rowIndex, vector<int>& starts) const; 

    /*  Find/convert sequence list and row indices  */
    int    GetSeqIndexForRowIndex(int rowIndex) const;        // map alignment row to sequence index (-1 if invalid row)
    int    GetMasterSeqIndex() const;                         // get sequence index of the master sequence (-1 if fails)
    int    GetSeqIndex(const CRef<CSeq_id>& SeqID) const;     // map seqId to sequence list index (-1 if fails)
    int    GetNthMatchFor(CRef<CSeq_id>& ID, int N);          // get RowIndex of Nth match
    /* ADDED:  find all row indices for a seqID (return # found) */ 
    int    GetAllRowIndicesForSeqId(const CRef<CSeq_id>& SeqID, list<int>& rows) const;    
    int    GetAllRowIndicesForSeqId(const CRef<CSeq_id>& SeqID, vector<int>& rows) const;  


    /*  Access CD info via alignment row number  */
    bool   GetGI(int Row, int& GI, bool ignorePDBs = true); // get GI of Row (if ignorePDBs = true, don't look @PDBs for the GI)
    bool   GetPDB(int Row, const CPDB_seq_id*& pPDB); // get PDB ID of Row
    int    GetLowerBound(int Row) const;              // get Row lower alignment bound; return INVALID_MAPPED_POSITION on failure
    int    GetUpperBound(int Row) const;              // get Row upper alignment bound; return INVALID_MAPPED_POSITION on failure
    bool   Get_GI_or_PDB_String_FromAlignment(int  RowIndex, std::string& Str, bool Pad, int Len) const ;
    /*  ADDED  */
    string GetSpeciesForRow(int Row);                 // find the species string for alignment row
    string GetSequenceStringByRow(int rowId);         // return the full sequence for rowId
    bool   GetSeqEntryForRow(int rowId, CRef< CSeq_entry >& seqEntry) const;  //  get the indicated seq_entry
    bool   GetBioseqForRow(int rowId, CRef< CBioseq >& bioseq);


    /*  Access CD info via sequence list index  */
    int    GetGIFromSequenceList(int SeqIndex) const;       // get GI from sequence list
    string GetDefline(int SeqIndex);                  // get description from sequence list
    /* ADDED */ 
    string GetSequenceStringByIndex(int SeqIndex);    // return the full sequence for index SeqIndex
    string GetSpeciesForIndex(int SeqIndex);          // get species name from sequence list
    bool   GetSeqEntryForIndex(int seqIndex, CRef< CSeq_entry > & seqEntry) const;  //  was cdGetSeq from algMerge
    bool   GetBioseqForIndex(int seqIndex, CRef< CBioseq >& bioseq) ;

    /*  Examine alignment for a SeqId or footprint */
    bool   HasSeqId(const CRef<CSeq_id>& ID) const;                 // see if ID matches any ID in alignment  (deprecate???)
    bool   HasSeqId(const CRef<CSeq_id>& ID, int& RowIndex) const;  // same, but return row that matches
    int    GetNumMatches(const CRef<CSeq_id>& ID) const;               // get num-times ID matches an ID in alignment

    /*  SeqID getters ... from alignment info */
    bool   GetSeqIDForRow(int Pair, int DenDiagRow, CRef<CSeq_id>& SeqID) const;   // get SeqID from alignment
    bool   GetSeqIDFromAlignment(int RowIndex, CRef<CSeq_id>& SeqID) const;

    /*  SeqID getters ... from sequence list  */
    bool   GetSeqIDForIndex(int SeqIndex, CRef<CSeq_id>& SeqID) const; // get SeqID from sequence list
    bool   GetSeqIDs(int SeqIndex, list< CRef< CSeq_id > >& SeqIDs);   // get all SeqIDs from sequence list
    const list< CRef< CSeq_id > >& GetSeqIDs(int SeqIndex) const;   // get all SeqIDs from sequence list

    /*  Sequence or row removal  */
    bool   EraseOtherRows(const std::vector<int>& KeepRows);  // erase all rows from alignment not in KeepRows
    bool   EraseTheseRows(const std::vector<int>& TossRows);  // erase all rows from alignment in TossRows
    void   EraseSequence(int SeqIndex);                       // erase a sequence from the set of seqs
    void   EraseSequences();                                  //  erase sequences not in alignment
	void   ErasePendingRows(set<int>& rows);
	void   ErasePendingRow(int row);

    /*  Methods for structures, structure alignments, MMDB identifiers  */
    bool   Has3DMaster() const;
 	int    Num3DAlignments() const;
 	bool   Has3DSuperpos(list<int>& MMDBId_vec) const;
	bool   GetRowsForMmdbId(int mmdbId, list<int>& rows) const;  // find all rows with this mmdbId
	bool   GetRowsWithMmdbId(vector<int>& rows) const;     // find all rows with a mmdbid
    bool   GetMmdbId(int SeqIndex, int& id) const;         // get mmdb-id from sequence list
	int	   GetMmdbIdWithEvidence(set<int>& MmdbIds);
	int		GetStructuralRowsWithEvidence(vector<int>& rows);
    /*  CD alignment methods  (most added or renamed) */

    //  Returns coordinate on 'otherRow' that is mapped to 'thisPos' on 'thisRow'.
    //  Returns INVALID_MAPPED_POSITION on failure.
    int    MapPositionToOtherRow(int thisRow, int thisPos, int otherRow) const;

    //  Formerly GetSeqPosition(...).  Returns INVALID_MAPPED_POSITION on failure.
    int    MapPositionToOtherRow(const CRef< CSeq_align >& seqAlign, int thisPos, CoordMapDir mapDir) const;

    bool   IsSeqAligns() const;                           //  true is CD has alignment
    bool   GetAlignment(CRef< CSeq_annot >& seqAnnot);    //  return the first Seq_annot of type 'align'
    const  CRef< CSeq_annot >& GetAlignment() const;      //  return the first seq_annot of type 'align'

    const  list< CRef< CSeq_align > >& GetSeqAligns() const;          // get the list of Seq-aligns in alignment
    list< CRef< CSeq_align > >& GetSeqAligns();          // get the list of Seq-aligns in alignment (editable)
    bool   GetSeqAlign(int Row, CRef< CSeq_align >& seqAlign);  // get the Rowth Seq-align (editable)
    const  CRef< CSeq_align >& GetSeqAlign(int Row) const;  // get the Rowth Seq-align
    //int    FindDDBySeqId(CRef<CSeq_id>& SeqID, TDendiag* & ResultDD, TDendiag* pNeedOverlapDD, int isSelf,int istart);

    bool   UsesConsensusSequenceAsMaster() const;
    bool   HasConsensusSequence() const;
    int    GetRowsWithConsensus(vector<int>& consensusRows) const;
    bool   FindConsensusInSequenceList(vector<int>* indices = NULL) const;

    bool   IsInPendingList(const CRef<CSeq_id>& ID, vector<int>& listIndex) const;  //  true if ID is in pending list; returns all indices found
    int    GetNumPending() {return(GetPending().size());}

	//add aligns or sequences to CD

	bool AddSeqAlign(CRef< CSeq_align > seqAlign);
	bool AddPendingSeqAlign(CRef< CSeq_align > seqAlign);
	bool AddSequence(CRef< CSeq_entry > seqAntry);
	void Clear();

    /*  Comment methods (there can be multiple comments)  */
    void   SetComment(CCdd_descr::TComment oldComment, CCdd_descr::TComment newComment);

    /*  Old root methods  */
    bool   IsOldRoot();                               // old-root of CD
    void   SetOldRoot(string Accession, int Version);
    bool   GetOldRoot(int Index, string& Accession, int& Version);
    int    GetNumIdsInOldRoot();


    /*  Alignment & structure annotation methods  */
    bool   AllResiduesInRangeAligned(int rowId, int from, int to) const;
    bool   AlignAnnotsValid(string* err = NULL) const;  // one of the checks for re-mastering/validation
    int    GetNumAlignmentAnnotations();
    string GetAlignmentAnnotationDescription(int Index);
    bool   DeleteAlignAnnot(int Index);
    void   EraseStructureEvidence();                  // scan structure-evidence, erase missing biostruc-ids

	bool   HasParentType(EClassicalOrComponent parentType) const;  
    bool   HasParentType(CDomain_parent::EParent_type parentType) const;  
    bool   GetClassicalParentId(const CCdd_id*& parentId) const; // get id of classical parent
    bool   GetComponentParentIds(vector< const CCdd_id* >& parentIds) const;    //  excludes classical parent
    string GetClassicalParentAccession(int& Version) const;        // get accession and version of parent
    string GetClassicalParentAccession() const;
	/*  Uses the 'ancestor' field in the spec; returns false if has a component parent
        or couldn't create the Domain_parent object.
        If 'parent' field is in use, reset 'parent' and create in the 'ancestor' field.
        If no 'parent' or 'ancestor' field is filled, create it new.
    */
    bool   SetClassicalParentAccessionNew(string Parent, int Version);// set accession and version of parent
    /*  Deprecated:  uses the old 'parent' field in the spec  */
    void   SetClassicalParentAccession(string Parent, int Version);// set accession and version of parent
private:

    bool GetBioseqWithSeqid(CSeq_id& seqid, const list< CRef< CSeq_entry > >& bsset, const CBioseq*& bioseq) const;
    bool IsNoEvidenceFor(list<int>& MmdbIds, list< CRef< CFeature_evidence > >::iterator& FeatureIterator);
    list< CRef< CFeature_evidence > >& GetFeatureSet(list<int>& MmdbIds);
    // Prohibit copy constructor and assignment operator
    CCdCore(const CCdCore& value);
    CCdCore& operator=(const CCdCore& value);
};

/////////////////// end of CCd inline methods


END_SCOPE(cd_utils)
END_NCBI_SCOPE


#endif // NEWCDCCD_HPP

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.1  2005/04/19 14:28:00  lanczyck
 * initial version under algo/structure
 *
 *
 * ===========================================================================
 */
