/* $Id$
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
 * Author:  .......
 *
 * File Description:
 *   .......
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using specifications from the data definition file
 *   'general.asn'.
 */

// standard includes

// generated includes
#include <ncbi_pch.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/User_object.hpp>
#include <objects/misc/sequence_util_macros.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CUser_field::~CUser_field(void)
{
}


/// add fields to the current user field
CUser_field& CUser_field::AddField(const string& label,
                                   const string& value,
                                   EParseField parse)
{
    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr(label);
    field->SetValue(value, parse);
    SetData().SetFields().push_back(field);
    return *this;
}


CUser_field& CUser_field::AddField(const string& label,
                                   const string& value)
{
    return AddField(label, value, eParse_String);
}


CUser_field& CUser_field::AddField(const string& label,
                                   const char* value)
{
    return AddField(label, string(value), eParse_String);
}


CUser_field& CUser_field::AddField(const string& label,
                                   const char* value,
                                   EParseField parse)
{
    return AddField(label, string(value), parse);
}


CUser_field& CUser_field::AddField(const string& label, int value)
{
    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr(label);
    field->SetValue(value);
    SetData().SetFields().push_back(field);
    return *this;
}


CUser_field& CUser_field::AddField(const string& label, Int8 value)
{
    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr(label);
    field->SetValue(value);
    SetData().SetFields().push_back(field);
    return *this;
}


#ifdef NCBI_STRICT_GI
CUser_field& CUser_field::AddField(const string& label, TGi value)
{
    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr(label);
    field->SetValue(value);
    SetData().SetFields().push_back(field);
    return *this;
}
#endif


CUser_field& CUser_field::AddField(const string& label, double value)
{
    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr(label);
    field->SetValue(value);
    SetData().SetFields().push_back(field);
    return *this;
}


CUser_field& CUser_field::AddField(const string& label, bool value)
{
    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr(label);
    field->SetValue(value);
    SetData().SetFields().push_back(field);
    return *this;
}

CUser_field& CUser_field::AddField(const string& label,
                                   const CUser_field_Base::TData::TStrs& value)
{
    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr(label);
    field->SetValue(value);
    SetData().SetFields().push_back(field);
    return *this;
}

CUser_field& CUser_field::AddField(const string& label,
                                   const vector<int>& value)
{
    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr(label);
    field->SetValue(value);
    SetData().SetFields().push_back(field);
    return *this;
}


CUser_field& CUser_field::AddField(const string& label,
                                   const vector<double>& value)
{
    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr(label);
    field->SetValue(value);
    SetData().SetFields().push_back(field);
    return *this;
}


CUser_field& CUser_field::AddField(const string& label,
                                   CUser_object& value)
{
    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr(label);
    field->SetValue(value);
    SetData().SetFields().push_back(field);
    return *this;
}


CUser_field& CUser_field::AddField(const string& label,
                                   const vector< CRef<CUser_object> >& value)
{
    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr(label);
    field->SetValue(value);
    SetData().SetFields().push_back(field);
    return *this;
}


CUser_field& CUser_field::AddField(const string& label,
                                   const vector< CRef<CUser_field> >& value)
{
    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr(label);
    field->SetValue(value);
    SetData().SetFields().push_back(field);
    return *this;
}


/// Access a named field in this user field.  This will tokenize the
/// string 'str' on the delimiters; if the field doesn't exist, an
/// exception will be thrown.
const CUser_field& CUser_field::GetField(const string& str,
                                         const string& delim,
                                         NStr::ECase use_case) const
{
    CConstRef<CUser_field> f = GetFieldRef(str, delim, use_case);
    if ( !f ) {
        NCBI_THROW(CException, eUnknown,
                   "failed to find field named " + str);
    }
    return *f;
}


