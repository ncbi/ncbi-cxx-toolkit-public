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
* Author: Eugene Vasilchenko
*
* File Description:
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  1999/01/21 16:18:06  sandomir
* minor changes due to NStr namespace to contain string utility functions
*
* Revision 1.2  1999/01/20 21:41:36  vasilche
* Fixed bug with lost current page.
*
* Revision 1.1  1999/01/19 21:17:42  vasilche
* Added CPager class
*
* ===========================================================================
*/

#include <html/pager.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbicgi.hpp>

BEGIN_NCBI_SCOPE

const string CPager::KParam_PageSize = "dispmax";
const string CPager::KParam_DisplayPage = "page";
const string CPager::KParam_Page = "page ";
const string CPager::KParam_x = ".x";
const string CPager::KParam_PreviousPages = "previous pages";
const string CPager::KParam_PreviousPagesX = KParam_PreviousPages + KParam_x;
const string CPager::KParam_NextPages = "next pages";
const string CPager::KParam_NextPagesX = KParam_NextPages + KParam_x;

string CPagerView::sm_DefaultImagesDir = "/images/";

CPager::CPager(CCgiRequest& request, int pageBlockSize)
    : m_PageBlockSize(max(1, pageBlockSize)), m_PageSize(GetPageSize(request)),
      m_PageChanged(false)
{
    const TCgiEntries& entries = request.GetEntries();

    if ( entries.find(KParam_PreviousPagesX) != entries.end() ) {
        // previous pages
        // round to previous page block
        m_PageChanged = true;
        int page = GetDisplayPage(request);
        m_DisplayPage = page - page % m_PageBlockSize - 1;
    }
    else if ( entries.find(KParam_NextPagesX) != entries.end() ) {
        // next pages
        // round to next page block
        m_PageChanged = true;
        int page = GetDisplayPage(request);
        m_DisplayPage = page - page % m_PageBlockSize + m_PageBlockSize;
    }
    else {
        // look for params like: "page 2.x=..."
        for ( TCgiEntriesCI i = entries.begin(); i != entries.end(); ++i ) {
            const string& param = i->first;
            if ( NStr::StartsWith(param, KParam_Page) && NStr::EndsWith(param, KParam_x) ) {
                // "page xxx"
                string page = param.substr(KParam_Page.size(),
                        param.size() - KParam_Page.size() - KParam_x.size());
                try {
                    m_DisplayPage = NStr::StringToInt(page) - 1;
                    m_PageChanged = true;
                    break;
                } catch (exception e) {
                    _TRACE( "Exception in CQSearchCommand::GetDisplayRange: "
                            << e.what() );
                    // ignore exception right now
                }
            }
        }
        if ( !m_PageChanged ) {
            TCgiEntriesCI page = entries.find(KParam_DisplayPage);
            if ( page != entries.end() ) {
                try {
                    m_DisplayPage = NStr::StringToInt(page->second);
                } catch (exception e) {
                    _TRACE( "Exception in CQSearchCommand::GetDisplayRange: "
                            << e.what() );
                    m_DisplayPage = 0;
                }
            }
            else {
                m_DisplayPage = 0;
            }
        }
    }

    if ( m_DisplayPage < 0 ) {
        _TRACE( "Negative page start in CQSearchCommand::GetDisplayRange: "
                << m_DisplayPage );
        m_DisplayPage = 0;
    }

    m_PageBlockStart = m_DisplayPage - m_DisplayPage % m_PageBlockSize;
}

int CPager::GetDisplayPage(CCgiRequest& request)
{
    const TCgiEntries& entries = request.GetEntries();
    TCgiEntriesCI entry = entries.find(KParam_DisplayPage);

    if ( entry != entries.end() ) {
        try {
            int displayPage = NStr::StringToInt(entry->second);
            if ( displayPage >= 0 )
                return displayPage;

            _TRACE( "Negative page start in CQSearchCommand::GetDisplayRange: " << displayPage );
        } catch (exception e) {
            _TRACE( "Exception in CQSearchCommand::GetPageSize " << e.what() );
        }
    }

    // use default page start
    return 0;
}

