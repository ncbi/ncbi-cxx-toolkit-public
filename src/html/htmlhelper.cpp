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
#include <html/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Html_Lib

BEGIN_NCBI_SCOPE


// CHTMLHelper

const char* kDefaultNL = "\n";
const char* CHTMLHelper::sm_newline = kDefaultNL;


void CHTMLHelper::SetNL(const string& nl)
{
    if (sm_newline != kDefaultNL) {
        free((char*)sm_newline);
    }
    sm_newline = strdup(nl.c_str());
}


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
                    ERR_POST_X_ONCE(2, Info << "string \"" <<  str <<
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
// http://en.wikipedia.org/wiki/List_of_XML_and_HTML_character_entity_references

static struct tag_HtmlEntities
{
    TUnicodeSymbol u;
    const char*    s;
    
} const s_HtmlEntities[] =
{
 {   34, "quot" }, 
 {   38, "amp" }, 
 {   39, "apos" }, 
 {   60, "lt" }, 
 {   62, "gt" }, 
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
 {  338, "OElig" }, 
 {  339, "oelig" }, 
 {  352, "Scaron" }, 
 {  353, "scaron" }, 
 {  376, "Yuml" }, 
 {  402, "fnof" }, 
 {  710, "circ" }, 
 {  732, "tilde" }, 
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
 { 8226, "bull" }, 
 { 8230, "hellip" }, 
 { 8240, "permil" }, 
 { 8242, "prime" }, 
 { 8243, "Prime" }, 
 { 8249, "lsaquo" }, 
 { 8250, "rsaquo" }, 
 { 8254, "oline" }, 
 { 8260, "frasl" }, 
 { 8364, "euro" },
 { 8465, "image" }, 
 { 8472, "weierp" }, 
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
 {    0, 0 }
};


CStringUTF8 CHTMLHelper::HTMLDecode(const string& str, EEncoding encoding,
                                    THTMLDecodeFlags* result_flags)
{
    CStringUTF8 ustr;
    THTMLDecodeFlags result = 0;
    if (encoding == eEncoding_Unknown) {
        encoding = CUtf8::GuessEncoding(str);
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
                        ustr += CUtf8::AsUTF8(&uch,1);
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
            ustr += CUtf8::AsUTF8(CTempString(&ch,1), encoding);
        }
    }
    if (result_flags) {
        *result_flags = result;
    }
    return ustr;
}

END_NCBI_SCOPE
