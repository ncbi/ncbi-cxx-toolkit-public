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
 * Author: Eugene Vasilchenko, Vladimir Ivanov
 *
 */


#include <ncbi_pch.hpp>
#include <html/html.hpp>

BEGIN_NCBI_SCOPE


// CHTMLHelper

string CHTMLHelper::sm_newline( "\n" );


static string s_HTMLEncode(const string& str, const string& set, 
                           CHTMLHelper::THTMLEncodeFlags flags)
{
    CNcbiOstrstream out;

    SIZE_TYPE last = 0;
    SIZE_TYPE semicolon = 0;

    // Find first symbol to encode.
    SIZE_TYPE ptr = str.find_first_of(set, last);
    while ( ptr != NPOS ) {
        // Copy plain part of the input string
        if ( ptr != last ) {
            out.write(str.data() + last, ptr - last);
        }
        // Append encoded symbol
        switch (str[ptr]) {
        case '"':
            out << "&quot;";
            break;
        case '&':
            {{
            out.put('&');
            bool is_entity = false;
            // Check on HTML entity
            if ((flags & CHTMLHelper::fSkipEntities) &&
                (ptr+2 < str.length())  &&
                (semicolon != NPOS)) {
                if ( ptr >= semicolon )
                    semicolon = str.find(";", ptr+1);
                if ( semicolon != NPOS ) {
                    SIZE_TYPE len = semicolon - ptr;
                    SIZE_TYPE p = ptr + 1;
                    if (str[ptr+1] == '#') {
                        // Check on numeric character reference encoding
                        if (flags & CHTMLHelper::fSkipNumericEntities) {
                            p++;
                            if (len  ||  len <= 4) {
                                for (; p < semicolon; ++p) {
                                    if (!isdigit((unsigned char)(str[p])))
                                        break;
                                }
                            }
                        }
                    } else {
                        // Check on literal entity
                        if (flags & CHTMLHelper::fSkipLiteralEntities) {
                            if (len  &&  len <= 10) {
                                for (; p < semicolon; ++p) {
                                    if (!isalpha((unsigned char)(str[p])))
                                        break;
                                }
                            }
                        }
                    }
                    is_entity = (p == semicolon);
                }
            }
            if ( is_entity ) {
                if (flags & CHTMLHelper::fCheckPreencoded) {
                    ERR_POST_ONCE(Warning << "string \"" <<  str <<
                                 "\" contains HTML encoded entities");
                }
            } else {
                out << "amp;";
            }
            }}
            break;

        case '<':
            out << "&lt;";
            break;
        case '>':
            out << "&gt;";
            break;
        }
        // Find next symbol to encode
        last = ptr + 1;
        ptr = str.find_first_of(set, last);
    }
    // Append last part of the source string
    if ( last != str.size() ) {
        out.write(str.data() + last, str.size() - last);
    }
    return CNcbiOstrstreamToString(out);
}


string CHTMLHelper::HTMLEncode(const string& str, THTMLEncodeFlags flags)
{
    return s_HTMLEncode(str, "\"&<>", flags);
}


string
CHTMLHelper::HTMLAttributeEncode(const string& str, THTMLEncodeFlags flags)
{
    return s_HTMLEncode(str, "\"&", flags);
}


string CHTMLHelper::StripTags(const string& str)
{
    SIZE_TYPE pos = 0;
    string s(str);

    // First, strip comments
    while ( (pos = s.find("<!--", pos)) != NPOS ) {
        SIZE_TYPE pos_end = s.find("-->", pos + 1);
        if ( pos_end == NPOS ) {
            break;
        }
        s.erase(pos, pos_end - pos + 3);
        pos++;
    }
    // Next, strip mapping tags <@...@>
    while ( (pos = s.find("<@", pos)) != NPOS ) {
        SIZE_TYPE pos_end = s.find("@>", pos + 1);
        if ( pos_end == NPOS ) {
            break;
        }
        s.erase(pos, pos_end - pos + 2);
        pos++;
    }
    // Now, strip balanced "<..>"
    pos =0;
    while ( (pos = s.find("<", pos)) != NPOS ) {
        SIZE_TYPE pos_end = s.find(">", pos + 1);
        if ( pos_end == NPOS ) {
            break;
        }
        if (pos < s.size()  &&
            (isalpha((unsigned char) s[pos + 1]) || s[pos + 1] == '/' )) {
            s.erase(pos, pos_end - pos + 1);
        } else {
            pos++;
        }
    }
    return s;
}


