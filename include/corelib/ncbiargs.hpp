#ifndef NCBIARGS__HPP
#define NCBIARGS__HPP

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
 * Author:  Denis Vakatov
 *
 *
 */

/// @file ncbiargs.hpp
/// Defines command line argument related classes.
///
/// The CArgDescriptions and CArgDesc classes are used for describing
/// unparsed arguments; CArgs and CArgValue for parsed argument values;
/// CArgException and CArgHelpException for argument exceptions; and CArgAllow, 
/// CArgAllow_{Strings, ..., Integers, Doubles} for argument constraints.
///
/// The following description is included as applies to several classes in
/// this file:
///
/// Parsing and validation of command-line arguments are done according to
/// user-provided descriptions. The command line has the following syntax:
///
/// Command string:
///
///    progname  {arg_key, arg_key_opt, arg_key_dflt, arg_flag} [--]
///              {arg_pos} {arg_pos_opt, arg_pos_dflt}
///              {arg_extra} {arg_extra_opt}
///
/// where:
///
///   arg_key        :=  -<key> <value>    -- (mandatory)
///   arg_key_opt    := [-<key> <value>]   -- (optional, without default value)
///   arg_key_dflt   := [-<key> <value>]   -- (optional, with default value)
///   arg_flag       := -<flag>            -- (always optional)
///   "--" is an optional delimiter to indicate the beginning of pos. args
///   arg_pos        := <value>            -- (mandatory)
///   arg_pos_opt    := [<value>]          -- (optional, without default value)
///   arg_pos_dflt   := [<value>]          -- (optional, with default value)
///   arg_extra      := <value>            -- (dep. on the constraint policy)
///   arg_extra_opt  := [<value>]          -- (dep. on the constraint policy)
///
/// and:
///
///   <key> must be followed by <value>
///   <flag> and <key> are case-sensitive, and they can contain
///                    only alphanumeric characters
///   <value> is an arbitrary string (additional constraints can
///           be applied in the argument description, see "EType")
///
/// {arg_pos***} and {arg_extra***} -- position-dependent arguments, with
/// no tag preceding them.
/// {arg_pos***} -- have individual names and descriptions (see methods
/// AddPositional***).
/// {arg_extra***} have one description for all (see method AddExtra).
/// User can apply constraints on the number of mandatory and optional
/// {arg_extra***} arguments.


#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbi_limits.h>
#include <memory>
#include <set>
#include <list>
#include <vector>


/** @addtogroup Args
 *
 * @{
 */


BEGIN_NCBI_SCOPE


// Some necessary forward declarations.
class CNcbiArguments;
class CArgAllow;


/////////////////////////////////////////////////////////////////////////////
///
/// CArgException --
///
/// Define exceptions class for incorrectly formed arguments.
///
/// CArgException inherits its basic functionality from CCoreException
/// and defines additional error codes for malformed arguments.

class CArgException : public CCoreException
{
public:
    /// Error types for improperly formatted arguments.
    ///
    /// These error conditions are checked for and caught when processing
    /// arguments.
    enum EErrCode {
        eInvalidArg,    ///< Invalid argument
        eNoValue,       ///< Expecting an argument value
        eWrongCast,     ///< Incorrect cast for an argument
        eConvert,       ///< Conversion problem
        eNoFile,        ///< Expecting a file
        eConstraint,    ///< Argument value outside constraints
        eArgType,       ///< Wrong argument type
        eNoArg,         ///< No argument
        eSynopsis       ///< Synopois error
    };

    /// Translate from the error code value to its string representation.
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eInvalidArg: return "eInvalidArg";
        case eNoValue:    return "eNoValue";
        case eWrongCast:  return "eWrongCast";
        case eConvert:    return "eConvert";
        case eNoFile:     return "eNoFile";
        case eConstraint: return "eConstraint";
        case eArgType:    return "eArgType";
        case eNoArg:      return "eNoArg";
        case eSynopsis:   return "eSynopsis";
        default:    return CException::GetErrCodeString();
        }
    }

    // Standard exception bolier plate code.
    NCBI_EXCEPTION_DEFAULT(CArgException, CCoreException);
};



/////////////////////////////////////////////////////////////////////////////
///
/// CArgHelpException --
///
/// Define exception class that gets thrown for Help messages.
///
/// CArgException inherits its basic functionality from CArgException
/// and defines an additional error code for help.

class CArgHelpException : public CArgException
{
public:
    /// Error type for help exception.
    enum EErrCode {
        eHelp,          ///< Error code for short help message
        eHelpFull       ///< Error code for detailed help message
    };

    /// Translate from the error code value to its string representation.
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eHelp:     return "eHelp";
        case eHelpFull: return "eHelpFull";
        default:    return CException::GetErrCodeString();
        }
    }

    // Standard exception bolier plate code.
    NCBI_EXCEPTION_DEFAULT(CArgHelpException, CArgException);
};


/////////////////////////////////////////////////////////////////////////////
///
/// CArgValue --
///
/// Generic abstract base class for argument values.

class NCBI_XNCBI_EXPORT CArgValue : public CObject
{
public:
    /// Get argument name.
    const string& GetName(void) const { return m_Name; }

    /// Check if argument holds a value.
    ///
    /// Argument does not hold value if it was described as optional argument
    /// without default value, and if it was not passed a value in the command
    /// line.  On attempt to retrieve the value from such "no-value" argument,
    /// exception will be thrown.
    virtual bool HasValue(void) const = 0;

