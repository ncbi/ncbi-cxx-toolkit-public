#ifndef HTML___EXCEPTION__HPP
#define HTML___EXCEPTION__HPP


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
 * Author:  Andrei Gourianov
 *
 * File Description:  HTML library exceptions
 *
 */

#include <corelib/ncbiexpt.hpp>

/** @addtogroup HTMLexpt
 *
 * @{
 */


BEGIN_NCBI_SCOPE


class CHTMLException : public CException
{
public:
    enum EErrCode {
        eNullPtr,
        eWrite,
        eTextUnclosedTag,
        eTableCellUse,
        eTableCellType,
        eTemplateAccess,
        eTemplateTooBig
    };
    virtual const char* GetErrCodeString(void) const {
        switch ( GetErrCode() ) {
        case eNullPtr:         return "eNullPtr";
        case eWrite:           return "eWrite";
        case eTextUnclosedTag: return "eTextUnclosedTag";
        case eTableCellUse:    return "eTableCellUse";
        case eTableCellType:   return "eTableCellType";
        case eTemplateAccess:  return "eTemplateAccess";
        case eTemplateTooBig:  return "eTemplateTooBig";
        default:               return CException::GetErrCodeString();
        }
    }
    NCBI_EXCEPTION_DEFAULT(CHTMLException,CException);
};


END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2003/11/03 14:43:21  ivanov
 * + eWrite
 *
 * Revision 1.1  2003/07/08 17:12:40  gouriano
 * changed thrown exceptions to CException-derived ones
 *
 * ===========================================================================
 */

#endif  /* HTML___EXCEPTION__HPP */
