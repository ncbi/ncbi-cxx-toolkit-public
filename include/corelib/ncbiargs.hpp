#ifndef CORELIB___NCBIARGS__HPP
#define CORELIB___NCBIARGS__HPP

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
 * Author:  Denis Vakatov, Andrei Gourianov
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
class CDir;


/////////////////////////////////////////////////////////////////////////////
///
/// CArgException --
///
/// Define exceptions class for incorrectly formed arguments.
///
/// CArgException inherits its basic functionality from CCoreException
/// and defines additional error codes for malformed arguments.

class NCBI_XNCBI_EXPORT CArgException : public CCoreException
{
public:
    /// Error types for improperly formatted arguments.
    ///
    /// These error conditions are checked for and caught when processing
    /// arguments.
    enum EErrCode {
        eInvalidArg,    ///< Invalid argument
        eNoValue,       ///< Expecting an argument value
        eExcludedValue, ///< The value is excluded by another argument
        eWrongCast,     ///< Incorrect cast for an argument
        eConvert,       ///< Conversion problem
        eNoFile,        ///< Expecting a file
        eConstraint,    ///< Argument value outside constraints
        eArgType,       ///< Wrong argument type
        eNoArg,         ///< No argument
        eSynopsis       ///< Synopsis error
    };

    /// Translate from the error code value to its string representation.
    virtual const char* GetErrCodeString(void) const;

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

class NCBI_XNCBI_EXPORT CArgHelpException : public CArgException
{
public:
    /// Error type for help exception.
    enum EErrCode {
        eHelp,          ///< Error code for short help message
        eHelpFull,      ///< Error code for detailed help message
        eHelpXml        ///< Error code for XML formatted help message
    };

    /// Translate from the error code value to its string representation.
    virtual const char* GetErrCodeString(void) const;

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

    enum EFileFlags {
        fBinary   = (1 <<  1),  ///< Open file in binary mode.
        fText     =  0,         ///< Open file in text mode.
        fAppend   = (1 <<  2),  ///< Open file in append mode.
        fTruncate = (1 << 12),  ///< Open file in truncate mode.
        fNoCreate = (1 << 11),  ///< Open existing file, never create it
        fCreatePath = (1 << 8)  ///< If needed, create directory where the file is located
    };
    typedef unsigned int TFileFlags;   ///< Bitwise OR of "EFileFlags"

    /// Get the argument as an input file stream.
    virtual CNcbiIstream& AsInputFile (TFileFlags flags = 0) const = 0;

    /// Get the argument as an output file stream.
    virtual CNcbiOstream& AsOutputFile(TFileFlags flags = 0) const = 0;

    /// Get the argument as a file stream.
    virtual CNcbiIostream& AsIOFile(TFileFlags flags = 0) const = 0;

    /// Get the argument as a directory.
    virtual const CDir& AsDirectory(void) const = 0;

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
    
    /// Get ordinal position of the value.
    /// NOTE: this is not the position in command line, rather
    /// this reflects the order in which values were added to the list.
    size_t GetOrdinalPosition(void) const
    {
        return m_Ordinal;
    }

protected:
    friend class CArgs;

    /// Protected constructor and destructor.
    ///
    /// Prohibit explicit instantiation of CArgValue with name.
    CArgValue(const string& name);
    virtual ~CArgValue(void);
    
    void SetOrdinalPosition(size_t pos)
    {
        m_Ordinal = pos;
    }

    string m_Name;          ///< Argument name
    size_t m_Ordinal;
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

    /// Get value of argument by name. If the name starts with '-'
    /// (e.g. '-arg') the argument can also be found by 'arg' name if there
    /// is no another argument named 'arg'.
    ///
    /// Throw an exception if such argument does not exist.
    /// @sa
    ///   Exist() above.
    const CArgValue& operator[] (const string& name) const;

    /// Get the number of unnamed positional (a.k.a. extra) args.
    size_t GetNExtra(void) const { return m_nExtra; }

    /// Return N-th extra arg value,  N = 1 to GetNExtra().
    const CArgValue& operator[] (size_t idx) const;

    /// Get all available arguments
    vector< CRef<CArgValue> > GetAll(void) const;

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

    /// Remove argument of name "name"
    void Remove(const string& name);

    /// Remove all arguments
    void Reset(void);

    /// Get current command
    /// @sa CCommandArgDescriptions
    string GetCommand(void) const
    {
        return m_Command;
    }

protected:
    /// Set current command
    /// @sa CCommandArgDescriptions
    CArgs* SetCommand(const string& command)
    {
        m_Command = command;
        return this;
    }

private:
    typedef set< CRef<CArgValue> >  TArgs;   ///< Type for arguments
    typedef TArgs::iterator         TArgsI;  ///< Type for iterator
    typedef TArgs::const_iterator   TArgsCI; ///< Type for const iterator

