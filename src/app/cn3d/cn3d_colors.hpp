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

    enum eColor {
        // sequence viewer colors
        eHighlight = 0,
        eUnalignedInUpdate,
        eGeometryViolation,
        eRealignBlock,

        // secondary structures
        eHelix,
        eStrand,
        eCoil,

        eNumColors,

        // color cycles
        eCycle1
    };

private:
    // storage for individual colors
    Vector colors[eNumColors];

    // storage for color cycles
    enum {
        nCycle1 = 10
    };
    Vector cycle1[nCycle1];

public:
    // color accessors
    const Vector& Get(eColor which, int n = 0) const;
};

END_SCOPE(Cn3D)

#endif // CN3D_COLORS__HPP
