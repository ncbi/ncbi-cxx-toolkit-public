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
 *  Please say you saw it at NCBI
 *
 * ===========================================================================
 *
 * Author:  Andrei Gourianov
 *
 * File Description:
 *   Formats object's dump as a text and sends it into any output stream
 *
 */

#include <corelib/ncbiobj.hpp>
#include <corelib/text_dump_context.hpp>
#include <sstream>

BEGIN_NCBI_SCOPE

CTextDumpContext::CTextDumpContext(ostream& os, const string& bundle)
    : m_Out(os), CDebugDumpContext(bundle)
{
}

CTextDumpContext::~CTextDumpContext(void)
{
    if (IsStarted()) {
        // EndBundle() actually, I just do not want to call
        // a virtual function here
        x_InsertPageBreak();
    }
}


bool CTextDumpContext::StartBundle(unsigned int level, const string& bundle)
{
    if (level == 0) {
        x_InsertPageBreak(bundle);
    } else {
        x_IndentLine(level);
        m_Out << (bundle.empty() ? "?" : bundle.c_str()) << " = {" << endl;
    }
    return true;
}


void CTextDumpContext::EndBundle(  unsigned int level, const string& /*bundle*/)
{
    if (level == 0) {
        x_InsertPageBreak();
    } else {
        x_IndentLine(level);
        m_Out << "}" << endl;
    }
}


bool CTextDumpContext::StartFrame(unsigned int level, const string& frame)
{
    x_IndentLine(level);
    m_Out << (frame.empty() ? "?" : frame.c_str()) << " {" << endl;
    return true;
}


void CTextDumpContext::EndFrame( unsigned int level, const string& /*frame*/)
{
    x_IndentLine(level);
    m_Out << "}" << endl;
}


void CTextDumpContext::PutValue(unsigned int level, const string& /*frame*/,
    const string& name, const string& value, const string& comment)
{
    x_IndentLine(level+1);
    m_Out << name << " = \'" << value << "\'";
    if (!comment.empty()) {
        m_Out << " (" << comment << ")";
    }
    m_Out << endl;
}



void CTextDumpContext::x_IndentLine(unsigned int level, char c, int len)
{
    stringstream tmp;
    for (int i=1; i<level; ++i) {
        for (int l=0; l<len; ++l) {
            tmp.put(c);
        }
    }
    m_Out << tmp.str();
}

void CTextDumpContext::x_InsertPageBreak(const string& title, char c, int len)
{
    stringstream tmp;
    if (!title.empty()) {
        int i1 = (len - title.length() - 2)/2;
        int l;
        for (l=0; l<i1; ++l) {
            tmp.put(c);
        }
        tmp << " " << title << " ";
        for (l=0; l<i1; ++l) {
            tmp.put(c);
        }
    } else {
        for (int l=0; l<len; ++l) {
            tmp.put(c);
        }
    }
    m_Out << tmp.str() << endl;
}

END_NCBI_SCOPE
/*
 * ===========================================================================
 *  $Log$
 *  Revision 1.1  2002/05/14 14:44:25  gouriano
 *  added DebugDump function to CObject
 *
 * ===========================================================================
*/