    TArgs  m_Args;    ///< Assoc. map of arguments' name/value
    size_t m_nExtra;  ///< Cached # of unnamed positional arguments 
    string m_Command;

    /// Find argument value with name "name".
    TArgsCI x_Find(const string& name) const;
    TArgsI  x_Find(const string& name);
    friend class CCommandArgDescriptions;
};



class CArgDesc;


/////////////////////////////////////////////////////////////////////////////
///
/// CArgErrorHandler --
///
/// Base/default error handler for arguments parsing.

class NCBI_XNCBI_EXPORT CArgErrorHandler : public CObject
{
public:
    /// Process invalid argument value. The base implementation returns NULL
    /// or throws exception depending on the CArgDesc flags.
    /// @param arg_desc
    ///   CArgDesc object which failed to initialize.
    /// @param value
    ///   String value which caused the error.
    /// @return
    ///   Return new CArgValue object or NULL if the argument should be
    ///   ignored (as if it has not been set in the command line).
    virtual CArgValue* HandleError(const CArgDesc& arg_desc,
                                   const string& value) const;
};



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
    /// Error handler is used to process errors when parsing arguments.
    /// If not set the default handler is used.
    CArgDescriptions(bool auto_help = true,
                     CArgErrorHandler* err_handler = 0);

    /// Destructor.
    virtual ~CArgDescriptions(void);

    /// Type of CArgDescriptions
    /// For a CGI application positional argumants and flags does not make
    /// sense (this sintax cannot be expressed by CGI protocol)
    enum EArgSetType {
        eRegularArgs,  ///< Regular application
        eCgiArgs       ///< CGI application
    };

    /// Set type of argument description (cmdline vs CGI).
    /// Method performs verification of arguments, 
    /// throws an exception if it finds positional args set for a CGI
    void SetArgsType(EArgSetType args_type);

    EArgSetType GetArgsType() const { return m_ArgsType; }

    /// Processing of positional arguments.
    /// In strict mode any value starting with '-' is treated as a key/flag
    /// unless any positional arguments have already been found (e.g. after
    /// '--' argument). In loose mode any argument is treated as positional
    /// if it can not be processed as a valid key or flag.
    enum EArgPositionalMode {
        ePositionalMode_Strict,  ///< Strict mode (default)
        ePositionalMode_Loose    ///< Loose mode
    };

    /// Select mode for processing positional arguments.
    void SetPositionalMode(EArgPositionalMode positional_mode)
        { m_PositionalMode = positional_mode; }

    EArgPositionalMode GetPositionalMode() const { return m_PositionalMode; }

    /// Available argument types.
    enum EType {
        eString = 0, ///< An arbitrary string
        eBoolean,    ///< {'true', 't', 'false', 'f'},  case-insensitive
        eInt8,       ///< Convertible into an integer number (Int8 only)
        eInteger,    ///< Convertible into an integer number (int or Int8)
        eDouble,     ///< Convertible into a floating point number (double)
        eInputFile,  ///< Name of file (must exist and be readable)
        eOutputFile, ///< Name of file (must be writeable)
        eIOFile,     ///< Name of file (must be writeable)
        eDirectory,  ///< Name of file directory

        k_EType_Size ///< For internal use only
    };

    /// Get argument type's name.
    static const char* GetTypeName(EType type);

    /// Additional flags, the first group is file related flags.
    ///
    /// Must match the argument type, or an exception will be thrown.
    /// ( File related are for eInputFile and eOutputFiler argument types.)
    enum EFlags {
        // File related flags:

        /// Open file right away; for eInputFile, eOutputFile, eIOFile
        fPreOpen = (1 << 0),
        /// Open as binary file; for eInputFile, eOutputFile, eIOFile
        fBinary  = (1 << 1), 
        /// Append to end-of-file; for eOutputFile or eIOFile 
        fAppend    = (1 << 2),
        /// Delete contents of an existing file; for eOutputFile or eIOFile 
        fTruncate  = (1 << 12),
        /// If the file does not exist, do not create it; for eOutputFile or eIOFile 
        fNoCreate = (1 << 11),
        /// If needed, create directory where the file is located
        fCreatePath = (1 << 8),

        /// Mask for all file-related flags
        fFileFlags = fPreOpen | fBinary | fAppend | fTruncate | fNoCreate | fCreatePath,
        // multiple keys flag:

        /// Repeated key arguments are legal (use with AddKey)
        fAllowMultiple = (1 << 3),

        // Error handling flags:

        /// Ignore invalid argument values. If not set, exceptions will be
        /// thrown on invalid values.
        fIgnoreInvalidValue = (1 << 4),
        /// Post warning when an invalid value is ignored (no effect
        /// if fIgnoreInvalidValue is not set).
        fWarnOnInvalidValue = (1 << 5),

