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
* Revision 1.6  2001/05/09 17:15:06  thiessen
* add automatic block removal upon demotion
*
* Revision 1.5  2001/04/05 22:55:35  thiessen
* change bg color handling ; show geometry violations
*
* Revision 1.4  2001/03/22 00:33:16  thiessen
* initial threading working (PSSM only); free color storage in undo stack
*
* Revision 1.3  2000/12/22 19:26:40  thiessen
* write cdd output files
*
* Revision 1.2  2000/12/01 19:35:57  thiessen
* better domain assignment; basic show/hide mechanism
*
* Revision 1.1  2000/11/30 15:49:37  thiessen
* add show/hide rows; unpack sec. struc. and domain features
*
* ===========================================================================
*/

#include "cn3d/cn3d_colors.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

// the global Colors object
static const Colors colors;

const Colors * GlobalColors(void)
{
    return &colors;
}


Colors::Colors(void)
{
    // default colors
    colors[eHighlight].Set(1, 1, 0);
    colors[eUnalignedInUpdate].Set(1, .8, .8);
    colors[eGeometryViolation].Set(.6, 1, .6);
    colors[eRealignBlock].Set(.8, .8, .8);

    colors[eHelix].Set(.1, .9, .1);
    colors[eStrand].Set(.9, .7, .2);
    colors[eCoil].Set(.3, .9, .9);

    // colors for cycle1 (basically copied from Cn3D 3.0)
    cycle1[0].Set(1, 0, 1);
    cycle1[1].Set(0, 0, 1);
    cycle1[2].Set(139.0/255, 87.0/255, 66.0/255);
    cycle1[3].Set(0, 1, .5);
    cycle1[4].Set(.7, .7, .7);
    cycle1[5].Set(1, 165.0/255, 0);
    cycle1[6].Set(1, 114.0/255, 86.0/255);
    cycle1[7].Set(0, 1, 0);
    cycle1[8].Set(0, 1, 1);
    cycle1[9].Set(1, 236.0/255, 139.0/255);
}

const Vector& Colors::Get(eColor which, int n) const
{
    if (which >= 0 && n >= 0) {

        if (which < eNumColors)
            return colors[which];

        else if (which == eCycle1)
            return cycle1[n % nCycle1];
    }

    ERR_POST(Warning << "Colors::Get() - bad color #");
    return colors[0];
}

END_SCOPE(Cn3D)
