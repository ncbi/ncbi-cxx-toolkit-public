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
*   Play around with the HTML library
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.15  2000/07/18 17:21:42  vasilche
* Added possibility to force output of empty attribute value.
* Added caching to CHTML_table, now large tables work much faster.
* Changed algorythm of emitting EOL symbols in html output.
*
* Revision 1.14  1999/10/28 13:40:39  vasilche
* Added reference counters to CNCBINode.
*
* Revision 1.13  1999/07/08 16:45:55  vakatov
* Get rid of the redundant `extern "C"' at "main()
*
* Revision 1.12  1999/06/11 20:30:34  vasilche
* We should catch exception by reference, because catching by value
* doesn't preserve comment string.
*
* Revision 1.11  1999/05/11 02:54:01  vakatov
* Moved CGI API from "corelib/" to "cgi/"
*
* Revision 1.10  1999/04/27 14:50:16  vasilche
* Added FastCGI interface.
* CNcbiContext renamed to CCgiContext.
*
* Revision 1.9  1999/03/08 22:24:42  lewisg
* make compilable
*
* Revision 1.8  1998/12/28 23:29:13  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.7  1998/12/28 16:48:12  vasilche
* Removed creation of QueryBox in CHTMLPage::CreateView()
* CQueryBox extends from CHTML_form
* CButtonList, CPageList, CPagerBox, CSmallPagerBox extend from CNCBINode.
*
* Revision 1.6  1998/12/24 16:15:43  vasilche
* Added CHTMLComment class.
* Added TagMappers from static functions.
*
* Revision 1.5  1998/12/23 21:21:06  vasilche
* Added more HTML tags (almost all).
* Importent ones: all lists (OL, UL, DIR, MENU), fonts (FONT, BASEFONT).
*
* Revision 1.4  1998/12/23 14:28:12  vasilche
* Most of closed HTML tags made via template.
*
* Revision 1.3  1998/12/21 22:25:06  vasilche
* A lot of cleaning.
*
* Revision 1.2  1998/12/11 22:43:35  lewisg
* added docsum page
*
* Revision 1.1  1998/12/11 18:11:17  lewisg
* frontpage added
* ===========================================================================
*/

#include <cgi/ncbicgi.hpp>
#include <cgi/cgiapp.hpp>
#include <cgi/cgictx.hpp>
#include <html/html.hpp>
#include <html/page.hpp>
#include <html/factory.hpp>
#include <html/components.hpp>
#include <html/querypages.hpp>

BEGIN_NCBI_SCOPE

class CMyApp : public CCgiApplication
{
public:
    virtual int ProcessRequest(CCgiContext& context);

};


SFactoryList < CHTMLBasicPage > PageList [] = {
    { CPmDocSumPage::New, "docsum", 0 },
    { CPmFrontPage::New, "", 0 }
};


int CMyApp::ProcessRequest(CCgiContext& ctx) 
{
    SetDiagStream(&NcbiCerr);
    CFactory < CHTMLBasicPage > Factory;

    try {
        int i = Factory.CgiFactory( (TCgiEntries&)(ctx.GetRequest().GetEntries()), PageList);
        auto_ptr<CHTMLBasicPage> Page((PageList[i].pFactory)());
        Page->SetApplication(this);
        Page->SetStyle(PageList[i].Style);
        Page->AppendChild(new CHTML_p(new CHTML_b("bold paragraph")));
        CNodeRef form;
        Page->AppendChild(form = new CHTML_form("FORM"));
        form->AppendChild(new CHTML_textarea("area", 10, 10));
        form->AppendChild(new CHTML_textarea("area1", 10, 10, "area1"));
        form->AppendChild((new CHTML_select("Select"))
                          ->AppendOption("One", "ONE")
                          ->AppendOption("Two")
                          ->AppendOption("THREE", "Three"));
        Page->AppendChild((new CHTML_ol("A"))
                          ->AppendItem("item 1")
                          ->AppendItem("item 2")
                          ->AppendItem("Item 3")
                          ->AppendItem("Fourth item"));
        Page->AppendChild(new CHTMLPlainText("Testing table"));
        CRef<CHTML_table> table(new CHTML_table);
        Page->AppendChild(table.GetPointer());
        table->SetAttribute("border");
        const unsigned ROWS = 20;
        const unsigned COLUMNS = 10;
        int xSizes[ROWS][COLUMNS];
        int ySizes[ROWS][COLUMNS];
        { // init used table
            for ( unsigned row = 0; row < ROWS; ++row ) {
                for ( unsigned col = 0; col < COLUMNS; ++col ) {
                    xSizes[row][col] = 0;
                }
            }
        }
        { // fill by cells
            for ( int ci = 0; ci < 1000; ++ci ) {
                unsigned xSize = rand() % 5 + 1;
                unsigned ySize = rand() % 5 + 1;
                unsigned x = rand() % (COLUMNS - xSize);
                unsigned y = rand() % (ROWS - ySize);
                // check whether area is unused
                bool free = true;
                for ( unsigned row = y; free && row < y + ySize; ++row ) {
                    for ( unsigned col = x; free && col < x + xSize; ++col ) {
                        if ( xSizes[row][col] )
                            free = false;
                    }
                }
                if ( free ) {
                    for ( unsigned row = y; row < y + ySize; ++row ) {
                        for ( unsigned col = x; col < x + xSize; ++col ) {
                            xSizes[row][col] = ySizes[row][col] = -1;
                        }
                    }
                    xSizes[y][x] = xSize;
                    ySizes[y][x] = ySize;
                }
            }
            // fill table by generated cells
            for ( unsigned row = 0; row < ROWS; ++row ) {
                for ( unsigned col = 0; col < COLUMNS; ++col ) {
                    int xSize = xSizes[row][col];
                    if ( xSize > 0 ) {
                        int ySize = ySizes[row][col];
                        CHTML_tc* cell = table->Cell(row, col,
                                                     CHTML_table::eAnyCell,
                                                     ySize, xSize);
                        cell->AppendPlainText('@'+NStr::UIntToString(row)+
                                              ','+NStr::UIntToString(col));
                        cell->AppendChild(new CHTML_br);
                        cell->AppendPlainText(NStr::UIntToString(xSize)+'x'+
                                              NStr::UIntToString(ySize));
                    }
                    else if ( xSize == 0 ) {
                        table->InsertAt(row, col, "*");
                    }
                }
            }
        }
        Page->AppendChild(new CHTMLComment("this is comment"));
        ctx.GetResponse().WriteHeader();
        Page->Print(ctx.GetResponse().out());
        return 0;
    }
    catch (exception& exc) {
        NcbiCerr << "\n" << exc.what() << NcbiEndl;
        return 1;
    }
}


END_NCBI_SCOPE



/////////////////////////////////////////////////////////////////////////////
//  MAIN()


USING_NCBI_SCOPE;  // this turns on the ncbi namespace

int main(int argc, char *argv[])
{
    return CMyApp().AppMain(argc, argv);
}
