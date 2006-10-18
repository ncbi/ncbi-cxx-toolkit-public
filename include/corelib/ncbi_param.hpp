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


#include <corelib/ncbistd.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbiapp.hpp>


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


/////////////////////////////////////////////////////////////////////////////
///
/// Parameter declaration and definition macros
///
/// Each parameter must be declared and defined using the macros
///

/// Generate typename for a parameter from its {section, name} attributes
#define NCBI_PARAM_TYPE(section, name)          \
    CParam<SNcbiParamDesc_##section##_##name>


/// Parameter declaration. Generates struct for storing the parameter.
/// Section and name may be used to set default value through a
/// registry or environment variable section_name.
/// @sa NCBI_PARAM_DEF
#define NCBI_PARAM_DECL(type, section, name)                      \
    struct SNcbiParamDesc_##section##_##name                      \
    {                                                             \
        typedef type TValueType;                                  \
        typedef SParamDescription<TValueType> TDescription;       \
        static TDescription sm_ParamDescription;                  \
    }


/// Enum parameter declaration. In addition to NCBI_PARAM_DECL also
/// specializes CParamParser<type> to convert between strings and
/// enum values.
/// @sa NCBI_PARAM_ENUM_ARRAY
/// @sa NCBI_PARAM_ENUM_DEF
#define NCBI_PARAM_ENUM_DECL(type, section, name)                 \
                                                                  \
    EMPTY_TEMPLATE inline                                         \
    CParamParser< SParamEnumDescription< type > >::TValueType     \
    CParamParser< SParamEnumDescription< type > >::               \
    StringToValue(const string&     str,                          \
                  const TParamDesc& descr)                        \
    { return CEnumParser< type >::StringToEnum(str, descr); }     \
                                                                  \
    EMPTY_TEMPLATE inline string                                  \
    CParamParser< SParamEnumDescription< type > >::               \
    ValueToString(const TValueType& val,                          \
                  const TParamDesc& descr)                        \
    { return CEnumParser< type >::EnumToString(val, descr); }     \
                                                                  \
    struct SNcbiParamDesc_##section##_##name                      \
    {                                                             \
        typedef type TValueType;                                  \
        typedef SParamEnumDescription<TValueType> TDescription;   \
        static TDescription sm_ParamDescription;                  \
    }


