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
*      Holds various color values and cycles
*
* ===========================================================================
*/

#ifndef CN3D_COLORS__HPP
#define CN3D_COLORS__HPP

#include <corelib/ncbistl.hpp>

#include <vector>

#include "vector_math.hpp"


BEGIN_SCOPE(Cn3D)

// for now, there is only a single global Colors object, which for convenience
// can be accessed anywhere via this function
class Colors;
const Colors * GlobalColors(void);


class Colors
{
public:
    Colors(void);

    // individual colors
    enum eColor {
        // sequence viewer colors
        eHighlight = 0,
        eMergeFail,
        eGeometryViolation,
        eMarkBlock,

        // secondary structures
        eHelix,
        eStrand,
        eCoil,

        // charge
        ePositive,
        eNegative,
        eNeutral,

        // nucleotide residues
        eNuc_A,
        eNuc_T_U,
        eNuc_C,
        eNuc_G,
        eNuc_X,

        // misc other colors
        eNoDomain,
        eNoTemperature,
        eNoHydrophobicity,
        eUnaligned,
        eNoCoordinates,

        eNumColors
    };
    const Vector& Get(eColor which) const;

    // color cycles
    enum eColorCycle {
        eCycle1 = 0,    // for molecule, domain, object coloring

        eNumColorCycles
    };
    static const unsigned int nCycle1;
    const Vector& Get(eColorCycle which, unsigned int n) const;

    // color maps
    enum eColorMap {
        eTemperatureMap = 0,
        eHydrophobicityMap,
        eConservationMap,
        eRainbowMap,

        eNumColorMaps
    };
    static const unsigned int nTemperatureMap, nHydrophobicityMap, nConservationMap, nRainbowMap;
    Vector Get(eColorMap which, double f) const;
    const Vector* Get(eColorMap which, unsigned int index) const;

    // light or dark color?
    static bool IsLightColor(const Vector& color)
    {
        return ((color[0] + color[1] + color[2]) > 1.5);
    }

private:
    // storage for individual colors
    Vector colors[eNumColors];

    // storage for color cycles
    std::vector < std::vector < Vector > > cycleColors;

    // storage for color maps
    std::vector < std::vector < Vector > > mapColors;
};

END_SCOPE(Cn3D)

#endif // CN3D_COLORS__HPP
