#ifndef TOOLPAGES__HPP
#define TOOLPAGES__HPP

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
*   Pages used by the tools cgi program
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  1998/12/01 19:09:07  lewisg
* uses CCgiApplication and new page factory
*
* Revision 1.1  1998/11/23 23:47:20  lewisg
* *** empty log message ***
*
* ===========================================================================
*/

#include <ncbistd.hpp>
#include <page.hpp>

BEGIN_NCBI_SCOPE


/////////////////////////////////
///// the front page with fasta

const int kNoSIDEBAR = 0x4;

class CToolFastaPage: public CHTMLPage {
public:
    virtual void InitMembers(int);
    virtual void InitSubPages(int Style);
    virtual void Draw(int Style);
    virtual CHTMLNode * CreateView(void);
    CHTMLNode * m_Sidebar;
    virtual CHTMLNode * CreateSidebar(void);

    CToolFastaPage() { m_Sidebar = NULL; }
    static CHTMLBasicPage * New(void) { return new CToolFastaPage;}

};


/////////////////////////////////
///// the options page

class CToolOptionPage: public CToolFastaPage {
public:
    virtual void InitMembers(int);
    virtual CHTMLNode * CreateView(void);
    virtual CHTMLNode * CreateSidebar(void);
    static CHTMLBasicPage * New(void) { return new CToolOptionPage;}

};


/////////////////////////////////
///// the report page. called by SEALS wrapper

class CToolReportPage: public CToolFastaPage {
public:
    virtual void InitMembers(int);
    virtual CHTMLNode * CreateView(void);  // can be fasta, pretty, or error
    static CHTMLBasicPage * New(void) { return new CToolReportPage;}

};


////////////////////////////////
///// 

class CHTMLOptionForm: public CHTMLBasicPage {
public: 
    virtual void Draw(int style);   // create and aggregate sub pages + other html
    static const int kBUFLEN = 4096;  // the maximum length of a line in param file

    string m_ParamFile;   // the name of the param file
    string m_SectionName; // which section to use in the param file
    string m_ActionURL;   // the url for the form action

    string GetParam(const string & name);  // get a param from the param file
    string GetParam(const string & name, int number);  // the number is appended to the name
  
};


END_NCBI_SCOPE
#endif
