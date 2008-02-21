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
* Authors:  Paul Thiessen
*
* File Description:
*      implementation of BMA refiner as MSAInterface
*
* ===========================================================================
*/

#ifndef BMA_REFINER__MSAA_INTERFACE__GUI__HPP
#define BMA_REFINER__MSAA_INTERFACE__GUI__HPP

#include <algo/structure/bma_refine/wx_tools.hpp>

#include <algo/structure/bma_refine/Interface.hpp>
#include <algo/structure/bma_refine/RefinerDefs.hpp>


BEGIN_SCOPE(align_refine)

class BMARefinerOptionsDialog : public wxDialog
{
public:

    BMARefinerOptionsDialog(wxWindow* parent,
        const GeneralRefinerParams& current_genl,
        const LeaveOneOutParams& current_loo,
        const BlockEditingParams& current_be,
        const vector < string >& rowTitles);
    ~BMARefinerOptionsDialog(void);

    bool GetParameters(GeneralRefinerParams* genl_params, LeaveOneOutParams* loo_params, BlockEditingParams* be_params);
    unsigned int GetNCycles();  //  out of range -> returns 1
    unsigned int GetNTrials();  //  out of range -> returns 1

    static bool SetRefinerOptionsViaDialog(wxWindow* parent, BMARefinerInterface& interface);

private:

    const vector < string >& rowTitles;
    vector < unsigned int > rowsToExclude;

	Cn3D::IntegerSpinCtrl *nCyclesSpin, *nTrialsSpin;
    Cn3D::IntegerSpinCtrl *lnoSpin, *loopExtensionSpin, *loopCutoffSpin, *rngSpin, *nExtSpin, *cExtSpin;
    Cn3D::IntegerSpinCtrl *minBlockSizeSpin, *medianSpin;
    Cn3D::FloatingPointSpinCtrl *loopPercentSpin, *rawVoteSpin, *weightedVoteSpin ;
    wxCheckBox *doLooCheck, *fixStructCheck, *fullSeqCheck, *extendFirstCheck, *allUnstSeqCheck;
    wxComboBox *phaseOrderCombo, *looSelectionOrderCombo, *esCombo;

    //  This is used as a proxy for the number of PDB rows in the CD to be refined
    //  (by convention, a PDB row title was left blank).  Since the master is not refined,
    //  'countMasterRowTitle' allows the row title corresponding to the master to not be counted.
    unsigned int GetNumEmptyRowTitles(bool countMasterRowTitle) const;

    void OnCloseWindow(wxCloseEvent& event);
    void OnButton(wxCommandEvent& event);
    void OnCombo(wxCommandEvent& event);
    void OnCheck(wxCommandEvent& event);
    void OnLooSelOrder(wxCommandEvent& event);
    DECLARE_EVENT_TABLE()
};

END_SCOPE(align_refine)

#endif // BMA_REFINER__MSAA_INTERFACE__GUI__HPP
