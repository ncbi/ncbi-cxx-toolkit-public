#ifndef HTML___HTMLHELPER__HPP
#define HTML___HTMLHELPER__HPP

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
 */

/// @file htmlhelper.hpp
/// HTML library helper classes and functions.


#include <corelib/ncbistd.hpp>
#include <map>
#include <set>
#include <list>
#include <algorithm>


/** @addtogroup HTMLHelper
 *
 * @{
 */


BEGIN_NCBI_SCOPE


class CNCBINode;
class CHTML_form;


// Utility functions

class NCBI_XHTML_EXPORT CHTMLHelper
{
public:

    /// Flags for HTMLEncode
    enum EHTMLEncodeFlags {
        fEncodeAll           = 0,       ///< Encode all symbols
        fSkipLiteralEntities = 1 << 1,  ///< Skip "&entity;"
        fSkipNumericEntities = 1 << 2,  ///< Skip "&#NNNN;"
        fSkipEntities        = fSkipLiteralEntities | fSkipNumericEntities,
        fCheckPreencoded     = 1 << 3   ///< Print warning if some preencoded
                                        ///< entity found in the string
    };
    typedef int THTMLEncodeFlags;       //<  bitwise OR of "EHTMLEncodeFlags"

    /// HTML encodes a string. E.g. &lt;.
    static string HTMLEncode(const string& str,
                             THTMLEncodeFlags flags = fEncodeAll);

    enum EHTMLDecodeFlags {
        fCharRef_Entity  = 1,       ///< Character entity reference(s) was found
        fCharRef_Numeric = 1 << 1,  ///< Numeric character reference(s) was found
        fEncoding        = 1 << 2   ///< Character encoding changed
    };
    typedef int THTMLDecodeFlags;       //<  bitwise OR of "EHTMLDecodeFlags"

    /// Decode HTML entities and character references    
    static CStringUTF8 HTMLDecode(const string& str,
                                  EEncoding encoding = eEncoding_Unknown,
                                  THTMLDecodeFlags* result_flags = NULL);

    /// Encode a string for JavaScript.
    ///
    /// Call HTMLEncode and also encode all non-printable characters.
    /// The non-printable characters will be represented as "\S"
    /// where S is the symbol code or as "\xDD" where DD is
    /// the character's code in hexadecimal.
    /// @sa NStr::JavaScriptEncode, HTMLEncode
    static string HTMLJavaScriptEncode(const string& str,
                                      THTMLEncodeFlags flags = fEncodeAll);

    /// HTML encodes a tag attribute ('&' and '"' symbols).
    static string HTMLAttributeEncode(const string& str,
                                      THTMLEncodeFlags flags = fSkipEntities);

    /// Strip all HTML code from a string.
    static string StripHTML(const string& str);

    /// Strip all HTML tags from a string.
    static string StripTags(const string& str);

    /// Strip all named and numeric character entities from a string.
    static string StripSpecialChars(const string& str);

    typedef set<int> TIDList;
    typedef multimap<string, string> TCgiEntries;

    // Load ID list from CGI request.
    // Args:
    //   ids            - resulting ID list
    //   values         - CGI values
    //   hiddenPrefix   - prefix for hidden values names
    //   checkboxPrefix - prefix for checkboxes names
    static void LoadIDList(TIDList& ids,
                           const TCgiEntries& values,
                           const string& hiddenPrefix,
                           const string& checkboxPrefix);
    // Store ID list to HTML form.
    // Args:
    //   form           - HTML form to fill
    //   ids            - ID list
    //   hiddenPrefix   - prefix for hidden values names
    //   checkboxPrefix - prefix for checkboxes names
    static void StoreIDList(CHTML_form* form,
                            const TIDList& ids,
                            const string& hiddenPrefix,
                            const string& checkboxPrefix);

    // Platform-dependent newline symbol.
    // Default value is "\n" as in UNIX.
    // Application program is to set it as correct.
    static void SetNL( const string& nl )
        { sm_newline = nl; }
    
    static const string& GetNL(void)
        { return sm_newline; }

protected:
    static string sm_newline;
};


/// CIDs class to hold sorted list of IDs.
///
/// It is here by historical reasons.
/// We cannot find place for it in any other library now.

class CIDs : public list<int>
{
public:
    CIDs(void)  {};
    ~CIDs(void) {};

    // If 'id' is not in list, return false.
    // If 'id' in list - return true and remove 'id' from list.
    bool ExtractID(int id);

