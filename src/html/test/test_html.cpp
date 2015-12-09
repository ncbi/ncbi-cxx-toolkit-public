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
 * File Description:  Play around with the HTML library
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/rwstream.hpp>

#include <html/html.hpp>
#include <html/page.hpp>
#include <html/writer_htmlenc.hpp>

#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;


/////////////////////////////////
// SpecialChars test
//

static void s_TEST_SpecialChars(void)
{
    CHTMLPage page("Simple test page"); 

    page.AppendChild(new CHTML_nbsp(2));
    page.AppendChild(new CHTML_gt);
    page.AppendChild(new CHTML_lt);
    page.AppendChild(new CHTML_quot);
    page.AppendChild(new CHTML_amp);
    page.AppendChild(new CHTML_copy);
    page.AppendChild(new CHTML_reg);
    page.AppendChild(new CHTMLSpecialChar("#177","+/-"));

    page.Print(cout, CNCBINode::eHTML);
    cout << endl;
    page.Print(cout, CNCBINode::ePlainText);
    cout << endl;

    {{
        CWriter_HTMLEncoder writer(cout,
                                   CWriter_HTMLEncoder::fPassNumericEntities);
        CWStream            wstream(&writer);
        page.Print(wstream, CNCBINode::ePlainText);
        wstream << endl;
        wstream << "&foo&" << flush;
        wstream << "#177;&" << flush;
        wstream << "bar&#177;&";
    }}
    cout << endl;
}


/////////////////////////////////
// Tags test
//

static void s_TEST_Tags(void)
{
    CNodeRef html, head, body;

    html = new CHTML_html;
    head = new CHTML_head;
    body = new CHTML_body;

    html->AppendChild(head); 
    html->AppendChild(body); 

    head->AppendChild(
          new CHTML_meta(CHTML_meta::eName,"author","I'm"));
    head->AppendChild(
          new CHTML_meta(CHTML_meta::eHttpEquiv,"refresh","3; URL=www.ru"));
    body->AppendChild(
          new CHTML_p(new CHTML_i("paragraph")));
    body->AppendChild(
          new CHTML_img("url","alt"));
    body->AppendChild(
          new CHTMLDualNode(new CHTML_p("Example1"),"Example1\n"));
    body->AppendChild(
          new CHTMLDualNode("<p>Example2</p>","Example2\n"));
    body->AppendChild(
          new CHTML_script("text/javascript","http://localhost/sample.js"));

    CHTML_script* script = new CHTML_script("text/vbscript");
    script->AppendScript("Place VB1 script here");
    script->AppendScript("Place VB2 script here");
    body->AppendChild(script);


    html->Print(cout);
    cout << endl << endl;

    html->Print(cout,CNCBINode::ePlainText);
    cout << endl;
}



/////////////////////////////////
// Table test
//

static void s_TEST_Table(void)
{
    cout << endl;

    CHTMLPage page("Table test"); 

    CHTML_table* table1 = new CHTML_table;
    CHTML_table* table2 = new CHTML_table;

    page.AppendChild(table1);
    page.AppendChild(new CHTML_br);
    page.AppendChild(table2);

    table1->SetPlainSeparators("| "," | "," |",'-',CHTML_table::ePrintRowSep);

    for ( unsigned row = 0; row < 12; ++row ) {
        for ( unsigned col = 0; col < 4; ++col ) {
             CHTML_tc* cell = table1->Cell(row, col, CHTML_table::eAnyCell, 1, 1 );
             cell->AppendPlainText('@'+NStr::UIntToString(row)+','+NStr::UIntToString(col));
        }
    }

    CHTML_tc* cell;
    cell = table2->Cell(0, 0, CHTML_table::eAnyCell, 2, 1 );
    cell->AppendPlainText("0,0 - 2x1");

    cell = table2->Cell(1, 1, CHTML_table::eAnyCell, 1, 1 );
    cell->AppendPlainText("1,1 - 1x1");

    cell = table2->Cell(2, 2, CHTML_table::eAnyCell, 1, 1 );
    cell->AppendPlainText("2,2 - 1x1");

    cell = table2->Cell(4, 1, CHTML_table::eAnyCell, 1, 1 );
    cell->AppendPlainText("4,1 - 1x1");

    page.Print(cout, CNCBINode::ePlainText);
    cout << endl;
}


////////////////////////////////
// Test application
//

class CTestApplication : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run (void);
};


void CTestApplication::Init(void)
{
    // Set error posting and tracing on maximum
    SetDiagTrace(eDT_Enable);
    SetDiagPostFlag(eDPF_All);
    SetDiagPostLevel(eDiag_Info);
}


int CTestApplication::Run(void)
{
    cout << "---------------------------------------------" << endl;
    s_TEST_SpecialChars();
    cout << "---------------------------------------------" << endl;
    s_TEST_Tags();
    cout << "---------------------------------------------" << endl;
    s_TEST_Table();
    cout << "---------------------------------------------" << endl;

    return 0;
}

  
///////////////////////////////////
// APPLICATION OBJECT  and  MAIN
//

int main(int argc, const char* argv[])
{
    return CTestApplication().AppMain(argc, argv);
}
