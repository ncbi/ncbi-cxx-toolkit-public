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
*      Interface to blocked multiple alignment refiner algorithm
*
* ===========================================================================
*/

#ifndef CN3D_REFINER_INTERFACE__HPP
#define CN3D_REFINER_INTERFACE__HPP

#include <corelib/ncbistl.hpp>

#include <list>
#include <vector>

class wxWindow;

//  Forward-declare refiner engine class
BEGIN_SCOPE(align_refine)
class CBMARefinerEngine;
END_SCOPE(align_refine)

BEGIN_SCOPE(struct_util)
class AlignmentUtility;
END_SCOPE(struct_util)


BEGIN_SCOPE(Cn3D)

class BMARefiner
{
public:
    BMARefiner(void);
    ~BMARefiner(void);

    typedef std::list < struct_util::AlignmentUtility * > AlignmentUtilityList;
    typedef AlignmentUtilityList::iterator AlignmentUtilityListIt;


    //  Sets the align_refine::LeaveOneOutParams.blocks and align_refine::BlockEditingParams.editableBlocks fields.
    //  Only block numbers listed in the vector will be refined; any previous list
    //  of refined blocks is erased first (unless clearOldList is false).
    void SetBlocksToRealign(const std::vector<unsigned int>& blocks, bool clearOldList = true);

    //  Sets the align_refine::LeaveOneOutParams.rowsToExclude field.  (The master and
    //  if requested, structured rows, need not be excluded here -- they will be handled
    //  automatically in the refiner engine as required.)
    //  Those row numbers listed in the vector will NOT be subjected to LOO/LNO refinement.
    //  Note that these rows are *not* excluded from block-size changing refinements.
    //  Any previous list of excluded rows is erased first (unless clearOldList is false).
    void SetRowsToExcludeFromLNO(const std::vector<unsigned int>& excludedRows, bool clearOldList = true);

    // Refines an initial blocked multiple alignment according to parameters in
    // a dialog filled in by user.  Unless SetBlocksToRealign or SetRowsToExcludeFromLNO
    // have been called prior to this method to 'freeze' blocks and rows during LNO,
    // all blocks and all non-master rows are refined.  (If LOO params
    // have been set to exclude structure rows, they are automatically excluded.)
    // The output AlignmentList contains the results ordered from best to worst refined
    // score (refiner engine 'owns' the alignments).  If the refinement alignment
    // algorithm fails in a trial, a null element is appended to the list for that trial.
    // Return 'false' only if all trials fail.
    bool RefineMultipleAlignment(struct_util::AlignmentUtility *originalMultiple,
        AlignmentUtilityList *refinedMultiples,
        wxWindow *parent,
        const std::vector < std::string >& rowTitles);    // one per row, but only non-structured rows should have size>0

    //  In case it's desired to get info results/settings directly from the refiner engine.
    const align_refine::CBMARefinerEngine* GetRefiner() {return m_refinerEngine;}

private:

    align_refine::CBMARefinerEngine* m_refinerEngine;
    std::vector<unsigned int> m_blocksToRefine;
    std::vector<unsigned int> m_rowsToExclude;

    //  Convenience method to specify all blocks for realignment (old list always cleared).
    void SetBlocksToRealign(unsigned int nAlignedBlocks);

    // Brings up a dialog that lets the user set refinement parameters; returns false if cancelled.
    // Initializes the refiner engine w/ values found in the dialog.
    // Realigns only blocks found in m_blocksToRefine during LNO.
    // Explicitly excludes from LNO those rows found in m_rowsToExclude.
    bool ConfigureRefiner(wxWindow* parent, const std::vector < std::string >& rowTitles);
};

END_SCOPE(Cn3D)

#endif // CN3D_REFINER_INTERFACE__HPP
