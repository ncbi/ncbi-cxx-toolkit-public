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
*       class to isolate and run the threader
*
* ===========================================================================
*/

#ifndef CN3D_THREADER__HPP
#define CN3D_THREADER__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistl.hpp>

#include <map>
#include <list>
#include <vector>

#include <algo/structure/threader/thrdatd.h>

#include "vector_math.hpp"


BEGIN_SCOPE(Cn3D)

// for convenience, optional threading parameters are all specified in this block
class ThreaderOptions
{
public:
    bool mergeAfterEachSequence;
    bool freezeIsolatedBlocks;
    double weightPSSM;
    double loopLengthMultiplier;
    int nRandomStarts;
    int nResultAlignments;
    int terminalResidueCutoff;

    ThreaderOptions(void);
};

extern ThreaderOptions globalThreaderOptions;

class BlockMultipleAlignment;
class Sequence;
class StructureBase;
class SequenceViewer;
class AlignmentManager;

class Threader
{
public:
    Threader(AlignmentManager *parentAlnMgr);
    ~Threader(void);

    static const unsigned int SCALING_FACTOR;
    static const std::string ThreaderResidues;

    // create new BlockMultipleAlignments from the given multiple and master/dependent pairs; returns
    // true if threading successful. If so, depending on options, nRowsAddedToMultiple will be
    // merged into the multiple, and newAlignments will contain all un-merged master/dependent pairs
    typedef std::list < BlockMultipleAlignment * > AlignmentList;
    bool Realign(const ThreaderOptions& options, BlockMultipleAlignment *masterMultiple,
        const AlignmentList *originalAlignments, AlignmentList *newAlignments,
        unsigned int *nRowsAddedToMultiple, SequenceViewer *sequenceViewer);

    // calculate scores for each row in the alignment (and store them in the alignment itself)
    bool CalculateScores(const BlockMultipleAlignment *multiple, double weightPSSM);

    // geometry violations - for each row of an alignment, get a list of seqIndex
    // (from, to) pairs for offending regions; return total number of violations
    typedef std::list < std::pair < unsigned int, unsigned int > > IntervalList;
    typedef std::vector < IntervalList > GeometryViolationsForRow;
    unsigned int GetGeometryViolations(const BlockMultipleAlignment *multiple,
        GeometryViolationsForRow *violations);

    // estimate the number of random starts needed to thread an alignment based on
    // the number of blocks to be aligned
    static unsigned int EstimateNRandomStarts(const BlockMultipleAlignment *coreAlignment,
        const BlockMultipleAlignment *toBeThreaded);

    // to hold virtual residue, sidechain positions
    enum { MISSING_COORDINATE = 0, VIRTUAL_RESIDUE, VIRTUAL_PEPTIDE };
    typedef struct {
        unsigned char type;
        Vector coord;
        int disulfideWith;    // if Cysteine, virtual coord index of any disulfide-bound Cys; -1 otherwise
    } VirtualCoordinate;
    typedef std::vector < VirtualCoordinate > VirtualCoordinateList;

    // for (temporary) storage of contacts
    typedef struct {
        int
          vc1, vc2,     // virtual coord index
          distanceBin;
    } Contact;
    typedef std::list < Contact > ContactList;

private:
    AlignmentManager *alignmentManager;

    // holds Fld_Mtf structures already calculated for a given object (Molecule or Sequence)
    typedef std::map < const StructureBase *, Fld_Mtf * > ContactMap;
    ContactMap contacts;

    // threading structure setups
    Cor_Def * CreateCorDef(const BlockMultipleAlignment *multiple, double loopLengthMultiplier);
    Qry_Seq * CreateQrySeq(const BlockMultipleAlignment *multiple,
        const BlockMultipleAlignment *pairwise, int terminalCutoff);
    Rcx_Ptl * CreateRcxPtl(double weightContacts);
    Gib_Scd * CreateGibScd(bool fast, unsigned int nRandomStarts);
    Fld_Mtf * CreateFldMtf(const Sequence *masterSequence);
    Seq_Mtf * CreateSeqMtf(const BlockMultipleAlignment *multiple, double weightPSSM);
};

END_SCOPE(Cn3D)

#endif // CN3D_THREADER__HPP