/// Return a field reference representing the tokenized key, or a
/// NULL reference if the key doesn't exist.
CConstRef<CUser_field> CUser_field::GetFieldRef(const string& str,
                                                const string& delim,
                                                NStr::ECase use_case) const
{
    list<string> toks;
    NStr::Split(str, delim, toks, NStr::fSplit_Tokenize);

    CConstRef<CUser_field> f(this);
    if ( !f->GetData().IsFields() ) {
        if (toks.size() == 1  &&
            f->GetLabel().IsStr()  &&
            NStr::Equal(f->GetLabel().GetStr(), toks.front(), use_case))
        {
            return f;
        } else {
            return CConstRef<CUser_field>();
        }
    }

    if (toks.size()) {
        list<string>::const_iterator last = toks.end();
        --last;

        ITERATE (list<string>, iter, toks) {
            CConstRef<CUser_field> new_f;

            ITERATE (TData::TFields, field_iter, f->GetData().GetFields()) {
                const CUser_field& field = **field_iter;
                if (field.GetLabel().IsStr()) {
                    if (NStr::Equal(field.GetLabel().GetStr(), *iter, use_case) ) {
                        if (iter == last  || field.GetData().IsFields()) {
                            new_f = *field_iter;
                            break;
                        }
                    }
                } else if (field.GetLabel().IsId()) {
                    if (field.GetLabel().GetId() == NStr::StringToInt(*iter)) {
                        if (iter == last  || field.GetData().IsFields()) {
                            new_f = *field_iter;
                            break;
                        }
                    }
                }
            }

            f = new_f;
            if ( !f ) {
                return f;
            }
        }
    }

    return f;
}

void CUser_field::SFieldNameChain::Join(
    ostream & out_name_strm, const string & delim) const
{
    bool bFirst = true;
    ITERATE(TFieldNameChainUnderlying, chain_iter, m_FieldNameChain) {
        if( bFirst ) {
            bFirst = false;
        } else {
            out_name_strm << delim;
        }
        out_name_strm << *chain_iter;
    }
}

void CUser_field::GetFieldsMap(
        CUser_field::TMapFieldNameToRef & out_mapFieldNameToRef,
        TFieldMapFlags fFieldMapFlags,
        const SFieldNameChain & parent_name) const
{
    // get the label
    if( ! FIELD_IS_SET_AND_IS(*this, Label, Str) ) {
        // we might eventually support numeric labels
        return;
    }

    // copying a vector of CTempStrings is much more efficient
    // than copying strings, so this should be okay.
    SFieldNameChain field_name_chain = parent_name;

    if( ! (fFieldMapFlags & fFieldMapFlags_ExcludeThis) ) {
        field_name_chain += GetLabel().GetStr();
        out_mapFieldNameToRef.insert( 
            TMapFieldNameToRef::value_type(field_name_chain, ConstRef(this) ) );
    }

    // recurse, if applicable
    if( FIELD_IS_SET_AND_IS(*this, Data, Fields) ) {
        // some flags do not get passed down recursively
        TFieldMapFlags fChildFieldMapFlags = 
            ( fFieldMapFlags & ~fFieldMapFlags_ExcludeThis );

        ITERATE( CUser_field::C_Data::TFields, field_iter, 
            GetData().GetFields() ) 
        {
            (*field_iter)->GetFieldsMap(out_mapFieldNameToRef, 
                fChildFieldMapFlags,
                field_name_chain);
        }
    }
}

/// Access a named field in this user field.  This will tokenize the
/// string 'str' on the delimiters and recursively add fields where needed
CUser_field& CUser_field::SetField(const string& str,
                                   const string& delim,
                                   NStr::ECase use_case)
{
    CRef<CUser_field> f = SetFieldRef(str, delim, use_case);
    return *f;
}


/// Return a field reference representing the tokenized key, or a
/// NULL reference if the key cannot be created for some reason.
CRef<CUser_field> CUser_field::SetFieldRef(const string& str,
                                           const string& delim,
                                           NStr::ECase use_case)
{
    list<string> toks;
    NStr::Split(str, delim, toks, NStr::fSplit_Tokenize);

    CRef<CUser_field> f(this);
    if ( ! f->GetData().IsFields()  &&  f->GetData().Which() != CUser_field::TData::e_not_set ) {
        // There is a value here, not a list of User_fields, no place to recurse downward. 
        NCBI_THROW(CException, eUnknown, "Too many parts in key: \"" + str + "\"");
    }
    list<string>::const_iterator last = toks.end();
    --last;
    ITERATE (list<string>, iter, toks) {
        CRef<CUser_field> new_f;
        NON_CONST_ITERATE (TData::TFields, field_iter, f->SetData().SetFields()) {
            const CUser_field& field = **field_iter;
            if (NStr::Equal(field.GetLabel().GetStr(), *iter, use_case) )
            {
                if (iter == last) {
                    new_f = *field_iter;
                    break;
                } else if (field.GetData().IsFields()  ||
                           field.GetData().Which() == CUser_field::TData::e_not_set) {
                    new_f = *field_iter;
                    break;
                } else {
                    // There is a value here, not a list of User_fields, no place to recurse downward. 
                    NCBI_THROW(CException, eUnknown, "Too many parts in key: \"" + str + "\"");
                }
            }
        }

        if ( !new_f ) {
            new_f.Reset(new CUser_field());
            new_f->SetLabel().SetStr(*iter);
            f->SetData().SetFields().push_back(new_f);
        }

        f = new_f;
    }

    return f;
}


