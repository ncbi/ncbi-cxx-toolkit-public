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

#include <corelib/ncbistd.hpp>
#include <corelib/ncbicgi.hpp>
#include <html/html.hpp>
#include <html/htmlhelper.hpp>

BEGIN_NCBI_SCOPE

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

void CHTMLHelper::LoadIDList(TIDList& ids,
                const TCgiEntries& values,
                const string& hiddenPrefix,
                const string& checkboxName)
{
    ids.clear();
    
    string value = sx_ExtractHidden(values, hiddenPrefix);
    if ( !value.empty() ) {
        sx_DecodeIDList(ids, value);
    }
    sx_GetCheckboxes(ids, values, checkboxName);
}

void CHTMLHelper::StoreIDList(CHTML_form* form,
                const TIDList& ids,
                const string& hiddenPrefix,
                const string& checkboxName)
{
    //    sx_SetCheckboxes(form, ids, checkboxName);
    string value = sx_EncodeIDList(ids);
    if ( !value.empty() )
        sx_InsertHidden(form, value, hiddenPrefix);
}

string CHTMLHelper::sx_EncodeIDList(const TIDList& ids_)
{
    string value;
    TIDList& ids = const_cast<TIDList&>(ids_); // SW_01
    for ( TIDList::const_iterator i = ids.begin(); i != ids.end(); ++i ) {
        if ( i->second )
            value += ',' + IntToString(i->first);
    }
    value.erase(0, 1); // remove first comma
    return value;
}

void CHTMLHelper::sx_AddID(TIDList& ids, const string& value)
{
    ids.insert(TIDList::value_type(StringToInt(value), true));
}

void CHTMLHelper::sx_DecodeIDList(TIDList& ids, const string& value)
{
    SIZE_TYPE pos = 0;
    SIZE_TYPE commaPos = value.find(',', pos);
    while ( commaPos != NPOS ) {
        sx_AddID(ids, value.substr(pos, commaPos));
        pos = commaPos + 1;
        commaPos = value.find(',', pos);
    }
    sx_AddID(ids, value.substr(pos));
}

static const int kMaxHiddenCount = 10;
static const SIZE_TYPE kMaxHiddenSize = 512;

string CHTMLHelper::sx_ExtractHidden(const TCgiEntries& values_,
                                     const string& hiddenPrefix)
{
    string value;
    TCgiEntries& values = const_cast<TCgiEntries&>(values_); // SW_01
    for ( int i = 0; i <= kMaxHiddenCount; ++i ) {
        TCgiEntries::const_iterator pointer = values.find(hiddenPrefix + IntToString(i));
        if ( pointer == values.end() ) // no more hidden values
            return value;
        value += pointer->second;
    }
    // what to do in this case?
    throw runtime_error("Too many hidden values beginning with "+hiddenPrefix);
}

void CHTMLHelper::sx_InsertHidden(CHTML_form* form, const string& value,
                                  const string& hiddenPrefix)
{
    SIZE_TYPE pos = 0;
    for ( int i = 0; pos < value.size() && i < kMaxHiddenCount; ++i ) {
        int end = min(value.size(), pos + kMaxHiddenSize);
        form->AddHidden(hiddenPrefix + IntToString(i), value.substr(pos, end));
        pos = end;
    }
    if ( pos != value.size() )
        throw runtime_error("ID list too long to store in hidden values");
}

void CHTMLHelper::sx_SetCheckboxes(CNCBINode* root, TIDList& ids,
                                   const string& checkboxName)
{
    if ( root->GetName() == CHTML_input::s_GetTagName() &&
         root->GetAttribute(KHTMLAttributeName_type) == CHTML_checkbox::s_GetInputType() &&
         root->GetAttribute(KHTMLAttributeName_name) == checkboxName) {
        // found it
        int id = StringToInt(root->GetAttribute(KHTMLAttributeName_value));
        TIDList::iterator pointer = ids.find(id);
        if ( pointer != ids.end() ) {
            root->SetOptionalAttribute(KHTMLAttributeName_checked, pointer->second);
            ids.erase(pointer);  // remove id from the list - already set
        }
    }
    for ( CNCBINode::TChildList::iterator i = root->ChildBegin();
          i != root->ChildEnd(); ++i ) {
        sx_SetCheckboxes(*i, ids, checkboxName);
    }
}

void CHTMLHelper::sx_GetCheckboxes(TIDList& ids, const TCgiEntries& values_,
                                   const string& checkboxName)
{
    TCgiEntries& values = const_cast<TCgiEntries&>(values_); // SW_01
    for ( TCgiEntries::const_iterator i = values.lower_bound(checkboxName),
              end = values.upper_bound(checkboxName); i != end; ++i ) {
        sx_AddID(ids, i->second);
    }
}

END_NCBI_SCOPE
