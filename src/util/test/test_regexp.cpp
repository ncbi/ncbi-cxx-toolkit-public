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
    string hit = pattern.GetMatch("The Dodgers play baseball.");
    cout << hit << endl;
    
    // Perl compatible regular expression pattern to match
    string pat("(q.*k).*f?x");
    
    // String to find matching pattern in
    string text("The quick brown fox jumped over the lazy dogs.\n");
    text += "Now is the time for all good men to come to the aid of their ";
    text += "country.\nTwas the night before Christmas and all through the ";
    text += "house, not a\n creature was stirring, not even a mouse.\n";
    
    // Create a CRegexp object and compile pattern
    CRegexp cpat(pat.c_str());
    
    // Find the matching substrings for pattern and sub patterns
    // Return results as array of CRegexp::SMatches
    const CRegexp::SMatches *rslt = cpat.Match(text.c_str());
    
    // Display m_Begin and m_End (1 past last) index for matching pattern 
    // and sub patterns
    for (size_t i = 0; i < 10 && rslt[i].m_Begin >= 0; i++) {
        cout << rslt[i].m_Begin << endl;
        cout << rslt[i].m_End << endl;
    }
        
    // Display the matching substrings for pattern and sub patterns
    for (size_t i = 0; i < 10 && rslt[i].m_Begin >= 0; i++) {
        cout << cpat.GetSub(text.c_str(), i) << endl;
    }
    
    // Use different pattern
    pat = "t\\w*e";
    
    // Compile new pattern
    cpat.Set(pat.c_str(), CRegexp::eCompile_ignore_case);
    
    // Create cstring from string
    char *cstr = new char[text.length() + 1];
    strcpy(cstr, text.c_str());
    
    // Loop through and display all non-overlapping matches
    int offset = 0;
    do {
        rslt = cpat.Match(cstr + offset);
        cout << cpat.GetSub(cstr + offset) << endl;
        if (rslt[0].m_Begin >= 0) {
            offset += rslt[0].m_End;
            cout << (cstr+offset) << endl;
        }
    } while (rslt[0].m_Begin > -1);
    
    // Loop through and display all matches   
    string mstr;
    size_t occ = 0;
    do {
        mstr = cpat.GetMatch(cstr, 0, occ);
        cout << "occ: " << occ << "\t" << mstr << endl;
        occ++;
    } while (!mstr.empty());
    
    delete[] cstr;
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
 * Revision 1.1  2003/06/03 14:51:53  clausen
 * test_regexp.cpp
 *
 * ===========================================================================
 */