    DECLARE_OPERATOR_BOOL(HasValue());

    /// Get the argument's string value.
    ///
    /// If it is a value of a flag argument, then return either "true"
    /// or "false".
    /// @sa
    ///   AsInteger(), AsInt8(), AsDouble(), AsBoolean()
    virtual const string& AsString(void) const = 0;

    /// Get the argument's integer (8-byte long) value.
    ///
    /// If you request a wrong value type, such as a call to "AsInt8()"
    /// for a "boolean" argument, an exception is thrown.
    /// This will however work okay for "plain integer" argument.
    /// @sa
    ///   AsInteger(), AsString(), AsDouble, AsBoolean()
    virtual Int8 AsInt8(void) const = 0;

    /// Get the argument's integer value.
    ///
    /// If you request a wrong value type, such as a call to "AsInteger()"
    /// for a "boolean" or even "Int8" argument, an exception is thrown.
    /// @sa
    ///   AsInt8(), AsString(), AsDouble, AsBoolean()
    virtual int    AsInteger(void) const = 0;

    /// Get the argument's double value.
    ///
    /// If you request a wrong value type, such as a call to "AsDouble()"
    /// for a "boolean" argument, an exception is thrown.
    /// @sa
    ///   AsString(), AsInt8(), AsInteger, AsBoolean()
    virtual double AsDouble (void) const = 0;

    /// Get the argument's boolean value.
    ///
    /// If you request a wrong value type, such as a call to "AsBoolean()"
    /// for a "integer" argument, an exception is thrown.
    /// @sa
    ///   AsString(), AsInt8(), AsInteger, AsDouble()
    virtual bool   AsBoolean(void) const = 0;

    /// Get the argument as an input file stream.
    virtual CNcbiIstream& AsInputFile (void) const = 0;

    /// Get the argument as an output file stream.
    virtual CNcbiOstream& AsOutputFile(void) const = 0;

    /// Close the file.
    virtual void CloseFile (void) const = 0;



    /// Some values types can contain several value lists
    ///
    /// Example: CGIs pass list selections by repeating the same name
    typedef vector<string>  TStringArray;

    /// Get the value list
    virtual const TStringArray& GetStringList() const;

    /// Get reference on value list for further modification
    virtual TStringArray& SetStringList();

protected:
    friend class CArgs;

    /// Protected constructor and destructor.
    ///
    /// Prohibit explicit instantiation of CArgValue with name.
    CArgValue(const string& name);
    virtual ~CArgValue(void);

    string m_Name;          ///< Argument name
};



/////////////////////////////////////////////////////////////////////////////
///
/// CArgs --
///
/// Defines parsed arguments.
///
/// Argument values are obtained from the unprocessed command-line arguments
/// (via CNcbiArguments) and then verified and processed according to the
/// argument descriptions defined by user in "CArgDescriptions".
///
/// NOTE:  the extra arguments can be accessed using virtual names:
///           "#1", "#2", "#3", ..., "#<GetNExtra()>"
///        in the order of insertion (by method Add).
///

class NCBI_XNCBI_EXPORT CArgs
{
public:
    /// Constructor.
    CArgs(void);

    /// Destructor.
    ~CArgs(void);

    /// Check existence of argument description.
    ///
    /// Return TRUE if arg "name" was described in the parent CArgDescriptions.
    bool Exist(const string& name) const;

    /// Get value of argument by name.
    ///
    /// Throw an exception if such argument does not exist.
    /// @sa
    ///   Exist() above.
    const CArgValue& operator[] (const string& name) const;

    /// Get the number of unnamed positional (a.k.a. extra) args.
    size_t GetNExtra(void) const { return m_nExtra; }

    /// Return N-th extra arg value,  N = 1 to GetNExtra().
    const CArgValue& operator[] (size_t idx) const;

    /// Print (append) all arguments to the string "str" and return "str".
    string& Print(string& str) const;

    /// Add new argument name and value.
    ///
    /// Throw an exception if the "name" is not an empty string, and if
    /// there is an argument with this name already and "update" parameter is 
    /// not set.
    ///
    /// HINT: Use empty "name" to add extra (unnamed) args, and they will be
    /// automagically assigned with the virtual names: "#1", "#2", "#3", etc.
    ///
    /// @param arg
    ///    argument value added to the collection
    /// @param update
    ///    when TRUE and argument already exists it will be replaced
    ///    when FALSE throws an exception 
    /// @param add_value
    ///    when TRUE and argument already exists the value is
    ///    added to the string list (multiple argument)
    void Add(CArgValue* arg, 
             bool       update    = false,
             bool       add_value = false);

    /// Check if there are no arguments in this container.
    bool IsEmpty(void) const;

    /// Remove argument
    void Remove(const string& name);

private:
    typedef set< CRef<CArgValue> >  TArgs;   ///< Type for arguments
    typedef TArgs::iterator         TArgsI;  ///< Type for iterator
    typedef TArgs::const_iterator   TArgsCI; ///< Type for const iterator

    TArgs  m_Args;    ///< Assoc. map of arguments' name/value
    size_t m_nExtra;  ///< Cached # of unnamed positional arguments 

    /// Find argument value with name "name".
    TArgsCI x_Find(const string& name) const;
    TArgsI x_Find(const string& name);
};



class CArgDesc;


