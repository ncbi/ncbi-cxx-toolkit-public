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
*      dialogs for annotating CDD's with interactions from IBIS
*
* ===========================================================================
*/

#ifndef CN3D_IBIS_ANNOT_DIALOG__HPP
#define CN3D_IBIS_ANNOT_DIALOG__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbiobj.hpp>

#ifdef __WXMSW__
#include <windows.h>
#include <wx/msw/winundef.h>
#endif
#include <wx/wx.h>

#include <set>
#include <vector>

#include <objects/cdd/Align_annot_set.hpp>
#include <objects/cdd/Feature_evidence.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

#include "molecule_identifier.hpp"

class wxListEvent;
class wxImageList;

BEGIN_SCOPE(Cn3D)

class StructureSet;
class Sequence;

typedef std::set<ncbi::TSeqPos> SeqPosSet;

class IBISInteraction : public ncbi::CObject
{

    static const MoleculeIdentifier* m_queryMolecule;

public:

    //  Value of properties not in the data returned for the interaction.
    static const int NOT_ASSIGNED;

    //  Values are synced with the type code used in IBIS.
    enum eIbisInteractionType {
        eIbisNoTypeAssigned = 0,
        eIbisProteinDNA = 1,
        eIbisProteinRNA = 2,
        eIbisProteinProtein = 3,
        eIbisProteinChemical = 6,
        eIbisProteinPeptide = 7,
        eIbisProteinIon = 8,
        eIbisProteinCombo_DNA_RNA = 12,
        eIbisOther
    };

    //  This is equivalent to the result of CAlign_annot::GetType.
    //  Although the 'type' field is not explicitly enumerated in 
    //  the ASN.1 spec, these are the conventional associations. 
    enum eCddInteractionType {
        eCddNoTypeAssigned = 0,
        eCddActiveSite = 1,        //  not used for IBIS; here for completeness
        eCddPolypeptideBinding = 2,
        eCddNABinding = 3,
        eCddIonBinding = 4,
        eCddChemicalBinding = 5,
        eCddPosttransMod = 6,
        eCddOther
    };

    //  Note:  This mapping is many->one.
    static bool IsIbisIntType(int integer, eIbisInteractionType* ibisType = NULL);

    //  Note:  This mapping is many->one.
    static eCddInteractionType IbisIntTypeToCddIntType(eIbisInteractionType ibisType);

    //  Note:  This mapping is ambiguous as a cdd type could have arisen from
    //  various ibis types.
    static eIbisInteractionType CddIntTypeToIbisIntType(eCddInteractionType cddType);

    static void SetQueryMolecule(const MoleculeIdentifier* queryMolecule) { m_queryMolecule = queryMolecule; }
    static const MoleculeIdentifier* GetQueryMolecule() { return m_queryMolecule; }
    static bool QueryMoleculeHasChain();
    static bool QueryMoleculeHasChain(const MoleculeIdentifier* queryMolecule);

    IBISInteraction(const ncbi::objects::CSeq_feat& seqfeat);

    //  The 'type' field will be set with the type defined in ibis
    ncbi::CRef < ncbi::objects::CAlign_annot > ToAlignAnnot(void) const;

    int GetRowId(void) const { return m_rowId; }
    int GetSdiId(void) const { return m_sdiId; }
    string GetDesc(void) const { return m_desc; }
    eIbisInteractionType GetType(void) const { return m_type; }
    eCddInteractionType GetCddType(void) const { return IbisIntTypeToCddIntType(m_type); }

    bool IsObserved(void) const { return m_isObs; }
    bool IsFiltered(void) const { return m_isFilt; }
    int GetScore(void) const { return m_score; }
    int GetNumInterfaceResidues(void) const { return m_nIntRes; }
    int GetNumMembers(void) const { return m_nMembers; }
    int GetNumUniqueChemicals(void) const { return m_nUnqChem; }
    int GetNumUniqueIons(void) const { return m_nUnqIons; }
    int GetMmdbId(void) const { return m_mmdbId; }
    double GetAverageIdentity(void) const { return m_pid; }

    //  'location' is mandatory, but if there's something weird
    //  (say, if ibis.cgi does not return a 'packed-int' Seq-loc),
    //  'isOK' will be false.  
    const ncbi::objects::CSeq_loc& GetLocation(bool& isOK) const;

    //  Return the set containing of all positions in interaction.  
    const SeqPosSet& GetPositions(void) const;

    //  The method returns false and from/to are 'NOT_ASSIGNED' 
    //  if there was a problem.
    bool GetFootprint(int& from, int& to) const;  

    //  The method returns false and gi is 'NOT_ASSIGNED' 
    //  if there was a problem (such as the gi is not the
    //  same in all intervals of a packed-int).
    bool GetGi(int& gi) const;  

private:

    //  This Seq-feat has zero-based positions
    ncbi::objects::CSeq_feat m_seqfeat;

