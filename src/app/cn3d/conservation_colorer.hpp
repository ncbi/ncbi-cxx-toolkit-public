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
*      Classes to color alignment blocks by sequence conservation
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2000/09/20 22:24:57  thiessen
* working conservation coloring; split and center unaligned justification
*
* ===========================================================================
*/

#ifndef CN3D_CONSERVATION_COLORER__HPP
#define CN3D_CONSERVATION_COLORER__HPP

#include <map>
#include <vector>

#include <corelib/ncbistl.hpp>

#include "cn3d/vector_math.hpp"


BEGIN_SCOPE(Cn3D)

class UngappedAlignedBlock;

class ConservationColorer
{
public:
    ConservationColorer(void);

    // add an aligned block to the profile
    void AddBlock(const UngappedAlignedBlock *block);

    // create colors - should be done only after all blocks have been added
    void CalculateConservationColors(void);

    static const Vector ZeroIdentityColor, FullIdentityColor;

private:
    typedef std::map < const UngappedAlignedBlock *, std::vector < int > > BlockMap;
    BlockMap blocks;
    int GetProfileIndex(const UngappedAlignedBlock *block, int blockColumn) const
        { return blocks.find(block)->second.at(blockColumn); }

    int nColumns;

    std::vector < bool > identities;

    typedef std::vector < Vector > AlignmentColors;
    AlignmentColors varietyColors, weightedVarietyColors;

public:
    // color accessors
    const Vector *GetIdentityColor(const UngappedAlignedBlock *block, int blockColumn) const
        { return &(identities[GetProfileIndex(block, blockColumn)]
            ? FullIdentityColor : ZeroIdentityColor); }

    const Vector *GetVarietyColor(const UngappedAlignedBlock *block, int blockColumn) const
        { return &(varietyColors[GetProfileIndex(block, blockColumn)]); }

    const Vector *GetWeightedVarietyColor(const UngappedAlignedBlock *block, int blockColumn) const
        { return &(weightedVarietyColors[GetProfileIndex(block, blockColumn)]); }
};

END_SCOPE(Cn3D)

#endif // CN3D_CONSERVATION_COLORER__HPP
