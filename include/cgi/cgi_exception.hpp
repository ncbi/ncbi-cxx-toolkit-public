#ifndef NCBI_CGI_EXCEPTION__HPP
#define NCBI_CGI_EXCEPTION__HPP

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
* Authors:
*	Andrei Gourianov
*
* File Description:
*   CGI library exceptions
*/

#include <corelib/ncbiexpt.hpp>


/** @addtogroup CGIExcep
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CCgiException : EXCEPTION_VIRTUAL_BASE public CException
{
    NCBI_EXCEPTION_DEFAULT(CCgiException,CException);
};

class CCgiErrnoException : public CErrnoTemplException<CCgiException>
{
public:
    enum EErrCode {
        eErrno,
        eModTime
    };
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eErrno:   return "eErrno";
        case eModTime: return "eModTime";
        default:       return CException::GetErrCodeString();
        }
    }
    NCBI_EXCEPTION_DEFAULT(CCgiErrnoException,CErrnoTemplException<CCgiException>);
};

class CCgiParseException : public CParseTemplException<CCgiException>
{
public:
    enum EErrCode {
        eCookie,
        eIndex,
        eEntry,
        eAttribute,
        eFormat
    };
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eCookie:    return "eCookie";
        case eIndex:     return "eIndex";
        case eEntry:     return "eEntry";
        case eAttribute: return "eAttribute";
        case eFormat:    return "eFormat";
        default:         return CException::GetErrCodeString();
        }
    }
    NCBI_EXCEPTION_DEFAULT2(CCgiParseException,
        CParseTemplException<CCgiException>,std::string::size_type);
};


END_NCBI_SCOPE


/* @} */


/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2003/04/10 19:01:39  siyan
* Added doxygen support
*
* Revision 1.1  2003/02/24 19:59:58  gouriano
* use template-based exceptions instead of errno and parse exceptions
*
*
* ===========================================================================
*/

#endif // NCBI_CGI_EXCEPTION__HPP
