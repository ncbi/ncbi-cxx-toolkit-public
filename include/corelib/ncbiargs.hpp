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
 * File Description:
 *   Command-line arguments' processing:
 *      descriptions  -- CArgDescriptions,  CArgDesc
 *      constraints   -- CArgAllow;  CArgAllow_{Strings,Integers,Doubles}
 *      parsed values -- CArgs,             CArgValue
 *      exceptions    -- CArgException, ARG_THROW()
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.16  2000/12/12 14:20:13  vasilche
 * Added operator bool to CArgValue.
 * Added standard typedef element_type to CRef<> and CConstRef<>.
 * Macro iterate() now calls method end() only once and uses temporary variable.
 * Various NStr::Compare() methods made faster.
 * Added class Upcase for printing strings to ostream with automatic conversion.
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

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbiobj.hpp>
#include <map>
#include <deque>

BEGIN_NCBI_SCOPE

class CNcbiArguments;
class CArgAllow;


///////////////////////////////////////////////////////
//  Parsing and validating command-line arguments according to
//  the user-provided descriptions
//
//  See also in:  "doc/programming_manual/argdescr.html"


// Command string:
//    progname  {arg_key, arg_key_opt, arg_flag}  {arg_plain}  {arg_extra}
//
// where:
//    arg_key       :=  -<key> <value>     -- (mandatory)
//    arg_key_opt   := [-<key> <value>]    -- (optional)
//    arg_flag      := -<flag>             -- (always optional)
//    arg_plain     := <value> | [value]   -- (dep. on the constraint policy)
//    arg_extra     := <value> | [value]   -- (dep. on the constraint policy)
//
// and:
//    <key> must be followed by <value>
//    <flag> and <key> are case-sensitive, and they can contain
//                     only alphanumeric characters
//    <value> is an arbitrary string (additional constraints can
//            be applied in the argument description, see "EType")
//
// {arg_plain} and {arg_extra} are position dependent arguments, with
// no tag preceeding them. {arg_plain} have individual names and descriptions
// (see method AddPlain), while {arg_extra} have only one description for all
// of them (see method AddExtra). User can apply constraints on the number
// of position arguments (see SetConstraint).



//
// Exception the CArg* code can throw
//

class CArgException : public runtime_error
{
public:
    CArgException(const string& what) THROWS_NONE;
    CArgException(const string& what, const string& arg_value) THROWS_NONE;
};


class CArgHelpException : public CArgException
{
public:
    CArgHelpException(void) THROWS_NONE : CArgException(kEmptyStr) {}
};



//
// Generic abstract base class to access arg.value
//

class CArgValue : public CObject
{
    friend class CArgs;
public:
    // check if value exists
    virtual bool HasValue(void) const;
    operator bool(void) const { return HasValue(); }
    bool operator!(void) const { return !HasValue(); }

    // check if value is default
    bool IsDefaultValue(void) const { return m_IsDefaultValue; }

    // Get the argument's string value.
    // (If it is a value of flag argument, then return one of "true", "false".)
    const string&  AsString(void) const { return m_String; }

    // These functions throw an exception if you requested the wrong
    // value type (e.g. if called "AsInteger()" for a "boolean" arg).
    virtual long   AsInteger(void) const;
    virtual double AsDouble (void) const;
    virtual bool   AsBoolean(void) const;

    virtual CNcbiIstream& AsInputFile (void) const;
    virtual CNcbiOstream& AsOutputFile(void) const;
    virtual void          CloseFile   (void) const;  // safe close and destroy

protected:
    // Prohibit explicit instantiation of "CArg" objects
    CArgValue(const string& value, bool is_default);
    virtual ~CArgValue(void);

private:
    // Value of the argument as passed to the constructor ("value")
    string m_String;
    // TRUE if was not specified by user (i.e. was assigned the default value)
    bool   m_IsDefaultValue;
};



//
// Argument values -- obtained from the raw cmd.-line arguments
// (such like in "CNcbiArguments") and then verified and processed
// according to the arg. descriptions given by the user in "CArgDescriptions".
//
// NOTE:  the extra arguments can be accessed using virtual names:
//           "#1", "#2", "#3", ..., "#<GetNExtra()>"
//        in the order of insertion (by method Add).
//