/////////////////////////////////////////////////////////////////////////////
///
/// CArgDescriptions --
///
/// Description of unparsed arguments.
///
/// Container to store the command-line argument descriptions. Provides the
/// means for the parsing and verification of command-line arguments against
/// the contained descriptions.
///
/// Example: Translating "CNcbiArguments" ---> "CArgs".
/// Can also be used to compose and print out the USAGE info.

class NCBI_XNCBI_EXPORT CArgDescriptions
{
public:
    /// Constructor.
    ///
    /// If "auto_help" is passed TRUE, then a special flag "-h" will be added
    /// to the list of accepted arguments. Passing "-h" in the command line
    /// will printout USAGE and ignore all other passed arguments.
    CArgDescriptions(bool auto_help = true);

    /// Destructor.
    virtual ~CArgDescriptions(void);

    /// Type of CArgDescriptions
    /// For a CGI application positional argumants and flags does not make
    /// sense (this sintax cannot be expressed by CGI protocol)
    enum EArgSetType {
        eRegularArgs,  ///< Regular application
        eCgiArgs       ///< CGI application
    };

    /// Set type of argument set.
    /// Method performs verisifation of arguments, 
    /// throws an exception if positional args were set for a CGI
    void SetArgsType(EArgSetType args_type);

    EArgSetType GetArgsType() const { return m_ArgsType; }

    /// Available argument types.
    enum EType {
        eString = 0, ///< An arbitrary string
        eBoolean,    ///< {'true', 't', 'false', 'f'},  case-insensitive
        eInt8,       ///< Convertible into an integer number (Int8 only)
        eInteger,    ///< Convertible into an integer number (int or Int8)
        eDouble,     ///< Convertible into a floating point number (double)
        eInputFile,  ///< Name of file (must exist and be readable)
        eOutputFile, ///< Name of file (must be writeable)

        k_EType_Size ///< For internal use only
    };

    /// Get argument type's name string.
    static const string& GetTypeName(EType type);

    /// Additional flags, the first group is file related flags.
    ///
    /// Must match the argument type, or an exception will be thrown.
    /// ( File related are for eInputFile and eOutputFiler argument types.)
    enum EFlags {
        // file related flags:

        /// Open file right away for eInputFile, eOutputFile
        fPreOpen = (1 << 0), 
        /// Open as binary file for eInputFile, eOutputFile
        fBinary  = (1 << 1), 
        ///< Append to end-of-file for eOutputFile only
        fAppend  = (1 << 2), 

        // multiple keys flag
        /// Repeated key arguments are legal (use with AddKey)
        fAllowMultiple = (1 << 3) 
    };
    typedef unsigned int TFlags;  ///< Binary OR of "EFlags"

    /// Add description for mandatory key.
    ///
    /// Mandatory key has the syntax:
    ///
    ///   arg_key := -<key> <value>
    ///
    /// Will throw exception CArgException if:
    ///  - description with name "name" already exists
    ///  - "name" contains symbols other than {alnum}
    ///  - "synopsis" contains symbols other than {alnum, '_'}
    ///  - "flags" are inconsistent with "type"
    ///
    /// Any argument can be later referenced using its unique name "name".
    void AddKey(const string& name,       ///< Name of argument key
                const string& synopsis,   ///< Synopis for argument
                const string& comment,    ///< Argument description
                EType         type,       ///< Argument type
                TFlags        flags = 0   ///< Optional file related flags
               );

    /// Add description for optional key without default value.
    ///
    /// Optional key without default value has the following syntax:
    ///
    ///   arg_key_opt := [-<key> <value>]
    ///
    /// Will throw exception CArgException if:
    ///  - description with name "name" already exists
    ///  - "name" contains symbols other than {alnum}
    ///  - "synopsis" contains symbols other than {alnum, '_'}
    ///  - "flags" are inconsistent with "type"
    ///
    /// Any argument can be later referenced using its unique name "name".
    void AddOptionalKey(const string& name,     ///< Name of argument key 
                        const string& synopsis, ///< Synopis for argument
                        const string& comment,  ///< Argument description
                        EType         type,     ///< Argument type
                        TFlags        flags = 0 ///< Optional file flags
                       );

    /// Add description for optional key with default value.
    ///
    /// Optional key with default value has the following syntax:
    ///
    ///   arg_key_dflt := [-<key> <value>]
    ///
    /// Will throw exception CArgException if:
    ///  - description with name "name" already exists
    ///  - "name" contains symbols other than {alnum}
    ///  - "synopsis" contains symbols other than {alnum, '_'}
    ///  - "flags" are inconsistent with "type"
    ///
    /// Any argument can be later referenced using its unique name "name".
    void AddDefaultKey(const string& name,          ///< Name of argument key 
                       const string& synopsis,      ///< Synopis for argument
                       const string& comment,       ///< Argument description
                       EType         type,          ///< Argument type
                       const string& default_value, ///< Default value
                       TFlags        flags = 0      ///< Optional file flags
                      );

    /// Add description for flag argument.
    ///
    /// Flag argument has the following syntax:
    ///
    ///  arg_flag  := -<flag>,     <flag> := "name"
    ///
    /// If argument "set_value" is TRUE, then:
    ///    - if the flag is provided (in the command-line), then the resultant
    ///      CArgValue::HasValue() will be TRUE;
    ///    - else it will be FALSE.
    ///
    /// Setting argument "set_value" to FALSE will reverse the above meaning.
    ///
    /// NOTE: If CArgValue::HasValue() is TRUE, then AsBoolean() is
    /// always TRUE.
    void AddFlag(const string& name,            ///< Name of argument
                 const string& comment,         ///< Argument description
                 bool          set_value = true ///< Is value set or not?
                );

