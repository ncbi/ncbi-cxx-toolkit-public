/*  $RCSfile$  $Revision$  $Date$
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
*   Some test code for the HTML library
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  1998/10/06 20:36:06  lewisg
* new html lib and test program
*
* ===========================================================================
*/

#include <html.hpp>

int main()
{
    string output("");

    CHTML_p * paragraph = new CHTML_p;  // create a paragraph
    paragraph->SetAttributes(string("class"), string("text"));  //set an attribute

    CHTMLText * text = new CHTMLText;
    text->Data(string("one <@testing@> three")); // some text
    paragraph->AppendChild(text); // make it a child of the paragraph tag

    CHTML_table * table = new CHTML_table("#FFFOOO",3,3); // make a table  
    paragraph->Rfind(string("<@testing@>"), table); // replace "<@testing@>
    
    CHTML_form * form = new CHTML_form("GET", "http://www.ncbi.gov");
    table->InsertInTable(2, 2, form);  // insert form in row 2 column 2
    table->ColumnWidth(table, 2, "65");  // set the width of column 2 to 65

    form->AppendChild(new CHTML_input("checkbox", "fred", "13", "some text"));
    form->AppendChild(new CHTML_textarea("lewis", "4", "3"));

    CHTML_select * select = new CHTML_select("fred");
    select->AppendOption("option 1");
    select->AppendOption("option 2", true);
    form->AppendChild(select);

    paragraph->Print(output);  // serialize it
    cout << output << endl;
    
    delete paragraph;
    return 0;
    
}