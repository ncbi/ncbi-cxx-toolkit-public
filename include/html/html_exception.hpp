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
 */

/// @file html_exception.hpp
/// HTML library exceptions.
///
/// Defines class to generate exceptions from the HTML library.


#include <corelib/ncbiexpt.hpp>


/** @addtogroup HTMLexpt
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
///
/// CHTMLException --
///
/// Define an extended exception class based on the CException
///
/// CHTMLException inherits its basic functionality from CException and
/// defines additional reporting capabilities.

class NCBI_XHTML_EXPORT CHTMLException
    : EXCEPTION_VIRTUAL_BASE public CException
{
public:
    enum EErrCode {
        eNullPtr,
        eWrite,
        eTextUnclosedTag,
        eTableCellUse,
        eTableCellType,
        eTemplateAccess,
        eTemplateTooBig,
        eEndlessRecursion,
        eUnknown
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch ( GetErrCode() ) {
        case eNullPtr:          return "eNullPtr";
        case eWrite:            return "eWrite";
        case eTextUnclosedTag:  return "eTextUnclosedTag";
        case eTableCellUse:     return "eTableCellUse";
        case eTableCellType:    return "eTableCellType";
        case eTemplateAccess:   return "eTemplateAccess";
        case eTemplateTooBig:   return "eTemplateTooBig";
        case eEndlessRecursion: return "eEndlessRecursion";
        case eUnknown:          return "eUnknown";
        default:                return CException::GetErrCodeString();
        }
    }

    /// Constructor.
    CHTMLException(const char* file, int line,
                   const CException* prev_exception, EErrCode err_code,
                   const string& message)
        : CException(file, line, prev_exception, CException::eInvalid,message)
        NCBI_EXCEPTION_DEFAULT_IMPLEMENTATION(CHTMLException, CException);

public:
    /// Add node name to tracing path.
    virtual void AddTraceInfo(const string& node_name);

    /// Report node trace into the "out" stream.
    virtual void ReportExtra(ostream& out) const;

protected:
    /// Helper method for copying exception data.
    virtual void x_Assign(const CException& src);

protected:
    list<string>  m_Trace;  ///< Node trace list.
};


END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2004/07/04 19:11:23  vakatov
 * Do not use "throw()" specification after constructors and assignment
 * operators of exception classes inherited from "std::exception" -- as it
 * causes ICC 8.0 generated code to abort in Release mode.
 *
 * Revision 1.5  2004/03/10 20:15:18  ivanov
 * + eEndlessRecursion
 *
 * Revision 1.4  2003/12/23 17:59:51  ivanov
 * Added exception tracing. Moved code to separate .cpp file.
 *
 * Revision 1.3  2003/11/03 17:02:53  ivanov
 * Some formal code rearrangement. Move log to end.
 *
 * Revision 1.2  2003/11/03 14:43:21  ivanov
 * + eWrite
 *
 * Revision 1.1  2003/07/08 17:12:40  gouriano
 * changed thrown exceptions to CException-derived ones
 *
 * ===========================================================================
 */

#endif  /* HTML___EXCEPTION__HPP */
