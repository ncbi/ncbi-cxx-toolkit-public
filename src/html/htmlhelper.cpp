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


#include <html/html.hpp>
#include <html/htmlhelper.hpp>


BEGIN_NCBI_SCOPE


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


int CIDs::GetNumber(const string& str)
{
    return NStr::StringToInt(str);
}


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


// CHTMLHelper

string CHTMLHelper::sm_newline( "\n" );

string CHTMLHelper::HTMLEncode(const string& input)
{
    string output;
    string::size_type last = 0;

    // Find first symbol to encode
    string::size_type ptr = input.find_first_of("\"&<>", last);
    while ( ptr != string::npos ) {
        // Copy plain part of input
        if ( ptr != last ) {
            output.append(input, last, ptr - last);
        }
        // Append encoded symbol
        switch ( input[ptr] ) {
        case '"':
            output.append("&quot;");
            break;
        case '&':
            output.append("&");
            // Disable numeric characters reference encoding
            if ( (ptr+1 >= input.length()) || (input[ptr+1] != '#') ) {
                output.append("amp;");
            }
            break;
        case '<':
            output.append("&lt;");
            break;
        case '>':
            output.append("&gt;");
            break;
        }

        // Skip it
        last = ptr + 1;

        // Find next symbol to encode
        ptr = input.find_first_of("\"&<>", last);
    }

    // Append last part of input
    if ( last != input.size() ) {
        output.append(input, last, input.size() - last);
    }
    return output;
}


string CHTMLHelper::StripTags (const string& input)
{
    size_t pos;
    string s(input);

    // First, strip comments
    while ( (pos = s.find("<!--")) != NPOS ) {
        int pos_end = s.find("-->", pos);
        s.erase(pos, pos_end - pos + 3);
    }
    // Now, strip balanced "<..>"
    while ( (pos = s.find_first_of("<")) != NPOS ) {
        int pos_end = s.find_first_of(">", pos);
        s.erase(pos, pos_end - pos + 1);
    }
    return s;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
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
 * string.  Implemented CHTMLText::PrintBegin() for mode = ePlainText
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
