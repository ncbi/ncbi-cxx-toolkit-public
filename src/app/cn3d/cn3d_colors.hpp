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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.10  2001/08/24 00:40:57  thiessen
* tweak conservation colors and opengl font handling
*
* Revision 1.9  2001/08/21 01:10:13  thiessen
* add labeling
*
* Revision 1.8  2001/08/09 19:07:19  thiessen
* add temperature and hydrophobicity coloring
*
* Revision 1.7  2001/07/12 17:34:22  thiessen
* change domain mapping ; add preliminary cdd annotation GUI
*
* Revision 1.6  2001/05/11 02:10:04  thiessen
* add better merge fail indicators; tweaks to windowing/taskbar
*
* Revision 1.5  2001/05/09 17:14:52  thiessen
* add automatic block removal upon demotion
*
* Revision 1.4  2001/04/05 22:54:50  thiessen
* change bg color handling ; show geometry violations
*
* Revision 1.3  2001/03/22 00:32:35  thiessen
* initial threading working (PSSM only); free color storage in undo stack
*
* Revision 1.2  2000/12/01 19:34:43  thiessen
* better domain assignment
*
* Revision 1.1  2000/11/30 15:49:07  thiessen
* add show/hide rows; unpack sec. struc. and domain features
*
* ===========================================================================
*/

#ifndef CN3D_COLORS__HPP
#define CN3D_COLORS__HPP

#include <corelib/ncbistl.hpp>

#include <vector>

#include "cn3d/vector_math.hpp"


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

        // misc other colors
        eNoDomain,
        eNoTemperature,
        eNoHydrophobicity,

        eNumColors
    };
    const Vector& Get(eColor which) const;

    // color cycles
    enum eColorCycle {
        eCycle1 = 0,    // for molecule, domain, object coloring

        eNumColorCycles
    };
    static const int nCycle1;
    const Vector& Get(eColorCycle which, int n) const;

    // color maps
    enum eColorMap {
        eTemperatureMap = 0,
        eHydrophobicityMap,
        eConservationMap,

        eNumColorMaps
    };
    static const int nTemperatureMap, nHydrophobicityMap, nConservationMap;
    Vector Get(eColorMap which, double f) const;
    const Vector* Get(eColorMap which, int index) const;

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
