#ifndef CGI___CGI_REDIRECT__HPP
#define CGI___CGI_REDIRECT__HPP

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

/// @cgi_redirect.hpp
/// Define class CCgiRedirectApplication used to redirect CGI requests.

#include <cgi/cgiapp.hpp>
#include <html/page.hpp>

/** @addtogroup CGIBase
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
///
/// CCgiRedirectApplication --
///
/// Defines class for CGI redirection.
///
/// CCgiRedirectApplication nherits its basic functionality from
/// CCgiApplication and defines additional method for remapping cgi entries.

class NCBI_XCGI_EXPORT CCgiRedirectApplication: public CCgiApplication
{
    typedef CCgiApplication CParent;
public:
    // 
    virtual void Init(void);
    virtual int  ProcessRequest(CCgiContext& ctx);

public:
    /// Remap CGI entries.
    ///
    /// Default implementation use registry file to obtain rules for
    /// entries remapping (see begin of the file for registry section
    /// and parameters description). New entries will be placed into
    /// "new_entries" accordingly received rules. It rules is not defined,
    /// that each old entrie just copies into new one.
    ///
    /// @param ctx
    ///   Current CGI context. Can be used to get original entries and
    ///   server context. 
    /// @param new_entries
    ///   Storage for new CGI entries. These entries will be used insteed of
    ///   original entries to generate redirected request.
    /// @return
    ///   Reference to "new_entries" parameter.
    /// @sa
    ///   ProcessRequest()
    virtual TCgiEntries& RemapEntries(CCgiContext& ctx, TCgiEntries& new_entries);

protected:
    const CHTMLPage& GetPage(void) const;
    CHTMLPage& GetPage(void);

private:
    CHTMLPage m_Page;  ///< HTML page used to print out redirect information.
};


END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2004/02/09 17:10:19  ivanov
 * Initial revision
 *
 * ===========================================================================
 */

#endif // CGI___CGI_REDIRECT__HPP
