#ifndef NCBIDBG_P__HPP
#define NCBIDBG_P__HPP

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
 * Author:  Andrei Gourianov, gouriano@ncbi.nlm.nih.gov
 *
 * File Description:
 *   declaration of debugging function(s)
 * 
 */

#include <corelib/ncbi_safe_static.hpp>
#include <corelib/ncbithr.hpp>

#include <assert.h>

BEGIN_NCBI_SCOPE


#if defined(_DEBUG)

// 'simple' verify
#  define xncbi_Verify(expression) assert(expression)

// Abort execution (by default), or throw exception (if explicitly specified
// by calling xncbi_SetValidateAction(eValidate_Throw)) if
// "expression" evaluates to FALSE.
#  define xncbi_Validate(expression, message) \
    do { \
        if ( !(expression) ) \
            NCBI_NS_NCBI::CNcbiDiag::DiagValidate(DIAG_COMPILE_INFO, #expression, message); \
    } while ( 0 )

#else // _DEBUG

// 'simple' verify - just evaluate the expression
#  define xncbi_Verify(expression) while ( expression ) break

// Throw exception if "expression" evaluates to FALSE.
#  define xncbi_Validate(expression, message) \
    do { \
        if ( !(expression) ) \
            NCBI_NS_NCBI::CNcbiDiag::DiagValidate(DIAG_COMPILE_INFO, #expression, message); \
    } while ( 0 )

#endif // _DEBUG


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.14  2006/12/18 18:07:16  ucko
 * Fix typo in previous revision.
 *
 * Revision 1.13  2006/12/18 17:50:05  gouriano
 * Get rid of compiler warnings
 *
 * Revision 1.12  2006/10/24 18:56:17  ivanov
 * Cosmetics: replaced tabulation with spaces
 *
 * Revision 1.11  2004/09/22 13:32:17  kononenk
 * "Diagnostic Message Filtering" functionality added.
 * Added function SetDiagFilter()
 * Added class CDiagCompileInfo and macro DIAG_COMPILE_INFO
 * Module, class and function attribute added to CNcbiDiag and CException
 * Parameters __FILE__ and __LINE in CNcbiDiag and CException changed to
 * CDiagCompileInfo + fixes on derived classes and their usage
 * Macro NCBI_MODULE can be used to set default module name in cpp files
 *
 * Revision 1.10  2002/09/19 20:05:42  vasilche
 * Safe initialization of static mutexes
 *
 * Revision 1.9  2002/07/15 18:17:23  gouriano
 * renamed CNcbiException and its descendents
 *
 * Revision 1.8  2002/07/11 14:18:26  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 1.7  2002/04/11 21:08:01  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.6  2002/04/11 20:00:45  ivanov
 * Returned standard assert() vice CORE_ASSERT()
 *
 * Revision 1.5  2002/04/10 18:32:04  ivanov
 * Fixed typo
 *
 * Revision 1.4  2002/04/10 18:30:02  ivanov
 * Added new macros CORE_ASSERT
 *
 * Revision 1.3  2002/04/10 14:47:04  ivanov
 * Changed assert() to condition with call Abort()
 *
 * Revision 1.2  2001/12/14 17:58:53  gouriano
 * changed GetValidateAction so it never returns Default
 *
 * Revision 1.1  2001/12/13 19:46:59  gouriano
 * added xxValidateAction functions
 *
 * ===========================================================================
 */

#endif // NCBIDBG_P__HPP
