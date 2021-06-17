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

#include <corelib/ncbistre.hpp>
#include <corelib/ncbi_safe_static.hpp>


BEGIN_NCBI_SCOPE


// When creating a CParam<> for a non-POD type, the default param value must be
// stored in a POD static variable. The proxy allows to use TStaticInit (POD type
// used for initialization) different from the actual parameter type (TValue).
// The default implementation assumes TValue is already a POD type.
// In most cases NCBI_PARAM_STATIC_PROXY macro can be used to create CSafeStatic_Proxy
// specialization.
template<class TValue>
class CSafeStatic_Proxy
{
public:
    // Disable default CSafeStatic_Proxy implementation for non-POD types.
    // Users need to implement type-specific proxy with TStaticInit defined
    // as a POD type and constructors/assignments accepting POD values.
    typedef typename enable_if<std::is_scalar<TValue>::value, TValue>::type TStaticInit;

    CSafeStatic_Proxy(void)
    {}

    CSafeStatic_Proxy(TValue value)
        : m_Value(value)
    {
    }

    CSafeStatic_Proxy& operator=(TValue value)
    {
        m_Value = value;
        return *this;
    }

    operator TValue& (void)
    {
        return m_Value;
    }

private:
    mutable TValue m_Value;
};


// Special case of CSafeStatic_Proxy for std::string - extends life span of
// string parameters using CSafeStatic<string>.
template<>
class CSafeStatic_Proxy<string>
{
public:
    typedef const char* TStaticInit;

    CSafeStatic_Proxy(void)
        : m_Value(CSafeStaticLifeSpan(CSafeStaticLifeSpan::eLifeSpan_Longest, 1))
    {}

    CSafeStatic_Proxy(const string& value)
        : m_Value(CSafeStaticLifeSpan(CSafeStaticLifeSpan::eLifeSpan_Longest, 1))
    {
        m_Value.Get() = value;
    }

    CSafeStatic_Proxy(const char* value)
        : m_Value(CSafeStaticLifeSpan(CSafeStaticLifeSpan::eLifeSpan_Longest, 1))
    {
        m_Value.Get() = value;
    }

    CSafeStatic_Proxy(const CSafeStatic_Proxy& proxy)
        : m_Value(CSafeStaticLifeSpan(CSafeStaticLifeSpan::eLifeSpan_Longest, 1))
    {
        m_Value.Get() = proxy.m_Value.Get();
    }

    CSafeStatic_Proxy& operator=(const CSafeStatic_Proxy& proxy)
    {
        m_Value.Get() = proxy.m_Value.Get();
        return *this;
    }

    CSafeStatic_Proxy& operator=(const string& value)
    {
        m_Value.Get() = value;
        return *this;
    }

    CSafeStatic_Proxy& operator=(const char* value)
    {
        m_Value.Get() = value;
        return *this;
    }

    operator string&(void)
    {
        return m_Value.Get();
    }

private:
    mutable CSafeStatic<string> m_Value;
};


// Internal structure describing parameter properties
template<class TValue>
struct SParamDescription
{
    typedef TValue TValueType;
    typedef CSafeStatic_Proxy<TValue> TStaticValue;
    typedef typename CSafeStatic_Proxy<TValue>::TStaticInit TStaticInit;
    // Initialization function. The string returned is converted to
    // the TValue type the same way as when loading from other sources.
    typedef string (*FInitFunc)(void);

    const char*           section;
    const char*           name;
    const char*           env_var_name;
    TStaticInit           default_value;
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
    typedef CSafeStatic_Proxy<TValue> TStaticValue;
    typedef typename CSafeStatic_Proxy<TValue>::TStaticInit TStaticInit;
    typedef string (*FInitFunc)(void);