/// Verify that a named field exists
bool CUser_field::HasField(const string& str,
                           const string& delim,
                           NStr::ECase use_case) const
{
    CConstRef<CUser_field> f = GetFieldRef(str, delim, use_case);
    return f.GetPointer() != NULL;
}



/// delete a named field.
bool CUser_field::DeleteField(const string& str,
                              const string& delim,
                              NStr::ECase use_case)
{
    list<string> toks;
    NStr::Split(str, delim, toks, NStr::fSplit_Tokenize);

    CRef<CUser_field> f(this);
    list<string>::const_iterator last = toks.end();
    --last;

    ITERATE (list<string>, iter, toks) {
        CRef<CUser_field> new_f;
        if ( !f->GetData().IsFields() ) {
            return false;
        }
        NON_CONST_ITERATE (TData::TFields, field_iter, f->SetData().SetFields()) {
            const CUser_field& field = **field_iter;
            if (field.GetLabel().IsStr()
                &&  NStr::Equal(field.GetLabel().GetStr(), *iter, use_case) )
            {
                if (iter != last  &&  field.GetData().IsFields()) {
                    new_f = *field_iter;
                    break;
                } else if (iter == last) {
                    // delete this one from f, its parent.
                    f->SetData().SetFields().erase(field_iter);
                    return true;
                }
            }
        }
        if ( !new_f ) {
            return false;
        }
        f = new_f;
    }
    // Never reached.
    return false;
}


CUser_field& CUser_field::SetString(const char* value)
{
    return SetValue(string(value));
}


CUser_field& CUser_field::SetValue(const string& value, EParseField parse)
{
    if ( parse == eParse_Number ) {
        try {
            return SetValue(NStr::StringToNumeric<Int8>(value));
        }
        catch (...) {
        }

        try {
            return SetValue(NStr::StringToDouble(value));
        }
        catch (...) {
        }
    }
    return SetValue(value);
}


CUser_field& CUser_field::SetValue(const char* value, EParseField parse)
{
    return SetValue(string(value), parse);
}


// 15 digits of REAL numbers are preserved in serialization roundtrip
// so if the value has 15 digits or less we'll store it in 'real' field
static const Int8 kMaxAsReal = NCBI_CONST_INT8(999999999999999);


CUser_field& CUser_field::SetInt8(Int8 value)
{
    if ( value == int(value) ) {
        // value fits in 'int' field
        return SetInt(int(value));
    }
    // otherwise the value is stored into 'str' field
    return SetString(NStr::NumericToString(value));
}


void CUser_field::SetNumFromSize(size_t value)
{
    if (value > numeric_limits<TNum>::max()) {
        NCBI_THROW_FMT(CSerialException, eOverflow,
            "array size is too big to fit into User-field.num: " << value);
    }
    SetNum(TNum(value));
}


CUser_field& CUser_field::SetValue(const vector<int>&    value)
{
    SetNumFromSize(value.size());
    SetData().SetInts() = value;
    return *this;
}


CUser_field& CUser_field::SetValue(const vector<double>& value)
{
    SetNumFromSize(value.size());
    SetData().SetReals() = value;
    return *this;
}


CUser_field& CUser_field::SetValue(const vector<string>& value)
{
    SetNumFromSize(value.size());
    SetData().SetStrs() = value;
    return *this;
}


CUser_field& CUser_field::SetValue(CUser_object& value)
{
    SetData().SetObject(value);
    return *this;
}


CUser_field& CUser_field::SetValue(const vector< CRef<CUser_object> >& value)
{
    SetNumFromSize(value.size());
    SetData().SetObjects() = value;
    return *this;
}


CUser_field& CUser_field::SetValue(const vector< CRef<CUser_field> >& value)
{
    SetNumFromSize(value.size());
    SetData().SetFields() = value;
    return *this;
}


Int8 CUser_field::GetInt8(void) const
{
    const C_Data& data = GetData();
    if ( data.IsInt() ) {
        return data.GetInt();
    }
    if ( data.IsReal() ) {
        double v = data.GetReal();
        if ( v >= -kMaxAsReal && v <= kMaxAsReal ) {
            return Int8(v);
        }
    }
    return NStr::StringToNumeric<Int8>(data.GetStr());
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE
