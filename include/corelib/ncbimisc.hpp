#ifndef NCBISTD__HPP
#define NCBISTD__HPP

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
* Author:  Denis Vakatov
*
* File Description:
*   The NCBI C++ standards
*
* --------------------------------------------------------------------------
* $Log$
* Revision 1.7  1998/11/04 23:46:36  vakatov
* Fixed the "ncbidbg/diag" header circular dependencies
*
* Revision 1.6  1998/10/30 20:08:31  vakatov
* Fixes to (first-time) compile and test-run on MSVS++
*
* Revision 1.5  1998/10/27 19:51:53  vakatov
* + #include <ncbistre.hpp>
* It was not really necessary to #include all this stuff to here,
* but this should force people to use CNcbiXXX stream wrappers
*
* Revision 1.4  1998/10/21 19:24:43  vakatov
* Moved all STL-related code to "ncbistl.hpp"
* Import NCBI typedefs and limits from "ncbitype.h"
*
* ==========================================================================
*/

#include <ncbitype.h>
#include <ncbidbg.hpp>
#include <ncbiexpt.hpp>

#endif /* NCBISTD__HPP */
