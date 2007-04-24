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

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <math.h>

#include "remove_header_conflicts.hpp"

#include "cn3d_colors.hpp"
#include "cn3d_tools.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

// the global Colors object
static const Colors colors;

const Colors * GlobalColors(void)
{
    return &colors;
}


// # colors for color cycles
const unsigned int
    Colors::nCycle1 = 10;

// # colors for color maps (must be >1)
const unsigned int
    Colors::nTemperatureMap = 5,
    Colors::nHydrophobicityMap = 3,
    Colors::nConservationMap = 2,
    Colors::nRainbowMap = 7;


Colors::Colors(void)
{
    // default colors
    colors[eHighlight].Set(1, 1, 0);
    colors[eMergeFail].Set(1, .8, .8);
    colors[eGeometryViolation].Set(.6, 1, .6);
    colors[eMarkBlock].Set(.8, .8, .8);

    colors[eHelix].Set(.1, .9, .1);
    colors[eStrand].Set(.9, .7, .2);
    colors[eCoil].Set(.3, .9, .9);

    colors[ePositive].Set(.2, .3, 1.0);
    colors[eNegative].Set(.9, .2, .2);
    colors[eNeutral].Set(.6, .6, .6);

    colors[eNuc_A].Set(.1, .9, .2);
    colors[eNuc_T_U].Set(.9, .1, .2);
    colors[eNuc_C].Set(.1, .2, 1.0);
    colors[eNuc_G].Set(.85, .7, 0);
    colors[eNuc_X].Set(.6, .6, .6);

    colors[eNoDomain].Set(.4, .4, .4);
    colors[eNoTemperature].Set(.4, .4, .4);
    colors[eNoHydrophobicity].Set(.4, .4, .4);
    colors[eUnaligned].Set(.4, .4, .4);
    colors[eNoCoordinates].Set(.4, .4, .4);

    // cycles and maps
    cycleColors.resize(eNumColorCycles);
    mapColors.resize(eNumColorMaps);

    // colors for cycle1
    cycleColors[eCycle1].resize(nCycle1);
    cycleColors[eCycle1][0].Set(1, 0, 1);
    cycleColors[eCycle1][1].Set(0, 0, 1);
    cycleColors[eCycle1][2].Set(139.0/255, 87.0/255, 66.0/255);
    cycleColors[eCycle1][3].Set(0, 1, .5);
    cycleColors[eCycle1][4].Set(.7, .7, .7);
    cycleColors[eCycle1][5].Set(1, 165.0/255, 0);
    cycleColors[eCycle1][6].Set(1, 114.0/255, 86.0/255);
    cycleColors[eCycle1][7].Set(0, 1, 0);
    cycleColors[eCycle1][8].Set(0, 1, 1);
    cycleColors[eCycle1][9].Set(1, 236.0/255, 139.0/255);

    // colors for temperature map
    mapColors[eTemperatureMap].resize(nTemperatureMap);
    mapColors[eTemperatureMap][0].Set(0.2, 0.2, 0.7);
    mapColors[eTemperatureMap][1].Set(0.1, 0.6, 0.2);
    mapColors[eTemperatureMap][2].Set(0.9, 0.8, 0.2);
    mapColors[eTemperatureMap][3].Set(0.9, 0.2, 0.2);
    mapColors[eTemperatureMap][4].Set(0.9, 0.9, 0.9);

    // colors for hydrophobicity map
    mapColors[eHydrophobicityMap].resize(nHydrophobicityMap);
    mapColors[eHydrophobicityMap][0].Set(0.2, 0.2, 0.7);
    mapColors[eHydrophobicityMap][1].Set(0.2, 0.5, 0.6);
    mapColors[eHydrophobicityMap][2].Set(0.7, 0.4, 0.3);

    // colors for conservation map
    mapColors[eConservationMap].resize(nConservationMap);
    mapColors[eConservationMap][0].Set(0.0, 75.0/255, 1.0);
    mapColors[eConservationMap][1].Set(1.0, 0.0, 0.0);

    // colors for rainbow map
    mapColors[eRainbowMap].resize(nRainbowMap);
    mapColors[eRainbowMap][0].Set(0.9, 0.1, 0.1);
    mapColors[eRainbowMap][1].Set(1.0, 0.5, 0.1);
    mapColors[eRainbowMap][2].Set(0.8, 0.9, 0.1);
    mapColors[eRainbowMap][3].Set(0.1, 0.9, 0.1);
    mapColors[eRainbowMap][4].Set(0.1, 0.1, 1.0);
    mapColors[eRainbowMap][5].Set(0.5, 0.2, 1.0);
    mapColors[eRainbowMap][6].Set(0.9, 0.2, 0.5);
}

const Vector& Colors::Get(eColor which) const
{
    if (which >= 0 && which < eNumColors) return colors[which];
    ERRORMSG("Colors::Get() - bad eColor " << (int) which);
    return colors[0];
}

const Vector& Colors::Get(eColorCycle which, unsigned int n) const
{
    if (which >= 0 && which < eNumColorCycles)
        return cycleColors[which][n % cycleColors[which].size()];
    ERRORMSG("Colors::Get() - bad eColorCycle " << (int) which);
    return cycleColors[0][0];
}

Vector Colors::Get(eColorMap which, double f) const
{
    if (which >= 0 && which < eNumColorMaps && f >= 0.0 && f <= 1.0) {
        const vector < Vector >& colorMap = mapColors[which];
        if (f == 1.0) return colorMap[colorMap.size() - 1];
        double bin = 1.0 / (colorMap.size() - 1);
        unsigned int low = (unsigned int) (f / bin);
        double fraction = fmod(f, bin) / bin;
        const Vector &color1 = colorMap[low], &color2 = colorMap[low + 1];
        return (color1 + fraction * (color2 - color1));
    }
    ERRORMSG("Colors::Get() - bad eColorMap " << (int) which << " at " << f);
    return mapColors[0][0];
}

const Vector* Colors::Get(eColorMap which, unsigned int index) const
{
    if (which < eNumColorMaps && index < mapColors[which].size())
        return &(mapColors[which][index]);
    else
        return NULL;
}

END_SCOPE(Cn3D)