class CArgs
{
public:
    CArgs(void);
    ~CArgs(void);

    // Return TRUE if there was a description for argument with name "name"
    bool Exist(const string& name) const;
    // Return TRUE if argument with name "name" was provided in command line
    bool IsProvided(const string& name) const;

    // Get value of any argument by its name
    // Throw an exception if such argument does not exist
    const CArgValue& operator [](const string& name) const;

    // Get the number of passed unnamed (extra) position args
    size_t GetNExtra(void) const { return m_nExtra; }
    // Return N-th extra (unnamed) position arg value,  N = 1..GetNExtra()
    const CArgValue& operator [](size_t idx) const;

    // Print (add to the end) all arguments to the string "str". Return "str".
    string& Print(string& str) const;

    // Add new argument name + value.
    // Throw an exception if the "name" is not an empty string, and if
    // there is an argument with this name already.
    // HINT:  use empty "name" to add extra (unnamed) args, and they will be
    // automagically assigned with the virtual names: "#1", "#2", "#3", ...
    void Add(const string& name, CArgValue* arg);

private:
    typedef map< string, CRef<CArgValue> >  TArgs;
    typedef TArgs::iterator                 TArgsI;
    typedef TArgs::const_iterator           TArgsCI;

    TArgs  m_Args;    // assoc.map of arguments' name/value
    size_t m_nExtra;  // cached # of unnamed arguments 
};



//
// Generic abstract base class for all arg. description classes
// (intended for internal use only!)
//

class CArgDesc
{
public:
    virtual ~CArgDesc(void);
    virtual string GetUsageSynopsis   (const string& name,
                                       bool optional=false) const = 0;
    virtual string GetUsageCommentAttr(bool optional=false) const = 0;
    virtual string GetUsageCommentBody(void)                const = 0;
    virtual string GetUsageConstraint(void)                 const = 0;
    virtual CArgValue* ProcessArgument(const string& value,
                                       bool is_default = false) const = 0;
    virtual void SetConstraint(CArgAllow* constraint) = 0;
};



//
// Container to store the command-line argument descriptions.
// Provides the means for the parsing and verification of
// cmd.-line arguments against the contained descriptions, thus
// e.g. translating "CNcbiArguments" ---> "CArgs".
// It can also compose and print out the USAGE info.
//

class CArgDescriptions
{
public:
    // If "auto_help" is passed TRUE, then a special flag "-h"
    // will be added to the list of accepted arguments. Passing "-h" in
    // the command line will printout USAGE and ignore all other passed args.
    CArgDescriptions(bool auto_help = true);
    ~CArgDescriptions(void);

    // Available argument types
    enum EType {
        eString = 0, // an arbitrary string
        eAlnum,      // [a-zA-Z0-9]
        eBoolean,    // {'true', 't', 'false', 'f'},  case-insensitive
        eInteger,    // conversible into an integer number (long)
        eDouble,     // conversible into a floating point number (double)
        eInputFile,  // name of file (must exist and be readable)
        eOutputFile, // name of file (must be writeable)

        // this one is for internal use only:
        k_EType_Size
    };
    // Arg.type names
    static const string& GetTypeName(EType type);

    // Flags (must match the arg.type, or an exception will be thrown)
    enum EFlags {
        // for "eInputFile" and "eOutputFile"
        fPreOpen = 0x1,   // open file right away
        fBinary  = 0x2,   // open as binary file
        // for "eOutputFile" only
        fAppend  = 0x10   // write at the end-of-file, do not truncate
    };
    typedef unsigned int TFlags;  // binary OR of "EFlags"

    /////  arg_key := -<key> <value>,     <key> := "name"
    // Throw an exception if description with name "name" already exists.
    void AddKey(const string& name,
                const string& synopsis,  // must be {alnum, '_'} word
                const string& comment,
                EType         type,
                TFlags        flags = 0);