        /// Allow to ignore separator between the argument's name and value.
        /// Usual ' ' or '=' separators can still be used with the argument.
        /// The following restrictions apply to a no-separator argument:
        ///   - the argument must be a key (including optional or default);
        ///   - the argument's name must be a single char;
        ///   - no other argument's name can start with the same char,
        ///     unless fOptionalSeparatorAllowConflict is also specified.
        fOptionalSeparator = (1 << 6),
        /// For arguments with fOptionalSeparator flag, allow
        /// other arguments which names begin with the same char.
        fOptionalSeparatorAllowConflict = (1 << 9),
        
        /// Require '=' separator
        fMandatorySeparator = (1 << 7),

        /// Hide it in Usage
        fHidden = (1 << 10)
    };
    typedef unsigned int TFlags;  ///< Bitwise OR of "EFlags"

    /// Add description for mandatory key.
    ///
    /// Mandatory key has the syntax:
    ///
    ///   arg_key := -<key> <value>
    ///
    /// Will throw exception CArgException if:
    ///  - description with name "name" already exists
    ///  - "name" contains symbols other than {alnum, '-', '_'}
    ///  - "name" starts with more than one '-'
    ///  - "synopsis" contains symbols other than {alnum, '_'}
    ///  - "flags" are inconsistent with "type"
    ///
    /// Any argument can be later referenced using its unique name "name".
    void AddKey(const string& name,       ///< Name of argument key
                const string& synopsis,   ///< Synopsis for argument
                const string& comment,    ///< Argument description
                EType         type,       ///< Argument type
                TFlags        flags = 0   ///< Optional flags
               );

    /// Add description for optional key without default value.
    ///
    /// Optional key without default value has the following syntax:
    ///
    ///   arg_key_opt := [-<key> <value>]
    ///
    /// Will throw exception CArgException if:
    ///  - description with name "name" already exists
    ///  - "name" contains symbols other than {alnum, '-', '_'}
    ///  - "name" starts with more than one '-'
    ///  - "synopsis" contains symbols other than {alnum, '_'}
    ///  - "flags" are inconsistent with "type"
    ///
    /// Any argument can be later referenced using its unique name "name".
    void AddOptionalKey(const string& name,     ///< Name of argument key 
                        const string& synopsis, ///< Synopsis for argument
                        const string& comment,  ///< Argument description
                        EType         type,     ///< Argument type
                        TFlags        flags = 0 ///< Optional flags
                       );

