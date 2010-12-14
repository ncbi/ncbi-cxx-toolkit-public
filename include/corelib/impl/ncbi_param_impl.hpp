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
    // Initialization function. The string returned is converted to
    // the TValue type the same way as when loading from other sources.
    typedef string (*FInitFunc)(void);

    const char*           section;
    const char*           name;
    const char*           env_var_name;
    TValue                default_value;
    FInitFunc             init_func;
    TNcbiParamFlags       flags;
};


// Internal enum value description
template<class TValue>
struct SEnumDescription
{
    const char*  alias; // string representation of enum value
    TValue       value; // int representation of enum value
};


// Internal structure describing enum parameter properties
template<class TValue>
struct SParamEnumDescription
{
    typedef TValue TValueType;
    typedef string (*FInitFunc)(void);

    const char*           section;
    const char*           name;
    const char*           env_var_name;
    TValue                default_value;
    FInitFunc             init_func;
    TNcbiParamFlags       flags;

    // List of enum values if any
    const SEnumDescription<TValue>* enums;
    size_t                          enums_size;
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

// CParamParser for double

EMPTY_TEMPLATE
inline
CParamParser< SParamDescription<double> >::TValueType
CParamParser< SParamDescription<double> >::StringToValue(const string& str,
                                                         const TParamDesc&)
{
    return NStr::StringToDouble(str,
        NStr::fDecimalPosixOrLocal |
        NStr::fAllowLeadingSpaces | NStr::fAllowTrailingSpaces);
}


EMPTY_TEMPLATE
inline
string
CParamParser< SParamDescription<double> >::ValueToString(const double& val,
                                                         const TParamDesc&)
{
    return NStr::DoubleToString(val, DBL_DIG,
        NStr::fDoubleGeneral | NStr::fDoublePosix);
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
typename CParam<TDescription>::TValueType&
CParam<TDescription>::sx_GetDefault(bool force_reset)
{
    TValueType& def = TDescription::sm_Default;
    bool& def_init = TDescription::sm_DefaultInitialized;
    if ( !TDescription::sm_ParamDescription.section ) {
        // Static data not initialized yet, nothing to do.
        return def;
    }
    if ( !def_init ) {
        def = TDescription::sm_ParamDescription.default_value;
        def_init = true;
    }

    if ( force_reset ) {
        def = TDescription::sm_ParamDescription.default_value;
        sx_GetState() = eState_NotSet;
    }

    if (sx_GetState() < eState_Func) {
        _ASSERT(sx_GetState() != eState_InFunc);
        if (sx_GetState() == eState_InFunc) {
            // Recursive initialization detected (in release only)
            NCBI_THROW(CParamException, eRecursion,
                "Recursion detected during CParam initialization.");
        }
        if ( TDescription::sm_ParamDescription.init_func ) {
            // Run the initialization function
            sx_GetState() = eState_InFunc;
            def = TParamParser::StringToValue(
                TDescription::sm_ParamDescription.init_func(),
                TDescription::sm_ParamDescription);
        }
        sx_GetState() = eState_Func;
    }

    if ( sx_GetState() < eState_Config  &&  !sx_IsSetFlag(eParam_NoLoad) ) {
        string config_value =
            g_GetConfigString(TDescription::sm_ParamDescription.section,
                                TDescription::sm_ParamDescription.name,
                                TDescription::sm_ParamDescription.env_var_name,
                                "");
        if ( !config_value.empty() ) {
            def = TParamParser::StringToValue(config_value,
                TDescription::sm_ParamDescription);
        }
        CNcbiApplication* app = CNcbiApplication::Instance();
        sx_GetState() = app  &&  app->HasLoadedConfig()
            ? eState_Config : eState_EnvVar;
    }

    return def;
}


template<class TDescription>
typename CParam<TDescription>::TTls&
CParam<TDescription>::sx_GetTls(void)
{
    return TDescription::sm_ValueTls;
}


template<class TDescription>
typename CParam<TDescription>::EParamState&
CParam<TDescription>::sx_GetState(void)
{
    return TDescription::sm_State;
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
    sx_GetState() = eState_User;
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
    if ( !sx_IsSetFlag(eParam_NoThread) ) {
        TValueType* v = sx_GetTls().GetValue();
        if ( v ) {
            return *v;
        }
    }
    return GetDefault();
}


template<class TDescription>
inline
void CParam<TDescription>::SetThreadDefault(const TValueType& val)
{
    if ( sx_IsSetFlag(eParam_NoThread) ) {
        NCBI_THROW(CParamException, eNoThreadValue,
            "The parameter does not allow thread-local values");
    }
    TTls& tls = sx_GetTls();
    tls.SetValue(new TValueType(val), g_ParamTlsValueCleanup<TValueType>);
}


template<class TDescription>
inline
void CParam<TDescription>::ResetThreadDefault(void)
{
    if ( sx_IsSetFlag(eParam_NoThread) ) {
        return; // already using global default value
    }
    sx_GetTls().SetValue(NULL);
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

#endif  /* CORELIB___NCBI_PARAM_IMPL__HPP */