    /////  arg_key_opt := [-<key> <value>],     <key> := "name"
    // Throw an exception if description with name "name" already exists.
    void AddOptionalKey(const string& name,
                        const string& synopsis,  // must be {alnum, '_'} word
                        const string& comment,
                        EType         type,
                        const string& default_value = kEmptyStr,
                        TFlags        flags         = 0);

    /////  arg_flag  := -<flag>,     <flag> := "name"
    // If the flag is provided (in the command-line), then its value
    // will be set to "set_value"; else it will be set to "!set_value".
    // Throw an exception if description with name "name" already exists.
    void AddFlag(const string& name,
                 const string& comment,
                 bool          set_value = true);

    /////  arg_plain := <value>
    // The order is important! -- the N-th plain argument passed in
    // the cmd.-line will be matched against (and processed according to)
    // the N-th added named plain arg. description.
    // This plain arg. can later be referenced by its name "name".
    // Throw an exception if description with name "name" already exists.
    void AddPlain(const string& name,
                  const string& comment,
                  EType         type,
                  const string& default_value = kEmptyStr,
                  TFlags        flags         = 0);

    // Provide description for the extra position args -- the ones
    // that have not been described by AddPlain().
    // By default, no extra (unnamed) position args are allowed, and you
    // have to call SetConstraint() to enable them.
    // The name of this description is always empty string.
    // Throw an exception if the extra arg. description already exists.
    void AddExtra(const string& comment,  // def = "extra argument"
                  EType         type,
                  TFlags        flags = 0);

    // Restrictions on the # of position args ({arg_plain} and {arg_extra})
    // that can be passed to the program (see method SetConstraint)
    enum EConstraint {
        eAny = 0,      // any # of position cmd.-line arguments can be passed
        // The # of passed args. must be:
        eLessOrEqual,  // less or equal to "num_args"
        eEqual,        // exactly equal to "num_args"
        eMoreOrEqual   // more or equal to "num_args"
    };

    // The default constraint policy is eEqual, and "num_args" is zero.
    // If "num_args" is zero, then the "num_args" will be the # of
    // described (named) position args (i.e. the # of calls to AddPlain).
    void SetConstraint(EConstraint policy, unsigned num_args = 0);

    // Additional (user-defined) restriction on the argument value
    // NOTE: "constraint" must be allocated by "new" and must not be
    //       freed by "delete" after it has been passed to CArgDescriptions!
    void SetConstraint(const string& name, CArgAllow* constraint);

    // Check if there is already a description with name "name".
    bool Exist(const string& name) const;

    // Delete description with name "name".
    // Throw an exception if cannot find one.
    void Delete(const string& name);

    // Set extra info to use by PrintUsage()
    void SetUsageContext(const string& usage_name,
                         const string& usage_description,
                         bool          usage_sort_args = false,
                         SIZE_TYPE     usage_width = 78);

    // Printout (add to the end of) USAGE to the string "str" using
    // provided arg. descriptions and usage context. Return "str".
    string& PrintUsage(string& str) const;

    // Check if the "name" is syntaxically correct: it can contain only
    // alphanumeric characters and underscore ('_')
    static bool VerifyName(const string& name);


private:
    typedef map< string, AutoPtr<CArgDesc> >  TArgs;
    typedef TArgs::iterator                   TArgsI;
    typedef TArgs::const_iterator             TArgsCI;
    typedef /*deque*/vector<string>           TPlainArgs;
    typedef list<string>                      TKeyFlagArgs;

    TArgs        m_Args;        // assoc.map of arguments' name/descr
    TPlainArgs   m_PlainArgs;   // pos. args, ordered by position in cmd.-line
    TKeyFlagArgs m_KeyFlagArgs; // key/flag args, ordered in order of insertion
    EConstraint  m_Constraint;  // policy for the position args
    unsigned     m_ConstrArgs;  // # of args to impose the constraint upon

    // extra USAGE info
    string    m_UsageName;         // program name
    string    m_UsageDescription;  // program description
    bool      m_UsageSortArgs;     // sort alphabetically on usage printout
    SIZE_TYPE m_UsageWidth;        // max. length of a usage line
    bool      m_AutoHelp;          // special flag "-h" activated

