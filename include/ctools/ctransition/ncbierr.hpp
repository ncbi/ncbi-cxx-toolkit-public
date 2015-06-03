#ifndef CTRANSITION___NCBIERR__HPP
#define CTRANSITION___NCBIERR__HPP

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
 * Author:  Vladimir Ivanov
 *
 *
 */

/// @file ncbierr.hpp
///
///   Wrapper for the C Toolkit ErrPostEx() macro.


#include <corelib/ncbidiag.hpp>
#include <ctools/ctransition/ncbilcl.hpp>

BEGIN_CTRANSITION_SCOPE


// C Toolkit error codes

#define E_NoMemory      1
#define E_Programmer  999

/*
#define E_File          2
#define E_FOpen         3
#define E_FRead         4
#define E_FWrite        5
#define E_CdEject       9
#define E_Math          4
#define E_SGML          3
*/


// C Toolkit severity
enum ErrSev { SEV_NONE=0, SEV_INFO, SEV_WARNING, SEV_ERROR, SEV_REJECT, SEV_FATAL, SEV_MAX };

// ?
#define MSG_OK  SEV_ERROR


// Convert C Toolkit severity to C++
extern NCBI_NS_NCBI::Severity ctransition_ErrSeverity(ErrSev sev);

// Convert variable list of arguments to string
extern std::string  ctransition_ErrMessage(const char* format, ...);


#define ErrPostEx(sev, err_code, err_subcode, ...)          \
    ( NCBI_NS_NCBI::CNcbiDiag(DIAG_COMPILE_INFO).GetRef()   \
      << NCBI_NS_NCBI::ErrCode( (err_code), (err_subcode) ) \
      << ctransition_ErrSeverity(sev)                       \
      << ctransition_ErrMessage(__VA_ARGS__)                \
      << NCBI_NS_NCBI::Endm )


// Redefine message function via ErrPostEx()

#define Nlm_Message(key, ...)  ErrPostEx((ErrSev)key, 0, 0, __VA_ARGS__)


END_CTRANSITION_SCOPE


#endif  /* CTRANSITION___NCBIERR__HPP */
