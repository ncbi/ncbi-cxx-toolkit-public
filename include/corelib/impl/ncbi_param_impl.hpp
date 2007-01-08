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

    const char*           section;
    const char*           name;
    const char*           env_var_name;
    const TValue          default_value;
    const TNcbiParamFlags flags;
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

    const char*           section;
    const char*           name;
    const char*           env_var_name;
    const TValue          default_value;
    const TNcbiParamFlags flags;

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
void g_ParamTlsValueCleanup(TValue* value, void*)
{
    delete value;
}


// Generic CParamParser

template<class TDescription>
inline
typename CParamParser<TDescription>::TValueType
CParamParser<TDescription>::StringToValue(const string& str,
                                          const TParamDesc&)
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
                                                 const TParamDesc&)
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
    try {
        return NStr::StringToBool(str);
    }
    catch ( ... ) {
        return NStr::StringToInt(str) != 0;
    }
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
CParam<TDescription>::CParam(EParamCacheFlag cache_flag)
    : m_ValueSet(false)
{
    if (cache_flag == eParamCache_Defer) {
        return;
    }
    if ( cache_flag == eParamCache_Force  ||
        CNcbiApplication::Instance() ) {
        Get();
    }
}


template<class TDescription>
inline
CParam<TDescription>::CParam(const string& section, const string& name)
{
    if ( !sx_IsSetFlag(eParam_NoLoad) ) {
        string str = g_GetConfigString
            (section, name,
            TParamParser::ValueToString(GetThreadDefault()));
        m_Value = TParamParser::StringToValue(str);
        m_ValueSet = true;
    }
}


template<class TDescription>
typename CParam<TDescription>::TValueType&
CParam<TDescription>::sx_GetDefault(bool force_reset)
{
    static TValueType s_Default =
        TDescription::sm_ParamDescription.default_value;

    if ( force_reset ) {
        s_Default = TDescription::sm_ParamDescription.default_value;
        sx_GetState() = eState_NotSet;
    }

    if ( sx_GetState() < eState_Config  &&  !sx_IsSetFlag(eParam_NoLoad) ) {
        string config_value =
            g_GetConfigString(TDescription::sm_ParamDescription.section,
                                TDescription::sm_ParamDescription.name,
                                TDescription::sm_ParamDescription.env_var_name,
                                "");
        if ( !config_value.empty() ) {
            s_Default =
                TParamParser::StringToValue(config_value,
                TDescription::sm_ParamDescription);
        }
        CNcbiApplication* app = CNcbiApplication::Instance();
        sx_GetState() = app  &&  app->HasLoadedConfig()
            ? eState_Config : eState_EnvVar;
    }

    return s_Default;
}


template<class TDescription>
CRef<typename CParam<TDescription>::TTls>&
CParam<TDescription>::sx_GetTls(void)
{
    static CRef<TTls> s_ValueTls;
    return s_ValueTls;
}


template<class TDescription>
typename CParam<TDescription>::EParamState&
CParam<TDescription>::sx_GetState(void)
{
    static EParamState s_State = eState_NotSet;
    return s_State;
}


template<class TDescription>
inline
bool CParam<TDescription>::sx_IsSetFlag(ENcbiParamFlags flag)
{
    return (TDescription::sm_ParamDescription.flags & flag) != 0;
}


template<class TDescription>
inline
typename CParam<TDescription>::EParamState
CParam<TDescription>::GetState(void)
{
    return sx_GetState();
}


template<class TDescription>
inline
typename CParam<TDescription>::TValueType
CParam<TDescription>::GetDefault(void)
{
    CMutexGuard guard(s_GetLock());
    return sx_GetDefault();
}


template<class TDescription>
inline
void CParam<TDescription>::SetDefault(const TValueType& val)
{
    CMutexGuard guard(s_GetLock());
    sx_GetDefault() = val;
}


template<class TDescription>
inline
void CParam<TDescription>::ResetDefault(void)
{
    CMutexGuard guard(s_GetLock());
    sx_GetDefault(true);
}


template<class TDescription>
inline
typename CParam<TDescription>::TValueType
CParam<TDescription>::GetThreadDefault(void)
{
    CMutexGuard guard(s_GetLock());
    if ( !sx_IsSetFlag(eParam_NoThread) ) {
        CRef<TTls>& tls = sx_GetTls();
        if ( tls.NotEmpty() ) {
            TValueType* v = tls->GetValue();
            if ( v ) {
                return *v;
            }
        }
    }
    return sx_GetDefault();
}


template<class TDescription>
inline
void CParam<TDescription>::SetThreadDefault(const TValueType& val)
{
    if ( sx_IsSetFlag(eParam_NoThread) ) {
        NCBI_THROW(CParamException, eNoThreadValue,
            "The parameter does not allow thread-local values");
    }
    CMutexGuard guard(s_GetLock());
    CRef<TTls>& tls = sx_GetTls();
    if ( !tls ) {
        tls.Reset(new TTls);
    }
    tls->SetValue(new TValueType(val), g_ParamTlsValueCleanup<TValueType>);
}


template<class TDescription>
inline
void CParam<TDescription>::ResetThreadDefault(void)
{
    if ( sx_IsSetFlag(eParam_NoThread) ) {
        return; // already using global default value
    }
    CMutexGuard guard(s_GetLock());
    CRef<TTls>& tls = sx_GetTls();
    if ( tls ) {
        tls->Reset();
    }
}


template<class TDescription>
inline
bool CParam<TDescription>::sx_CanGetDefault(void)
{
    return CNcbiApplication::Instance();
}


template<class TDescription>
inline
typename CParam<TDescription>::TValueType
CParam<TDescription>::Get(void) const
{
    if ( !m_ValueSet ) {
        m_Value = GetThreadDefault();
        m_ValueSet = true;
    }
    return m_Value;
}


template<class TDescription>
inline
void CParam<TDescription>::Set(const TValueType& val)
{
    m_Value = val;
    m_ValueSet = true;
}


template<class TDescription>
inline
void CParam<TDescription>::Reset(void)
{
    m_ValueSet = false;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.13  2007/01/08 16:49:20  grichenk
 * Use normal (non-fast) mutex in CParam.
 *
 * Revision 1.12  2006/11/16 20:12:10  grichenk
 * Added CParam state (stage of initialization).
 *
 * Revision 1.11  2006/11/13 16:57:27  grichenk
 * Added methods to reset CParam.
 *
 * Revision 1.10  2006/11/06 17:25:23  grichenk
 * Fixed SParamEnumDescription - added env_var_name.
 *
 * Revision 1.9  2006/01/05 20:40:17  grichenk
 * Added explicit environment variable name for params.
 * Added default value caching flag to CParam constructor.
 *
 * Revision 1.8  2005/12/27 16:02:30  grichenk
 * Fixed warnings
 *
 * Revision 1.7  2005/12/22 16:56:23  grichenk
 * Added NoThread and NoLoad flags.
 *
 * Revision 1.6  2005/12/08 16:40:34  ucko
 * Fix compilation under GCC 2.95, which gets confused when trying to
 * use a CRef<>& type as a bool.
 *
 * Revision 1.5  2005/12/06 18:56:17  grichenk
 * Fixed parsing of numeric values in StringToValue() for bools.
 *
 * Revision 1.4  2005/12/02 21:32:52  vasilche
 * Methods with static singleton variables should not be inlined.
 *
 * Revision 1.3  2005/11/17 21:49:13  ucko
 * Use standard syntax when defining CParam<>'s constructor.
 *
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
