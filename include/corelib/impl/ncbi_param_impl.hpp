#ifndef CORELIB___NCBI_PARAM_IMPL__HPP
#define CORELIB___NCBI_PARAM_IMPL__HPP

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
 * Author:  Aleksey Grichenko
 *
 * File Description:
 *   Parameters storage implementation
 *
 */


#include <corelib/ncbistr.hpp>
#include <corelib/ncbistre.hpp>


BEGIN_NCBI_SCOPE


// Internal structure describing parameter properties
template<class TValue>
struct SParamDescription
{
    typedef TValue TValueType;

    const char*  section;
    const char*  name;
    const TValue default_value;
};


// Internal enum value description
template<class TValue>
struct SEnumDescription
{
    const char*  alias; // string representation of enum value
    const TValue value; // int representation of enum value
};


// Internal structure describing enum parameter properties
template<class TValue>
struct SParamEnumDescription
{
    typedef TValue TValueType;

    const char*  section;
    const char*  name;
    const TValue default_value;

    // List of enum values if any
    const SEnumDescription<TValue>* enums;
    const size_t                    enums_size;
};


/////////////////////////////////////////////////////////////////////////////
//
// CEnumParser
//
// Enum parameter parser template.
//
// Converts between string and enum. Is used by NCBI_PARAM_ENUM_DECL.
//


template<class TEnum>
class CEnumParser
{
public:
    typedef TEnum                        TEnumType;
    typedef SParamEnumDescription<TEnum> TParamDesc;
    typedef SEnumDescription<TEnum>      TEnumDesc;

    static TEnumType  StringToEnum(const string&     str,
                                   const TParamDesc& descr);
    static string     EnumToString(const TEnumType&  val,
                                   const TParamDesc& descr);
};


// TLS cleanup function template

template<class TValue>
inline
void g_ParamTlsValueCleanup(TValue* value, void*)
{
    delete value;
}


// Generic CParamParser

template<class TDescription>
inline
typename CParamParser<TDescription>::TValueType
CParamParser<TDescription>::StringToValue(const string& str,
                                          const TParamDesc& descr)
{
    CNcbiIstrstream in(str.c_str());
    TValueType val;
    in >> val;

    if ( in.fail() ) {
        in.clear();
        NCBI_THROW(CParamException, eParserError,
            "Can not initialize parameter from string: " + str);
    }

    return val;
}


template<class TDescription>
inline
string CParamParser<TDescription>::ValueToString(const TValueType& val,
                                                 const TParamDesc& descr)
{
    CNcbiOstrstream buffer;
    buffer << val;
    return CNcbiOstrstreamToString(buffer);
}


// CParamParser for string

EMPTY_TEMPLATE
inline
CParamParser< SParamDescription<string> >::TValueType
CParamParser< SParamDescription<string> >::StringToValue(const string& str,
                                                         const TParamDesc&)
{
    return str;
}


EMPTY_TEMPLATE
inline
string
CParamParser< SParamDescription<string> >::ValueToString(const string& val,
                                                         const TParamDesc&)
{
    return val;
}


// CParamParser for bool

EMPTY_TEMPLATE
inline
CParamParser< SParamDescription<bool> >::TValueType
CParamParser< SParamDescription<bool> >::StringToValue(const string& str,
                                                       const TParamDesc&)
{
    return NStr::StringToBool(str);
}


EMPTY_TEMPLATE
inline
string
CParamParser< SParamDescription<bool> >::ValueToString(const bool& val,
                                                       const TParamDesc&)
{
    return NStr::BoolToString(val);
}


// CParamParser for enums

template<class TEnum>
inline
typename CEnumParser<TEnum>::TEnumType
CEnumParser<TEnum>::StringToEnum(const string& str,
                                 const TParamDesc& descr)
{
    for (size_t i = 0;  i < descr.enums_size;  ++i) {
        if ( NStr::EqualNocase(str, descr.enums[i].alias) ) {
            return descr.enums[i].value;
        }
    }

    NCBI_THROW(CParamException, eParserError,
        "Can not initialize enum from string: " + str);

    // Enum name not found
    // return descr.default_value;
}


template<class TEnum>
inline string
CEnumParser<TEnum>::EnumToString(const TEnumType& val,
                                 const TParamDesc& descr)
{
    for (size_t i = 0;  i < descr.enums_size;  ++i) {
        if (descr.enums[i].value == val) {
            return string(descr.enums[i].alias);
        }
    }

    NCBI_THROW(CParamException, eBadValue,
        "Unexpected enum value: " + NStr::IntToString(int(val)));

    // Enum name not found
    // return kEmptyStr;
}


// CParam implementation

template<class TDescription>
inline
CParam<TDescription>::CParam<TDescription>(const string& section,
                                           const string& name)
{
    string str = g_GetConfigString
        (section, name,
         TParamParser::ValueToString(GetThreadDefault()));
    m_Value = TParamParser::StringToValue(str);
}


template<class TDescription>
inline
typename CParam<TDescription>::TValueType&
CParam<TDescription>::sx_GetDefault(void)
{
    static TValueType s_Default =
        TDescription::sm_ParamDescription.default_value;

    static bool initialized = false;
    if ( !initialized ) {
        string config_value = g_GetConfigString
            (TDescription::sm_ParamDescription.section,
             TDescription::sm_ParamDescription.name,
             "");
        if ( !config_value.empty() ) {
            s_Default =
                TParamParser::StringToValue(config_value,
                                            TDescription::sm_ParamDescription);
        }
        initialized = true;
    }

    return s_Default;
}


template<class TDescription>
inline
CRef<typename CParam<TDescription>::TTls>&
CParam<TDescription>::sx_GetTls(void)
{
    static CRef<TTls> s_ValueTls;
    return s_ValueTls;
}


template<class TDescription>
inline
typename CParam<TDescription>::TValueType
CParam<TDescription>::GetDefault(void)
{
    CFastMutexGuard guard(s_GetLock());
    return sx_GetDefault();
}


template<class TDescription>
inline
void CParam<TDescription>::SetDefault(const TValueType& val)
{
    CFastMutexGuard guard(s_GetLock());
    sx_GetDefault() = val;
}


template<class TDescription>
inline
typename CParam<TDescription>::TValueType
CParam<TDescription>::GetThreadDefault(void)
{
    CFastMutexGuard guard(s_GetLock());
    CRef<TTls>& tls = sx_GetTls();
    if ( tls ) {
        TValueType* v = tls->GetValue();
        if ( v ) {
            return *v;
        }
    }
    return sx_GetDefault();
}


template<class TDescription>
inline
void CParam<TDescription>::SetThreadDefault(const TValueType& val)
{
    CFastMutexGuard guard(s_GetLock());
    CRef<TTls>& tls = sx_GetTls();
    if ( !tls ) {
        tls.Reset(new TTls);
    }
    tls->SetValue(new TValueType(val), g_ParamTlsValueCleanup<TValueType>);
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2005/11/17 18:45:53  grichenk
 * Added comments and examples.
 * Moved CParam<> static mutex to base class.
 * Case insensitive parameter names.
 * Moved GetConfigXXX to ncbi_param, renamed to g_GetConfig.
 *
 *
 * ===========================================================================
 */

#endif  /* CORELIB___NCBI_PARAM_IMPL__HPP */
