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

    // Platform-dependent newline symbol.
    // Default value is "\n" as in UNIX.
    // Application program is to set it as correct.
    static void SetNL(const string& nl);
    
    static string GetNL(void)
        { return sm_newline; }

protected:
    static const char* sm_newline;
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

#endif  /* HTMLHELPER__HPP */
