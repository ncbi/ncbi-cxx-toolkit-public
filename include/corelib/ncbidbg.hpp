#ifndef NCBIDBG__HPP
#define NCBIDBG__HPP

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
 *
 */

 /// @file ncbidbg.hpp
 /// NCBI C++ auxiliary debug macros
 ///
 /// NOTE:These macros are NOT for use in test applications!!!
 ///      Test applications must use normal assert() and must
 ///      include <test/test_assert.h> as a last header file.
 ///      [test apps in 'connect' branch include "test/test_assert.h" instead]


#include <corelib/ncbidiag.hpp>
// Use standard _ASSERT macro on MSVC in Debug modes.
// See NCBI_ASSERT declaration below.
#if defined(NCBI_COMPILER_MSVC)  &&  defined(_DEBUG)
#  include <crtdbg.h>
#endif

/** @addtogroup Debug
 *
 * @{
 */


BEGIN_NCBI_SCOPE

/// Define macros to support debugging.
#if defined(_DEBUG)

#  define _TRACE(message) \
    ( NCBI_NS_NCBI::CNcbiDiag(DIAG_COMPILE_INFO, NCBI_NS_NCBI::eDiag_Trace) \
      << message << NCBI_NS_NCBI::Endm )

#  define NCBI_TROUBLE(mess) \
    NCBI_NS_NCBI::CNcbiDiag::DiagTrouble(DIAG_COMPILE_INFO, mess)

#  if defined(NCBI_COMPILER_MSVC)  &&  defined(_DEBUG)
    // Use standard _ASSERT macro on MSVC in Debug modes
