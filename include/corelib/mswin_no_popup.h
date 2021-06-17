#ifndef CORELIB___MSWIN_NO_POPUP__H
#define CORELIB___MSWIN_NO_POPUP__H

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
 * File Description:   Suppress popup messages on execution errors.
 *                     MS Windows specific.
 *
 * Include this header only to applications, not libraries.
 * 
 */

#include <ncbiconf.h>

/* To avoid code duplication reuse code from <common/test_assert[_impl].h>.
   This preprocessor macro turn OFF all assert-related tune-ups and turn ON 
   suppress popup messages code. Environment variable DIAG_SILENT_ABORT
   must be set to "Y" or "y" to suppress popup messages.
*/
#define NCBI_MSWIN_NO_POPUP

/* In case anyone needs to always disable the popup messages (regardless of DIAG_SILENT_ABDORT)
   another pre-processor macro can be defined before #include'ing either 
   <corelib/mswin_no_popup.h> (or <common/test_assert.h>).
*/
/* #define NCBI_MSWIN_NO_POPUP_EVER */
 
#include <common/test_assert.h>


#endif  /* CORELIB___MSWIN_NO_POPUP__H */
