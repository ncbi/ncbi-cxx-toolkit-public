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

#include <html/html.hpp>
#include <html/htmlhelper.hpp>

BEGIN_NCBI_SCOPE

void CIDs::Decode(const string& str)
{
    _TRACE( " decode: \"" << str << "\"" );
    if ( str.empty() )
        return;

    if ( str[0] == '+' ) {
        int id = 0;
        SIZE_TYPE pos = 1;
        SIZE_TYPE nextPos = str.find('+', pos);
        while ( nextPos != NPOS ) {
            _TRACE( " next id: \"" << str.substr(pos, nextPos - pos) << "\"" );
            AddID(id += StringToInt(str.substr(pos, nextPos - pos)));
            pos = nextPos + 1;
            nextPos = str.find('+', pos);
        }
        _TRACE( " next id: \"" << str.substr(pos) << "\"" );
        AddID(id += StringToInt(str.substr(pos)));
    }
    else {
        SIZE_TYPE pos = 0;
        SIZE_TYPE nextPos = str.find(',', pos);
        while ( nextPos != NPOS ) {
            AddID(StringToInt(str.substr(pos, nextPos - pos)));
            pos = nextPos + 1;
            nextPos = str.find(',', pos);
        }
        AddID(StringToInt(str.substr(pos)));
    }
}

string CIDs::Encode(void) const
{
    string out;
    int idPrev = 0;
    for ( const_iterator i = begin(); i != end(); ++i ) {
        int id = i->first;
        out += IntToString(id - idPrev, true);
        idPrev = id;
    }
    return out;
}

string CHTMLHelper::HTMLEncode(const string& input)
{
    string output;
    string::size_type last = 0;

    // find first symbol to encode
    string::size_type ptr = input.find_first_of("\"&<>", last);
    while ( ptr != string::npos ) {
        // copy plain part of input
        if ( ptr != last )
            output.append(input, last, ptr - last);

        // append encoded symbol
        switch ( input[ptr] ) {
        case '"':
            output.append("&quot;");
            break;
        case '&':
            output.append("&amp;");
            break;
        case '<':
            output.append("&lt;");
            break;
        case '>':
            output.append("&gt;");
            break;
        }

        // skip it
        last = ptr + 1;

        // find next symbol to encode
        ptr = input.find_first_of("\"&<>", last);
    }

    // append last part of input
    if ( last != input.size() )
        output.append(input, last, input.size() - last);

    return output;
}

END_NCBI_SCOPE
