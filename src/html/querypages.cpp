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
* Revision 1.10  1999/01/15 17:47:56  vasilche
* Changed CButtonList options: m_Name -> m_SubmitName, m_Select ->
* m_SelectName. Added m_Selected.
* Fixed CIDs Encode/Decode.
*
* Revision 1.9  1999/01/14 21:25:20  vasilche
* Changed CPageList to work via form image input elements.
*
* Revision 1.8  1998/12/28 23:29:10  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.7  1998/12/28 21:48:18  vasilche
* Made Lewis's 'tool' compilable
*
* Revision 1.6  1998/12/28 16:48:10  vasilche
* Removed creation of QueryBox in CHTMLPage::CreateView()
* CQueryBox extends from CHTML_form
* CButtonList, CPageList, CPagerBox, CSmallPagerBox extend from CNCBINode.
*
* Revision 1.5  1998/12/22 16:39:16  vasilche
* Added ReadyTagMapper to map tags to precreated nodes.
*
* Revision 1.4  1998/12/21 22:25:05  vasilche
* A lot of cleaning.
*
* Revision 1.3  1998/12/12 00:06:19  lewisg
* *** empty log message ***
*
* Revision 1.2  1998/12/11 22:52:01  lewisg
* added docsum page
*
* Revision 1.1  1998/12/11 18:12:46  lewisg
* frontpage added
* ===========================================================================
*/

#include <html/querypages.hpp>
#include <html/components.hpp>
#include <html/nodemap.hpp>
#include <memory>
BEGIN_NCBI_SCOPE

//
// basic search page
//

CPmFrontPage::CPmFrontPage()
{
    m_PageName = "Pubmed Search";
    m_TemplateFile = "tabletemplate.html";

    //    AddTagMap("QUERYBOX", CreateTagMapper(this, &CreateQueryBox));
}

CNCBINode* CPmFrontPage::CloneSelf(void) const
{
    return new CPmFrontPage(*this);
}

CNCBINode* CPmFrontPage::CreateView(void)
{
    //
    // demo purposes only
    //
    CQueryBox * QueryBox = new CQueryBox("http://www.ncbi.nlm.nih.gov/htbin-post/Entrez/query");
    QueryBox->m_Width = 400;
    QueryBox->m_BgColor = "#CCCCCFF";
    QueryBox->m_DispMax = "dispmax";
    QueryBox->m_DefaultDispMax = "20";
    QueryBox->m_DbName = "db";
    QueryBox->m_TermName = "term";
    QueryBox->m_URL = "http://www.ncbi.nlm.nih.gov/htbin-post/Entrez/query";
    QueryBox->m_Disp.push_back("10");
    QueryBox->m_Disp.push_back("20");
    QueryBox->m_Disp.push_back("50");
    QueryBox->m_Disp.push_back("100");
    QueryBox->m_Disp.push_back("200");
    QueryBox->m_Disp.push_back("1000");
    QueryBox->m_Disp.push_back("2000");
    QueryBox->m_Disp.push_back("5000");
    QueryBox->m_Databases["m"] = "Medline";
    QueryBox->m_Databases["n"] = "GenBank DNA Sequences";
    QueryBox->m_Databases["p"] = "GenBank Protein Sequences";
    QueryBox->m_Databases["t"] = "Biomolecule 3D Structures";
    QueryBox->m_Databases["c"] = "Complete Genomes";
    QueryBox->m_HiddenValues["form"] = "4";

    return QueryBox;
}


//
// Docsum page
//

template class TagMapper<CPmDocSumPage>;

CPmDocSumPage::CPmDocSumPage()
{
    m_PageName = "Search Results";
    m_TemplateFile = "template.html";
    AddTagMap("PAGER", CreateTagMapper(this, &CreatePager));
    AddTagMap("QUERYBOX", CreateTagMapper(this, &CreateQueryBox));
}

CNCBINode* CPmDocSumPage::CloneSelf(void) const
{
    return new CPmDocSumPage(*this);
}

CNCBINode* CPmDocSumPage::CreateView(void) 
{
    return 0;
}

CNCBINode* CPmDocSumPage::CreatePager(void) 
{
    if ( GetStyle() & kNoPAGER )
        return 0;

    //
    //  demo only
    //
    CPagerBox * Pager = new CPagerBox;
    Pager->m_Width = 600;
    Pager->m_TopButton->m_SubmitName = "Display";
    Pager->m_TopButton->m_SelectName = "display";
    Pager->m_TopButton->m_List["dopt"] = "Top";
    Pager->m_RightButton->m_SubmitName = "Save";
    Pager->m_RightButton->m_SelectName = "save";
    Pager->m_RightButton->m_List["m_s"] = "Right";
    Pager->m_LeftButton->m_SubmitName = "Order";
    Pager->m_LeftButton->m_SelectName = "order";
    Pager->m_LeftButton->m_List["m_o"] = "Left";
    Pager->m_PageList->m_Pages[1] = "page_one";
    Pager->m_PageList->m_Pages[2] = "page_two";
    Pager->m_PageList->m_Pages[3] = "page_three";
    Pager->m_PageList->m_Pages[4] = "page_four";
    Pager->m_PageList->m_Pages[5] = "page_five";
    Pager->m_PageList->m_Pages[6] = "page_six";
    Pager->m_PageList->m_Pages[7] = "page_seven";
    Pager->m_PageList->m_Pages[10] = "page_ten";
    Pager->m_PageList->m_Pages[11] = "page_eleven";
    Pager->m_PageList->m_Forward = "page_next";
    Pager->m_PageList->m_Backward = "page_prev";

    CHTML_form* Form = new CHTML_form;
    Form->AppendChild(Pager);
    return new CHTML_font(1, false, Form);
}


CNCBINode* CPmDocSumPage::CreateQueryBox(void) 
{
    if ( GetStyle() & kNoQUERYBOX ) 
        return 0;

    //
    // demo only
    //
    CQueryBox * QueryBox = new CQueryBox("http://www.ncbi.nlm.nih.gov/htbin-post/Entrez/query");
    QueryBox->m_Width = 600;
    QueryBox->m_BgColor = "#FFFFFF";
    QueryBox->m_DispMax = "dispmax";
    QueryBox->m_DefaultDispMax = "20";
    QueryBox->m_DbName = "db";
    QueryBox->m_TermName = "term";
    QueryBox->m_Disp.push_back("10");
    QueryBox->m_Disp.push_back("20");
    QueryBox->m_Disp.push_back("50");
    QueryBox->m_Disp.push_back("100");
    QueryBox->m_Disp.push_back("200");
    QueryBox->m_Disp.push_back("1000");
    QueryBox->m_Disp.push_back("2000");
    QueryBox->m_Disp.push_back("5000");
/*
    QueryBox->m_Databases["m"] = "Medline";
    QueryBox->m_Databases["n"] = "GenBank DNA Sequences";
    QueryBox->m_Databases["p"] = "GenBank Protein Sequences";
    QueryBox->m_Databases["t"] = "Biomolecule 3D Structures";
    QueryBox->m_Databases["c"] = "Complete Genomes";
*/
    QueryBox->m_HiddenValues["form"] = "4";
    return QueryBox;
}

END_NCBI_SCOPE