    const char*           section;
    const char*           name;
    const char*           env_var_name;
    TStaticInit           default_value;
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


template<class TEnum, class TParam>
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

template<class TDescription, class TParam>
inline
typename CParamParser<TDescription, TParam>::TValueType
CParamParser<TDescription, TParam>::StringToValue(const string& str,
                                                  const TParamDesc&)
{
    CNcbiIstrstream in(str);
    TValueType val;
    in >> val;

    if ( in.fail() ) {
        in.clear();
        NCBI_THROW(CParamException, eParserError,
            "Can not initialize parameter from string: " + str);
    }

    return val;
}


template<class TDescription, class TParam>
inline
string CParamParser<TDescription, TParam>::ValueToString(const TValueType& val,
                                                         const TParamDesc&)
{
    CNcbiOstrstream buffer;
    buffer << val;
    return CNcbiOstrstreamToString(buffer);
}


// CParamParser for string

EMPTY_TEMPLATE
inline
CParamParser< SParamDescription<string>, string>::TValueType
CParamParser< SParamDescription<string>, string>::StringToValue(const string& str,
                                                                const TParamDesc&)
{
    return str;
}


EMPTY_TEMPLATE
inline
string
CParamParser< SParamDescription<string>, string>::ValueToString(const string& val,
                                                                const TParamDesc&)
{
    return val;
}


// CParamParser for bool

EMPTY_TEMPLATE
inline
CParamParser< SParamDescription<bool>, bool>::TValueType
CParamParser< SParamDescription<bool>, bool>::StringToValue(const string& str,
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
CParamParser< SParamDescription<bool>, bool>::ValueToString(const bool& val,
                                                            const TParamDesc&)
{
    return NStr::BoolToString(val);
}

// CParamParser for double

EMPTY_TEMPLATE
inline
CParamParser< SParamDescription<double>, double>::TValueType
CParamParser< SParamDescription<double>, double>::StringToValue(const string& str,
                                                                const TParamDesc&)
{
    return NStr::StringToDouble(str,
        NStr::fDecimalPosixOrLocal |
        NStr::fAllowLeadingSpaces | NStr::fAllowTrailingSpaces);
}


EMPTY_TEMPLATE
inline
string
CParamParser< SParamDescription<double>, double>::ValueToString(const double& val,
                                                                const TParamDesc&)
{
    return NStr::DoubleToString(val, DBL_DIG,
        NStr::fDoubleGeneral | NStr::fDoublePosix);
}


// CParamParser for enums

template<class TEnum, class TParam>
inline
typename CEnumParser<TEnum, TParam>::TEnumType
CEnumParser<TEnum, TParam>::StringToEnum(const string& str,
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


template<class TEnum, class TParam>
inline string
CEnumParser<TEnum, TParam>::EnumToString(const TEnumType& val,
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
SSystemMutex& CParam<TDescription>::s_GetLock(void)
{
    DEFINE_STATIC_MUTEX(s_ParamValueLock);
    return s_ParamValueLock;
}


template<class TDescription>
typename CParam<TDescription>::TValueType&
CParam<TDescription>::sx_GetDefault(bool force_reset)
{
    bool& def_init = TDescription::sm_DefaultInitialized;
    _ASSERT(TDescription::sm_ParamDescription.section);
    if ( !def_init ) {
        TDescription::sm_Default = TDescription::sm_ParamDescription.default_value;
        def_init = true;
        sx_GetSource() = eSource_Default;
    }

    if ( force_reset ) {
        TDescription::sm_Default = TDescription::sm_ParamDescription.default_value;
        sx_GetState() = eState_NotSet;
        sx_GetSource() = eSource_Default;
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
            try {
                TDescription::sm_Default = TParamParser::StringToValue(
                    TDescription::sm_ParamDescription.init_func(),
                    TDescription::sm_ParamDescription);
                sx_GetSource() = eSource_Func;
            }
            catch (...) {
                sx_GetState() = eState_Error;
                throw;
            }
        }
        sx_GetState() = eState_Func;
    }

    if (sx_GetState() < eState_Config) {
        if ( sx_IsSetFlag(eParam_NoLoad) ) {
            EParamState& state = sx_GetState();
            state = eState_Config; // No need to check anything else.
        }
        else {
            EParamSource src = eSource_NotSet;
            string config_value =
                g_GetConfigString(TDescription::sm_ParamDescription.section,
                                    TDescription::sm_ParamDescription.name,
                                    TDescription::sm_ParamDescription.env_var_name,
                                    "",
                                    &src);
            if ( !config_value.empty() ) {
                try {
                    TDescription::sm_Default = TParamParser::StringToValue(config_value,
                        TDescription::sm_ParamDescription);
                    sx_GetSource() = (EParamSource)src;
                }
                catch (...) {
                    sx_GetState() = eState_Error;
                    ERR_POST("Error reading CParam value " <<
                        TDescription::sm_ParamDescription.section << "/" << TDescription::sm_ParamDescription.name);
                    throw;
                }
            }
            CNcbiApplicationGuard app = CNcbiApplication::InstanceGuard();
            sx_GetState() = app  &&  app->FinishedLoadingConfig()
                ? eState_Config : eState_EnvVar;
        }
    }

    return TDescription::sm_Default;
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
typename CParam<TDescription>::EParamSource&
CParam<TDescription>::sx_GetSource(void)
{
    return TDescription::sm_Source;
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
CParam<TDescription>::GetState(bool* sourcing_complete, EParamSource* param_source)
{
    EParamState state = sx_GetState();
    if (sourcing_complete) {
        *sourcing_complete = state == eState_Config;
    }
    if (param_source) {
        *param_source = sx_GetSource();
    }
    return state;
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
    EParamState& state = sx_GetState();
    if (state < eState_User) {
        state = eState_User;
    }
    sx_GetSource() = eSource_User;
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
        // The lock prevents multiple initializations with the default value
        // in Get(), but does not prevent Set() from modifying the value
        // while another thread is reading it.
        CMutexGuard guard(s_GetLock());
        if ( !m_ValueSet ) {
            m_Value = GetThreadDefault();
            if (GetState() >= eState_Config) {
                // All sources checked or the value is set by user.
                m_ValueSet = true;
            }
        }
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
