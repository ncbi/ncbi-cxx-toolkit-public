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
* Author:  Lewis Geer
*
* File Description:
*   pages used by query
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  1998/12/11 18:12:46  lewisg
* frontpage added
*

*
* ===========================================================================
*/

#include <ncbistd.hpp>
#include <querypages.hpp>
#include <components.hpp>
#include <ncbi.h>
#include <ncbienv.h>
#include <memory>
BEGIN_NCBI_SCOPE

CPmFrontPage::CPmFrontPage() {}


void CPmFrontPage::InitMembers(int style)
{
    m_PageName = "Pubmed Search";
    m_TemplateFile = "tabletemplate.html";
}


CHTMLNode * CPmFrontPage::CreateView(void) 
{
    CQueryBox * QueryBox;
    
    QueryBox = new CQueryBox;
    QueryBox->Create();
    return QueryBox;
}


#if 0

void CToolOptionPage::InitMembers(int style)
{
    m_PageName = "Tool Options";
    m_TemplateFile = "tool.html";
}


CHTMLNode * CToolOptionPage::CreateSidebar(void) 
{
    return NULL;
}


CHTMLNode * CToolOptionPage::CreateView(void) 
{
    CHTMLOptionForm * Options;
    try {
	Options = new CHTMLOptionForm();
	Options->m_ParamFile = "tool";
	Options->m_SectionName = "seg";
	Options->m_ActionURL = "http://ray/cgi-bin/tools/segify";
	Options->SetApplication(GetApplication());
	Options->Create();
    } 
    catch(...) {
	delete Options;
	throw;
    }
    return Options;
}

#endif



END_NCBI_SCOPE
