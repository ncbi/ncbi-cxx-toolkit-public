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
* Author:  Aaron Ucko
*
* File Description:
*   CGI diagnostic handler for embedding diagnostics in comments.
*   This code really belongs in libcgi, but is here to avoid extra
*   dependencies.
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2001/10/04 18:17:57  ucko
* Accept additional query parameters for more flexible diagnostics.
* Support checking the readiness of CGI input and output streams.
*
* =========================================================================== */

#include <corelib/ncbistd.hpp>
#include <cgi/cgiapp.hpp>
#include <html/html.hpp>


// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE


class CCgiCommentDiagHandler : public CCgiDiagHandler
{
public:
    CCgiCommentDiagHandler() : m_Node(NULL) {}
    virtual void SetDiagNode(CNCBINode* node) { m_Node = node; }
    virtual CCgiCommentDiagHandler& operator <<(const SDiagMessage& mess);

private:
    CNCBINode* m_Node;
};

CCgiCommentDiagHandler&
CCgiCommentDiagHandler::operator <<(const SDiagMessage& mess)
{
    if (m_Node != NULL) {
        string s;
        mess.Write(s);
        m_Node->AppendChild(new CHTMLComment(s));
    }
    return *this;
}


CCgiDiagHandler* CommentDiagHandlerFactory(const string&, CCgiContext&)
{
    return new CCgiCommentDiagHandler;
}


// (END_NCBI_SCOPE must be preceded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE
