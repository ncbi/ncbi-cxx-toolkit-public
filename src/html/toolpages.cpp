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
* Author:  !!! PUT YOUR NAME(s) HERE !!!
*
* File Description:
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  1998/12/03 22:02:51  lewisg
* *** empty log message ***
*
* Revision 1.2  1998/12/01 19:10:40  lewisg
* uses CCgiApplication and new page factory
*
* Revision 1.1  1998/11/23 23:45:20  lewisg
* *** empty log message ***
*
* ===========================================================================
*/

#include <ncbistd.hpp>
#include <toolpages.hpp>
#include <ncbi.h>
#include <ncbienv.h>
#include <memory>
BEGIN_NCBI_SCOPE


void CToolFastaPage::InitSubPages(int style)
{
    CHTMLPage::InitSubPages(style);

    try {
	if(!(style & kNoSIDEBAR)) {
	    if (!m_Sidebar) m_Sidebar = CreateSidebar();
	    if (!m_Sidebar) m_Sidebar = new CHTMLNode;
	}
    }
    catch (...) {
	delete m_Sidebar;
	throw;
    }
}


void CToolFastaPage::Draw(int style)
{
    CHTMLPage::Draw(style);

    if(!(style & kNoSIDEBAR) && m_Sidebar) 
        Rfind("<@SIDEBAR@>", m_Sidebar);
}


void CToolFastaPage::InitMembers(int style)
{
    m_PageName = "Tools";
    m_TemplateFile = "tool.html";
}


CHTMLNode * CToolFastaPage::CreateView(void) 
{
    CHTMLNode * Form;
    CHTML_textarea * Textarea;
    try {
	Form = new CHTML_form( "http://ray/cgi-bin/tools/tool", "GET", "toolform");
	Form->AppendText("Enter Fasta (and later on, Accession #):<br>");
	map <string, string>:: iterator iCgi = m_CgiApplication->m_CgiEntries.find("supplemental_input");

	if(iCgi != m_CgiApplication->m_CgiEntries.end()) {
	    Textarea = new CHTML_textarea("supplemental_input", "40", "10", CHTMLHelper::HTMLEncode((*iCgi).second));
	    Form->AppendChild(Textarea);
	    Textarea->SetAttributes("wrap", "virtual");
	}
	else {
	    Textarea = new CHTML_textarea("supplemental_input", "40", "10");
	    Form->AppendChild(Textarea);
	    Textarea->SetAttributes("wrap", "virtual");
	}
	Form->AppendText("<br>Then click on a tool in the left column.");
	Form->AppendChild(new CHTML_input("hidden", "toolname", "\"\""));
    }
    catch(...) {
	delete Form;
	delete Textarea;
	throw;
    }
    return Form;
}


CHTMLNode * CToolFastaPage::CreateSidebar(void) 
{
    CHTML_p * Choices;
    CHTML_a * Hyperlink;

    try {
	Choices = new CHTML_p;
	Choices->AppendText("<span class=GUTTER1>Tools</span><br>");
	Hyperlink = new CHTML_a("\"javascript:document.toolform.toolname.value='seg';document.toolform.submit()\"","seg");
	Choices->AppendChild(Hyperlink);
	Hyperlink->SetAttributes("class", "GUTTER2");
	return Choices;
    }
    catch(...) {
	delete Choices;
	delete Hyperlink;
	throw;
    }
}


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
	Options->m_CgiApplication = m_CgiApplication;
	Options->Create();
    } 
    catch(...) {
	delete Options;
	throw;
    }
    return Options;
}


void CToolReportPage::InitMembers(int style)
{
    m_PageName = "Results Page";
    m_TemplateFile = "tool.html";
    
}

#if 0
CHTMLNode * CToolReportPage::CreateView(void) 
{
    return NULL;
}
#endif