    // internal methods
    void x_AddDesc(const string& name, CArgDesc& arg);
    void x_PreCheck(void) const;
    void x_CheckAutoHelp(const string& arg) const;
    bool x_CreateArg(const string& arg1, bool have_arg2, const string& arg2,
                     unsigned* n_plain, CArgs& args) const;
    void x_PostCheck(CArgs& args, unsigned n_plain) const;

public:
    //
    // Parse cmd.-line arguments, and to create "CArgs" out
    // of the passed cmd.-line arguments "argc" and "argv".
    // Throw an exception on error.
    // Throw CArgHelpException if USAGE printout was requested ("-h" flag).
    // NOTE:  you can deallocate the resulting "CArgs" object using 'delete'.
    //
    // Examples:
    // (1) int main(int argc, const char* argv[]) {
    //         CreateArgs(desc, argc, argv);
    //     }
    //
    // (2) CNcbiArguments ncbi_args;
    //     CreateArgs(desc, ncbi_args.Size(), ncbi_args);
    //
#ifdef NO_INCLASS_TMPL
    // (this is temporary, until we abandon Sun WorkShop 5.0 compiler)
    CArgs* CreateArgs(int       argc, const char*           argv[]) const;
    CArgs* CreateArgs(SIZE_TYPE argc, const CNcbiArguments& argv  ) const;
#else
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
        unsigned n_plain = 0;
        for (TSize i = 1;  i < argc;  i++) {
            bool have_arg2 = (i + 1 < argc);
            if ( x_CreateArg(argv[i], have_arg2,
                             have_arg2 ? (string)argv[i+1] : kEmptyStr,
                             &n_plain, *args) )
                i++;
        }
        // Post-processing consistency checks
        x_PostCheck(*args, n_plain);
        return args.release();
    }
#endif /* NO_INCLASS_TMPL */

    CArgs* CreateArgs(const CNcbiArguments& argv) const;
};



///////////////////////////////////////////////////////
// Classes to describe additional (user-defined) constraints
// to impose upon the argument value.
//


// Abstract base class
//

class CArgAllow : public CObject
{
public:
    virtual bool   Verify(const string &value) const = 0;
    virtual string GetUsage(void) const = 0;
protected:
    // prohibit from the allocation in stack or in the static data segment,
    // and from the explicit deallocation by "delete"
    virtual ~CArgAllow(void);
};



// Allow an argument to have only particular string values.
// Use "Allow()" to add the allowed string values, can daisy-chain it:
//  SetConstraint("a", (new CArgAllow_Strings)->
//                      Allow("foo")->Allow("bar")->Allow("etc"));
// One can also use "operator,()" in order to shorten the notation:
//  SetConstraint("b", &(*new CArgAllow_Strings, "foo", "bar", "etc"));
//

class CArgAllow_Strings : public CArgAllow
{
public:
    CArgAllow_Strings(void);
    CArgAllow_Strings* Allow(const string& value);
    CArgAllow_Strings& operator,(const string& value) { return *Allow(value); }
protected:
    virtual bool   Verify(const string& value) const;
    virtual string GetUsage(void) const;
private:
    set<string> m_Strings;
};



// Allow an argument to have only integer values falling within given interval
// Example:  SetConstraint("a2", new CArgAllow_Integers(-3, 34))
//

class CArgAllow_Integers : public CArgAllow
{
public:
    CArgAllow_Integers(long x_min, long x_max);
protected:
    virtual bool   Verify(const string& value) const;
    virtual string GetUsage(void) const;
private:
    long m_Min;
    long m_Max;
};


// Allow an argument to have only integer values falling within given interval
// Example:  SetConstraint("a2", new CArgAllow_Doubles(0.01, 0.99))
//

class CArgAllow_Doubles : public CArgAllow
{
public:
    CArgAllow_Doubles(double x_min, double x_max);
protected:
    virtual bool   Verify(const string& value) const;
    virtual string GetUsage(void) const;
private:
    double m_Min;
    double m_Max;
};


END_NCBI_SCOPE

#endif  /* NCBIARGS__HPP */
