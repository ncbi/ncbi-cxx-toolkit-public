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
 *   Implementation of debugging function(s)
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <corelib/ncbithr.hpp>
#include "ncbidbg_p.hpp"


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
// xncbi_Validate() related functions


// Action is defined on per-thread basis -- so we store it in TLS.
// TLS stores pointers, so just cast EValidateAction to pointer and back.
static CStaticTls<int> s_ValidateTLS;


void xncbi_SetValidateAction(EValidateAction action)
{
    s_ValidateTLS.SetValue(reinterpret_cast<int*> (action));
}


EValidateAction xncbi_GetValidateAction(void)
{
    // some 64 bit compilers refuse to cast from int* to EValidateAction
    EValidateAction action =
        EValidateAction(intptr_t(s_ValidateTLS.GetValue()));

    // we can store Default, but we cannot return Default
    if (action == eValidate_Default) {
#if defined(_DEBUG)
        action = eValidate_Abort;
#else
        action = eValidate_Throw;
#endif
    }
    return action;
}


/////////////////////////////////////////////////////////////////////////////
// CCheckMe reporting


void xncbi_CCheckMe_ReportError(ECheckMeError _DEBUG_ARG(error))
{
#if defined(_DEBUG)
    if (error == eCheckMe_Unused) {
        NCBI_TROUBLE("CCheckMe -- The value has not been used");
    } else /*if (error == eCheckMe_Unset)*/ {
        NCBI_TROUBLE("CCheckMe -- Attempt to read value that is not set");
    }
#endif
}


END_NCBI_SCOPE