    /// Add description for mandatory postional argument.
    ///
    /// Mandatory positional argument has the following syntax:
    ///
    ///   arg_pos := <value>
    ///
    /// NOTE: For all types of positional arguments:
    /// - The order is important! That is, the N-th positional argument passed
    ///   in the cmd.-line will be matched against (and processed according to)
    ///   the N-th added named positional argument description.
    /// - Mandatory positional args always go first.
    ///
    /// Will throw exception CArgException if:
    ///  - description with name "name" already exists
    ///  - "name" contains symbols other than {alnum}
    ///  - "flags" are inconsistent with "type"
    ///
    /// Any argument can be later referenced using its unique name "name".
    void AddPositional(const string& name,     ///< Name of argument
                       const string& comment,  ///< Argument description
                       EType         type,     ///< Argument type
                       TFlags        flags = 0 ///< Optional file flags
                      );

    /// Add description for optional positional argument without default
    /// value.
    ///
    /// Optional positional argument, without default value has the following
    /// syntax:
    ///
    ///  arg_pos_opt := [<value>]
    ///
    /// Will throw exception CArgException if:
    ///  - description with name "name" already exists
    ///  - "name" contains symbols other than {alnum}
    ///  - "flags" are inconsistent with "type"
    ///
    /// Any argument can be later referenced using its unique name "name".
    /// @sa
    ///   NOTE for AddPositional()
    void AddOptionalPositional(const string& name,     ///< Name of argument
                               const string& comment,  ///< Argument descr.
                               EType         type,     ///< Argument type
                               TFlags        flags = 0 ///< Optional file flgs
                              );

    /// Add description for optional positional argument with default value.
    ///
    /// Optional positional argument with default value has the following
    /// syntax:
    ///
    ///  arg_pos_dflt := [<value>]
    ///
    /// Will throw exception CArgException if:
    ///  - description with name "name" already exists
    ///  - "name" contains symbols other than {alnum}
    ///  - "flags" are inconsistent with "type"
    ///
    /// @sa
    ///   NOTE for AddPositional()
    void AddDefaultPositional(const string& name,   ///< Name of argument
                              const string& comment,///< Argument description
                              EType         type,   ///< Argument type
                              const string& default_value, ///< Default value
                              TFlags        flags = 0 ///< Optional file flags
                             );

    /// Add description for extra unnamed positional arguments.
    ///
    /// Provide description for the extra unnamed positional arguments.
    /// By default, no extra args are allowed.
    /// The name of this description is always an empty string.
    /// Names of the resulting arg.values will be:  "#1", "#2", ...
    ///
    /// Will throw exception CArgException if:
    ///  - description with name "name" already exists
    ///  - "flags" are inconsistent with "type"
    void AddExtra(unsigned      n_mandatory, ///< Number of mandatory args.
                  unsigned      n_optional,  ///< Number of optional args.
                  const string& comment,     ///< Argument description
                  EType         type,        ///< Argument type
                  TFlags        flags = 0    ///< Optional file flags
                 );

    /// Flag to invert constraint logically
    enum EConstraintNegate {
        eConstraintInvert,  ///< Logical NOT
        eConstraint         ///< Constraint is not inverted (taken as is)
    };

    /// Set additional user defined constraint on argument value.
    ///
    /// Constraint is defined by CArgAllow and its derived classes.
    /// The constraint object must be allocated by "new", and it must NOT be
    /// freed by "delete" after it has been passed to CArgDescriptions!
    ///
    /// @param name
    ///    Name of the parameter(flag) to check
    /// @param constraint
    ///    Constraint class
    /// @param negate
    ///    Flag indicates if this is inverted(NOT) constaint
    /// 
    /// @sa
    ///   See "CArgAllow_***" classes for some pre-defined constraints
    void SetConstraint(const string&      name, 
                       CArgAllow*         constraint,
                       EConstraintNegate  negate = eConstraint);

    /// Check if there is already an argument description with specified name.
    bool Exist(const string& name) const;

    /// Delete description with name "name".
    ///
    /// Throw the CArgException (eSynposis error code) exception if the
    /// specified name cannot be found.
    void Delete(const string& name);

    /// Set extra info to be used by PrintUsage().
    void SetUsageContext(const string& usage_name,           ///< Program name  
                         const string& usage_description,    ///< Usage descr.
                         bool          usage_sort_args = false,///< Sort args.
                         SIZE_TYPE     usage_width = 78);    ///< Format width

    /// Print usage and exit.
    ///
    /// Force to print USAGE unconditionally (and then exit) if no
    /// command-line args are present.
    void PrintUsageIfNoArgs(bool do_print = true);

    /// Print usage message to end of specified string.
    ///
    /// Printout USAGE and append to the string "str" using  provided
    /// argument descriptions and usage context.
    /// @return
    ///   Appended "str"
    virtual string& PrintUsage(string& str, bool detailed = false) const;

    /// Verify if argument "name" is spelled correctly.
    ///
    /// Argument name can contain only  alphanumeric characters and
    /// underscore ('_'), or be empty.
    static bool VerifyName(const string& name, bool extended = false);

