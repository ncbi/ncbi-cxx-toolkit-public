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
 ///      include <common/test_assert.h> as a last header file.
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
#ifdef _DEBUG

#  define _TRACE(message)                        \
    ( NCBI_NS_NCBI::CNcbiDiag(DIAG_COMPILE_INFO, \
      NCBI_NS_NCBI::eDiag_Trace).GetRef()        \
      << message << NCBI_NS_NCBI::Endm )

#  define _TRACE_EX(err_code, err_subcode, message)         \
    ( NCBI_NS_NCBI::CNcbiDiag(DIAG_COMPILE_INFO             \
      NCBI_NS_NCBI::eDiag_Trace).GetRef()                   \
      << NCBI_NS_NCBI::ErrCode( (err_code), (err_subcode) ) \
      << message << NCBI_NS_NCBI::Endm )

#  define _TRACE_X(err_subcode, message)                  \
    ( (NCBI_CHECK_ERR_SUBCODE_X(err_subcode)),            \
      _TRACE_EX(NCBI_ERRCODE_X, err_subcode, message) )

#  define _TRACE_XX(error_name, err_subcode, message)                      \
    ( (NCBI_CHECK_ERR_SUBCODE_X_NAME(error_name, err_subcode)),            \
      _TRACE_EX(NCBI_ERRCODE_X_NAME(error_name), err_subcode, message) )

#  define NCBI_TROUBLE(mess) \
    NCBI_NS_NCBI::CNcbiDiag::DiagTrouble(DIAG_COMPILE_INFO, mess)

#  ifdef NCBI_COMPILER_MSVC
    // Use standard _ASSERT macro on MSVC in Debug modes
#    define NCBI_ASSERT(expr, mess) \
      if ( !(expr) ) { \
          NCBI_NS_NCBI::CNcbiDiag(DIAG_COMPILE_INFO, NCBI_NS_NCBI::eDiag_Error, NCBI_NS_NCBI::eDPF_Trace) << \
              "Assertion failed: (" << \
              (#expr ? #expr : "") << ") " << \
              (mess ? mess : "") << NCBI_NS_NCBI::Endm; \
          _ASSERT_BASE((expr), NULL); \
      }
#  else  /* NCBI_COMPILER_MSVC */
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

#  define _TRACE(message)                               ((void)0)
#  define _TRACE_EX(err_code, err_subcode, message)     ((void)0)
#  define _TRACE_X(err_subcode, message)                ((void)0)
#  define _TRACE_XX(error_name, err_subcode, message)   ((void)0)

#  define NCBI_TROUBLE(mess)
#  define NCBI_ASSERT(expr, mess)   ((void)0)
#  define NCBI_VERIFY(expr, mess)   while ( expr ) break
#  define _DEBUG_ARG(arg)
#  define _DEBUG_CODE(code)         ((void)0)

#endif  /* else!_DEBUG */


#ifdef   _ASSERT
#  undef _ASSERT
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

#endif  /* NCBIDBG__HPP */
