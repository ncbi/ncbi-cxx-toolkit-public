#ifndef NCBIPARAM__HPP
#define NCBIPARAM__HPP

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
 *
 */

/// @file ncbiparam.hpp
/// Classes for storing parameters.
///


#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbi_config_value.hpp>


/** @addtogroup Param
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
///
/// NCBI_PARAM_DECL
/// NCBI_PARAM_DEF
///
/// Parameter declaration and definition macros
///
/// Each parameter must be declared and defined using the macros
///

// Internal enum value description
template<class TValue>
struct SEnumDescription
{
    char*  alias; // string representation of enum value
    TValue value; // int representation of enum value
};


// Internal structure describing parameter properties
template<class TValue>
struct SParamDescription
{
    char*  section;
    char*  name;
    TValue initial_value;

    // List of enum values if any
    SEnumDescription<TValue>* enums;
    int                       enums_size;
};


/// Parameter declaration. Generates type TParam_section_name.
/// Section and name may be used to set default value through a
/// registry or environment variable section_name.
#define NCBI_PARAM_DECL(type, section, name) \
    struct SNcbiParamDesc_##section##_##name \
    { \
        typedef type TValueType; \
        static SParamDescription< type > sm_ParamDescription; \
    }; \
    typedef CParam< SNcbiParamDesc_##section##_##name > \
            TParam_##section##_##name


/// Enum parameter declaration. In addition to NCBI_PARAM_DECL also
/// specializes CParamParser< type >.
#define NCBI_ENUM_PARAM_DECL(type, section, name) \
    EMPTY_TEMPLATE inline CParamParser< type >::TValueType \
    CParamParser< type >::StringToValue(const string& str, \
                                        const TParamDesc& descr) \
    { return CEnumParser< type >::StringToEnum(str, descr); } \
    EMPTY_TEMPLATE inline string \
    CParamParser< type >::ValueToString(const TValueType& val, \
                                        const TParamDesc& descr) \
    { return CEnumParser< type >::EnumToString(val, descr); } \
    NCBI_PARAM_DECL(type, section, name);


#define NCBI_PARAM_DEF_INTERNAL(type, \
                                section, \
                                name, \
                                initial_value, \
                                enums, \
                                enums_size) \
    SParamDescription< type > \
    SNcbiParamDesc_##section##_##name::sm_ParamDescription = \
    { #section, #name, initial_value, enums, enums_size }


/// Parameter definition. "value" is used to set the initial parameter
/// value, which may be overriden by registry or environment.
#define NCBI_PARAM_DEF(type, section, name, initial_value) \
    NCBI_PARAM_DEF_INTERNAL(type, section, name, initial_value, NULL, 0)


/// Static array of enum name+value pairs. Must be defined before
/// NCBI_ENUM_PARAM_DEF
#define NCBI_ENUM_PARAM_ARRAY(type, section, name) \
    static SEnumDescription< type > s_EnumData_##section##_##name[] =


/// Enum parameter definition. Additional 'enums' argument should provide
/// static array of SEnumDescription<type>.
#define NCBI_ENUM_PARAM_DEF(type, section, name, initial_value) \
    NCBI_PARAM_DEF_INTERNAL(type, section, name, initial_value, \
    s_EnumData_##section##_##name, ArraySize(s_EnumData_##section##_##name))


/////////////////////////////////////////////////////////////////////////////
///
/// CParamParser
///
/// Parameter parser template.
///
/// Used to read parameter value from registry/environment.
/// Default implementation requires TValue to be readable from and writable
/// to a stream. Optimized specializations exist for string and bool types.
///


template<class TValue>
class CParamParser
{
public:
    typedef TValue                    TValueType;
    typedef SParamDescription<TValue> TParamDesc;

    static TValueType StringToValue(const string& str,
                                    const TParamDesc& descr);
    static string ValueToString(const TValueType& val,
                                const TParamDesc& descr);
};


/////////////////////////////////////////////////////////////////////////////
///
/// CEnumParser
///
/// Enum parameter parser template.
///
/// Converts between string and enum. Is used by NCBI_ENUM_PARAM_DECL.
///


template<class TValue>
class CEnumParser
{
public:
    typedef TValue                    TValueType;
    typedef SParamDescription<TValue> TParamDesc;
    typedef SEnumDescription<TValue>  TEnumDesc;

    static TValueType StringToEnum(const string& str,
                                   const TParamDesc& descr);
    static string EnumToString(const TValueType& val,
                               const TParamDesc& descr);
};


/////////////////////////////////////////////////////////////////////////////
///
/// CParam
///
/// Parameter template.
///
/// Used to store parameters with per-object values, thread-wide and
/// application-wide defaults. Global default value may be set through
/// application registry or environment.
///
/// Do not use the class directly. Create parameters through NCBI_PARAM_DECL
/// and NCBI_PARAM_DEF macros.
///

template<class TDescription>
class CParam
{
public:
    typedef CParam<TDescription>                   TParam;
    typedef typename TDescription::TValueType      TValueType;
    typedef CParamParser<TValueType>               TParamParser;

    /// Create parameter with the thread default or global default value.
    /// Changing defaults does not affect the existing parameter objects.
    CParam(void) : m_Value(GetThreadDefaultValue()) {}

    /// Create parameter with a given value, ignore defaults.
    CParam(const TValueType& val) : m_Value(val) {}

    /// Load parameter value from registry or environment.
    /// Overrides section and name set in the parameter description.
    /// Does not affect the existing default values.
    CParam(const string& section, const string& name);

    ~CParam(void) {}

    /// Get current parameter value.
    TValueType GetValue(void) const { return m_Value; }
    /// Set new parameter value (this instance only).
    void SetValue(const TValueType& val) { m_Value = val; }

    /// Get global default value. If not yet set, attempts to load the value
    /// from application registry or environment.
    static TValueType GetDefaultValue(void);
    /// Set new global default value. Does not affect values of existing
    /// CParam<> objects or thread-local default values.
    static void SetDefaultValue(const TValueType& val);

    /// Get thread-local default value if set or global default value.
    static TValueType GetThreadDefaultValue(void);
    /// Set new thread-local default value.
    static void SetThreadDefaultValue(const TValueType& val);

private:
    typedef CTls<TValueType> TValueTls;

    static SSystemFastMutex& sx_GetLock(void);
    static TValueType& sx_GetDefaultValue(void);
    static CRef<TValueTls>& sx_GetValueTls(void);

    TValueType m_Value;
};


// TLS cleanup function template

template<class TValue>
inline
void ParamTlsValueCleanup(TValue* value, void*)
{
    delete value;
}


// Generic CParamParser

template<class TValue>
inline
typename CParamParser<TValue>::TValueType
CParamParser<TValue>::StringToValue(const string& str,
                                    const TParamDesc& descr)
{
    CNcbiIstrstream in(str.c_str());
    TValueType val;
    in >> val;
    return val;
}


template<class TValue>
inline
string CParamParser<TValue>::ValueToString(const TValueType& val,
                                           const TParamDesc& descr)
{
    CNcbiOstrstream out;
    out << val;
    return string(out.str());
}


// CParamParser for string

EMPTY_TEMPLATE
inline
CParamParser<string>::TValueType
CParamParser<string>::StringToValue(const string& str,
                                    const TParamDesc&)
{
    return str;
}


EMPTY_TEMPLATE
inline
string CParamParser<string>::ValueToString(const string& val,
                                           const TParamDesc&)
{
    return val;
}


// CParamParser for bool

EMPTY_TEMPLATE
inline
CParamParser<bool>::TValueType
CParamParser<bool>::StringToValue(const string& str,
                                  const TParamDesc&)
{
    return NStr::StringToBool(str);
}


EMPTY_TEMPLATE
inline
string CParamParser<bool>::ValueToString(const bool& val,
                                         const TParamDesc&)
{
    return NStr::BoolToString(val);
}


// CEnumParser implementation

template<class TValue>
inline
typename CEnumParser<TValue>::TValueType
CEnumParser<TValue>::StringToEnum(const string& str,
                                  const TParamDesc& descr)
{
    for (int i = 0; i < descr.enums_size; ++i) {
        if ( NStr::Equal(str, descr.enums[i].alias) ) {
            return descr.enums[i].value;
        }
    }
    // Enum name not found
    return descr.initial_value;
}


template<class TValue>
inline
string CEnumParser<TValue>::EnumToString(const TValueType& val,
                                         const TParamDesc& descr)
{
    for (int i = 0; i < descr.enums_size; ++i) {
        if (descr.enums[i].value == val) {
            return string(descr.enums[i].alias);
        }
    }
    // Enum name not found
    return kEmptyStr;
}


// CParam implementation

template<class TDescription>
inline
CParam<TDescription>::CParam<TDescription>(const string& section,
                                           const string& name)
{
    string str = GetConfigString(section, name,
        TParamParser::ValueToString(GetThreadDefaultValue()));
    m_Value = TParamParser::StringToValue(str);
}


template<class TDescription>
inline
SSystemFastMutex& CParam<TDescription>::sx_GetLock(void)
{
    DEFINE_STATIC_FAST_MUTEX(s_ParamValueLock);
    return s_ParamValueLock;
}


template<class TDescription>
inline
typename CParam<TDescription>::TValueType&
CParam<TDescription>::sx_GetDefaultValue(void)
{
    static TValueType s_DefaultValue =
        TDescription::sm_ParamDescription.initial_value;
    static bool initialized = false;
    if (!initialized) {
        string config_value = GetConfigString(
            TDescription::sm_ParamDescription.section,
            TDescription::sm_ParamDescription.name,
            "");
        if (config_value != kEmptyStr) {
            s_DefaultValue =
                TParamParser::StringToValue(config_value,
                TDescription::sm_ParamDescription);
        }
        initialized = true;
    }
    return s_DefaultValue;
}


template<class TDescription>
inline
CRef<typename CParam<TDescription>::TValueTls>&
CParam<TDescription>::sx_GetValueTls(void)
{
    static CRef<TValueTls> s_ValueTls;
    return s_ValueTls;
}


template<class TDescription>
inline
typename CParam<TDescription>::TValueType
CParam<TDescription>::GetDefaultValue(void)
{
    CFastMutexGuard guard(sx_GetLock());
    return sx_GetDefaultValue();
}


template<class TDescription>
inline
void CParam<TDescription>::SetDefaultValue(const TValueType& val)
{
    CFastMutexGuard guard(sx_GetLock());
    sx_GetDefaultValue() = val;
}


template<class TDescription>
inline
typename CParam<TDescription>::TValueType
CParam<TDescription>::GetThreadDefaultValue(void)
{
    CFastMutexGuard guard(sx_GetLock());
    CRef<TValueTls>& tls = sx_GetValueTls();
    if ( tls ) {
        TValueType* v = tls->GetValue();
        if ( v ) {
            return *v;
        }
    }
    return sx_GetDefaultValue();
}


template<class TDescription>
inline
void CParam<TDescription>::SetThreadDefaultValue(const TValueType& val)
{
    CFastMutexGuard guard(sx_GetLock());
    CRef<TValueTls>& tls = sx_GetValueTls();
    if ( !tls ) {
        tls.Reset(new TValueTls);
    }
    tls->SetValue(new TValueType(val), ParamTlsValueCleanup<TValueType>);
}


END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/11/14 16:59:22  grichenk
 * Initial revision
 *
 *
 * ===========================================================================
 */

#endif  /* NCBIPARAM__HPP */