    /// Convert argument map (key-value pairs) into arguments in accordance
    /// with the argument description
    template<class T>
    void ConvertKeys(CArgs* args, const T& arg_map, bool update) const
    {
        ITERATE(TKeyFlagArgs, it, m_KeyFlagArgs) {
            const string& param_name = *it;

            // find first element in the input multimap

            typename T::const_iterator vit = arg_map.find(param_name);
            typename T::const_iterator vend = arg_map.end();

            if (vit != vend) {   // at least one value found
                const string& v = vit->second;
                CArgValue* new_arg_value;

                x_CreateArg(param_name, param_name, 
                            true, /* value is present */
                            v,
                            1,
                            *args,
                            update,
                            &new_arg_value);

                if (new_arg_value) {

                    if (x_IsMultiArg(param_name)) {

                        CArgValue::TStringArray& varr = 
                            new_arg_value->SetStringList();

                        // try to add all additional arguments to arg value
                        for (++vit; vit != vend; ++vit) {
                            const string& n = vit->first;
                            if (n != param_name) {
                                break;
                            }
                            varr.push_back(vit->second);
                        } // for
                    }
                }

            } else {
                x_CreateArg(param_name, param_name, 
                            false, /* value is not present */
                            kEmptyStr,
                            1,
                            *args,
                            update);
            }


        } // ITERATE
    }

private:
    typedef set< AutoPtr<CArgDesc> >  TArgs;    ///< Argument descr. type
    typedef TArgs::iterator           TArgsI;   ///< Arguments iterator
    typedef TArgs::const_iterator     TArgsCI;  ///< Const arguments iterator
    typedef /*deque*/vector<string>   TPosArgs; ///< Positional arg. vector
    typedef list<string>              TKeyFlagArgs; ///< List of flag arguments

private:
    EArgSetType  m_ArgsType;  ///< Type of arguments
    TArgs        m_Args;      ///< Assoc.map of arguments' name/descr
    TPosArgs     m_PosArgs;   ///< Pos. args, ordered by position in cmd.-line
    TKeyFlagArgs m_KeyFlagArgs; ///< Key/flag args, in order of insertion
    unsigned     m_nExtra;    ///> # of mandatory extra args
    unsigned     m_nExtraOpt; ///< # of optional  extra args

    // Extra USAGE info
    string    m_UsageName;         ///< Program name
    string    m_UsageDescription;  ///< Program description
    bool      m_UsageSortArgs;     ///< Sort alphabetically on usage printout
    SIZE_TYPE m_UsageWidth;        ///< Maximum length of a usage line
    bool      m_AutoHelp;          ///< Special flag "-h" activated
    bool      m_UsageIfNoArgs;     ///< Print usage and exit if no args passed

    // Internal methods

    /// Helper method to find named parameter.
    TArgsI  x_Find(const string& name);

    /// Helper method to find named parameter -- const version.
    TArgsCI x_Find(const string& name) const;

    // Helper method for adding description.
    void x_AddDesc(CArgDesc& arg); 

    /// Helper method for doing pre-processing consistency checks.
    void x_PreCheck(void) const; 

    /// Helper method for checking if auto help requested and throw
    /// CArgHelpException if help requested.
    void x_CheckAutoHelp(const string& arg) const;

    /// Process arguments.
    ///
    /// Helper method to process arguments and build a CArgs object that is
    /// passed as the args parameter.
    /// @return
    ///   TRUE if specified "arg2" was used.
    bool    x_CreateArg(const string& arg1, ///< Argument to process 
                        bool have_arg2, ///< Is there an arg. that follows?
                        const string& arg2, ///< Following argument
                        unsigned* n_plain,  ///< Indicates number of args 
                        CArgs& args         ///< Contains processed args
                       ) const;

    /// @return
    ///   TRUE if specified "arg2" was used.
    bool x_CreateArg(const string& arg1,
                     const string& name, 
                     bool          have_arg2,
                     const string& arg2,
                     unsigned      n_plain,
                     CArgs&        args,
                     bool          update = false,
                     CArgValue**   new_value = 0) const;

    /// Helper method for doing post-processing consistency checks.
    void    x_PostCheck(CArgs& args, unsigned n_plain) const;

    /// Returns TRUE if parameter supports multiple arguments
    bool x_IsMultiArg(const string& name) const;

public:
    /// Create parsed arguments in CArgs object.
    ///
    /// Parse command-line arguments, and create "CArgs" args object 
    /// from the passed command-line arguments "argc" and "argv".
    ///
    /// Throw 
    ///  - CArgException on error
    ///  - CArgHelpException if USAGE printout was requested ("-h" flag)
    ///
    /// NOTE: You can deallocate the resulting "CArgs" object using 'delete'.
    ///
    /// Examples:
    ///
    /// (1) int main(int argc, const char* argv[]) {
    ///         CreateArgs(desc, argc, argv);
    ///     }
    ///
    /// (2) CNcbiArguments ncbi_args;
    ///     CreateArgs(desc, ncbi_args.Size(), ncbi_args);
    template<class TSize, class TArray>
    CArgs* CreateArgs(TSize argc, TArray argv) const
    {
        // Pre-processing consistency checks
        x_PreCheck();

        // Check for "-h" flag
        if ( m_AutoHelp ) {
            for (TSize i = 1;  i < argc;  i++) {
                x_CheckAutoHelp(argv[i]);
            }
        }

        // Create new "CArgs" to fill up, and parse cmd.-line args into it
        auto_ptr<CArgs> args(new CArgs());
        unsigned int n_plain = kMax_UInt;
        for (TSize i = 1;  i < argc;  i++) {
            bool have_arg2 = (i + 1 < argc);
            if ( x_CreateArg(argv[i], have_arg2,
                             have_arg2 ? (string) argv[i+1] : kEmptyStr,
                             &n_plain, *args) )
                i++;
        }

        // Check if there were any arguments at all
        if (n_plain == kMax_UInt) {
            n_plain = 0;
        }

        // Post-processing consistency checks
        x_PostCheck(*args, n_plain);
        return args.release();
    }

