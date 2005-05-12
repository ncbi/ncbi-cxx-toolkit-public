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
 * Author:  Eugene Vasilchenko, Anton Golikov
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <cgi/ncbicgi.hpp>
#include <html/pager.hpp>

#include <stdio.h>

BEGIN_NCBI_SCOPE


const string CPager::KParam_PageSize      = "dispmax";
const string CPager::KParam_ShownPageSize = "showndispmax";
const string CPager::KParam_DisplayPage   = "page";
const string CPager::KParam_Page          = "page ";
const string CPager::KParam_PreviousPages = "previous pages";
const string CPager::KParam_NextPages     = "next pages";
const string CPager::KParam_InputPage     = "inputpage";
const string CPager::KParam_NextPage      = "Next Page";
const string CPager::KParam_PrevPage      = "Prev Page";
const string CPager::KParam_GoToPage      = "GoTo Page";

CPager::CPager(const CCgiRequest& request,
               int pageBlockSize,
               int defaultPageSize,
               EPagerView view /* = eImage */)
    : m_PageSize(GetPageSize(request, defaultPageSize)),
      m_PageBlockSize(max(1, pageBlockSize)),
      m_PageChanged(false), m_view(view)
{
    const TCgiEntries& entries = request.GetEntries();
    
    if ( IsPagerCommand(request) ) {
        // look in preprocessed IMAGE values with empty string key
        TCgiEntriesCI i = entries.find(NcbiEmptyString);
        if (i != entries.end()) {
            const string& value = i->second;
            if (value == KParam_PreviousPages) {
                // previous pages
                // round to previous page block
                m_PageChanged = true;
                int page = GetDisplayedPage(request);
                m_DisplayPage = page - page % m_PageBlockSize - 1;
            }
            else if (value == KParam_NextPages) {
                // next pages
                // round to next page block
                m_PageChanged = true;
                int page = GetDisplayedPage(request);
                m_DisplayPage = page - page % m_PageBlockSize +
                    m_PageBlockSize;
            }
            else if ( NStr::StartsWith(value, KParam_Page) ) {
                // look for params like: "page 2"
                string page = value.substr(KParam_Page.size());
                try {
                    m_DisplayPage = NStr::StringToInt(page) - 1;
                    m_PageChanged = true;
                } catch (exception& _DEBUG_ARG(e)) {
                    _TRACE( "Exception in CPager::CPager: " << e.what() );
                    m_PageChanged = false;
                }
            }
        }
        i = entries.find(KParam_InputPage);
        if (i != entries.end()) {
            try {
                m_DisplayPage = NStr::StringToInt(i->second) - 1;
                m_DisplayPage = max(m_DisplayPage, 0);
                m_PageChanged = true;
            } catch (exception& _DEBUG_ARG(e)) {
                _TRACE( "Exception in CPager::IsPagerCommand: " << e.what() );
                m_PageChanged = false;
            }
        }
    } else {
        try {
            m_PageChanged = true;
            int page = GetDisplayedPage(request);
            TCgiEntriesCI oldPageSize = entries.find(KParam_ShownPageSize);
            if( !page || oldPageSize == entries.end() )
                throw runtime_error("Error getting page params");
            //number of the first element in old pagination
            int oldFirstItem = page * NStr::StringToInt( oldPageSize->second );
            m_DisplayPage = oldFirstItem / m_PageSize;
        } catch(exception& _DEBUG_ARG(e)) {
            _TRACE( "Exception in CPager::CPager: " << e.what() );
            m_DisplayPage = 0;
            m_PageChanged = false;
        }
         
    }
    if( !m_PageChanged )
            m_DisplayPage = GetDisplayedPage(request);

    m_PageBlockStart = m_DisplayPage - m_DisplayPage % m_PageBlockSize;
}


bool CPager::IsPagerCommand(const CCgiRequest& request)
{
    TCgiEntries& entries = const_cast<TCgiEntries&>(request.GetEntries());

    // look in preprocessed IMAGE values with empty string key
    TCgiEntriesI i = entries.find(NcbiEmptyString);
    if (i != entries.end()) {
        const string& value = i->second.GetValue();
        if (value == KParam_PreviousPages) {
            // previous pages
            return true;
        }
        else if (value == KParam_NextPages) {
            // next pages
            return true;
        }
        else if ( NStr::StartsWith(value, KParam_Page) ) {
            // look for params like: "page 2"
            string page = value.substr(KParam_Page.size());
            try {
                NStr::StringToInt(page);
                return true;
            } catch (exception& _DEBUG_ARG(e)) {
                _TRACE( "Exception in CPager::IsPagerCommand: " << e.what() );
            }
        }
    }
    i = entries.find(KParam_InputPage);
    if (i != entries.end()) {
        try {
            NStr::StringToInt(i->second.GetValue());
            return true;
        } catch (exception& _DEBUG_ARG(e)) {
            _TRACE( "Exception in CPager::IsPagerCommand: " << e.what() );
        }
    }
    return false;
}


