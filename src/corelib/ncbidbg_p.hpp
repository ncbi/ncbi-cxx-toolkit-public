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


BEGIN_NCBI_SCOPE


#if defined(_DEBUG)

#  define CORE_ASSERT(expression) \
    if ( !(expression) ) Abort();

// 'simple' verify
#  define xncbi_Verify(expression) CORE_ASSERT(expression)

// Abort execution (by default), or throw exception (if explicitly specified
// by calling xncbi_SetValidateAction(eValidate_Throw)) if
// "expression" evaluates to FALSE.
#  define xncbi_Validate(expression, message) \
    do { \
        EValidateAction action = xncbi_GetValidateAction(); \
        if (action == eValidate_Throw) { \
            if ( !(expression) ) { \
                throw runtime_error( message ); \
            } \
        } \
        else { \
            CORE_ASSERT(expression) \
        } \
    } while (0)

#else // _DEBUG

#  define CORE_ASSERT(expr) ((void)0)

// 'simple' verify - just evaluate the expression
#  define xncbi_Verify(expression) ((void)(expression))

// Throw exception if "expression" evaluates to FALSE.
#  define xncbi_Validate(expression, message) \
    do { \
        if ( !(expression) ) { \
            throw runtime_error( message ); \
        } \
    } while (0)

#endif // _DEBUG


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
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
