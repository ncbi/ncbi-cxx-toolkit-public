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

BEGIN_NCBI_SCOPE

class CNCBINode;
class CHTML_form;

// utility functions

class CIDs : public map<int, int>
{
public:
    CIDs(void);

    ~CIDs(void);

    bool HaveID(int id) const;

    bool ExtractID(int id);

    void AddID(int id);

    void RemoveID(int id);

    int CountIDs(void) const;

    void Decode(const string& str);

    string Encode(void) const;
};

class CHTMLHelper
{
public:
    // HTML encodes a string. E.g. &lt;
    static string HTMLEncode(const string &);

    typedef map<int, bool> TIDList;
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

};

#include <html/htmlhelper.inl>

END_NCBI_SCOPE

#endif  /* HTMLHELPER__HPP */