#    define NCBI_ASSERT(expr, mess) \
      if ( !(expr) ) { \
          NCBI_NS_NCBI::CNcbiDiag(DIAG_COMPILE_INFO, NCBI_NS_NCBI::eDiag_Error, eDPF_Trace) << \
              "Assertion failed: (" << \
              (#expr ? #expr : "") << ") " << \
              (mess ? mess : "") << Endm; \
          _ASSERT_BASE((expr), NULL); \
      }
#  else
#    define NCBI_ASSERT(expr, mess) \
      do { if ( !(expr) ) \
          NCBI_NS_NCBI::CNcbiDiag::DiagAssert(DIAG_COMPILE_INFO, #expr, mess); \
      } while ( 0 )
#  endif


#  define NCBI_VERIFY(expr, mess) NCBI_ASSERT(expr, mess)

#  define _DEBUG_ARG(arg) arg

#  define _DEBUG_CODE(code) \
    do { code } while ( 0 )


#else  /* _DEBUG */

#  define _TRACE(message)           ((void)0)
#  define NCBI_TROUBLE(mess)
#  define NCBI_ASSERT(expr, mess)   ((void)0)
#  define NCBI_VERIFY(expr, mess)   while ( expr ) break
#  define _DEBUG_ARG(arg)
#  define _DEBUG_CODE(code)         ((void)0)

#endif  /* else!_DEBUG */


#ifdef _ASSERT
#   undef _ASSERT
#endif
#define _ASSERT(expr)   NCBI_ASSERT(expr, NULL)
#define _VERIFY(expr)   NCBI_VERIFY(expr, NULL)
#define _TROUBLE        NCBI_TROUBLE(NULL)


/// Which action to perform.
///
/// Specify action to be performed when expression under
/// "xncbi_Validate(expr, ...)" evaluates to FALSE.
enum EValidateAction {
    eValidate_Default = 0,  ///< Default action
    eValidate_Abort,        ///< abort() if not valid
    eValidate_Throw         ///< Throw an exception if not valid
};

/// Set the action to be performed.
extern NCBI_XNCBI_EXPORT void xncbi_SetValidateAction(EValidateAction action);

/// Get the action to be performed.
extern NCBI_XNCBI_EXPORT EValidateAction xncbi_GetValidateAction(void);


END_NCBI_SCOPE


/* @} */


/*
 * ==========================================================================
 * $Log$
 * Revision 1.40  2006/12/12 16:55:34  ivanov
 * Use standard _ASSERT macro on MSVC in Debug modes
 *
 * Revision 1.39  2006/10/24 19:11:55  ivanov
 * Cosmetics: replaced tabulation with spaces
 *
 * Revision 1.38  2006/09/12 16:19:36  ivanov
 * Fixed warning about _ASSERT redefinition on MSVC8
 *
 * Revision 1.37  2006/07/24 13:37:40  grichenk
 * Fixed warning in NCBI_VERIFY
 *
 * Revision 1.36  2006/05/23 16:03:54  grichenk
 * Added NCBI_TROUBLE, NCBI_ASSERT, NCBI_VERIFY and _DEBUG_CODE
 *
 * Revision 1.35  2006/02/14 15:48:21  lavr
 * Indenting
 *
 * Revision 1.34  2005/12/27 14:54:36  gouriano
 * Before defining _ASSERT check if it is already defined
 *
 * Revision 1.33  2004/09/22 13:32:16  kononenk
 * "Diagnostic Message Filtering" functionality added.
 * Added function SetDiagFilter()
 * Added class CDiagCompileInfo and macro DIAG_COMPILE_INFO
 * Module, class and function attribute added to CNcbiDiag and CException
 * Parameters __FILE__ and __LINE in CNcbiDiag and CException changed to
 * CDiagCompileInfo + fixes on derived classes and their usage
 * Macro NCBI_MODULE can be used to set default module name in cpp files
 *
 * Revision 1.32  2004/03/10 19:52:42  gorelenk
 * Added NCBI_XNCBI_EXPORT for functions xncbi_SetValidateAction and
 * xncbi_GetValidateAction.
 *
 * Revision 1.31  2003/07/11 12:47:09  siyan
 * Documentation changes.
 *
 * Revision 1.30  2003/03/31 14:29:23  siyan
 * Added doxygen support
 *
 * Revision 1.29  2002/09/24 20:59:47  vasilche
 * Added "extern" keyword.
 *
 * Revision 1.28  2002/09/24 18:27:17  vasilche
 * Removed redundant "extern" keyword
 *
 * Revision 1.27  2002/09/19 20:05:41  vasilche
 * Safe initialization of static mutexes
 *
 * Revision 1.26  2002/04/16 22:03:16  lavr
 * Added a note about proper use of debug macros
 *
 * Revision 1.25  2002/04/10 18:28:09  ivanov
 * Moved CVS log to end of file
 *
 * Revision 1.24  2001/12/13 19:38:01  gouriano
 * *** empty log message ***
 *
 * Revision 1.23  2001/07/30 14:40:47  lavr
 * eDiag_Trace and eDiag_Fatal always print as much as possible
 *
 * Revision 1.22  2001/05/17 14:50:41  lavr
 * Typos corrected
 *
 * Revision 1.21  2000/06/23 15:12:57  thiessen
 * minor change to _ASSERT macro to avoid frequent Mac warnings about semicolons
 *
 * Revision 1.20  2000/05/24 20:01:07  vasilche
 * Added _DEBUG_ARG macro for arguments used only in debug mode.
 *
 * Revision 1.19  2000/03/10 14:17:40  vasilche
 * Added missing namespace specifier to macro.
 *
 * Revision 1.18  1999/12/27 19:39:12  vakatov
 * [_DEBUG]  Forcibly flush ("<< Endm") the diag. stream
 *
 * Revision 1.17  1999/09/27 16:23:19  vasilche
 * Changed implementation of debugging macros (_TRACE, _THROW*, _ASSERT etc),
 * so that they will be much easier for compilers to eat.
 *
 * Revision 1.16  1999/09/23 21:15:48  vasilche
 * Added namespace modifiers.
 *
 * Revision 1.15  1999/05/04 00:03:05  vakatov
 * Removed the redundant severity arg from macro ERR_POST()
 *
 * Revision 1.14  1999/05/03 20:32:25  vakatov
 * Use the (newly introduced) macro from <corelib/ncbidbg.h>:
 *   RETHROW_TRACE,
 *   THROW0_TRACE(exception_class),
 *   THROW1_TRACE(exception_class, exception_arg),
 *   THROW_TRACE(exception_class, exception_args)
 * instead of the former (now obsolete) macro _TRACE_THROW.
 *
 * Revision 1.13  1999/04/30 19:20:56  vakatov
 * Added more details and more control on the diagnostics
 * See #ERR_POST, EDiagPostFlag, and ***DiagPostFlag()
 *
 * Revision 1.12  1999/04/22 14:18:18  vasilche
 * Added _TRACE_THROW() macro which can be configured to make coredump at 
 * some point fo throwing exception.
 *
 * Revision 1.11  1999/03/15 16:08:09  vakatov
 * Fixed "{...}" macros to "do {...} while(0)" lest it to mess up with "if"s
 *
 * Revision 1.10  1999/01/07 16:15:09  vakatov
 * Explicitly specify "NCBI_NS_NCBI::" in the preprocessor macros
 *
 * Revision 1.9  1998/12/28 17:56:26  vakatov
 * New CVS and development tree structure for the NCBI C++ projects
 *
 * Revision 1.8  1998/11/19 22:17:41  vakatov
 * Forgot "()" in the _ASSERT macro
 *
 * Revision 1.7  1998/11/18 21:00:24  vakatov
 * [_DEBUG]  Fixed a typo in _ASSERT macro
 *
 * Revision 1.6  1998/11/06 22:42:37  vakatov
 * Introduced BEGIN_, END_ and USING_ NCBI_SCOPE macros to put NCBI C++
 * API to namespace "ncbi::" and to use it by default, respectively
 * Introduced THROWS_NONE and THROWS(x) macros for the exception
 * specifications
 * Other fixes and rearrangements throughout the most of "corelib" code
 *
 * Revision 1.5  1998/11/04 23:46:35  vakatov
 * Fixed the "ncbidbg/diag" header circular dependencies
 *
 * ==========================================================================
 */

#endif  /* NCBIDBG__HPP */