    CArgs* CreateArgs(const CNcbiArguments& argv) const;
};



/////////////////////////////////////////////////////////////////////////////
///
/// CArgAllow --
///
/// Abstract base class for defining argument constraints.
///
/// Other user defined constraints are defined by deriving from this abstract
/// base class:
///
///  - CArgAllow_Symbols  -- symbol from a set of allowed symbols
///  - CArgAllow_String   -- string to contain only allowed symbols 
///  - CArgAllow_Strings  -- string from a set of allowed strings
///  - CArgAllow_Int8s    -- Int8    value to fall within a given interval
///  - CArgAllow_Integers -- integer value to fall within a given interval
///  - CArgAllow_Doubles  -- floating-point value to fall in a given interval

class NCBI_XNCBI_EXPORT CArgAllow : public CObject
{
public:
    /// Verify if specified value is allowed.
    virtual bool Verify(const string &value) const = 0;

    /// Get usage information.
    virtual 
    string GetUsage(void) const = 0;

protected:
    /// Protected destructor.
    ///
    /// Prohibit from the allocation on stack or in the static data segment,
    /// and from the explicit deallocation by "delete".
    virtual ~CArgAllow(void);
};



/////////////////////////////////////////////////////////////////////////////
///
/// CArgAllow_Symbols --
///
/// Define constraint to describe exactly one symbol.
///
/// Argument to be exactly one symbol from the specified set of symbols.
///
/// Examples:
/// - To allow only symbols 'a', 'b' and 'Z' for argument "MyArg":
///   SetConstraint("MyArg", new CArgAllow_Symbols("abZ"))
/// - To allow only printable symbols (according to "isprint()" from <ctype.h>):
///   SetConstraint("MyArg", new CArgAllow_Symbols(CArgAllow_Symbols::ePrint))

class NCBI_XNCBI_EXPORT CArgAllow_Symbols : public CArgAllow
{
public:
    /// Symbol class for defining sets of characters.
    ///
    /// Symbol character classes patterned after those defined in <ctype.h>.
    enum ESymbolClass {
        // Standard character class from <ctype.h>:  isalpha(), isdigit(), etc.
        eAlnum,  ///< Alphanumeric characters
        eAlpha,  ///< Alphabet characters
        eCntrl,  ///< Control characters
        eDigit,  ///< Digit characters
        eGraph,  ///< Graphical characters
        eLower,  ///< Lowercase characters
        ePrint,  ///< Printable characters
        ePunct,  ///< Punctuation characters
        eSpace,  ///< Space characters
        eUpper,  ///< Uppercase characters
        eXdigit, ///< Hexadecimal characters
        eUser    ///< User defined characters using constructor with string&
    };

    /// Constructor.
    CArgAllow_Symbols(ESymbolClass symbol_class);

    /// Constructor for user defined eUser class.
    CArgAllow_Symbols(const string& symbol_set);

protected:
    /// Verify if specified value is allowed.
    virtual bool Verify(const string& value) const;

    /// Get usage information.
    virtual string GetUsage(void) const;

    /// Protected destructor.
    virtual ~CArgAllow_Symbols(void);

    ESymbolClass m_SymbolClass; ///< Symbol class for constraint
    string       m_SymbolSet;   ///< Use if  m_SymbolClass == eUser
};



/////////////////////////////////////////////////////////////////////////////
///
/// CArgAllow_String --
///
/// Define constraint to describe string argument.
///
/// Argument to be a string containing only allowed symbols.
///
/// Examples:
/// - To allow string containing only symbols 'a', 'b' and 'Z' for arg MyArg:
///   SetConstraint("MyArg", new CArgAllow_String("abZ"))
/// - To allow only numeric symbols (according to "isdigit()" from <ctype.h>):
///   SetConstraint("MyArg", new CArgAllow_String(CArgAllow_String::eDigit))

class NCBI_XNCBI_EXPORT CArgAllow_String : public CArgAllow_Symbols
{
public:
    /// Constructor.
    CArgAllow_String(ESymbolClass symbol_class);

    /// Constructor for user defined eUser class.
    CArgAllow_String(const string& symbol_set);

protected:
    /// Verify if specified value is allowed.
    virtual bool Verify(const string& value) const;

    /// Get usage information.
    virtual string GetUsage(void) const;
};



/////////////////////////////////////////////////////////////////////////////
///
/// CArgAllow_Strings --
///
/// Define constraint to describe set of string values.
///
/// Argument to have only particular string values. Use the Allow() method to
/// add the allowed string values, which can be daisy-chained.
///
/// Examples:
/// - SetConstraint("a", (new CArgAllow_Strings)->
///                  Allow("foo")->Allow("bar")->Allow("etc"))
/// - You can use "operator,()" to shorten the notation:
///   SetConstraint("b", &(*new CArgAllow_Strings, "foo", "bar", "etc"))

class NCBI_XNCBI_EXPORT CArgAllow_Strings : public CArgAllow
{
public:
    /// Constructor.
    CArgAllow_Strings(void);

