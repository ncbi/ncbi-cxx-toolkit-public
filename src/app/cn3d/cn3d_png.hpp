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
*      save structure window to PNG file
*
* ===========================================================================
*/

#ifndef CN3D_PNG__HPP
#define CN3D_PNG__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>


BEGIN_SCOPE(Cn3D)

class Cn3DGLCanvas;
class OpenGLRenderer;

// export a PNG - will bring up a dialog asking for filename and size params
bool ExportPNG(Cn3DGLCanvas *glCanvas,
    // the following parameters should only be used in non-windowed mode (and glCanvas=NULL)
    OpenGLRenderer *renderer = NULL,
    const std::string& outputFilename = kEmptyStr,
    int outputWidth = 0,
    int outputHeight = 0,
    bool outputInterlaced = false
);

END_SCOPE(Cn3D)

#endif // CN3D_PNG__HPP

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2003/02/03 19:20:03  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.1  2001/10/23 13:53:38  thiessen
* add PNG export
*
*/