    eIbisInteractionType m_type;
    int m_rowId;    //  interaction id, used to open a 'row' in IBIS web display
    int m_mmdbId;   //  from the data labeled 'mmdbId'
    int m_sdiId;    //  from the data labeled 'sdiId'
    string m_desc;  //  name assigned to the interaction

    bool m_isObs;   //  from the data labeled 'isObserverd'
    bool m_isFilt;  //  from the data labeled ''; true if the interaction is filtered from the IBIS web display
    int m_score;    //  from the data labeled 'comb_scr'
    int m_nIntRes;  //  from the data labeled 'nInterfaceRes'
    int m_nMembers; //  from the data labeled 'nMembers'
    int m_nUnqChem; //  from the data labeled 'nUniqueChemicals'
    int m_nUnqIons; //  from the data labeled 'nUniqueIons'
    double m_pid;   //  from the data labeled 'avgIdent', converted to a double

    //  mutable so they can be lazy-initialized.
    mutable SeqPosSet m_positions;  

    void Initialize(void);

};

class IBISAnnotateDialog : public wxDialog
{
public:
    // this is intended to be used as a non-modal dialog
    IBISAnnotateDialog(wxWindow *parent, IBISAnnotateDialog **handle, StructureSet *set);
    ~IBISAnnotateDialog(void);

    bool HasInteractionData(void) const;
    bool HasInteractionData(IBISInteraction::eIbisInteractionType type) const;

private:

    IBISAnnotateDialog **dialogHandle;
    StructureSet *structureSet;
    ncbi::CRef < ncbi::objects::CAlign_annot_set > annotSet;
    vector< ncbi::CRef<IBISInteraction> > vecIbisInteractions;

    wxImageList* m_images;

    typedef std::list < ncbi::CRef < ncbi::objects::CSeq_interval > > IntervalList;
    typedef struct {
        void* ptr;                             //  pointer to a CAlign_annot (== map key)
        unsigned int nRes;                     //  number of residues covered by *ptr
        vector< SeqPosSet > overlaps;  //  vector of positions that overlap ith Ibis intn
    } AlignAnnotInfo;
    typedef std::map<unsigned long, AlignAnnotInfo> AnnotIbisOverlapMap;

    // get highlighted+aligned intervals on master
//    void GetCurrentHighlightedIntervals(IntervalList *intervals);

    //  As users can generate new Align_annots with this dialog - 
    //  and, in principle, can edit existing ones while it's open - 
    //  I've opted to recompute the map vs. trying to track updates to 
    //  user annotations needed to maintain the map as a member variable.
    void GetAnnotIbisOverlaps(AnnotIbisOverlapMap& aioMap) const;
    //  Same as above, but for a specific interaction/annotation pair.
    void GetAnnotIbisOverlaps(const IBISInteraction& interaction, const ncbi::objects::CAlign_annot& annot, SeqPosSet& overlaps) const;
    //  Invert the above to get the positions NOT overlapping.  
    //  onAnnotation = true:   returns those positions in 'annot' that don't overlap with 'interaction'
    //  onAnnotation = false:  returns those positions in 'interaction' that don't overlap with 'annot'
    void GetAnnotIbisNonOverlaps(const IBISInteraction& interaction, const ncbi::objects::CAlign_annot& annot, SeqPosSet& nonOverlaps, bool onAnnotation) const;

    // action functions
    void DeleteAnnotation(void);
    void HighlightInteraction(void);
    void HighlightAnnotation(int eventId);  //bool overlapOnly = false);
    void LaunchIbisWebPage(void);
    void MakeAnnotationFromInteraction(void);

    // event callbacks
    void OnButton(wxCommandEvent& event);
    void OnChoice(wxCommandEvent& event);
    void OnListCtrlSelection(wxListEvent& event);
    void OnListBoxSelection(wxCommandEvent& event);
    void OnCloseWindow(wxCloseEvent& event);

    // other utility functions
    void PopulateInteractionData(void);
    unsigned int GetIntervalsForSet(const SeqPosSet& positions, IntervalList& intervals);
    //  Returns false if there are residues specified outside of the aligned blocks.
    //  Does *not* return false if the seq-id interval does not match a seqid in the master
    //  (the caller should check this if it's important).
    bool HighlightInterval(const ncbi::objects::CSeq_interval& interval);

    enum {
        eDontRemakeControls = 0x00,     
        eRemakeListCtrl = 0x01,
        eRemakeListBox  = 0x02
    };
    
    //  If the first two arguments are < 0, signifies no selection in the relevant widget.
    //  'updateFlags' allows caller to specify how to handle GUI element setup.
    void SetupGUIControls(int selectInteraction, int selectAnnot, unsigned int updateFlags = eRemakeListCtrl | eRemakeListBox);

    DECLARE_EVENT_TABLE()
};
END_SCOPE(Cn3D)

#endif // CN3D_IBIS_ANNOT_DIALOG__HPP