    /// Add allowed string values.
    CArgAllow_Strings* Allow(const string& value);

    /// Short notation operator for adding allowed string values.
    /// @sa
    ///   Allow()
    CArgAllow_Strings& operator,(const string& value) { return *Allow(value); }

protected:
    /// Verify if specified value is allowed.
    virtual bool   Verify(const string& value) const;

    /// Get usage information.
    virtual string GetUsage(void) const;

    /// Protected destructor.
    virtual ~CArgAllow_Strings(void);
private:
    set<string> m_Strings;  ///< Set of allowed string values
};



/////////////////////////////////////////////////////////////////////////////
///
/// CArgAllow_Int8s --
///
/// Define constraint to describe range of 8-byte integer values.
///
/// Argument to have only integer values falling within given interval.
///
/// Example:
/// - SetConstraint("a2", new CArgAllow_Int8s(-1001, 123456789012))

class NCBI_XNCBI_EXPORT CArgAllow_Int8s : public CArgAllow
{
public:
    /// Constructor specifying range of allowed integer values.
    CArgAllow_Int8s(Int8 x_min, Int8 x_max);

protected:
    /// Verify if specified value is allowed.
    virtual bool   Verify(const string& value) const;

    /// Get usage information.
    virtual string GetUsage(void) const;

private:
    Int8 m_Min;  ///< Minimum value of range
    Int8 m_Max;  ///< Maximum value of range 
};



/////////////////////////////////////////////////////////////////////////////
///
/// CArgAllow_Integers --
///
/// Define constraint to describe range of integer values.
///
/// Argument to have only integer values falling within given interval.
///
/// Example:
/// - SetConstraint("i", new CArgAllow_Integers(kMin_Int, 34))

class NCBI_XNCBI_EXPORT CArgAllow_Integers : public CArgAllow_Int8s
{
public:
    /// Constructor specifying range of allowed integer values.
    CArgAllow_Integers(int x_min, int x_max);
};



/////////////////////////////////////////////////////////////////////////////
///
/// CArgAllow_Doubles --
///
/// Define constraint to describe range of double values.
///
/// Argument to have only double values falling within given interval.
///
/// Example:
/// - SetConstraint("d", new CArgAllow_Doubles(0.01, 0.99))

class NCBI_XNCBI_EXPORT CArgAllow_Doubles : public CArgAllow
{
public:
    /// Constructor specifying range of allowed double values.
    CArgAllow_Doubles(double x_min, double x_max);

protected:
    /// Verify if specified value is allowed.
    virtual bool   Verify(const string& value) const;

    /// Get usage information.
    virtual string GetUsage(void) const;

private:
    double m_Min;   ///< Minimum value of range
    double m_Max;   ///< Maximum value of range 
};



/////////////////////////////////////////////////////////////////////////////
///
/// CArgDesc --
///
/// Base class for the description of various types of argument.
///
/// This was a pre-declaration; in MSVC, a predeclaration here causes a heap
/// corruption on termination because this class's virtual destructor isn't
/// defined at the moment the compiler instantiates the destructor of
/// AutoPtr<CArgDesc>.

class NCBI_XNCBI_EXPORT CArgDesc
{
public:
    /// Constructor.
    CArgDesc(const string& name,    ///< Argument name
             const string& comment  ///< Argument description
            );

    /// Destructor.
    virtual ~CArgDesc(void);

    /// Get argument name.
    const string& GetName   (void) const { return m_Name; }

    /// Get arument description.
    const string& GetComment(void) const { return m_Comment; }

    /// Get usage synopis.
    virtual string GetUsageSynopsis(bool name_only = false) const = 0;

    /// Get usage comment attribute.
    virtual string GetUsageCommentAttr(void) const = 0;

    /// Process argument with specified value.
    virtual CArgValue* ProcessArgument(const string& value) const = 0;

    /// Process argument default.
    virtual CArgValue* ProcessDefault(void) const = 0;

    /// Verify argument default value.
    virtual void VerifyDefault (void) const;

    /// Set argument constraint.
    virtual 
    void SetConstraint(CArgAllow*                           constraint,
                       CArgDescriptions::EConstraintNegate  negate 
                                    = CArgDescriptions::eConstraint);

    /// Returns TRUE if associated contraint is inverted (NOT)
    /// @sa SetConstraint
    virtual bool IsConstraintInverted() const { return false; }

    /// Get argument constraint.
    virtual const CArgAllow* GetConstraint(void) const;

    /// Get usage constraint.
    string GetUsageConstraint(void) const;

private:
    string m_Name;      ///< Argument name
    string m_Comment;   ///< Argument description
};