    // Add 'id' to list.
    void AddID(int id) { push_back(id); }

    // Return number of ids in list.
    size_t CountIDs(void) const { return size(); }

    // Decode id list from text representation.
    void Decode(const string& str);

    // Encode id list to text representation.
    string Encode(void) const;

private:
    // Decoder helpers.
    int GetNumber(const string& str);
    int AddID(char cmd, int id, int number);
};



//=============================================================================
//  Inline methods
//=============================================================================

inline
string CHTMLHelper::HTMLJavaScriptEncode(const string& str,
                                         THTMLEncodeFlags flags)
{
    return NStr::JavaScriptEncode(HTMLEncode(str, flags));
}

inline
string CHTMLHelper::StripHTML(const string& str)
{
    return CHTMLHelper::StripSpecialChars(CHTMLHelper::StripTags(str));
}

inline
bool CIDs::ExtractID(int id)
{
    iterator i = find(begin(), end(), id);
    if ( i != end() ) {
        erase(i);
        return true;
    }
    return false;
}

inline
int CIDs::GetNumber(const string& str)
{
    return NStr::StringToInt(str);
}


inline
void CIDs::Decode(const string& str)
{
    if ( str.empty() ) {
        return;
    }
    int id = 0;         // previous ID
    SIZE_TYPE pos;      // current position
    char cmd = str[0];  // command

    // If string begins with digit
    if ( cmd >= '0' && cmd <= '9' ) {
        cmd = ',';      // default command: direct ID
        pos = 0;        // start of number
    }
    else {
        pos = 1;        // start of number
    }

    SIZE_TYPE end;      // end of number
    while ( (end = str.find_first_of(" +_,", pos)) != NPOS ) {
        id = AddID(cmd, id, GetNumber(str.substr(pos, end - pos)));
        cmd = str[end];
        pos = end + 1;
    }
    id = AddID(cmd, id, GetNumber(str.substr(pos)));
}


inline
int CIDs::AddID(char cmd, int id, int number)
{
    switch ( cmd ) {
    case ' ':
    case '+':
    case '_':
        // incremental ID
        id += number;
        break;
    default:
        id = number;
        break;
    }
    AddID(id);
    return id;
}


inline
string CIDs::Encode(void) const
{
    string out;
    int idPrev = 0;
    for ( const_iterator i = begin(); i != end(); ++i ) {
        int id = *i;
        if ( !out.empty() )
            out += ' ';
        out += NStr::IntToString(id - idPrev);
        idPrev = id;
    }
    return out;
}

END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.20  2006/09/11 12:42:30  gouriano
 * Modified HTMLDecode to also return result of decoding
 *
 * Revision 1.19  2006/08/31 17:25:44  gouriano
 * Added HTMLDecode method
 *
 * Revision 1.18  2006/08/08 18:06:48  ivanov
 * CHTMLHelper -- added flags parameter to HTMLEncode/HTMLJavaScriptEncode.
 * Added new method HTMLAttributeEncode to encode HTML tags attributes.
 *
 * Revision 1.17  2005/12/19 16:55:10  jcherry
 * Inline all methods of CIDs and remove export specifier to get around
 * multiply defined symbol proble on windows (list<int> ctor and dtor)
 *
 * Revision 1.16  2005/08/25 18:55:06  ivanov
 * CHTMLHelper:: Renamed JavaScriptEncode() -> HTMLJavaScriptEncode()
 *
 * Revision 1.15  2005/08/24 12:15:54  ivanov
 * + CHTMLHelper::JavaScriptEncode()
 *
 * Revision 1.14  2005/08/22 12:13:02  ivanov
 * Dropped htmlhelper.inl
 *
 * Revision 1.13  2004/02/02 14:18:33  ivanov
 * Added CHTMLHelper::StripHTML(), StripSpecialChars()
 *
 * Revision 1.12  2003/11/05 18:41:06  dicuccio
 * Added export specifiers
 *
 * Revision 1.11  2003/11/03 17:02:53  ivanov
 * Some formal code rearrangement. Move log to end.
 *
 * Revision 1.10  2003/04/25 13:45:28  siyan
 * Added doxygen groupings
 *
 * Revision 1.9  2002/09/25 01:24:29  dicuccio
 * Added CHTMLHelper::StripTags() - strips HTML comments and tags from any
 * string
 *
 * Revision 1.8  2000/10/13 19:54:52  vasilche
 * Fixed warnings on 64 bit compiler.
 *
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

#endif  /* HTMLHELPER__HPP */
