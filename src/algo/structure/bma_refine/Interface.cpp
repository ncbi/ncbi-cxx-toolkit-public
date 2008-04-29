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

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <algo/structure/bma_refine/Interface.hpp>
#include <algo/structure/bma_refine/RefinerEngine.hpp>
#include <algo/structure/struct_util/struct_util.hpp>


BEGIN_SCOPE(align_refine)

// diagnostic streams
#define TRACEMSG(stream) ERR_POST(ncbi::Trace << stream)
#define INFOMSG(stream) ERR_POST(ncbi::Info << stream)
#define WARNINGMSG(stream) ERR_POST(ncbi::Warning << stream)
#define ERRORMSG(stream) ERR_POST(ncbi::Error << stream)
#define FATALMSG(stream) ERR_POST(ncbi::Fatal << stream)

#define ERROR_RETURN_FALSE(stream) \
    do { \
        ERR_POST(ncbi::Error << stream); \
        return false; \
    } while (0)

BMARefinerOptions::~BMARefinerOptions(void)
{
}

BMARefinerInterface::BMARefinerInterface(void)
{
    options.reset(new BMARefinerOptions);

    //  Set some defaults for LOO.
    options->loo.doLOO = true;
    options->loo.nExt = 20;
    options->loo.cExt = 20;
    options->loo.percentile = 1.0;
    options->loo.extension = 10;
    options->loo.selectorCode = align_refine::eWorstScoreFirst;

    //  By default, turn off block editing.  (Not changing in default BEP ctor in
    //  case something else assumes the default is true.)
    options->be.editBlocks = false;

    //  Use a fixed column scoring method for now.
    //  (the '3.3.3' method when using defaults for median, negRows & negScore, and not extension/shrinkageThresholds)
    options->be.columnMethod = align_refine::eCompoundScorer;
    options->be.columnScorerThreshold = 0;  //  this is the 'magic' PSSM score in the compound scorer
}

BMARefinerInterface::~BMARefinerInterface(void)
{
}

bool BMARefinerInterface::SetOptions(const BMARefinerOptions& opts)
{
    *options = opts;
    return true;
}

BMARefinerOptions * BMARefinerInterface::GetOptions(void) const
{
    BMARefinerOptions *ret = new BMARefinerOptions;
    *ret = *options;
    return ret;
}

bool BMARefinerInterface::SetInitialAlignment(const objects::CCdd& cd, unsigned int nB, unsigned int nR)
{
    initialAlignment.Assign(cd);
    nBlocks = nB;
    nRows = nR;
    rowTitles.clear();
    rowTitles.resize(nR);
    return true;
}

bool BMARefinerInterface::SetRowTitles(const vector < string >& t)
{
    if (t.size() != nRows)
        ERROR_RETURN_FALSE("SetRowTitles() - vector size mismatch");

    rowTitles = t;
    return true;
}

bool BMARefinerInterface::SetRowsWithStructure(const vector < bool >& s)
{
    if (s.size() != nRows)
        ERROR_RETURN_FALSE("SetRowsWithStructure() - vector size mismatch");

    for (unsigned int i=0; i<nBlocks; ++i) {
        if (s[i]) {
            rowTitles[i].erase();
        }
    }
    return true;
}

bool BMARefinerInterface::SetRowsToRealign(const vector < bool >& r)
{
    if (r.size() != nRows)
        ERROR_RETURN_FALSE("SetRowsToRealign() - vector size mismatch");

    options->loo.rowsToExclude.clear();

    for (unsigned int i=0; i<nBlocks; ++i) {
        if (!r[i]) {
            options->loo.rowsToExclude.push_back(i);
        }
    }
    return true;
}

bool BMARefinerInterface::SetBlocksToRealign(const vector < bool >& b)
{
    if (b.size() != nBlocks)
        ERROR_RETURN_FALSE("SetBlocksToRealign() - vector size mismatch");

    options->loo.blocks.clear();
    options->be.editableBlocks.clear();

    for (unsigned int i=0; i<nBlocks; ++i) {
        if (b[i]) {
            options->loo.blocks.push_back(i);
            options->be.editableBlocks.insert(i);
        }
    }
    return true;
}

int g_meterMax = 0;
BMARefinerInterface::ProgressCallback g_callback = NULL;

void ProgressAsInt(int num)
{
    if (g_callback && g_meterMax > 0)
        (*g_callback)(((double) num) / g_meterMax);
}

bool BMARefinerInterface::Run(objects::CCdd::TSeqannot& results, ProgressCallback progress)
{
    results.clear();

    try {

        AlignmentUtility au(initialAlignment.GetSequences(), initialAlignment.GetSeqannot());
        if (!au.Okay())
            ERROR_RETURN_FALSE("BMARefinerInterface::Run() - failed to create AlignmentUtility");

        align_refine::CBMARefinerEngine refinerEngine(
            options->loo,
            options->be,
            options->genl.nCycles, options->genl.nTrials, options->genl.lnoFirst, options->genl.verbose, options->genl.trialConvThold);

        if (progress) {
            g_meterMax = refinerEngine.NumTrials() * refinerEngine.NumCycles() * (nRows - options->loo.rowsToExclude.size() - 1);
            g_callback = progress;
        } else {
            g_meterMax = 0;
            g_callback = NULL;
        }

        align_refine::RefinerResultCode result = refinerEngine.Refine(&au, NULL, (progress ? ProgressAsInt : NULL));

        //  store results (in order of decreasing score so best alignment is first).
        bool errorsEncountered = true;
        if (result == align_refine::eRefinerResultOK) {
            const align_refine::RefinedAlignments& refinedAlignments = refinerEngine.GetAllResults();
            align_refine::RefinedAlignmentsRevCIt rcit = refinedAlignments.rbegin(), rend = refinedAlignments.rend();
            for (; rcit != rend; ++rcit) {
                errorsEncountered &= (rcit->second.au == NULL);  // ends up true only if all are null
                if (rcit->second.au)
                    results.insert(results.end(), rcit->second.au->GetSeqAnnots().begin(), rcit->second.au->GetSeqAnnots().end());
            }
        }

        // no alignment or block aligner failed (caller should revert to using original alignment)
        if (errorsEncountered || result != align_refine::eRefinerResultOK) {
            if (result == align_refine::eRefinerResultNoRowsToRefine) {
                ERRORMSG("Alignment refiner did not execute:  no rows eligible for refinement.\nTry again with different parameter settings.\nSee message log for details");
                errorsEncountered = false; //  this avoids another 'Severe Error' in calling function
            } else {
                ERRORMSG("Alignment refiner encountered problem(s) (code " << result << ") - current alignment unchanged.\nSee message log for details");
            }
        }

        return !errorsEncountered;

    } catch (...) {
        ERRORMSG("BMARefinerInterface::Run() - caught unexpected exception");
    }
    return false;
}

END_SCOPE(align_refine)

