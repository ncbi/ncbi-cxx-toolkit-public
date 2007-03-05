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

#endif // NCBIDBG_P__HPP
