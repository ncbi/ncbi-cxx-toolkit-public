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
*      Pure virtual interface to generic Multiple Sequence Alignment Algorithms (MSAA)
*
* ===========================================================================
*/

#ifndef ALGO_STRUCTURE_MSAA_INTERFACE__HPP
#define ALGO_STRUCTURE_MSAA_INTERFACE__HPP

#include <corelib/ncbistd.hpp>

#include <objects/cdd/Cdd.hpp>

#include <vector>
#include <string>

class wxWindow;


BEGIN_NCBI_SCOPE

class MSAAInterface
{
public:
    // required: initial alignment as a CD containing sequences and blocked pairwise alignments;
    // generally the order of sequences should match the order of rows in the multiple alignment
    virtual bool SetInitialAlignment(const objects::CCdd&, unsigned int nBlocks, unsigned int nRows) = 0;

    // optional: various per-row properties; rows are assumed to match the order of
    // the initial pairwise alignments, that is the master is row 0, the dependent
    // of the first pairwise alignment is row 1, the dependent of the second
    // pairwise alignment is row 2, etc. Each vector of row info must match exactly
    // the total number of rows in the alignment.
    virtual bool SetRowTitles(const vector < string >&) = 0;            // text title of each sequence
    virtual bool SetRowsWithStructure(const vector < bool >&) = 0;      // rows associated with PDB structure
    virtual bool SetRowsToRealign(const vector < bool >&) = 0;          // rows that should be realigned (default all but master)

    // optional: blocks to realign (default all); vector should match exactly the
    // number of blocks in the alignment
    virtual bool SetBlocksToRealign(const vector < bool >&) = 0;

    // run the algorithm; returns true on success
    virtual bool Run(objects::CCdd::TSeqannot&) = 0;

    // the derived class will probably have some options structure, which will be specific to
    // that class, so there's not much that can be put in this interface for that...

protected:
    virtual ~MSAAInterface(void) { }
};

END_NCBI_SCOPE

#endif // ALGO_STRUCTURE_MSAA_INTERFACE__HPP
