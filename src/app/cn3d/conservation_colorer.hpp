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

extern char ScreenResidueCharacter(char original);

END_SCOPE(Cn3D)

#endif // CN3D_CONSERVATION_COLORER__HPP

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.17  2005/03/08 17:22:31  thiessen
* apparently working C++ PSSM generation
*
* Revision 1.16  2004/09/27 15:33:04  thiessen
* add block info content optimization on ctrl+shift+click
*
* Revision 1.15  2004/02/19 17:04:54  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.14  2003/02/13 18:31:39  thiessen
* separate basic from fit color calculation
*
* Revision 1.13  2003/02/06 16:39:53  thiessen
* add block row fit coloring
*
* Revision 1.12  2003/02/03 19:20:03  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.11  2003/01/30 14:00:23  thiessen
* add Block Z Fit coloring
*
* Revision 1.10  2003/01/28 21:07:56  thiessen
* add block fit coloring algorithm; tweak row dragging; fix style bug
*
* Revision 1.9  2001/08/24 00:40:57  thiessen
* tweak conservation colors and opengl font handling
*
* Revision 1.8  2001/06/04 14:33:54  thiessen
* add proximity sort; highlight sequence on browser launch
*
* Revision 1.7  2001/06/02 17:22:58  thiessen
* fixes for GCC
*
* Revision 1.6  2001/03/22 00:32:36  thiessen
* initial threading working (PSSM only); free color storage in undo stack
*
* Revision 1.5  2001/02/13 20:31:45  thiessen
* add information content coloring
*
* Revision 1.4  2000/10/16 14:25:20  thiessen
* working alignment fit coloring
*
* Revision 1.3  2000/10/05 18:34:35  thiessen
* first working editing operation
*
* Revision 1.2  2000/10/04 17:40:45  thiessen
* rearrange STL #includes
*
* Revision 1.1  2000/09/20 22:24:57  thiessen
* working conservation coloring; split and center unaligned justification
*
*/