int CPager::GetDisplayedPage(const CCgiRequest& request)
{
    const TCgiEntries& entries = request.GetEntries();
    TCgiEntriesCI entry = entries.find(KParam_DisplayPage);

    if (entry != entries.end()) {
        try {
            int displayPage = NStr::StringToInt(entry->second);
            if ( displayPage >= 0 )
                return displayPage;
            _TRACE( "Negative page start in CPager::GetDisplayedPage: " <<
                    displayPage );
        } catch (exception& _DEBUG_ARG(e)) {
            _TRACE( "Exception in CPager::GetDisplayedPage " << e.what() );
        }
    }
    // use default page start
    return 0;
}


int CPager::GetPageSize(const CCgiRequest& request, int defaultPageSize)
{
    TCgiEntries& entries = const_cast<TCgiEntries&>(request.GetEntries());
    TCgiEntriesCI entry;
    
    if( IsPagerCommand(request) ) {
        entry = entries.find(KParam_ShownPageSize);
    } else {
        entry = entries.find(KParam_PageSize);
    }
    if (entry != entries.end()) {
        try {
            string dispMax = entry->second;
            int pageSize = NStr::StringToInt(dispMax);
            if( pageSize > 0 ) {
                //replace dispmax for current page size
                entries.erase(KParam_PageSize);
                entries.insert(TCgiEntries::value_type(KParam_PageSize,
                                                       dispMax));
                return pageSize;
            }	
            _TRACE( "Nonpositive page size in CPager::GetPageSize: " <<
                    pageSize );
        } catch (exception& _DEBUG_ARG(e)) {
            _TRACE( "Exception in CPager::GetPageSize " << e.what() );
        }
    }
    // use default page size
    return defaultPageSize;
}


void CPager::SetItemCount(int itemCount)
{
    m_ItemCount = itemCount;
    if (m_DisplayPage * m_PageSize >= itemCount) {
        m_DisplayPage = 0;
    }
}


pair<int, int> CPager::GetRange(void) const
{
    int firstItem = m_DisplayPage * m_PageSize;
    return pair<int, int>(firstItem, min(firstItem + m_PageSize, m_ItemCount));
}


void CPager::CreateSubNodes(void)
{
    AppendChild(new CHTML_hidden(KParam_ShownPageSize, m_PageSize));
    AppendChild(new CHTML_hidden(KParam_DisplayPage, m_DisplayPage));
}


CNCBINode* CPager::GetPageInfo(void) const
{
    if (m_ItemCount <= m_PageSize) {
        return 0;
    }
    int lastPage = (m_ItemCount - 1) / m_PageSize;
    return new CHTMLPlainText(
        "Page " + NStr::IntToString(m_DisplayPage + 1) +
        " of " + NStr::IntToString(lastPage + 1));
}

CNCBINode* CPager::GetItemInfo(void) const
{
    char buf[1024];
    CHTML_div* node = new CHTML_div;
    node->SetClass("medium2");
    
    if (m_ItemCount == 0) {
        node->AppendChild(new CHTMLPlainText("0 items found"));
    } else {
        int firstItem = m_DisplayPage * m_PageSize + 1;
        int endItem = min((m_DisplayPage + 1) * m_PageSize, m_ItemCount);
        if (firstItem != endItem) {
            sprintf(buf, "Items %'d - %'d", firstItem, endItem);
            node->AppendChild(new CHTMLPlainText(buf));
        } else {
            sprintf(buf, "Item %'d", firstItem);
            node->AppendChild(new CHTMLPlainText(buf));
        }
        if( m_view != eTabs ) {
            sprintf(buf, " of %'d", m_ItemCount);
            node->AppendChild(new CHTMLPlainText(buf));
        }
    }
    return node;
}


CNCBINode* CPager::GetPagerView(const string& imgDir,
                                const int imgX, const int imgY,
                                const string& js_suffix /*kEmptyStr*/) const
{
    if (m_ItemCount <= m_PageSize) {
        return 0;
    }
    switch (m_view) {
        case eButtons:
        case eTabs:
            return new CPagerViewButtons(*this, js_suffix);
        case eJavaLess:
            return new CPagerViewJavaLess(*this, js_suffix);
        default:
            break;
    }
    // Default old behavor
    return new CPagerView(*this, imgDir, imgX, imgY);
}