END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.43  2005/02/11 16:02:22  gouriano
 * Distinguish short and detailed help message
 *
 * Revision 1.42  2005/01/24 17:04:28  vasilche
 * Safe boolean operators.
 *
 * Revision 1.41  2004/12/15 15:30:10  kuznets
 * Implemented constraint invertion (NOT)
 *
 * Revision 1.40  2004/12/07 12:19:12  kuznets
 * Fixed warning
 *
 * Revision 1.39  2004/12/03 14:29:51  kuznets
 * Introduced new argument flag fAllowMultiple
 *
 * Revision 1.38  2004/12/02 14:23:59  kuznets
 * Implement support of multiple key arguments (list of values)
 *
 * Revision 1.37  2004/12/01 13:48:03  kuznets
 * Changes to make CGI parameters available as arguments
 *
 * Revision 1.36  2004/08/19 13:01:51  dicuccio
 * Dropped unnecessary export specifier on exceptions
 *
 * Revision 1.35  2004/08/17 14:33:53  dicuccio
 * Export CArgDesc
 *
 * Revision 1.34  2004/07/22 15:26:09  vakatov
 * Allow "Int8" arguments
 *
 * Revision 1.33  2003/07/30 16:14:01  siyan
 * Added explicit documentation for operators () and !()
 *
 * Revision 1.32  2003/07/24 11:48:02  siyan
 * Made @sa text indentation consistent.
 *
 * Revision 1.31  2003/07/10 14:59:09  siyan
 * Documentation changes.
 *
 * Revision 1.30  2003/05/28 18:00:11  kuznets
 * CArgDescription::PrintUsage declared virtual to enable custom help screens
 *
 * Revision 1.29  2003/05/16 16:00:39  vakatov
 * + CArgs::IsEmpty()
 * + CArgDescriptions::PrintUsageIfNoArgs()
 *
 * Revision 1.28  2003/03/31 13:36:39  siyan
 * Added doxygen support
 *
 * Revision 1.27  2003/02/10 18:06:15  kuznets
 * Fixed problem with mandatory extra args
 *
 * Revision 1.26  2002/12/26 12:51:41  dicuccio
 * Fixed some minor niggling errors with export specifiers in the wrong places.
 *
 * Revision 1.25  2002/12/18 22:53:21  dicuccio
 * Added export specifier for building DLLs in windows.  Added global list of
 * all such specifiers in mswin_exports.hpp, included through ncbistl.hpp
 *
 * Revision 1.24  2002/07/15 18:17:50  gouriano
 * renamed CNcbiException and its descendents
 *
 * Revision 1.23  2002/07/11 14:17:53  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 1.22  2002/04/24 04:02:43  vakatov
 * Do not use #NO_INCLASS_TMPL anymore -- apparently all modern
 * compilers seem to be supporting in-class template methods.
 *
 * Revision 1.21  2002/04/11 20:39:16  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.20  2001/05/17 14:50:34  lavr
 * Typos corrected
 *
 * Revision 1.19  2001/03/16 16:39:13  vakatov
 * + <corelib/ncbi_limits.h>
 *
 * Revision 1.18  2001/01/22 23:07:12  vakatov
 * CArgValue::AsInteger() to return "int" (rather than "long")
 *
 * Revision 1.17  2000/12/24 00:12:59  vakatov
 * Radically revamped NCBIARGS.
 * Introduced optional key and posit. args without default value.
 * Added new arg.value constraint classes.
 * Passed flags to be detected by HasValue() rather than AsBoolean.
 * Simplified constraints on the number of mandatory and optional extra args.
 * Improved USAGE info and diagnostic messages. Etc...
 *
 * Revision 1.15  2000/11/29 00:07:25  vakatov
 * Flag and key args not to be sorted in alphabetical order by default; see
 * "usage_sort_args" in SetUsageContext().
 *
 * Revision 1.14  2000/11/24 23:28:31  vakatov
 * CArgValue::  added CloseFile()
 * CArgValue::  get rid of "change_mode" feature in AsInput/OutputFile()
 *
 * Revision 1.13  2000/11/22 22:04:29  vakatov
 * Added special flag "-h" and special exception CArgHelpException to
 * force USAGE printout in a standard manner
 *
 * Revision 1.12  2000/11/17 22:04:28  vakatov
 * CArgDescriptions::  Switch the order of optional args in methods
 * AddOptionalKey() and AddPlain(). Also, enforce the default value to
 * match arg. description (and constraints, if any) at all times.
 *
 * Revision 1.11  2000/11/13 20:31:05  vakatov
 * Wrote new test, fixed multiple bugs, ugly "features", and the USAGE.
 *
 * Revision 1.10  2000/10/20 22:23:26  vakatov
 * CArgAllow_Strings customization;  MSVC++ fixes;  better diagnostic messages
 *
 * Revision 1.9  2000/10/20 20:25:53  vakatov
 * Redesigned/reimplemented the user-defined arg.value constraints
 * mechanism (CArgAllow-related classes and methods). +Generic clean-up.
 *
 * Revision 1.8  2000/10/06 21:57:51  butanaev
 * Added Allow() function. Added classes CArgAllowValue, CArgAllowIntInterval.
 *
 * Revision 1.7  2000/09/29 17:11:22  butanaev
 * Got rid of IsDefaultValue(), added IsProvided().
 *
 * Revision 1.6  2000/09/28 21:01:58  butanaev
 * fPreOpen with opposite meaning took over fDelayOpen.
 * IsDefaultValue() added which returns true if no
 * value for an optional argument was provided in cmd. line.
 *
 * Revision 1.5  2000/09/22 21:27:13  butanaev
 * Fixed buf in handling default arg values.
 *
 * Revision 1.4  2000/09/19 21:18:11  butanaev
 * Added possibility to change file open mode on the fly
 *
 * Revision 1.3  2000/09/18 19:38:59  vasilche
 * Added CreateArgs() from CNcbiArguments.
 *
 * Revision 1.2  2000/09/06 18:56:56  butanaev
 * Added stdin, stdout support. Fixed bug in PrintOut.
 *
 * Revision 1.1  2000/08/31 23:54:47  vakatov
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* NCBIARGS__HPP */
