#ifndef CGI___CGI_EXCEPTION__HPP
#define CGI___CGI_EXCEPTION__HPP

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
*/

/// @file cgi_exception.hpp
/// Exception classes used by the NCBI CGI framework
///


#include <corelib/ncbiexpt.hpp>


/** @addtogroup CGIExcep
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
///
/// CCgiException --
///
///   Base class for the exceptions used by CGI framework

class CCgiException : EXCEPTION_VIRTUAL_BASE public CException
{
    NCBI_EXCEPTION_DEFAULT(CCgiException, CException);
};



/////////////////////////////////////////////////////////////////////////////
///
/// CCgiErrnoException --
///
///   Exceptions used by CGI framework when the error is more system-related
///   and there is an "errno" status from the system call that can be obtained

class CCgiErrnoException : public CErrnoTemplException<CCgiException>
{
public:
    enum EErrCode {
        eErrno,   //< Generic system call failure
        eModTime  //< File modification time cannot be obtained
    };
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eErrno:   return "eErrno";
        case eModTime: return "eModTime";
        default:       return CException::GetErrCodeString();
        }
    }
    NCBI_EXCEPTION_DEFAULT
    (CCgiErrnoException,
     CErrnoTemplException<CCgiException>);
};



/////////////////////////////////////////////////////////////////////////////
///
/// CCgiParseException --
///
///   Exceptions used by CGI framework when the error has occured while
///   parsing the contents (header and/or body) of the HTTP request

class CCgiParseException : public CParseTemplException<CCgiException>
{
public:
    /// Bad (malformed or missing) HTTP request components
    enum EErrCode {
        eCookie,     //< Cookie
        eRead,       //< Error in reading raw content of HTTP request
        eIndex,      //< ISINDEX
        eEntry,      //< Entry value
        eAttribute,  //< Entry attribute
        eFormat      //< Format or encoding
    };
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eCookie:    return "eCookie";
        case eRead:      return "eRead";
        case eIndex:     return "eIndex";
        case eEntry:     return "eEntry";
        case eAttribute: return "eAttribute";
        case eFormat:    return "eFormat";
        default:         return CException::GetErrCodeString();
        }
    }
    NCBI_EXCEPTION_DEFAULT2
    (CCgiParseException,
     CParseTemplException<CCgiException>, std::string::size_type);
};


END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2004/08/04 15:52:57  vakatov
 * CCgiParseException::EErrCode += eRead.
 * Also, commented and Doxynen'ized.
 *
 * Revision 1.2  2003/04/10 19:01:39  siyan
 * Added doxygen support
 *
 * Revision 1.1  2003/02/24 19:59:58  gouriano
 * use template-based exceptions instead of errno and parse exceptions
 *
 * ===========================================================================
 */

#endif  // CGI___CGI_EXCEPTION__HPP
