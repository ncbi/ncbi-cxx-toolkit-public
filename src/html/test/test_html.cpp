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
* File Description:
*   Play around with the HTML library
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2001/06/08 19:05:27  ivanov
* Initialization. Test application for html library
*
* ===========================================================================
*/


#include <html/html.hpp>
#include <html/page.hpp>


//BEGIN_NCBI_SCOPE

//END_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
//  MAIN()


USING_NCBI_SCOPE;  // this turns on the ncbi namespace


int main(int argc, char *argv[])
{
    // Tags test;

    CHTMLPage page("Simple test page"); 

    page.AppendChild(new CHTML_nbsp(2));
    page.AppendChild(new CHTML_gt);
    page.AppendChild(new CHTML_lt);
    page.AppendChild(new CHTML_quot);
    page.AppendChild(new CHTML_amp);
    page.AppendChild(new CHTML_copy);
    page.AppendChild(new CHTML_reg);
    page.AppendChild(new CHTMLSpecialChar("#177","+/-"));

    cout << "---------------------------------------------" << endl;
    page.Print(cout, CNCBINode::eHTML);
    cout << endl;
    page.Print(cout, CNCBINode::ePlainText);
    cout << endl;
    cout << "---------------------------------------------" << endl;
    cout << endl;


    // Page test

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
    cout << endl << endl;

    return 0;
}