/// Parameter definition. "value" is used to set the initial parameter
/// value, which may be overriden by registry or environment.
/// @sa NCBI_PARAM_DECL
#define NCBI_PARAM_DEF(type, section, name, default_value)     \
    SParamDescription< type >                                  \
    SNcbiParamDesc_##section##_##name::sm_ParamDescription =   \
        { #section, #name, 0, default_value, eParam_Default }


/// Similar to NCBI_PARAM_DEF except it adds "scope" (class name or 
/// namespace) to the parameter's type.
/// @sa NCBI_PARAM_DECL, NCBI_PARAM_DEF
#define NCBI_PARAM_DEF_IN_SCOPE(type, section, name, default_value, scope)    \
    SParamDescription< type >                                  \
    scope::SNcbiParamDesc_##section##_##name::sm_ParamDescription =  \
        { #section, #name, 0, default_value, eParam_Default }


/// Static array of enum name+value pairs. Must be defined before
/// NCBI_PARAM_ENUM_DEF
/// @sa NCBI_PARAM_ENUM_DEF
#define NCBI_PARAM_ENUM_ARRAY(type, section, name) \
    static SEnumDescription< type > s_EnumData_##section##_##name[] =


/// Enum parameter definition. Additional 'enums' argument should provide
/// static array of SEnumDescription<type>.
/// @sa NCBI_PARAM_ENUM_ARRAY
#define NCBI_PARAM_ENUM_DEF(type, section, name, default_value) \
    SParamEnumDescription< type >                               \
    SNcbiParamDesc_##section##_##name::sm_ParamDescription =    \
        { #section, #name, 0, default_value, eParam_Default,    \
          s_EnumData_##section##_##name,                        \
          ArraySize(s_EnumData_##section##_##name) }


/// Definition of a parameter with additional flags.
/// @sa NCBI_PARAM_DEF
/// @sa ENcbiParamFlags
#define NCBI_PARAM_DEF_EX(type, section, name, default_value, flags, env) \
    SParamDescription< type >                                             \
    SNcbiParamDesc_##section##_##name::sm_ParamDescription =              \
        { #section, #name, #env, default_value, flags }


/// Definition of an enum parameter with additional flags.
/// @sa NCBI_PARAM_ENUM_DEF
/// @sa ENcbiParamFlags
#define NCBI_PARAM_ENUM_DEF_EX(type, section, name,            \
                               default_value, flags, env)      \
    SParamEnumDescription< type >                              \
    SNcbiParamDesc_##section##_##name::sm_ParamDescription =   \
        { #section, #name, #env, default_value, flags,         \
          s_EnumData_##section##_##name,                       \
          ArraySize(s_EnumData_##section##_##name) }


/////////////////////////////////////////////////////////////////////////////
///
/// CParamException
///
/// Exception generated by param parser

class CParamException : public CCoreException
{
public:
    enum EErrCode {
        eParserError,      ///< Can not convert string to value
        eBadValue,         ///< Unexpected parameter value
        eNoThreadValue     ///< Per-thread value is prohibited by flags
    };

    /// Translate from the error code value to its string representation.
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eParserError:   return "eParserError";
        case eBadValue:      return "eBadValue";
        case eNoThreadValue: return "eNoThreadValue";
        default:            return CException::GetErrCodeString();
        }
    }

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
protected:
    static SSystemFastMutex& s_GetLock(void);
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

    /// Get current parameter value.
    TValueType Get(void) const;
    /// Set new parameter value (this instance only).
    void Set(const TValueType& val);

    /// Get global default value. If not yet set, attempts to load the value
    /// from application registry or environment.
    static TValueType GetDefault(void);
    /// Set new global default value. Does not affect values of existing
    /// CParam<> objects or thread-local default values.
    static void SetDefault(const TValueType& val);

    /// Get thread-local default value if set or global default value.
    static TValueType GetThreadDefault(void);
    /// Set new thread-local default value.
    static void SetThreadDefault(const TValueType& val);

private:
    typedef CTls<TValueType> TTls;

    static TValueType&       sx_GetDefault (void);
    static CRef<TTls>&       sx_GetTls     (void);

    static bool sx_IsSetFlag(ENcbiParamFlags flag);
    static bool sx_CanGetDefault(void);

    mutable bool       m_ValueSet;
    mutable TValueType m_Value;
};


END_NCBI_SCOPE


/* @} */

#include <corelib/impl/ncbi_param_impl.hpp>

/*
 * ===========================================================================
 * $Log$
 * Revision 1.10  2006/10/18 18:30:27  vakatov
 * + NCBI_PARAM_DEF_IN_SCOPE
 *
 * Revision 1.9  2006/01/05 20:40:17  grichenk
 * Added explicit environment variable name for params.
 * Added default value caching flag to CParam constructor.
 *
 * Revision 1.8  2005/12/22 21:18:29  grichenk
 * Removed duplicate comments.
 *
 * Revision 1.7  2005/12/22 16:56:23  grichenk
 * Added NoThread and NoLoad flags.
 *
 * Revision 1.6  2005/11/17 21:50:00  ucko
 * Clean up gunk left over from a CVS merge.
 *
 * Revision 1.5  2005/11/17 21:33:09  grichenk
 * ncbi_param_impl.hpp moved to impl dir.
 *
 * Revision 1.4  2005/11/17 18:45:53  grichenk
 * Added comments and examples.
 * Moved CParam<> static mutex to base class.
 * Case insensitive parameter names.
 * Moved GetConfigXXX to ncbi_param, renamed to g_GetConfig.
 *
 * Revision 1.3  2005/11/15 17:46:40  grichenk
 * Redesigned enum parameters.
 * Moved implementation details to separate file.
 *
 * Revision 1.2  2005/11/14 22:12:49  vakatov
 * First code review, with fixes, proposals, etc.
 *
 * Revision 1.1  2005/11/14 16:59:22  grichenk
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* CORELIB___NCBI_PARAM__HPP */