CPagerView::CPagerView(const CPager& pager, const string& imgDir,
                       const int imgX, const int imgY)
    : m_ImagesDir(imgDir), m_ImgSizeX(imgX), m_ImgSizeY(imgY), m_Pager(pager)
{
    return;
}


void CPagerView::AddImageString(CNCBINode* node, int number,
                                const string& prefix, const string& suffix)
{
    string s = NStr::IntToString(number + 1);
    string name = CPager::KParam_Page + s;
    CHTML_image* img;

    for ( size_t i = 0; i < s.size(); ++i ) {
        img = new CHTML_image(name, m_ImagesDir + prefix + s[i] + suffix, 0);
        img->SetAttribute("Alt", name);
        if ( m_ImgSizeX )
            img->SetWidth( m_ImgSizeX );
        if ( m_ImgSizeY )
            img->SetHeight( m_ImgSizeY );
        node->AppendChild( img );
    }
}


void CPagerView::AddInactiveImageString(CNCBINode* node, int number,
                                        const string& prefix,
                                        const string& suffix)
{
    string s = NStr::IntToString(number + 1);
    CHTML_img* img;

    for ( size_t i = 0; i < s.size(); ++i ) {
        img = new CHTML_img(m_ImagesDir + prefix + s[i] + suffix);
        img->SetAttribute("Alt", s);
        if( m_ImgSizeX )
            img->SetWidth( m_ImgSizeX );
        if( m_ImgSizeY )
            img->SetHeight( m_ImgSizeY );
        node->AppendChild( img );
    }
}


void CPagerView::CreateSubNodes()
{
    int column         = 0;
    int pageSize       = m_Pager.m_PageSize;
    int blockSize      = m_Pager.m_PageBlockSize;

    int currentPage    = m_Pager.m_DisplayPage;
    int itemCount      = m_Pager.m_ItemCount;

    int firstBlockPage = currentPage - currentPage % blockSize;
    int lastPage       = max(0, (itemCount + pageSize - 1) / pageSize - 1);
    int lastBlockPage  = min(firstBlockPage + blockSize - 1, lastPage);

    if (firstBlockPage > 0) {
        CHTML_image* img = new CHTML_image(CPager::KParam_PreviousPages,
                                           m_ImagesDir + "prev.gif", 0);
        img->SetAttribute("Alt", CPager::KParam_PreviousPages);
        if ( m_ImgSizeX )
            img->SetWidth( m_ImgSizeX );
        if ( m_ImgSizeY )
            img->SetHeight( m_ImgSizeY );
        InsertAt(0, column++, img);
    }

    for (int i = firstBlockPage; i <= lastBlockPage ; ++i) {
        if (i == currentPage) {
            // current link
            AddImageString(Cell(0, column++), i, "black_", ".gif");
        }
        else {
            // normal link
            AddImageString(Cell(0, column++), i, "", ".gif");
        }
    }

    if (lastPage != lastBlockPage) {
        CHTML_image* img = new CHTML_image(CPager::KParam_NextPages,
                                           m_ImagesDir + "next.gif", 0);
        img->SetAttribute("Alt", CPager::KParam_NextPages);
        if ( m_ImgSizeX )
            img->SetWidth( m_ImgSizeX );
        if ( m_ImgSizeY )
            img->SetHeight( m_ImgSizeY );
        InsertAt(0, column++, img);
    }
}


CPagerViewButtons::CPagerViewButtons(const CPager& pager, const string& js_suffix)
    : m_Pager(pager), m_jssuffix(js_suffix)
{
}

