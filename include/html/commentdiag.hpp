#ifndef HTML___COMMENTDIAG__HPP
#define HTML___COMMENTDIAG__HPP

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
 */

/// @file commentdiag.hpp
/// Diagnostic handler for embedding diagnostics in comments.


#include <corelib/ncbistd.hpp>
#include <html/html.hpp>

/** @addtogroup DiagHandler
 *
 * @{
 */


BEGIN_NCBI_SCOPE


class NCBI_XHTML_EXPORT CCommentDiagHandler : public CDiagHandler
{
public:
    CCommentDiagHandler() : m_Node(NULL) {}
    void SetDiagNode(CNCBINode* node) { m_Node = node; }
    virtual void Post(const SDiagMessage& mess);

private:
    CNCBINode* m_Node;
};


class CCommentDiagFactory : public CDiagFactory
{
public:
    virtual CDiagHandler* New(const string&)
        { return new CCommentDiagHandler; }
};


inline NCBI_XHTML_EXPORT
CCommentDiagHandler* SetDiagNode(CNCBINode* node, CDiagHandler* handler = NULL)
{
    if (handler == NULL) {
        handler = GetDiagHandler();
    }

    CCommentDiagHandler* comment_handler
        = dynamic_cast<CCommentDiagHandler*>(handler);
    if (comment_handler != NULL) {
        comment_handler->SetDiagNode(node);
    }
    return comment_handler;
}


END_NCBI_SCOPE


/* @} */

#endif  /* HTML___COMMENTDIAG__HPP */
