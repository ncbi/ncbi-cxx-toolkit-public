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
* ===========================================================================
*/

#ifndef CN3D_CONSERVATION_COLORER__HPP
#define CN3D_CONSERVATION_COLORER__HPP

#include <corelib/ncbistl.hpp>

#include <map>
#include <vector>

#include "vector_math.hpp"
#include "cn3d_colors.hpp"


BEGIN_SCOPE(Cn3D)

class UngappedAlignedBlock;
class BlockMultipleAlignment;

class ConservationColorer
{
private:
    const BlockMultipleAlignment *alignment;

    typedef std::map < const UngappedAlignedBlock *, std::vector < int > > BlockMap;
    BlockMap blocks;

    typedef std::map < char, int > ColumnProfile;
    typedef std::vector < ColumnProfile > AlignmentProfile;
    AlignmentProfile alignmentProfile;

    int GetProfileIndex(const UngappedAlignedBlock *block, int blockColumn) const
        { return blocks.find(block)->second[blockColumn]; }
    void GetProfileIndexAndResidue(const UngappedAlignedBlock *block, int blockColumn, int row,
        int *profileIndex, char *residue) const;

    int nColumns;
    bool basicColorsCurrent, fitColorsCurrent;

    void CalculateBasicConservationColors(void);
    void CalculateFitConservationColors(void);

    std::vector < bool > identities;

    typedef std::vector < Vector > ColumnColors;
    ColumnColors varietyColors, weightedVarietyColors, informationContentColors;

    typedef std::map < char, Vector > ResidueColors;
    typedef std::vector < ResidueColors > FitColors;
    FitColors fitColors;

    typedef std::map < const UngappedAlignedBlock * , std::vector < Vector > >  BlockFitColors;
    BlockFitColors blockFitColors, blockZFitColors, blockRowFitColors;

public:
    ConservationColorer(const BlockMultipleAlignment *parent);

    // add an aligned block to the profile
    void AddBlock(const UngappedAlignedBlock *block);

    // frees memory used by color storage (but keeps blocks around)
    void FreeColors(void);

    // clears everything, including alignment blocks
    void Clear(void);

    // color accessors
    const Vector *GetIdentityColor(const UngappedAlignedBlock *block, int blockColumn)
    {
        CalculateBasicConservationColors();
        return GlobalColors()->Get(Colors::eConservationMap,
            (identities[GetProfileIndex(block, blockColumn)] ? Colors::nConservationMap - 1 : 0));
    }

    const Vector *GetVarietyColor(const UngappedAlignedBlock *block, int blockColumn)
    {
        CalculateBasicConservationColors();
        return &(varietyColors[GetProfileIndex(block, blockColumn)]);
    }

    const Vector *GetWeightedVarietyColor(const UngappedAlignedBlock *block, int blockColumn)
    {
        CalculateBasicConservationColors();
        return &(weightedVarietyColors[GetProfileIndex(block, blockColumn)]);
    }

    const Vector *GetInformationContentColor(const UngappedAlignedBlock *block, int blockColumn)
    {
        CalculateBasicConservationColors();
        return &(informationContentColors[GetProfileIndex(block, blockColumn)]);
    }

    const Vector *GetFitColor(const UngappedAlignedBlock *block, int blockColumn, int row)
    {
        int profileIndex;
        char residue;
        CalculateFitConservationColors();
        GetProfileIndexAndResidue(block, blockColumn, row, &profileIndex, &residue);
        return &(fitColors[profileIndex].find(residue)->second);
    }

    const Vector *GetBlockFitColor(const UngappedAlignedBlock *block, int row)
    {
        CalculateFitConservationColors();
        return &(blockFitColors.find(block)->second[row]);
    }

    const Vector *GetBlockZFitColor(const UngappedAlignedBlock *block, int row)
    {
        CalculateFitConservationColors();
        return &(blockZFitColors.find(block)->second[row]);
    }

    const Vector *GetBlockRowFitColor(const UngappedAlignedBlock *block, int row)
    {
        CalculateFitConservationColors();
        return &(blockRowFitColors.find(block)->second[row]);
    }
};

// screen residue to be in ncbistdaa
extern char ScreenResidueCharacter(char original);

END_SCOPE(Cn3D)

#endif // CN3D_CONSERVATION_COLORER__HPP