    /// Add description for optional key with default value.
    ///
    /// Optional key with default value has the following syntax:
    ///
    ///   arg_key_dflt := [-<key> <value>]
    ///
    /// Will throw exception CArgException if:
    ///  - description with name "name" already exists
    ///  - "name" contains symbols other than {alnum, '-', '_'}
    ///  - "name" starts with more than one '-'
    ///  - "synopsis" contains symbols other than {alnum, '_'}
    ///  - "flags" are inconsistent with "type"
    ///
    /// Any argument can be later referenced using its unique name "name".
    void AddDefaultKey(const string& name,          ///< Name of argument key 
                       const string& synopsis,      ///< Synopsis for argument
                       const string& comment,       ///< Argument description
                       EType         type,          ///< Argument type
                       const string& default_value, ///< Default value
                       TFlags        flags = 0,     ///< Optional flags
    /// Optional name of environment variable that contains default value
                       const string& env_var = kEmptyStr
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


    /// Add description of mandatory opening positional argument.
    ///
    /// Mandatory opening argument has the following syntax:
    ///   arg_pos := <value>
    ///
    /// NOTE:
    ///   In command line, mandatory opening arguments must go first,
    ///   before any other arguments; their order is defined by the order
    ///   in which they were described and added into CArgDescriptions.
    ///
    /// Will throw exception CArgException if:
    ///  - description with name "name" already exists
    ///  - "name" contains symbols other than {alnum, '-', '_'}
    ///  - "name" starts with more than one '-'
    ///  - "flags" are inconsistent with "type"
    ///
    /// Any argument can be later referenced using its unique name "name".
    void AddOpening(const string& name,     ///< Name of argument
                    const string& comment,  ///< Argument description
                    EType         type,     ///< Argument type
                    TFlags        flags = 0 ///< Optional flags
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
    ///  - "name" contains symbols other than {alnum, '-', '_'}
    ///  - "name" starts with more than one '-'
    ///  - "flags" are inconsistent with "type"
    ///
    /// Any argument can be later referenced using its unique name "name".
    void AddPositional(const string& name,     ///< Name of argument
                       const string& comment,  ///< Argument description
                       EType         type,     ///< Argument type
                       TFlags        flags = 0 ///< Optional flags
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
    ///  - "name" contains symbols other than {alnum, '-', '_'}
    ///  - "name" starts with more than one '-'
    ///  - "flags" are inconsistent with "type"
    ///
    /// Any argument can be later referenced using its unique name "name".
    /// @sa
    ///   NOTE for AddPositional()
    void AddOptionalPositional(const string& name,     ///< Name of argument
                               const string& comment,  ///< Argument descr.
                               EType         type,     ///< Argument type
                               TFlags        flags = 0 ///< Optional flags
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
    ///  - "name" contains symbols other than {alnum, '-', '_'}
    ///  - "name" starts with more than one '-'
    ///  - "flags" are inconsistent with "type"
    ///
    /// @sa
    ///   NOTE for AddPositional()
    void AddDefaultPositional(const string& name,   ///< Name of argument
                              const string& comment,///< Argument description
                              EType         type,   ///< Argument type
                              const string& default_value, ///< Default value
                              TFlags        flags = 0, ///< Optional flags
    /// Optional name of environment variable that contains default value
                              const string& env_var = kEmptyStr
                             );

    /// Add description for the extra, unnamed positional arguments.
    ///
    /// The name of this description is always an empty string.
    /// Names of the resulting arg.values will be:  "#1", "#2", ...
    /// By default, no extra args are allowed.
    ///
    /// To allow an unlimited # of optional argumens pass
    /// "n_optional" = kMax_UInt.
    ///
    /// Will throw exception CArgException if:
    ///  - description with name "name" already exists
    ///  - "flags" are inconsistent with "type"
    void AddExtra(unsigned      n_mandatory, ///< Number of mandatory args
                  unsigned      n_optional,  ///< Number of optional args
                  const string& comment,     ///< Argument description
                  EType         type,        ///< Argument type
                  TFlags        flags = 0    ///< Optional flags
                 );

    /// Add argument alias. The alias can be used in the command line instead
    /// of the original argument. Accessing argument value by its alias is
    /// not allowed (will be reported as an unknown argument). The alias will
    /// be printed in USAGE after the original argument name.
    /// @param alias
    ///    New alias for a real argument.
    /// @param arg_name
    ///    The real argument's name.
    void AddAlias(const string& alias, const string& arg_name);
    /// Add negated alias for a flag argument. Using the alias in the
    /// command line produces the same effect as using the original
    /// flag with the opposite value. If 'arg_name' does not describe
    /// a flag argument, an exception is thrown.
    /// @sa
    ///   AddAlias()
    void AddNegatedFlagAlias(const string& alias,
                             const string& arg_name,
                             const string& comment = kEmptyStr);

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
    ///    Constraint object
    /// @param negate
    ///    Flag indicates if this is inverted(NOT) constaint
    /// 
    /// @sa
    ///   See "CArgAllow_***" classes for some pre-defined constraints
    void SetConstraint(const string&      name,
                       CArgAllow*         constraint,
                       EConstraintNegate  negate = eConstraint);

    /// This version of SetConstraint doesn't take the ownership of object
    /// 'constraint'. Rather, it creates and uses a clone of the object.
    void SetConstraint(const string&      name,
                       const CArgAllow&   constraint,
                       EConstraintNegate  negate = eConstraint);

    /// Dependencies between arguments.
    enum EDependency {
        eRequires, ///< One argument requires another
        eExcludes  ///< One argument excludes another
    };

    /// Define a dependency. If arg1 was specified and requires arg2,
    /// arg2 is treated as a mandatory one even if was defined as optional.
    /// If arg1 excludes arg2, arg2 must not be set even if it's mandatory.
    /// This allows to create a set of arguments exactly one of which
    /// must be set.
    void SetDependency(const string& arg1,
                       EDependency   dep,
                       const string& arg2);

    /// Set current arguments group name. When printing descriptions for
    /// optional arguments (on -help command), they will be arranged by
    /// group name. Empty group name resets the group. Arguments without
    /// group are listed first immediately after mandatory arguments.
    void SetCurrentGroup(const string& group);

    /// Set individual error handler for the argument.
    /// The handler overrides the default one (if any) provided through
    /// the constructor. The same handler may be shared by several arguments.
    /// The handler object must be allocated by "new", and it must NOT be
    /// freed by "delete" after it has been passed to CArgDescriptions!
    ///
    /// @param name
    ///    Name of the parameter (flag) to set handler for
    /// @param err_handler
    ///    Error handler
    void SetErrorHandler(const string&      name,
                         CArgErrorHandler*  err_handler);

    /// Check if there is already an argument description with specified name.
    bool Exist(const string& name) const;

    /// Delete description of argument with name "name".
    /// Extra arguments get deleted by the name passed as "".
    ///
    /// Throw the CArgException (eSynopsis error code) exception if the
    /// specified name cannot be found.
    void Delete(const string& name);

    /// Set extra info to be used by PrintUsage().
    /// @sa SetDetailedDescription
    void SetUsageContext(const string& usage_name,           ///< Program name  
                         const string& usage_description,    ///< Usage descr.
                         bool          usage_sort_args = false,///< Sort args.
                         SIZE_TYPE     usage_width = 78);    ///< Format width

    /// Set detailed usage description
    ///
    /// In short help message, program will print short
    /// description defined in SetUsageContext method.
    /// In detailed help message, program will use detailed
    /// description defined here.
    ///
    /// @param usage_description
    ///    Detailed usage description
    /// @sa SetUsageContext
    void SetDetailedDescription( const string& usage_description);

    /// Print usage and exit.
    ///
    /// Force to print USAGE unconditionally (and then exit) if no
    /// command-line args are present.
    /// @deprecated Use SetMiscFlags(fUsageIfNoArgs) instead.
    NCBI_DEPRECATED void PrintUsageIfNoArgs(bool do_print = true);

    /// Miscellaneous flags.
    enum EMiscFlags {
        fNoUsage        = 1 << 0,  ///< Do not print USAGE on argument error.
        fUsageIfNoArgs  = 1 << 1,  ///< Force printing USAGE (and then exit)
                                   ///< if no command line args are present.
        fUsageSortArgs  = 1 << 2,  ///< Sort args when printing USAGE.
        fDupErrToCerr   = 1 << 3,  ///< Print arg error to both log and cerr.

        fMisc_Default   = 0
    };
    typedef int TMiscFlags;  ///< Bitwise OR of "EMiscFlags"

    /// Set the selected flags.
    void SetMiscFlags(TMiscFlags flags)
    {
        m_MiscFlags |= flags;
    }
    
    /// Clear the selected usage flags.
    void ResetMiscFlags(TMiscFlags flags)
    {
        m_MiscFlags &= ~flags;
    }

    /// Check if the flag is set.
    bool IsSetMiscFlag(EMiscFlags flag) const
    {
        return (m_MiscFlags & flag) != 0;
    }

    /// Print usage message to end of specified string.
    ///
    /// Printout USAGE and append to the string "str" using  provided
    /// argument descriptions and usage context.
    /// @return
    ///   Appended "str"
    virtual string& PrintUsage(string& str, bool detailed = false) const;

    /// Print argument description in XML format
    ///
    /// @param out
    ///   Print into this output stream
    virtual void PrintUsageXml(CNcbiOstream& out) const;

    /// Verify if argument "name" is spelled correctly.
    ///
    /// Argument name can contain only alphanumeric characters, dashes ('-')
    /// and underscore ('_'), or be empty. If the leading dash is present,
    /// it must be followed by a non-dash char ('-' or '--foo' are not valid
    /// names).
    static bool VerifyName(const string& name, bool extended = false);

    /// See if special flag "-h" is activated
    bool IsAutoHelpEnabled(void) const
    {
        return m_AutoHelp;
    }
private:
    typedef set< AutoPtr<CArgDesc> >  TArgs;    ///< Argument descr. type
    typedef TArgs::iterator           TArgsI;   ///< Arguments iterator
    typedef TArgs::const_iterator     TArgsCI;  ///< Const arguments iterator
    typedef /*deque*/vector<string>   TPosArgs; ///< Positional arg. vector
    typedef list<string>              TKeyFlagArgs; ///< List of flag arguments
    typedef vector<string>            TArgGroups;   ///< Argument groups

    // Dependencies
    struct SArgDependency
    {
        SArgDependency(const string arg, EDependency dep)
            : m_Arg(arg), m_Dep(dep) {}
        string      m_Arg;
        EDependency m_Dep;
    };
    // Map arguments to their dependencies
    typedef multimap<string, SArgDependency> TDependencies;
    typedef TDependencies::const_iterator TDependency_CI;

private:
    EArgSetType  m_ArgsType;     ///< Type of arguments
    TArgs        m_Args;         ///< Assoc.map of arguments' name/descr
    TPosArgs     m_PosArgs;      ///< Pos. args, ordered by position in cmd.-line
    TPosArgs     m_OpeningArgs;  ///< Opening args, ordered by position in cmd.-line
    TKeyFlagArgs m_KeyFlagArgs;  ///< Key/flag args, in order of insertion
    string       m_NoSeparator;  ///< Arguments allowed to use no separator
    unsigned     m_nExtra;       ///> # of mandatory extra args
    unsigned     m_nExtraOpt;    ///< # of optional  extra args
    TArgGroups   m_ArgGroups;    ///< Argument groups
    size_t       m_CurrentGroup; ///< Currently selected group (0 = no group)
    EArgPositionalMode m_PositionalMode; ///< Processing of positional args
    TDependencies      m_Dependencies;   ///< Arguments' dependencies
    TMiscFlags   m_MiscFlags;    ///< Flags for USAGE, error handling etc.

    // Extra USAGE info
protected:
    string    m_UsageName;         ///< Program name
    string    m_UsageDescription;  ///< Program description
    string    m_DetailedDescription;  ///< Program long description
    SIZE_TYPE m_UsageWidth;        ///< Maximum length of a usage line
    bool      m_AutoHelp;          ///< Special flag "-h" activated

private:

    CRef<CArgErrorHandler> m_ErrorHandler; ///< Global error handler or NULL

    // Internal methods

    void x_PrintAliasesAsXml(CNcbiOstream& out, const string& name,
                                                bool negated=false) const;

    /// Helper method to find named parameter.
    /// 'negative' (if provided) will indicate if the name refered to a
    /// negative alias.
    TArgsI  x_Find(const string& name,
                   bool*         negative = NULL);

    /// Helper method to find named parameter -- const version.
    /// 'negative' (if provided) will indicate if the name refered to a
    /// negative alias.
    TArgsCI x_Find(const string& name,
                   bool*         negative = NULL) const;

    /// Get group index. Returns group index in the m_ArgGroups, 0 for empty
    /// group name or the next group number for undefined group.
    size_t x_GetGroupIndex(const string& group) const;

    /// Helper method for adding description.
    void x_AddDesc(CArgDesc& arg); 

    /// Helper method for doing pre-processing consistency checks.
    void x_PreCheck(void) const; 

    void x_PrintComment(list<string>&   arr,
                        const CArgDesc& arg,
                        SIZE_TYPE       width) const;

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
                     unsigned int  n_plain,
                     CArgs&        args,
                     bool          update = false,
                     CArgValue**   new_value = 0) const;

    /// @sa x_PostCheck()
    enum EPostCheckCaller {
        eCreateArgs,  ///< called by CreateArgs()
        eConvertKeys  ///< called by ConvertKeys()
    };
    /// Helper method for doing post-processing consistency checks.
    void x_PostCheck(CArgs&           args,
                     unsigned int     n_plain,
                     EPostCheckCaller caller)
        const;

    /// Returns TRUE if parameter supports multiple arguments
    bool x_IsMultiArg(const string& name) const;

protected:

    /// Helper method for checking if auto help requested and throw
    /// CArgHelpException if help requested.
    void x_CheckAutoHelp(const string& arg) const;

// PrintUsage helpers
    class CPrintUsage
    {
    public:
        CPrintUsage(const CArgDescriptions& desc);
        ~CPrintUsage();
        void AddSynopsis(list<string>& arr, const string& intro, const string& prefix) const;
        void AddDescription(list<string>& arr, bool detailed) const;
        void AddCommandDescription(list<string>& arr, const string& cmd, 
            const map<string,string>* aliases, size_t max_cmd_len, bool detailed) const;
        void AddDetails(list<string>& arr) const;
    private:
        const CArgDescriptions& m_desc;
        list<const CArgDesc*> m_args;
    };
    class CPrintUsageXml
    {
    public:
        CPrintUsageXml(const CArgDescriptions& desc, CNcbiOstream& out);
        ~CPrintUsageXml();
        void PrintArguments(const CArgDescriptions& desc) const;
    private:
        CNcbiOstream& m_out;
    };

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
        // Check the consistency of argument descriptions
        x_PreCheck();

        // Create new "CArgs" to fill up, and parse cmd.-line args into it
        auto_ptr<CArgs> args(new CArgs());

        // Special case for CGI -- a lone positional argument
        if (GetArgsType() == eCgiArgs  &&  argc == 2) {
            return args.release();
        }

        // Regular case for both CGI and non-CGI
        unsigned int n_plain = kMax_UInt;
        for (TSize i = 1;  i < argc;  i++) {
            bool have_arg2 = (i + 1 < argc);
            if ( x_CreateArg(argv[i], have_arg2,
                             have_arg2 ? (string) argv[i+1] : kEmptyStr,
                             &n_plain, *args) ) {
                i++;
            }
        }

        // Check if there were any arguments at all
        if (n_plain == kMax_UInt) {
            n_plain = 0;
        }

        // Extra checks for the consistency of resultant argument values
        x_PostCheck(*args, n_plain, eCreateArgs);
        return args.release();
    }

    /// Parse command-line arguments 'argv'
    virtual CArgs* CreateArgs(const CNcbiArguments& argv) const;

    /// Convert argument map (key-value pairs) into arguments in accordance
    /// with the argument descriptions
    template<class T>
    void ConvertKeys(CArgs* args, const T& arg_map, bool update) const
    {
        // Check the consistency of argument descriptions
        x_PreCheck();

        // Retrieve the arguments and their values
        ITERATE(TKeyFlagArgs, it, m_KeyFlagArgs) {
            const string& param_name = *it;

            // find first element in the input multimap
            typename T::const_iterator vit  = arg_map.find(param_name);
            typename T::const_iterator vend = arg_map.end();

            if (vit != vend) {   // at least one value found
                CArgValue* new_arg_value;
                x_CreateArg(param_name, param_name, 
                            true, /* value is present */
                            vit->second,
                            1,
                            *args,
                            update,
                            &new_arg_value);

                if (new_arg_value  &&  x_IsMultiArg(param_name)) {
                    CArgValue::TStringArray& varr =
                        new_arg_value->SetStringList();

                    // try to add all additional arguments to arg value
                    for (++vit;  vit != vend;  ++vit) {
                        if (vit->first != param_name)
                            break;
                        varr.push_back(vit->second);
                    }
                }
            }
        } // ITERATE

        // Extra checks for the consistency of resultant argument values
        x_PostCheck(*args, 0, eConvertKeys);
    }
};

/////////////////////////////////////////////////////////////////////////////
///
/// CCommandArgDescriptions --
///
/// Container for several CArgDescriptions objects.
///
/// Sometimes, it is convenient to use a command line tool as follows:
///    tool <command> [options] [args]
/// Here, <command> is an alphanumeric string,
/// and options and arguments are different for each command.
/// With this mechanism, it is possible to describe arguments for
/// each command separately. At run time, argument parser will choose
/// proper CArgDescriptions object based on the value of the first argument.

class NCBI_XNCBI_EXPORT CCommandArgDescriptions : public CArgDescriptions
{
public:

    enum ECommandArgFlags {
        eCommandMandatory = 0,      ///< Nonempty command is required
        eCommandOptional  = 1,      ///< Command is not necessary
        eNoSortCommands   = (1<<1), ///< On PrintUsage, keep commands unsorted
        eNoSortGroups     = (1<<2)  ///< On PrintUsage, keep command groups unsorted
    };
    typedef unsigned int TCommandArgFlags;   ///< Bitwise OR of ECommandArgFlags

    /// Constructor.
    ///
    /// If "auto_help" is passed TRUE, then a special flag "-h" will be added
    /// to the list of accepted arguments. Passing "-h" in the command line
    /// will printout USAGE and ignore all other passed arguments.
    /// Error handler is used to process errors when parsing arguments.
    /// If not set the default handler is used.
    CCommandArgDescriptions(bool auto_help = true,
                          CArgErrorHandler* err_handler = 0,
                          TCommandArgFlags cmd_flags = eCommandMandatory);
    /// Destructor.
    virtual ~CCommandArgDescriptions(void);

    /// Set current command group name. When printing help,
    /// commands will be arranged by group name.
    void SetCurrentCommandGroup(const string& group);

    /// Add command argument descriptions
    ///
    /// @param cmd
    ///    Command string
    /// @param description
    ///    Argument description
    /// @param alias
    ///    Alternative name of the command
    void AddCommand(const string& cmd, CArgDescriptions* description,
                    const string& alias = kEmptyStr);

    /// Parse command-line arguments 'argv'
    virtual CArgs* CreateArgs(const CNcbiArguments& argv) const;

    /// Print usage message to end of specified string.
    ///
    /// Printout USAGE and append to the string "str" using  provided
    /// argument descriptions and usage context.
    /// @return
    ///   Appended "str"
    virtual string& PrintUsage(string& str, bool detailed = false) const;

    /// Print argument description in XML format
    ///
    /// @param out
    ///   Print into this output stream
    virtual void PrintUsageXml(CNcbiOstream& out) const;

private:
    // not allowed
    void SetArgsType(EArgSetType /*args_type*/) { }

    bool x_IsCommandMandatory(void) const;
    size_t x_GetCommandGroupIndex(const string& group) const;
    string x_IdentifyCommand(const string& command) const;

    typedef map<string, AutoPtr<CArgDescriptions> > TDescriptions;
    TCommandArgFlags m_Cmd_req;
    TDescriptions m_Description;    ///< command to ArgDescriptions
    map<string, size_t > m_Groups;  ///< command to group #
    map<string,string> m_Aliases;   ///< command to alias; one alias only
    list<string> m_Commands;        ///< command names, and order
    list<string> m_CmdGroups;       ///< group names, and order
    size_t m_CurrentGroup;          ///< current group #
    mutable string m_Command;       ///< current command
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
///
/// @sa CArgAllow_Regexp

class NCBI_XNCBI_EXPORT CArgAllow : public CObject
{
public:
    /// Verify if specified value is allowed.
    virtual bool Verify(const string &value) const = 0;

    /// Get usage information.
    virtual
    string GetUsage(void) const = 0;

    /// Print constraints in XML format
    virtual void PrintUsageXml(CNcbiOstream& out) const;

    virtual ~CArgAllow(void);

    /// Create object's clone, moving it from stack memory into heap.
    /// The method is required only when using objects created on stack.
    ///
    /// NOTE: Default implementation returns NULL.
    /// Inherited classes must override this method.
    ///
    /// @sa CArgDescriptions::SetConstraint
    virtual CArgAllow* Clone(void) const;

protected:
    // In the absence of the following constructor, new compilers (as required
    // by the new C++ standard) may fill the object memory with zeros,
    // erasing flags set by CObject::operator new (see CXX-1808)
    CArgAllow(void) {}
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

    /// Add allowed symbols
    CArgAllow_Symbols& Allow(ESymbolClass  symbol_class);
    CArgAllow_Symbols& Allow(const string& symbol_set);

protected:
    /// Verify if specified value is allowed.
    virtual bool Verify(const string& value) const;

    /// Get usage information.
    virtual string GetUsage(void) const;

    /// Print constraints in XML format
    virtual void PrintUsageXml(CNcbiOstream& out) const;

    CArgAllow_Symbols(void) {
    }
    virtual CArgAllow* Clone(void) const;

    typedef pair<ESymbolClass, string> TSymClass;
    set< TSymClass > m_SymClass;
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

    /// Print constraints in XML format
    virtual void PrintUsageXml(CNcbiOstream& out) const;

    CArgAllow_String(void) {
    }
    virtual CArgAllow* Clone(void) const;
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
    /// Constructor
    /// @param use_case
    ///   If to ignore the case of the characters
    CArgAllow_Strings(NStr::ECase use_case = NStr::eCase);

    /// Add allowed string values
    /// @param value
    ///   String to add to the set of allowed string values
    CArgAllow_Strings* Allow(const string& value);

    /// Add allowed string values
    /// @param value
    ///   String to add to the set of allowed string values
    CArgAllow_Strings& AllowValue(const string& value);

    /// Short notation operator for adding allowed string values
    /// @param value
    ///   String to add to the set of allowed string values
    /// @sa
    ///   Allow()
    CArgAllow_Strings& operator,(const string& value) {
        return AllowValue(value);
    }

protected:
    /// Verify if specified value is allowed.
    virtual bool Verify(const string& value) const;

    /// Get usage information.
    virtual string GetUsage(void) const;

    /// Print constraints in XML format
    virtual void PrintUsageXml(CNcbiOstream& out) const;

    virtual CArgAllow* Clone(void) const;

    /// Type of the container that contains the allowed string values
    /// @sa m_Strings
    typedef set<string, PNocase_Conditional> TStrings;

    TStrings     m_Strings;  ///< Set of allowed string values
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
    /// Constructor specifying an allowed integer value.
    CArgAllow_Int8s(Int8 x_value);

    /// Constructor specifying range of allowed integer values.
    CArgAllow_Int8s(Int8 x_min, Int8 x_max);

    /// Add allow values
    CArgAllow_Int8s& AllowRange(Int8 from, Int8 to);
    CArgAllow_Int8s& Allow(Int8 value);

protected:
    /// Verify if specified value is allowed.
    virtual bool   Verify(const string& value) const;

    /// Get usage information.
    virtual string GetUsage(void) const;

    /// Print constraints in XML format
    virtual void PrintUsageXml(CNcbiOstream& out) const;

    CArgAllow_Int8s(void) {
    }
    virtual CArgAllow* Clone(void) const;


    typedef pair<Int8, Int8> TInterval;
    set< TInterval >  m_MinMax;
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
    /// Constructor specifying an allowed integer value.
    CArgAllow_Integers(int x_value);
    /// Constructor specifying range of allowed integer values.
    CArgAllow_Integers(int x_min, int x_max);

protected:
    /// Get usage information.
    virtual string GetUsage(void) const;

    CArgAllow_Integers(void) {
    }
    virtual CArgAllow* Clone(void) const;
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
    /// Constructor specifying an allowed integer value.
    CArgAllow_Doubles(double x_value);

    /// Constructor specifying range of allowed double values.
    CArgAllow_Doubles(double x_min, double x_max);

    /// Add allowed values
    CArgAllow_Doubles& AllowRange(double from, double to);
    CArgAllow_Doubles& Allow(double value);

protected:
    /// Verify if specified value is allowed.
    virtual bool   Verify(const string& value) const;

    /// Get usage information.
    virtual string GetUsage(void) const;

    /// Print constraints in XML format
    virtual void PrintUsageXml(CNcbiOstream& out) const;

    CArgAllow_Doubles(void) {
    }
    virtual CArgAllow* Clone(void) const;

    typedef pair<double,double> TInterval;
    set< TInterval >  m_MinMax;
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

    /// Get argument group
    virtual size_t GetGroup(void) const { return 0; }
    /// Set argument group
    virtual void SetGroup(size_t /* group */) {}

    /// Get usage synopsis.
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

    /// Get error handler for the argument
    virtual const CArgErrorHandler* GetErrorHandler(void) const { return 0; }
    /// Set error handler for the argument
    virtual void SetErrorHandler(CArgErrorHandler*) { return; }

    /// Get argument flags
    virtual CArgDescriptions::TFlags GetFlags(void) const { return 0; }

    /// Print description in XML format
    string PrintXml(CNcbiOstream& out) const;

private:
    string m_Name;      ///< Argument name
    string m_Comment;   ///< Argument description
};


END_NCBI_SCOPE


/* @} */

#endif  /* CORELIB___NCBIARGS__HPP */