CHTMLNode * CToolReportPage::CreateView(void) 
{
    CHTMLNode * Form;
    CHTML_textarea * Textarea;
    string input;  
    char ch, buf[1024];
    
    try {
	Form = new CHTML_form( "http://ray/cgi-bin/tools/tool", "GET", "toolform");
	Form->AppendText("Enter Fasta (and later on, Accession #):<br>");
	//	NcbiCin >> buf; // Content type
	//	NcbiCin >> buf;
	input = "fasta";// buf;
	if(input == "fasta") {
	    	    NcbiCin.get(ch);  // skip the blank line
	    	    input = "";
	    	    while( NcbiCin.get(ch)) input += ch;
	    Textarea = new CHTML_textarea("supplemental_input", "40", "10", CHTMLHelper::HTMLEncode(input));
	    Form->AppendChild(Textarea);
	    Textarea->SetAttributes("wrap", "virtual");
	    Form->AppendText("<br>Then click on a tool in the left column.");
	}
	else { 
	    NcbiCin.get(ch);  // skip the blank line	   
	    input = "";
	    while( NcbiCin.get(ch)) input += ch;
	    Textarea = new CHTML_textarea("supplemental_input", "40", "10");
	    Form->AppendChild(Textarea);
	    Textarea->SetAttributes("wrap", "virtual");
	    Form->AppendText("<br>Then click on a tool in the left column. <h3>Results:</h3>");
	    Form->AppendChild(new CHTML_pre(input));
	}
	Form->AppendChild(new CHTML_input("hidden", "toolname", "\"\""));
    }
    catch(...) {
	delete Form;
	delete Textarea;
	throw;
    }
    return Form;
}



///////////////////////////
// functions to retrieve options

string CHTMLOptionForm::GetParam(const string & name, int number)
{
    char buf[kBUFLEN];
    CNcbiOstrstream type;

    type << name.c_str() << number << '\0';
    GetAppParam( (char *)m_ParamFile.c_str(), (char *)m_SectionName.c_str(), type.str(), NULL, buf, kBUFLEN-1);
    return buf;
}
 

string CHTMLOptionForm::GetParam(const string & name)
{
    char buf[kBUFLEN];

    GetAppParam( (char *)m_ParamFile.c_str(), (char *)m_SectionName.c_str(),(char *) name.c_str(), NULL, buf, kBUFLEN-1);
    return buf;
}


void CHTMLOptionForm::Draw(int style)
{
    CHTMLNode * Form;
    try {
	Form = new CHTML_form("http://ray/cgi-bin/tools/segify", "GET");
	AppendChild(Form);
	map <string, string>:: iterator iCgi = m_CgiApplication->m_CgiEntries.find("supplemental_input");
	if(iCgi != m_CgiApplication->m_CgiEntries.end()) Form->AppendChild(new CHTML_input("hidden", "supplemental_input", "\""+ CHTMLHelper::HTMLEncode((*iCgi).second)+"\""));
	Form->AppendChild(new CHTML_input("hidden", "toolpage_output", "TRUE"));
	Form->AppendChild(new CHTML_input("hidden", "toolpage_options", "notcgi"));

	int i = 1;
	string Description;
	while((Description = GetParam("PROMPT_", i)) != "") {
	    string Type = GetParam("TYPE_", i);
	    string Parameter = GetParam("PARAM_", i);
	    string DefaultValue = GetParam("DEFAULT_", i);

	    // walk through the type of controls
	    ////////// todo: add radio, etc.

	    if(Type == "checkbox") {
		Form->AppendChild(new CHTML_checkbox(Parameter, DefaultValue, Description, DefaultValue == "TRUE"));
		Form->AppendText("<p>");
	    }
	    else if (Type == "text") {
		Form->AppendText(Description+" ");
		if (DefaultValue == "") DefaultValue = "\"\"";
		Form->AppendChild(new CHTML_input("text", Parameter, DefaultValue));
		Form->AppendText("<p>");
	    }
	    else if (Type == "popup") {
		Form->AppendText(Description+" ");
		SIZE_TYPE Start = 0, End;
		string Choices = GetParam("CHOICES_", i);
		if(Choices != "") {
		    CHTML_select * Select = new CHTML_select(Parameter);
		    Form->AppendChild(Select);
		    if (DefaultValue == "") Select->AppendOption("");
		    do {
			End = Choices.find(',', Start);
			string Subchoices = Choices.substr(Start, End - Start);
			Select->AppendOption(Subchoices, Subchoices == DefaultValue);
			Start = End + 1;
		    } while(End != NPOS);
		}
		Form->AppendText("<p>");
	    }
	    i++;
	}
	Form->AppendChild(new CHTML_input("submit", ".submit", "Calculate"));
	Form->AppendText("<p>&nbsp;</p>");
    }
    catch (...) {
	delete Form;
	throw;
    }
}


END_NCBI_SCOPE
