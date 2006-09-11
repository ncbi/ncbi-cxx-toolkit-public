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

// Character entity references
// http://www.w3.org/TR/html4/sgml/entities.html
// http://www.w3.org/TR/1998/REC-html40-19980424/charset.html#h-5.3

static struct tag_HtmlEntities
{
    TUnicodeSymbol u;
    const char*    s;
    
} const s_HtmlEntities[] =
{
 {  160, "nbsp" }, 
 {  161, "iexcl" }, 
 {  162, "cent" }, 
 {  163, "pound" }, 
 {  164, "curren" }, 
 {  165, "yen" }, 
 {  166, "brvbar" }, 
 {  167, "sect" }, 
 {  168, "uml" }, 
 {  169, "copy" }, 
 {  170, "ordf" }, 
 {  171, "laquo" }, 
 {  172, "not" }, 
 {  173, "shy" }, 
 {  174, "reg" }, 
 {  175, "macr" }, 
 {  176, "deg" }, 
 {  177, "plusmn" }, 
 {  178, "sup2" }, 
 {  179, "sup3" }, 
 {  180, "acute" }, 
 {  181, "micro" }, 
 {  182, "para" }, 
 {  183, "middot" }, 
 {  184, "cedil" }, 
 {  185, "sup1" }, 
 {  186, "ordm" }, 
 {  187, "raquo" }, 
 {  188, "frac14" }, 
 {  189, "frac12" }, 
 {  190, "frac34" }, 
 {  191, "iquest" }, 
 {  192, "Agrave" }, 
 {  193, "Aacute" }, 
 {  194, "Acirc" }, 
 {  195, "Atilde" }, 
 {  196, "Auml" }, 
 {  197, "Aring" }, 
 {  198, "AElig" }, 
 {  199, "Ccedil" }, 
 {  200, "Egrave" }, 
 {  201, "Eacute" }, 
 {  202, "Ecirc" }, 
 {  203, "Euml" }, 
 {  204, "Igrave" }, 
 {  205, "Iacute" }, 
 {  206, "Icirc" }, 
 {  207, "Iuml" }, 
 {  208, "ETH" }, 
 {  209, "Ntilde" }, 
 {  210, "Ograve" }, 
 {  211, "Oacute" }, 
 {  212, "Ocirc" }, 
 {  213, "Otilde" }, 
 {  214, "Ouml" }, 
 {  215, "times" }, 
 {  216, "Oslash" }, 
 {  217, "Ugrave" }, 
 {  218, "Uacute" }, 
 {  219, "Ucirc" }, 
 {  220, "Uuml" }, 
 {  221, "Yacute" }, 
 {  222, "THORN" }, 
 {  223, "szlig" }, 
 {  224, "agrave" }, 
 {  225, "aacute" }, 
 {  226, "acirc" }, 
 {  227, "atilde" }, 
 {  228, "auml" }, 
 {  229, "aring" }, 
 {  230, "aelig" }, 
 {  231, "ccedil" }, 
 {  232, "egrave" }, 
 {  233, "eacute" }, 
 {  234, "ecirc" }, 
 {  235, "euml" }, 
 {  236, "igrave" }, 
 {  237, "iacute" }, 
 {  238, "icirc" }, 
 {  239, "iuml" }, 
 {  240, "eth" }, 
 {  241, "ntilde" }, 
 {  242, "ograve" }, 
 {  243, "oacute" }, 
 {  244, "ocirc" }, 
 {  245, "otilde" }, 
 {  246, "ouml" }, 
 {  247, "divide" }, 
 {  248, "oslash" }, 
 {  249, "ugrave" }, 
 {  250, "uacute" }, 
 {  251, "ucirc" }, 
 {  252, "uuml" }, 
 {  253, "yacute" }, 
 {  254, "thorn" }, 
 {  255, "yuml" }, 
 {  402, "fnof" }, 
 {  913, "Alpha" }, 
 {  914, "Beta" }, 
 {  915, "Gamma" }, 
 {  916, "Delta" }, 
 {  917, "Epsilon" }, 
 {  918, "Zeta" }, 
 {  919, "Eta" }, 
 {  920, "Theta" }, 
 {  921, "Iota" }, 
 {  922, "Kappa" }, 
 {  923, "Lambda" }, 
 {  924, "Mu" }, 
 {  925, "Nu" }, 
 {  926, "Xi" }, 
 {  927, "Omicron" }, 
 {  928, "Pi" }, 
 {  929, "Rho" }, 
 {  931, "Sigma" }, 
 {  932, "Tau" }, 
 {  933, "Upsilon" }, 
 {  934, "Phi" }, 
 {  935, "Chi" }, 
 {  936, "Psi" }, 
 {  937, "Omega" }, 
 {  945, "alpha" }, 
 {  946, "beta" }, 
 {  947, "gamma" }, 
 {  948, "delta" }, 
 {  949, "epsilon" }, 
 {  950, "zeta" }, 
 {  951, "eta" }, 
 {  952, "theta" }, 
 {  953, "iota" }, 
 {  954, "kappa" }, 
 {  955, "lambda" }, 
 {  956, "mu" }, 
 {  957, "nu" }, 
 {  958, "xi" }, 
 {  959, "omicron" }, 
 {  960, "pi" }, 
 {  961, "rho" }, 
 {  962, "sigmaf" }, 
 {  963, "sigma" }, 
 {  964, "tau" }, 
 {  965, "upsilon" }, 
 {  966, "phi" }, 
 {  967, "chi" }, 
 {  968, "psi" }, 
 {  969, "omega" }, 
 {  977, "thetasym" }, 
 {  978, "upsih" }, 
 {  982, "piv" }, 
 { 8226, "bull" }, 
 { 8230, "hellip" }, 
 { 8242, "prime" }, 
 { 8243, "Prime" }, 
 { 8254, "oline" }, 
 { 8260, "frasl" }, 
 { 8472, "weierp" }, 
 { 8465, "image" }, 
 { 8476, "real" }, 
 { 8482, "trade" }, 
 { 8501, "alefsym" }, 
 { 8592, "larr" }, 
 { 8593, "uarr" }, 
 { 8594, "rarr" }, 
 { 8595, "darr" }, 
 { 8596, "harr" }, 
 { 8629, "crarr" }, 
 { 8656, "lArr" }, 
 { 8657, "uArr" }, 
 { 8658, "rArr" }, 
 { 8659, "dArr" }, 
 { 8660, "hArr" }, 
 { 8704, "forall" }, 
 { 8706, "part" }, 
 { 8707, "exist" }, 
 { 8709, "empty" }, 
 { 8711, "nabla" }, 
 { 8712, "isin" }, 
 { 8713, "notin" }, 
 { 8715, "ni" }, 
 { 8719, "prod" }, 
 { 8721, "sum" }, 
 { 8722, "minus" }, 
 { 8727, "lowast" }, 
 { 8730, "radic" }, 
 { 8733, "prop" }, 
 { 8734, "infin" }, 
 { 8736, "ang" }, 
 { 8743, "and" }, 
 { 8744, "or" }, 
 { 8745, "cap" }, 
 { 8746, "cup" }, 
 { 8747, "int" }, 
 { 8756, "there4" }, 
 { 8764, "sim" }, 
 { 8773, "cong" }, 
 { 8776, "asymp" }, 
 { 8800, "ne" }, 
 { 8801, "equiv" }, 
 { 8804, "le" }, 
 { 8805, "ge" }, 
 { 8834, "sub" }, 
 { 8835, "sup" }, 
 { 8836, "nsub" }, 
 { 8838, "sube" }, 
 { 8839, "supe" }, 
 { 8853, "oplus" }, 
 { 8855, "otimes" }, 
 { 8869, "perp" }, 
 { 8901, "sdot" }, 
 { 8968, "lceil" }, 
 { 8969, "rceil" }, 
 { 8970, "lfloor" }, 
 { 8971, "rfloor" }, 
 { 9001, "lang" }, 
 { 9002, "rang" }, 
 { 9674, "loz" }, 
 { 9824, "spades" }, 
 { 9827, "clubs" }, 
 { 9829, "hearts" }, 
 { 9830, "diams" }, 
 {   34, "quot" }, 
 {   38, "amp" }, 
 {   60, "lt" }, 
 {   62, "gt" }, 
 {  338, "OElig" }, 
 {  339, "oelig" }, 
 {  352, "Scaron" }, 
 {  353, "scaron" }, 
 {  376, "Yuml" }, 
 {  710, "circ" }, 
 {  732, "tilde" }, 
 { 8194, "ensp" }, 
 { 8195, "emsp" }, 
 { 8201, "thinsp" }, 
 { 8204, "zwnj" }, 
 { 8205, "zwj" }, 
 { 8206, "lrm" }, 
 { 8207, "rlm" }, 
 { 8211, "ndash" }, 
 { 8212, "mdash" }, 
 { 8216, "lsquo" }, 
 { 8217, "rsquo" }, 
 { 8218, "sbquo" }, 
 { 8220, "ldquo" }, 
 { 8221, "rdquo" }, 
 { 8222, "bdquo" }, 
 { 8224, "dagger" }, 
 { 8225, "Dagger" }, 
 { 8240, "permil" }, 
 { 8249, "lsaquo" }, 
 { 8250, "rsaquo" }, 
 { 8364, "euro" },
 {    0, 0 }
};