string CHTMLHelper::StripSpecialChars(const string& str)
{
    SIZE_TYPE pos = 0;
    string s(str);

    // Strip named and numeric character entities "&[#]...;"
    while ( (pos = s.find("&", pos)) != NPOS ) {
        SIZE_TYPE pos_end = s.find(";", pos + 1);
        if ( pos_end == NPOS ) {
            break;
        }
        if ( (pos_end - pos) > 2  &&  (pos_end - pos) < 8 ) {
            int (*check)(int c);
            SIZE_TYPE start = pos + 1;
            if ( s[start] == '#') {
                check = &isdigit;
                start++;
            } else {
                check = &isalpha;
            }
            bool need_delete = true;
            for (SIZE_TYPE i = start; i < pos_end; i++ ) {
                if ( !check((int)s[i]) ) {
                    need_delete = false;
                    break;
                }
            }
            if ( need_delete ) {
                s.erase(pos, pos_end - pos + 1);
            }
        }
        pos++;
    }
    return s;
}

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.29  2006/08/08 18:07:52  ivanov
 * CHTMLHelper -- added flags parameter to HTMLEncode/HTMLJavaScriptEncode.
 * Added new method HTMLAttributeEncode to encode HTML tags attributes.
 * Move common encode code to s_HTMLEncode.
 *
 * Revision 1.28  2005/12/19 16:55:10  jcherry
 * Inline all methods of CIDs and remove export specifier to get around
 * multiply defined symbol proble on windows (list<int> ctor and dtor)
 *
 * Revision 1.27  2005/08/25 18:55:38  ivanov
 * CHTMLHelper:: Renamed JavaScriptEncode() -> HTMLJavaScriptEncode().
 *
 * Revision 1.26  2005/08/24 12:16:04  ivanov
 * + CHTMLHelper::JavaScriptEncode()
 *
 * Revision 1.25  2005/08/24 11:12:51  ivanov
 * Rollback to R1.23
 *
 * Revision 1.24  2005/08/23 17:54:56  ivanov
 * HTMLEncode() -- added symbol &#39;
 *
 * Revision 1.23  2005/08/22 12:13:43  ivanov
 * Minor code rearrangement
 *
 * Revision 1.22  2005/06/03 16:48:43  lavr
 * Explicit (unsigned char) casts in ctype routines
 *
 * Revision 1.21  2005/05/05 12:19:50  ivanov
 * Fixed CHTMLHelper::StripTags() to correct strip consecutive tags
 *
 * Revision 1.20  2004/10/21 17:39:47  ivanov
 * CHTMLHelper::StripTags() : strip mapping tags <@...@> before stripping tags
 *
 * Revision 1.19  2004/10/20 18:58:10  ivanov
 * CHTMLHelper::StripTags() -- remove tag also if last symbol before tag
 * closing is not an alpha.
 *
 * Revision 1.18  2004/09/28 14:05:41  ivanov
 * Fixed CHTMLHelper::StripTags() to remove single closing tags
 *
 * Revision 1.17  2004/05/17 20:59:50  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.16  2004/02/04 00:39:24  ucko
 * Revert last change -- check() was biting a lot of other code, so we
 * now centrally undefine it in the corelib headers that include CoreServices.h.
 *
 * Revision 1.15  2004/02/03 22:59:58  ucko
 * Make sure to avoid getting check() from Darwin's AssertMacros.h.
 *
 * Revision 1.14  2004/02/02 15:14:03  ivanov
 * Fixed compilation warnings
 *
 * Revision 1.13  2004/02/02 14:17:45  ivanov
 * Added CHTMLHelper::StripHTML(), StripSpecialChars(). Fixed StripTags().
 *
 * Revision 1.12  2004/01/14 13:47:45  ivanov
 * HTMLEncode() performance improvement
 *
 * Revision 1.11  2003/11/03 17:03:08  ivanov
 * Some formal code rearrangement. Move log to end.
 *
 * Revision 1.10  2002/12/23 15:53:53  ivanov
 * HTMLEncode(): disable numeric characters reference encoding (like &#N;)
 *
 * Revision 1.9  2002/12/09 22:09:53  ivanov
 * Changed "pos" type from int to type_t in CHTMLHelper::StripTags()
 *
 * Revision 1.8  2002/09/25 01:24:56  dicuccio
 * Added CHTMLHelper::StripTags() - strips HTML comments and tags from any
 * string. Implemented CHTMLText::PrintBegin() for mode = ePlainText
 *
 * Revision 1.7  1999/05/20 16:52:33  pubmed
 * SaveAsText action for query; minor changes in filters,labels, tabletemplate
 *
 * Revision 1.6  1999/03/15 19:58:35  vasilche
 * CIDs now use set instead of map.
 *
 * Revision 1.5  1999/01/22 17:46:50  vasilche
 * Fixed/cleaned encoding/decoding.
 * Encoded string now shorter.
 *
 * Revision 1.4  1999/01/21 16:18:06  sandomir
 * minor changes due to NStr namespace to contain string utility functions
 *
 * Revision 1.3  1999/01/15 17:47:56  vasilche
 * Changed CButtonList options: m_Name -> m_SubmitName, m_Select ->
 * m_SelectName. Added m_Selected.
 * Fixed CIDs Encode/Decode.
 *
 * Revision 1.2  1999/01/11 15:13:36  vasilche
 * Fixed CHTML_font size.
 * CHTMLHelper extracted to separate file.
 *
 * Revision 1.1  1999/01/07 16:41:57  vasilche
 * CHTMLHelper moved to separate file.
 * TagNames of CHTML classes ara available via s_GetTagName() static
 * method.
 * Input tag types ara available via s_GetInputType() static method.
 * Initial selected database added to CQueryBox.
 * Background colors added to CPagerBax & CSmallPagerBox.
 *
 * ===========================================================================
 */
