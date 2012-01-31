#ifndef CORELIB___NCBI_PARAM__HPP
#define CORELIB___NCBI_PARAM__HPP

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
 * Authors:  Eugene Vasilchenko, Aleksey Grichenko
 *
 * File Description:
 *   Parameters storage interface
 *
 */

/// @file ncbiparam.hpp
/// Classes for storing parameters.
///


#include <corelib/ncbiapp.hpp>
#include <corelib/ncbithr.hpp>


/** @addtogroup Param
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
///
/// Usage of the parameters:
///
/// - Declare the parameter with NCBI_PARAM_DECL (NCBI_PARAM_ENUM_DECL for
///   enums):
///   NCBI_PARAM_DECL(int, MySection, MyIntParam);
///   NCBI_PARAM_DECL(string, MySection, MyStrParam);
///   NCBI_PARAM_ENUM_DECL(EMyEnum, MySection, MyEnumParam);
///
/// - Add parameter definition (this will also generate static data):
///   NCBI_PARAM_DEF(int, MySection, MyIntParam, 12345);
///   NCBI_PARAM_DEF(string, MySection, MyStrParam, "Default string value");
///
/// - For enum parameters define mappings between strings and values
///   before defining the parameter:
///   NCBI_PARAM_ENUM_ARRAY(EMyEnum, MySection, MyEnumParam)
///   {
///       {"My_A", eMyEnum_A},
///       {"My_B", eMyEnum_B},
///       {"My_C", eMyEnum_C}
///   };
///
///   NCBI_PARAM_ENUM_DEF(EMyEnum, MySection, MyEnumParam, eMyEnum_B);
///
/// - Use NCBI_PARAM_TYPE() as parameter type:
///   NCBI_PARAM_TYPE(MySection, MyIntParam)::GetDefault();
///   typedef NCBI_PARAM_TYPE(MySection, MyStrParam) TMyStrParam;
///   TMyStrParam str_param; str_param.Set("Local string value");
///


/////////////////////////////////////////////////////////////////////////////
///
/// Helper functions for getting values from registry/environment
///

/// Get string configuration value.
///
/// @param section
///   Check application configuration named section first if not null.
/// @param variable
///   Variable name within application section.
///   If no value found in configuration file, environment variable with
///   name NCBI_CONFIG__section__variable or NCBI_CONFIG__variable will be
///   checked, depending on wether section is null.
/// @param env_var_name
///   If not empty, overrides the default NCBI_CONFIG__section__name
///   name of the environment variable.
/// @param default_value
///   If no value found neither in configuration file nor in environment,
///   this value will be returned, or empty string if this value is null.
/// @return
///   string configuration value.
/// @sa g_GetConfigInt(), g_GetConfigFlag()
string NCBI_XNCBI_EXPORT g_GetConfigString(const char* section,
                                           const char* variable,
                                           const char* env_var_name,
                                           const char* default_value);

/// Get integer configuration value.
///
/// @param section
///   Check application configuration named section first if not null.
/// @param variable
///   Variable name within application section.
///   If no value found in configuration file, environment variable with
///   name NCBI_CONFIG__section__variable or NCBI_CONFIG__variable will be
///   checked, depending on wether section is null.
/// @param env_var_name
///   If not empty, overrides the default NCBI_CONFIG__section__name
///   name of the environment variable.
/// @param default_value
///   If no value found neither in configuration file nor in environment,
///   this value will be returned.
/// @return
///   integer configuration value.
/// @sa g_GetConfigString(), g_GetConfigFlag()
int NCBI_XNCBI_EXPORT g_GetConfigInt(const char* section,
                                     const char* variable,
                                     const char* env_var_name,
                                     int         default_value);

/// Get boolean configuration value.
///
/// @param section
///   Check application configuration named section first if not null.
/// @param variable
///   Variable name within application section.
///   If no value found in configuration file, environment variable with
///   name NCBI_CONFIG__section__variable or NCBI_CONFIG__variable will be
///   checked, depending on wether section is null.
/// @param env_var_name
///   If not empty, overrides the default NCBI_CONFIG__section__name
///   name of the environment variable.
/// @param default_value
///   If no value found neither in configuration file nor in environment,
///   this value will be returned.
/// @return
///   boolean configuration value.
/// @sa g_GetConfigString(), g_GetConfigInt()
bool NCBI_XNCBI_EXPORT g_GetConfigFlag(const char* section,
                                       const char* variable,
                                       const char* env_var_name,
                                       bool        default_value);


/// Get double configuration value.
///
/// @param section
///   Check application configuration named section first if not null.
/// @param variable
///   Variable name within application section.
///   If no value found in configuration file, environment variable with
///   name NCBI_CONFIG__section__variable or NCBI_CONFIG__variable will be
///   checked, depending on wether section is null.
/// @param env_var_name
///   If not empty, overrides the default NCBI_CONFIG__section__name
///   name of the environment variable.
/// @param default_value
///   If no value found neither in configuration file nor in environment,
///   this value will be returned.
/// @return
///   double configuration value.
/// @sa g_GetConfigString(), g_GetConfigInt()
double NCBI_XNCBI_EXPORT g_GetConfigDouble(const char* section,
                                           const char* variable,
                                           const char* env_var_name,
                                           double  default_value);

/////////////////////////////////////////////////////////////////////////////
///
/// Parameter declaration and definition macros
///
/// Each parameter must be declared and defined using the macros
///


// Internal definitions
#define X_NCBI_PARAM_DECLNAME(section, name)                                \
    SNcbiParamDesc_##section##_##name

#define X_NCBI_PARAM_DECLNAME_SCOPE(scope, section, name)                   \
    scope::SNcbiParamDesc_##section##_##name

#define X_NCBI_PARAM_ENUMNAME(section, name)                                \
    s_EnumData_##section##_##name

// Common part of the param description structure. 'desctype' can be
// SParamDescription or SParamEnumDescription.
#define X_NCBI_PARAM_DESC_DECL(type, desctype)                              \
    {                                                                       \
        typedef type TValueType;                                            \
        typedef desctype<TValueType> TDescription;                          \
        typedef CStaticTls< type > TTls;                                    \
        static TDescription sm_ParamDescription;                            \
        static TValueType sm_Default;                                       \
        static bool sm_DefaultInitialized;                                  \
        static TTls sm_ValueTls;                                            \
        static CParamBase::EParamState sm_State;                            \
    }

// Common definitions related to enum parser.
#define X_NCBI_PARAM_ENUM_PARSER_DECL(type)                                 \
    EMPTY_TEMPLATE inline                                                   \
    CParamParser< SParamEnumDescription< type > >::TValueType               \
    CParamParser< SParamEnumDescription< type > >::                         \
    StringToValue(const string&     str,                                    \
                  const TParamDesc& descr)                                  \
    { return CEnumParser< type >::StringToEnum(str, descr); }               \
    EMPTY_TEMPLATE inline string                                            \
    CParamParser< SParamEnumDescription< type > >::                         \
    ValueToString(const TValueType& val,                                    \
                  const TParamDesc& descr)                                  \
    { return CEnumParser< type >::EnumToString(val, descr); }

// Defenition of SNcbiParamDesc_XXXX static members common for normal
// and enum parameters.
#define X_NCBI_PARAM_STATIC_DEF(type, descname, defval)                     \
    type descname::sm_Default = defval;                                     \
    bool descname::sm_DefaultInitialized = false;                           \
    descname::TTls descname::sm_ValueTls;                                   \
    CParamBase::EParamState descname::sm_State = CParamBase::eState_NotSet  \


/// Generate typename for a parameter from its {section, name} attributes
#define NCBI_PARAM_TYPE(section, name)                                      \
    CParam< X_NCBI_PARAM_DECLNAME(section, name) >


/// Parameter declaration. Generates struct for storing the parameter.
/// Section and name may be used to set default value through a
/// registry or environment variable section_name.
/// @sa NCBI_PARAM_DEF
#define NCBI_PARAM_DECL(type, section, name)                                \
    struct X_NCBI_PARAM_DECLNAME(section, name)                             \
    X_NCBI_PARAM_DESC_DECL(type, SParamDescription)


/// Same as NCBI_PARAM_DECL but with export specifier (e.g. NCBI_XNCBI_EXPORT)
/// @sa NCBI_PARAM_DECL
#define NCBI_PARAM_DECL_EXPORT(expname, type, section, name)                \
    struct expname X_NCBI_PARAM_DECLNAME(section, name)                     \
    X_NCBI_PARAM_DESC_DECL(type, SParamDescription)


/// Enum parameter declaration. In addition to NCBI_PARAM_DECL also
/// specializes CParamParser<type> to convert between strings and
/// enum values.
/// @sa NCBI_PARAM_ENUM_ARRAY
/// @sa NCBI_PARAM_ENUM_DEF
#define NCBI_PARAM_ENUM_DECL(type, section, name)                           \
    X_NCBI_PARAM_ENUM_PARSER_DECL(type)                                     \
    struct X_NCBI_PARAM_DECLNAME(section, name)                             \
    X_NCBI_PARAM_DESC_DECL(type, SParamEnumDescription)


/// Same as NCBI_PARAM_ENUM_DECL but with export specifier (e.g. NCBI_XNCBI_EXPORT)
/// @sa NCBI_PARAM_ENUM_DECL
#define NCBI_PARAM_ENUM_DECL_EXPORT(expname, type, section, name)           \
    X_NCBI_PARAM_ENUM_PARSER_DECL(type)                                     \
    struct expname X_NCBI_PARAM_DECLNAME(section, name)                     \
    X_NCBI_PARAM_DESC_DECL(type, SParamEnumDescription)


/// Parameter definition. "value" is used to set the initial parameter
/// value, which may be overriden by registry or environment.
/// @sa NCBI_PARAM_DECL
#define NCBI_PARAM_DEF(type, section, name, default_value)                  \
    SParamDescription< type >                                               \
    X_NCBI_PARAM_DECLNAME(section, name)::sm_ParamDescription =             \
        { #section, #name, 0, default_value, NULL, eParam_Default };        \
    X_NCBI_PARAM_STATIC_DEF(type,                                           \
        X_NCBI_PARAM_DECLNAME(section, name),                               \
        default_value)


/// Parameter definition. The same as NCBI_PARAM_DEF, but with a callback
/// used to initialize the default value.
/// @sa NCBI_PARAM_DEF
#define NCBI_PARAM_DEF_WITH_INIT(type, section, name, default_value, init)  \
    SParamDescription< type >                                               \
    X_NCBI_PARAM_DECLNAME(section, name)::sm_ParamDescription =             \
        { #section, #name, 0, default_value, init, eParam_Default };        \
    X_NCBI_PARAM_STATIC_DEF(type,                                           \
        X_NCBI_PARAM_DECLNAME(section, name),                               \
        default_value)


/// Definition of a parameter with additional flags.
/// @sa NCBI_PARAM_DEF
/// @sa ENcbiParamFlags
#define NCBI_PARAM_DEF_EX(type, section, name, default_value, flags, env)   \
    SParamDescription< type >                                               \
    X_NCBI_PARAM_DECLNAME(section, name)::sm_ParamDescription =             \
        { #section, #name, #env, default_value, NULL, flags };              \
    X_NCBI_PARAM_STATIC_DEF(type,                                           \
        X_NCBI_PARAM_DECLNAME(section, name),                               \
        default_value)


/// Definition of a parameter with additional flags and initialization
/// callback.
/// @sa NCBI_PARAM_DEF_WITH_INIT
/// @sa ENcbiParamFlags
#define NCBI_PARAM_DEF_EX_WITH_INIT(type, section, name, def_value, init, flags, env) \
    SParamDescription< type >                                               \
    X_NCBI_PARAM_DECLNAME(section, name)::sm_ParamDescription =             \
        { #section, #name, #env, def_value, init, flags };                  \
    X_NCBI_PARAM_STATIC_DEF(type,                                           \
        X_NCBI_PARAM_DECLNAME(section, name),                               \
        def_value)


/// Similar to NCBI_PARAM_DEF except it adds "scope" (class name or 
/// namespace) to the parameter's type.
/// @sa NCBI_PARAM_DECL, NCBI_PARAM_DEF
#define NCBI_PARAM_DEF_IN_SCOPE(type, section, name, default_value, scope)   \
    SParamDescription< type >                                                \
    X_NCBI_PARAM_DECLNAME_SCOPE(scope, section, name)::sm_ParamDescription = \
        { #section, #name, 0, default_value, NULL, eParam_Default };         \
    X_NCBI_PARAM_STATIC_DEF(type,                                            \
        X_NCBI_PARAM_DECLNAME_SCOPE(scope, section, name),                   \
        default_value)


// Static array of enum name+value pairs.
#define NCBI_PARAM_ENUM_ARRAY(type, section, name)                        \
    static SEnumDescription< type > X_NCBI_PARAM_ENUMNAME(section, name)[] =

/// Enum parameter definition. Additional 'enums' argument should provide
/// static array of SEnumDescription<type>.
/// @sa NCBI_PARAM_ENUM_ARRAY
#define NCBI_PARAM_ENUM_DEF(type, section, name, default_value)             \
    SParamEnumDescription< type >                                           \
    X_NCBI_PARAM_DECLNAME(section, name)::sm_ParamDescription =             \
        { #section, #name, 0, default_value, NULL, eParam_Default,          \
          X_NCBI_PARAM_ENUMNAME(section, name),                             \
          ArraySize(X_NCBI_PARAM_ENUMNAME(section, name)) };                \
    X_NCBI_PARAM_STATIC_DEF(type,                                           \
        X_NCBI_PARAM_DECLNAME(section, name),                               \
        default_value)


/// Definition of an enum parameter with additional flags.
/// @sa NCBI_PARAM_ENUM_DEF
/// @sa ENcbiParamFlags
#define NCBI_PARAM_ENUM_DEF_EX(type, section, name,                         \
                               default_value, flags, env)                   \
    SParamEnumDescription< type >                                           \
    X_NCBI_PARAM_DECLNAME(section, name)::sm_ParamDescription =             \
        { #section, #name, #env, default_value, NULL, flags,                \
          X_NCBI_PARAM_ENUMNAME(section, name),                             \
          ArraySize(X_NCBI_PARAM_ENUMNAME(section, name)) };                \
    X_NCBI_PARAM_STATIC_DEF(type,                                           \
        X_NCBI_PARAM_DECLNAME(section, name),                               \
        default_value)


/////////////////////////////////////////////////////////////////////////////
///
/// CParamException
///
/// Exception generated by param parser

class NCBI_XNCBI_EXPORT CParamException : public CCoreException
{
public:
    enum EErrCode {
        eParserError,      ///< Can not convert string to value
        eBadValue,         ///< Unexpected parameter value
        eNoThreadValue,    ///< Per-thread value is prohibited by flags
        eRecursion         ///< Recursion while initializing param
    };

    /// Translate from the error code value to its string representation.
    virtual const char* GetErrCodeString(void) const;

    // Standard exception boilerplate code.
    NCBI_EXCEPTION_DEFAULT(CParamException, CCoreException);
};



/////////////////////////////////////////////////////////////////////////////
///
/// CParamParser
///
/// Parameter parser template.
///
/// Used to read parameter value from registry/environment.
/// Default implementation requires TValue to be readable from and writable
/// to a stream. Optimized specializations exist for string and bool types.
/// The template is also specialized for each enum parameter.
///


template<class TDescription>
class CParamParser
{
public:
    typedef TDescription                      TParamDesc;
    typedef typename TDescription::TValueType TValueType;

    static TValueType StringToValue(const string& str,
                                    const TParamDesc& descr);
    static string     ValueToString(const TValueType& val,
                                    const TParamDesc& descr);
};


/////////////////////////////////////////////////////////////////////////////
///
/// CParamBase
///
/// Base class to provide single static mutex for parameters.
///

class NCBI_XNCBI_EXPORT CParamBase
{
public:
    /// Current param state flag - indicates which possible sources for
    /// the param have been checked. This flag does not indicate where
    /// does the current value originate from (except eState_User).
    /// It just shows the stage of parameter loading process.
    enum EParamState {
        eState_NotSet = 0, ///< The param's value has not been set yet
        eState_InFunc = 1, ///< The initialization function is being executed
        eState_Func   = 2, ///< Initialized using FParamInit function
        eState_User   = 3, ///< Value has been set by user
        eState_EnvVar = 4, ///< The environment variable has been checked
        eState_Config = 5  ///< The app. config file has been checked
    };

protected:
    static SSystemMutex& s_GetLock(void);
};


/////////////////////////////////////////////////////////////////////////////
///
/// ENcbiParamFlags
///
/// CParam flags

enum ENcbiParamFlags {
    eParam_Default  = 0,       ///< Default flags
    eParam_NoLoad   = 1 << 0,  ///< Do not load from registry or environment
    eParam_NoThread = 1 << 1   ///< Do not use per-thread values
};

typedef int TNcbiParamFlags;

/// Caching default value on construction of a param
enum EParamCacheFlag {
    eParamCache_Force,  ///< Force caching currently set default value.
    eParamCache_Try,    ///< Cache the default value if the application
                        ///< registry is already initialized.
    eParamCache_Defer   ///< Do not try to cache the default value.
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
class CParam : public CParamBase
{
public:
    typedef CParam<TDescription>                   TParam;
    typedef typename TDescription::TDescription    TParamDescription;
    typedef typename TParamDescription::TValueType TValueType;
    typedef CParamParser<TParamDescription>        TParamParser;
    typedef typename TDescription::TTls            TTls;

    /// Create parameter with the thread default or global default value.
    /// Changing defaults does not affect the existing parameter objects.
    CParam(EParamCacheFlag cache_flag = eParamCache_Try);

    /// Create parameter with a given value, ignore defaults.
    CParam(const TValueType& val) : m_ValueSet(true), m_Value(val) {}

    /// Load parameter value from registry or environment.
    /// Overrides section and name set in the parameter description.
    /// Does not affect the existing default values.
    CParam(const string& section, const string& name);

    ~CParam(void) {}

    /// Get current state of the param.
    static EParamState GetState(void);

    /// Get current parameter value.
    TValueType Get(void) const;
    /// Set new parameter value (this instance only).
    void Set(const TValueType& val);
    /// Reset value as if it has not been initialized yet. Next call to
    /// Get() will cache the thread default (or global default) value.
    void Reset(void);

    /// Get global default value. If not yet set, attempts to load the value
    /// from application registry or environment.
    static TValueType GetDefault(void);
    /// Set new global default value. Does not affect values of existing
    /// CParam<> objects or thread-local default values.
    static void SetDefault(const TValueType& val);
    /// Reload the global default value from the environment/registry
    /// or reset it to the initial value specified in NCBI_PARAM_DEF.
    static void ResetDefault(void);

    /// Get thread-local default value if set or global default value.
    static TValueType GetThreadDefault(void);
    /// Set new thread-local default value.
    static void SetThreadDefault(const TValueType& val);
    /// Reset thread default value as if it has not been set. Unless
    /// SetThreadDefault() is called, GetThreadDefault() will return
    /// global default value.
    static void ResetThreadDefault(void);

private:
    static TValueType& sx_GetDefault(bool force_reset = false);
    static TTls&       sx_GetTls    (void);
    static EParamState& sx_GetState(void);

    static bool sx_IsSetFlag(ENcbiParamFlags flag);
    static bool sx_CanGetDefault(void);

    mutable bool       m_ValueSet;
    mutable TValueType m_Value;
};


END_NCBI_SCOPE


/* @} */

#include <corelib/impl/ncbi_param_impl.hpp>

#endif  /* CORELIB___NCBI_PARAM__HPP */