CStringUTF8 CHTMLHelper::HTMLDecode(const string& str, EEncoding encoding,
                                    THTMLDecodeFlags* result_flags)
{
    CStringUTF8 ustr;
    THTMLDecodeFlags result = 0;
    if (encoding == eEncoding_Unknown) {
        encoding = CStringUTF8::GuessEncoding(str);
        if (encoding == eEncoding_Unknown) {
            NCBI_THROW2(CStringException, eBadArgs,
                "Unable to guess the source string encoding", 0);
        }
    }
    // wild guess...
    ustr.reserve(str.size());

    string::const_iterator i, e = str.end();
    char ch;
    TUnicodeSymbol uch;

    for (i = str.begin(); i != e;) {
        ch = *(i++);
        //check for HTML entities and character references
        if (i != e && ch == '&') {
            string::const_iterator itmp, end_of_entity, start_of_entity;
            itmp = end_of_entity = start_of_entity = i;
            bool ent, dec, hex, parsed=false;
            ent = isalpha((unsigned char)(*itmp)) != 0;
            dec = !ent && *itmp == '#' && ++itmp != e &&
                  isdigit((unsigned char)(*itmp)) != 0;
            hex = !dec && itmp != e &&
                  (*itmp == 'x' || *itmp == 'X') && ++itmp != e &&
                  isxdigit((unsigned char)(*itmp)) != 0;
            start_of_entity = itmp;
            if (itmp != e && (ent || dec || hex)) {
                // do not look too far
                for (int len=0; len<16 && itmp != e; ++len, ++itmp) {
                    if (*itmp == '&' || *itmp == '#') {
                        break;
                    }
                    if (*itmp == ';') {
                        end_of_entity = itmp;
                        break;
                    }
                    ent = ent && isalnum( (unsigned char)(*itmp)) != 0;
                    dec = dec && isdigit( (unsigned char)(*itmp)) != 0;
                    hex = hex && isxdigit((unsigned char)(*itmp)) != 0;
                }
                if (end_of_entity != i && (ent || dec || hex)) {
                    uch = 0;
                    if (ent) {
                        string entity(start_of_entity,end_of_entity);
                        const struct tag_HtmlEntities* p = s_HtmlEntities;
                        for ( ; p->u != 0; ++p) {
                            if (entity.compare(p->s) == 0) {
                                uch = p->u;
                                parsed = true;
                                result |= fCharRef_Entity;
                                break;
                            }
                        }
                    } else {
                        parsed = true;
                        result |= fCharRef_Numeric;
                        for (itmp = start_of_entity;
                             itmp != end_of_entity; ++itmp) {
                            TUnicodeSymbol ud = *itmp;
                            if (dec) {
                                uch = 10 * uch + (ud - '0');
                            } else if (hex) {
                                if (ud >='0' && ud <= '9') {
                                    ud -= '0';
                                } else if (ud >='a' && ud <= 'f') {
                                    ud -= 'a';
                                    ud += 10;
                                } else if (ud >='A' && ud <= 'F') {
                                    ud -= 'A';
                                    ud += 10;
                                }
                                uch = 16 * uch + ud;
                            }
                        }
                    }
                    if (parsed) {
                        ustr.Append(uch);
                        i = ++end_of_entity;
                        continue;
                    }
                }
            }
        }
// no entity - append as is
        if (encoding == eEncoding_UTF8 || encoding == eEncoding_Ascii) {
            ustr.append( 1, ch );
        } else {
            result |= fEncoding;
            ustr.Append(CStringUTF8::CharToSymbol( ch, encoding ));
        }
    }
    if (result_flags) {
        *result_flags = result;
    }
    return ustr;
}

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.32  2006/09/11 12:42:20  gouriano
 * Modified HTMLDecode to also return result of decoding
 *
 * Revision 1.31  2006/08/31 19:35:08  gouriano
 * cosmetics
 *
 * Revision 1.30  2006/08/31 17:26:04  gouriano
 * Added HTMLDecode method
 *
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
