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
* Revision 1.1  2000/11/30 15:49:37  thiessen
* add show/hide rows; unpack sec. struc. and domain features
*
* ===========================================================================
*/

#include "cn3d/cn3d_colors.hpp"


BEGIN_SCOPE(Cn3D)

Colors::Colors(void)
{
// default colors
    colors[eHighlight].Set(1, 1, 0);
    colors[eHelix].Set(.1, .9, .1);
    colors[eStrand].Set(.9, .7, .2);
    colors[eCoil].Set(.3, .9, .9);
}

END_SCOPE(Cn3D)
