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
*      implementation of BMA refiner as MSAAInterface
*
* ===========================================================================
*/

#ifndef BMA_REFINER__MSAA_INTERFACE__HPP
#define BMA_REFINER__MSAA_INTERFACE__HPP

#include <algo/structure/MSAAInterface.hpp>
#include <algo/structure/bma_refine/RefinerDefs.hpp>


BEGIN_SCOPE(align_refine)

class CBMARefinerEngine;
class BMARefinerOptionsDialog;
class BMARefinerOptions;

class BMARefinerInterface : public MSAAInterface
{
    friend class BMARefinerOptionsDialog;

public:
    BMARefinerInterface(void);
    virtual ~BMARefinerInterface(void);

    // required: initial alignment as a CD containing sequences and blocked pairwise alignments;
    // generally the order of sequences should match the order of rows in the multiple alignment
    virtual bool SetInitialAlignment(const objects::CCdd& cd, unsigned int nBlocks, unsigned int nRows);

    // optional: various per-row properties; rows are assumed to match the order of
    // the initial pairwise alignments, that is the master is row 0, the dependent
    // of the first pairwise alignment is row 1, the dependent of the second
    // pairwise alignment is row 2, etc. Each vector of row info must match exactly
    // the total number of rows in the alignment.
    virtual bool SetRowTitles(const vector < string >& t);            // text title of each sequence
    virtual bool SetRowsWithStructure(const vector < bool >& s);      // rows associated with PDB structure
    virtual bool SetRowsToRealign(const vector < bool >& r);          // rows that should be realigned (default all but master)

    // optional: blocks to realign (default all); vector should match exactly the
    // number of blocks in the alignment
    virtual bool SetBlocksToRealign(const vector < bool >& b);

    // run the algorithm; returns true on success
    virtual bool Run(objects::CCdd::TSeqannot& results, ProgressCallback progress = NULL);

    // options stuff
    bool SetOptions(const BMARefinerOptions& opts);
    BMARefinerOptions * GetOptions(void) const;   // returns new copy of current options, that caller should destroy

private:
    objects::CCdd initialAlignment;
    unsigned int nBlocks, nRows;
    vector < string > rowTitles;
    auto_ptr < BMARefinerOptions > options;
    auto_ptr < align_refine::CBMARefinerEngine > m_refinerEngine;
};

struct GeneralRefinerParams {
    bool lnoFirst;          //  true if do Leave-N-Out phase before block edit phase
    unsigned int nTrials;   //  number of independent refinement trials
    unsigned int nCycles;   //  number of cycles per trial (cycle = LOO + block boundary editing)
    bool verbose;
    double trialConvThold;  //  trial convergence threshold:  % change < this value

    GeneralRefinerParams() {
        lnoFirst = true;
        nTrials = 1;
        nCycles = 3;
        verbose = false;
        trialConvThold = 0.001;
    }
    GeneralRefinerParams(const GeneralRefinerParams& rhs) {
        Copy(rhs);
    }

    GeneralRefinerParams& operator=(const GeneralRefinerParams& rhs) {
        if (this != &rhs) {
            Copy(rhs);
        }
        return *this;
    }

    void Copy(const GeneralRefinerParams& rhs) {
        lnoFirst = rhs.lnoFirst;
        nTrials = rhs.nTrials;
        nCycles = rhs.nCycles;
        verbose = rhs.verbose;
        trialConvThold = rhs.trialConvThold;
    }
};

class BMARefinerOptions
{
public:
    GeneralRefinerParams genl;
    align_refine::LeaveOneOutParams loo;
    align_refine::BlockEditingParams be;

    BMARefinerOptions& operator = (const BMARefinerOptions& orig)
    {
        genl.Copy(orig.genl);
        loo.Copy(orig.loo);
        be.Copy(orig.be);
        return *this;
    }

    ~BMARefinerOptions(void);
};

END_SCOPE(align_refine)

#endif // BMA_REFINER__MSAA_INTERFACE__HPP
