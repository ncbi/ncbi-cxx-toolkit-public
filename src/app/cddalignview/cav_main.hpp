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
*      Main application class for CDDAlignmentViewer
*
* ===========================================================================
*/

#ifndef CAV_MAIN__HPP
#define CAV_MAIN__HPP

#include <corelib/ncbiapp.hpp>


BEGIN_NCBI_SCOPE

// class for standalone application
class CAVApp : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};

END_NCBI_SCOPE

#endif // CAV_MAIN__HPP

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2003/03/19 05:33:43  thiessen
* move to src/app/cddalignview
*
* Revision 1.8  2003/02/03 17:52:03  thiessen
* move CVS Log to end of file
*
* Revision 1.7  2003/01/21 12:32:23  thiessen
* move includes into src dir
*
* Revision 1.6  2002/11/08 19:38:15  thiessen
* add option for lowercase unaligned in FASTA
*
* Revision 1.5  2001/01/29 18:13:41  thiessen
* split into C-callable library + main
*
* Revision 1.4  2001/01/25 20:18:39  thiessen
* fix in-memory asn read/write
*
* Revision 1.3  2001/01/25 00:50:51  thiessen
* add command-line args; can read asn data from stdin
*
* Revision 1.2  2001/01/22 15:54:11  thiessen
* correctly set up ncbi namespacing
*
* Revision 1.1  2001/01/22 13:15:52  thiessen
* initial checkin
*
*/