int CPager::GetPageSize(CCgiRequest& request)
{
    const TCgiEntries& entries = request.GetEntries();
    TCgiEntriesCI entry = entries.find(KParam_DisplayPage);

    entry = entries.find(KParam_PageSize);
    if ( entry != entries.end() ) {
        try {
            int pageSize = NStr::StringToInt(entry->second);
            if ( pageSize > 0 )
                return pageSize;
            _TRACE( "Nonpositive page size in CQSearchCommand::GetDisplayRange: " << pageSize );
        } catch (exception e) {
            _TRACE( "Exception in CQSearchCommand::GetPageSize " << e.what() );
        }
    }
    // use default page size
    return 20;
}

void CPager::SetItemCount(int itemCount)
{
    m_ItemCount = itemCount;
}

pair<int, int> CPager::GetRange(void) const
{
    int firstItem = m_DisplayPage * m_PageSize;
    return pair<int, int>(firstItem, min(firstItem + m_PageSize, m_ItemCount));
}

void CPager::CreateSubNodes(void)
{
    AppendChild(new CHTML_hidden(KParam_PageSize, m_PageSize));
    AppendChild(new CHTML_hidden(KParam_DisplayPage, m_DisplayPage));
}

CNCBINode* CPager::GetPageInfo(void) const
{
    int lastPage = max(0, (m_ItemCount + m_PageSize - 1) / m_PageSize - 1);
    return new CHTMLText(
        NStr::IntToString(m_DisplayPage + 1) +
        " page of " + NStr::IntToString(lastPage + 1));
}

CNCBINode* CPager::GetItemInfo(void) const
{
    return new CHTMLText(
        NStr::IntToString(m_DisplayPage*m_PageSize + 1) + "-" +
        NStr::IntToString(min((m_DisplayPage + 1)*m_PageSize, m_ItemCount)) +
        " items of " + NStr::IntToString(m_ItemCount));
}

CNCBINode* CPager::GetPagerView(void) const
{
    return new CPagerView(*this);
}

// pager view

CPagerView::CPagerView(const CPager& pager)
    : m_ImagesDir(sm_DefaultImagesDir), m_Pager(pager)
{
}

void CPagerView::AddImageString(CNCBINode* node, int number,
                                const string& prefix, const string& suffix)
{
    string s = NStr::IntToString(number + 1);
    string name = CPager::KParam_Page + s;

    for ( int i = 0; i < s.size(); ++i ) {
        node->AppendChild(new CHTML_image(name,
                    m_ImagesDir + prefix + s[i] + suffix, 0));
    }
}

void CPagerView::AddInactiveImageString(CNCBINode* node, int number,
                                  const string& prefix, const string& suffix)
{
    string s = NStr::IntToString(number + 1);

    for ( int i = 0; i < s.size(); ++i ) {
        node->AppendChild(new CHTML_img(
                    m_ImagesDir + prefix + s[i] + suffix));
    }
}

void CPagerView::CreateSubNodes()
{
    int column = 0;
    int pageSize = m_Pager.m_PageSize;
    int blockSize = m_Pager.m_PageBlockSize;

    int currentPage = m_Pager.m_DisplayPage;
    int itemCount = m_Pager.m_ItemCount;

    int firstBlockPage = currentPage - currentPage % blockSize;
    int lastPage = max(0, (itemCount + pageSize - 1) / pageSize - 1);
    int lastBlockPage = min(firstBlockPage + blockSize - 1, lastPage);

    if ( firstBlockPage > 0 ) {
        InsertAt(0, column++, new CHTML_image(CPager::KParam_PreviousPages,
                                              m_ImagesDir + "prev.gif", 0));
    }

    for ( int i = firstBlockPage; i <= lastBlockPage ; ++i ) {
        if ( i == currentPage ) {
            // current link
            AddImageString(Cell(0, column++), i, "black_", ".gif");
        }
        else {
            // normal link
            AddImageString(Cell(0, column++), i, "", ".gif");
        }
    }

    if ( lastPage != lastBlockPage ) {
        InsertAt(0, column++, new CHTML_image(CPager::KParam_NextPages,
                                              m_ImagesDir + "next.gif", 0));
    }
}

END_NCBI_SCOPE