void CPagerViewButtons::CreateSubNodes()
{
    int column      = 0;
    int pageSize    = m_Pager.m_PageSize;
    int currentPage = m_Pager.m_DisplayPage;
    int itemCount   = m_Pager.m_ItemCount;
    int lastPage    = max(0, (itemCount + pageSize - 1) / pageSize - 1);

    if (currentPage > 0) {
        CHTML_a* prev = new CHTML_a("javascript:var frm = " \
                                    "document.frmQueryBox; " \
									"frm.inputpage.value=" +
                                    NStr::IntToString(currentPage) + 
                                    "; Go('Pager');", "Previous");
        prev->SetClass("dblinks");
        InsertAt(0, column, prev);
        InsertAt(0, column++, new CHTML_nbsp);
    }
        
    CHTML_input* butt = new CHTML_input("BUTTON", "GoToPage");
    butt->SetClass("dblinks");
    butt->SetAttribute("value", "Page");
    butt->SetEventHandler(eHTML_EH_Click,
                          "form.cmd.value='';form." +
                          CPager::KParam_InputPage +
                          ".value=form.textpage" + m_jssuffix +
                          ".value;Go('Pager');");
    InsertAt(0, column, butt);
    InsertAt(0, column, new CHTML_nbsp);

    CHTML_input* textpage =  new CHTML_text("textpage" + m_jssuffix, 4,
                                         NStr::IntToString(currentPage + 1));
    textpage->SetClass("dblinks");
    
    string suffix;
    if ( m_jssuffix.empty() ) {
       suffix = "1";
    }
    
    textpage->SetEventHandler(eHTML_EH_Change,
                              "if(form.textpage" + suffix + "){form.textpage" +
                              suffix +".value=" + "this.value; " 
                              "form." + CPager::KParam_InputPage +".value=" 
							  "this.value;}");
    
    textpage->SetEventHandler(eHTML_EH_KeyPress,
                              "form." + CPager::KParam_InputPage +".value=" 
                              "this.value;KeyPress('Pager',event);");
                                         
    InsertAt(0, column++, textpage);

    CHTML_div* div = new CHTML_div;
    div->SetClass("medium2");
    div->AppendChild(new CHTML_nbsp);
    div->AppendChild(new CHTMLPlainText("of"));
    div->AppendChild(new CHTML_nbsp);
    char buf[1024];
    sprintf(buf, "%'d", lastPage + 1);
    div->AppendChild(new CHTMLPlainText(buf));
    InsertAt(0, column++, div);
    
    // place holder for page num, to explicitly tell about new page num
    InsertAt(0, column++, new CHTML_hidden(CPager::KParam_InputPage +
                                           m_jssuffix, kEmptyStr));
    if (currentPage < lastPage) {
        CHTML_a* next = new CHTML_a("javascript:var frm = " \
                                    "document.frmQueryBox;" \
									"frm.inputpage.value=" +
                                    NStr::IntToString(currentPage + 2) + 
                                    ";Go('Pager');", "Next");
        next->SetClass("dblinks");
        InsertAt(0, column, next);
        InsertAt(0, column++, new CHTML_nbsp(2));
    }
}


CPagerViewJavaLess::CPagerViewJavaLess(const CPager& pager, const string& js_suffix)
    : m_Pager(pager), m_jssuffix(js_suffix)
{
}

void CPagerViewJavaLess::CreateSubNodes()
{
    int item_count = m_Pager.m_ItemCount;
    // container
    this->SetCellPadding(0)->SetCellSpacing(0)->SetWidth("100%");

    if(item_count > 20) {
        this->InsertNextCell(m_Pager.GetPageInfo())
            ->SetWidth("20%")->SetAlign("Right");
    
        this->InsertNextCell(
            new CHTML_submit("cmd", CPager::KParam_PrevPage))
            ->SetWidth("20%")->SetAlign("Right");

        this->InsertNextCell(new CHTML_submit("cmd", CPager::KParam_NextPage))
            ->SetWidth("20%")->SetAlign("Right");

        string page_no = "1";
        if( m_Pager.m_DisplayPage * 20 < (item_count + 20)) {
            page_no = NStr::IntToString(m_Pager.m_DisplayPage+1);
        }
        
        this->InsertNextCell((
             new CHTML_text(CPager::KParam_InputPage + m_jssuffix, 6, page_no))
             ->AppendChild(new CHTML_submit("cmd", CPager::KParam_GoToPage)))
             ->SetWidth("20%")->SetAlign("Right");
    }                    
}

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.45  2005/05/12 11:09:39  ivanov
 * Dropped default parameter in the NStr::IntToString()
 *
 * Revision 1.44  2004/10/21 22:22:24  yasmax
 * Added javaless pager view for queryd
 *
 * Revision 1.43  2004/09/01 21:41:46  ucko
 * #include <stdio.h> for sprintf() calls introduced in previous commit
 * (which had no log for some reason, but enabled locale-dependent
 * numeric display).
 *
 * Revision 1.42  2004/09/01 17:00:03  golikov
 * *** empty log message ***
 *
 * Revision 1.41  2004/07/12 21:26:58  golikov
 * additions for tab view
 *
 * Revision 1.40  2004/06/09 15:28:45  yasmax
 * Bug fix: Added synchronization inputpage field and textpage by OnChange event
 *
 * Revision 1.39  2004/05/17 20:59:50  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.38  2003/11/03 17:03:08  ivanov
 * Some formal code rearrangement. Move log to end.
 *
 * Revision 1.37  2003/10/02 18:24:38  ivanov
 * Get rid of compilation warnings; some formal code rearrangement
 *
 * ===========================================================================
 */
