#ifndef HTMLHELPER__HPP
#define HTMLHELPER__HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.7  2000/01/24 14:21:28  vasilche
* Fixed missing find() declaration.
*
* Revision 1.6  2000/01/21 20:06:53  pubmed
* Volodya: support of non-existing documents for Sequences
*
* Revision 1.5  1999/05/20 16:49:12  pubmed
* Changes for SaveAsText: all Print() methods get mode parameter that can be HTML or PlainText
*
* Revision 1.4  1999/03/15 19:57:55  vasilche
* CIDs now use set instead of map.
*
* Revision 1.3  1999/01/22 17:46:48  vasilche
* Fixed/cleaned encoding/decoding.
* Encoded string now shorter.
*
* Revision 1.2  1999/01/15 19:46:18  vasilche
* Added CIDs class to hold sorted list of IDs.
*
* Revision 1.1  1999/01/07 16:41:54  vasilche
* CHTMLHelper moved to separate file.
* TagNames of CHTML classes ara available via s_GetTagName() static
* method.
* Input tag types ara available via s_GetInputType() static method.
* Initial selected database added to CQueryBox.
* Background colors added to CPagerBax & CSmallPagerBox.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <map>
#include <set>
#include <list>
#include <algorithm>

BEGIN_NCBI_SCOPE

class CNCBINode;
class CHTML_form;

// utility functions

class CIDs : public list<int>
{
public:
    CIDs(void);

    ~CIDs(void);

    // if 'id' is not in list, return false
    // if 'id' in list - return true and remove 'id' from list
    bool ExtractID(int id);

    // add 'id' to list
    void AddID(int id);

    // return number of ids in list
    int CountIDs(void) const;

    // decode id list from text representation
    void Decode(const string& str);

    // encode id list to text representation
    string Encode(void) const;

private:
    // decoder helpers
    int GetNumber(const string& str);
    int AddID(char cmd, int id, int number);
    
};

class CHTMLHelper
{
public:
    // HTML encodes a string. E.g. &lt;
    static string HTMLEncode(const string &);

    typedef set<int> TIDList;
    typedef multimap<string, string> TCgiEntries;

    // load ID list from CGI request
    // Args:
    //   ids            - resulting ID list
    //   values         - CGI values
    //   hiddenPrefix   - prefix for hidden values names
    //   checkboxPrefix - prefix for checkboxes names
    static void LoadIDList(TIDList& ids,
                           const TCgiEntries& values,
                           const string& hiddenPrefix,
                           const string& checkboxPrefix);
    // store ID list to HTML form
    // Args:
    //   form           - HTML form to fill
    //   ids            - ID list
    //   hiddenPrefix   - prefix for hidden values names
    //   checkboxPrefix - prefix for checkboxes names
    static void StoreIDList(CHTML_form* form,
                            const TIDList& ids,
                            const string& hiddenPrefix,
                            const string& checkboxPrefix);

    // platform-dependent newline symbol
    // default value is "\n" as in UNIX
    // application program is to set it as correct
    static void SetNL( const string& nl )
        { sm_newline = nl; }
    
    static const string& GetNL(void)
        { return sm_newline; }

protected:

    static string sm_newline;

};

#include <html/htmlhelper.inl>

END_NCBI_SCOPE

#endif  /* HTMLHELPER__HPP */
