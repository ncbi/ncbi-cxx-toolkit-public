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
 * Authors:  Clifford Clausen
 *
 * File Description:
 *   Test program for CRegexp:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <util/regexp.hpp>

USING_NCBI_SCOPE;


class CRegexApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};


void CRegexApplication::Init(void)
{
}


int CRegexApplication::Run(void)
{
    // Simple way to use regular expressions
    CRegexp pattern("D\\w*g");
    cout << pattern.GetMatch("The Dodgers play baseball.") << endl;
    
    // Perl compatible regular expression pattern to match
    string pat("(q.*k).*f?x");
    pattern.Set(pat);
    
    // String to find matching pattern in
    string text("The quick brown fox jumped over the lazy dogs.\n");
    text += "Now is the time for all good men to come to the aid of their ";
    text += "country.\nTwas the night before Christmas and all through the ";
    text += "house, not a\n creature was stirring, not even a mouse.\n";
    
    // Display pattern and sub pattern matches
    cout << pattern.GetMatch(text.c_str()) << endl;
    for (int k = 1; k < pattern.NumFound(); k++) {
        cout << pattern.GetSub(text.c_str(), 1) << endl;
    }    
    
    // Set new pattern and ignore case
    pattern.Set("t\\w*e", CRegexp::eCompile_ignore_case);
    
    // Find all matches to pattern
    size_t start = 0;
    while (start != string::npos) {
        string match = pattern.GetMatch(text.c_str(), start);
        if (pattern.NumFound() > 0) {
            cout << match << endl;
            start = text.find(match, start) + 1;            
        } else {
            break;
        }
    }
    
    // Same as above but with cstrings and elinates string return
    start = 0;
    char *txt = new char[text.length() + 1];
    strcpy(txt, text.c_str());
    while (true)
    {
        pattern.GetMatch(txt, start, 0, 0, true);
        if (pattern.NumFound() > 0) {
            const int *rslt = pattern.GetResults(0);
            start = rslt[1];
            for (int i = rslt[0]; i < rslt[1]; i++) {
                cout << txt[i];
            }
            cout << endl;
        } else {
            break;
        }
    }
    delete[] txt;                  
    return 0;
}


void CRegexApplication::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CRegexApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2004/05/17 21:09:26  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.3  2003/06/26 15:37:41  lavr
 * Remove unused variable "len", line 97
 *
 * Revision 1.2  2003/06/20 18:31:51  clausen
 * Switched to work with new CRegexp interface
 *
 * Revision 1.1  2003/06/03 14:51:53  clausen
 * test_regexp.cpp
 *
 * ===========================================================================
 */
