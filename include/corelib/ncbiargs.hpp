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
 *      parsed values -- CArgs,             CArgValue
 *      exceptions    -- CArgException,     CArgHelpException
 *      constraints   -- CArgAllow; CArgAllow_{Strings, ..., Integers, Doubles}
 *
 * ---------------------------------------------------------------------------
 * $Log$
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

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbi_limits.h>
#include <memory>
#include <set>
#include <list>
#include <vector>

BEGIN_NCBI_SCOPE


// Some necessary forward declarations
class CNcbiArguments;
class CArgAllow;



///////////////////////////////////////////////////////
//  Parsing and validating command-line arguments according to
//  user-provided descriptions.
//
//  See also in:  "doc/programming_manual/argdescr.html"
//
//
// Command string:
//    progname  {arg_key, arg_key_opt, arg_key_dflt, arg_flag} [--]
//              {arg_pos} {arg_pos_opt, arg_pos_dflt}
//              {arg_extra} {arg_extra_opt}
//
// where:
//    arg_key        :=  -<key> <value>    -- (mandatory)
//    arg_key_opt    := [-<key> <value>]   -- (optional, without default value)
//    arg_key_dflt   := [-<key> <value>]   -- (optional, with default value)
//    arg_flag       := -<flag>            -- (always optional)
//    "--" is an optional delimiter to indicate the beginning of pos. args
//    arg_pos        := <value>            -- (mandatory)
//    arg_pos_opt    := [<value>]          -- (optional, without default value)
//    arg_pos_dflt   := [<value>]          -- (optional, with default value)
//    arg_extra      := <value>            -- (dep. on the constraint policy)
//    arg_extra_opt  := [<value>]          -- (dep. on the constraint policy)
//
// and:
//    <key> must be followed by <value>
//    <flag> and <key> are case-sensitive, and they can contain
//                     only alphanumeric characters
//    <value> is an arbitrary string (additional constraints can
//            be applied in the argument description, see "EType")
//
// {arg_pos***} and {arg_extra***} -- position-dependent arguments, with
// no tag preceding them.
// {arg_pos***} -- have individual names and descriptions (see methods
// AddPositional***).
// {arg_extra***} have one description for all (see method AddExtra).
// User can apply constraints on the number of mandatory and optional
// {arg_extra***} arguments.



//
// Exception the CArg* code can throw
//

class CArgException : public runtime_error
{
public:
    CArgException(const string& what) THROWS_NONE;
    CArgException(const string& what, const string& attr) THROWS_NONE;
    CArgException(const string& name, const string& what, const string& attr)
        THROWS_NONE;
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
public:
    // Get argument name
    const string& GetName(void) const { return m_Name; }

    // Check if the argument holds a value.
    // Argument does not hold value if it was described as optional argument
    // without default value, and if it was not passed a value in the command
    // line.  On attempt to retrieve the value from such "no-value" argument,
    // exception will be thrown.
    virtual bool HasValue(void) const = 0;
    operator bool (void) const { return  HasValue(); }
    bool operator!(void) const { return !HasValue(); }

    // Get the argument's string value.
    // (If it is a value of flag argument, then return one of "true", "false".)
    virtual const string& AsString(void) const = 0;

    // These functions throw an exception if you requested the wrong
    // value type (e.g. if called "AsInteger()" for a "boolean" arg).
    virtual int    AsInteger(void) const = 0;
    virtual double AsDouble (void) const = 0;
    virtual bool   AsBoolean(void) const = 0;

    virtual CNcbiIstream& AsInputFile (void) const = 0;
    virtual CNcbiOstream& AsOutputFile(void) const = 0;
    virtual void          CloseFile   (void) const = 0; // safe close&destroy

protected:
    friend class CArgs;

    // Prohibit explicit instantiation of "CArg" objects
    CArgValue(const string& name);
    virtual ~CArgValue(void);

    string m_Name;
};



//
// Argument values -- obtained from the raw cmd.-line arguments
// (such like in "CNcbiArguments") and then verified and processed
// according to the arg. descriptions defined by user in "CArgDescriptions".
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

    // Return TRUE if arg "name" was described in the parent CArgDescriptions
    bool Exist(const string& name) const;

    // Get value of any argument by its name.
    // Throw an exception if such argument does not exist;  see Exist() above.
    const CArgValue& operator[] (const string& name) const;

    // Get the number of unnamed positional (a.k.a. extra) args
    size_t GetNExtra(void) const { return m_nExtra; }
    // Return N-th extra arg value,  N = 1..GetNExtra()
    const CArgValue& operator[] (size_t idx) const;

    // Print (add to the end) all arguments to the string "str". Return "str".
    string& Print(string& str) const;

    // Add new argument name + value.
    // Throw an exception if the "name" is not an empty string, and if
    // there is an argument with this name already.
    // HINT:  use empty "name" to add extra (unnamed) args, and they will be
    // automagically assigned with the virtual names: "#1", "#2", "#3", ...
    void Add(CArgValue* arg);

private:
    typedef set< CRef<CArgValue> >  TArgs;
    typedef TArgs::iterator         TArgsI;
    typedef TArgs::const_iterator   TArgsCI;

    TArgs  m_Args;    // assoc.map of arguments' name/value
    size_t m_nExtra;  // cached # of unnamed positional arguments 

    // Find arg.value with name "name"
    TArgsCI x_Find(const string& name) const;
};



//
// Base class for the description of various types of argument
//

class CArgDesc;



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
        eBoolean,    // {'true', 't', 'false', 'f'},  case-insensitive
        eInteger,    // convertible into an integer number (int)
        eDouble,     // convertible into a floating point number (double)
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

    //// AddXXX() -- the methods to add argument description.
    // They throw exception (CArgException) if:
    //  - description with name "name" already exists;
    //  - "name"     contains symbols other than {alnum}
    //  - "synopsis" contains symbols other than {alnum, '_'}
    //  - "flags" are inconsistent with "type"
    // Any argument can be later referenced using his (unique) name "name".

    // Mandatory key::   arg_key := -<key> <value>
    void AddKey(const string& name,     // <key>
                const string& synopsis,
                const string& comment,
                EType         type,
                TFlags        flags = 0);

    // Optional key without default value::   arg_key_opt := [-<key> <value>]
    void AddOptionalKey(const string& name,     // <key>
                        const string& synopsis,
                        const string& comment,
                        EType         type,
                        TFlags        flags = 0);

    // Optional key with default value::   arg_key_dflt := [-<key> <value>]
    void AddDefaultKey(const string& name,     // <key>
                       const string& synopsis,
                       const string& comment,
                       EType         type,
                       const string& default_value,
                       TFlags        flags = 0);

    /////  arg_flag  := -<flag>,     <flag> := "name"
    // If "set_value" is TRUE, then:
    //   if the flag is provided (in the command-line), then the resultant
    //   CArgValue::HasValue() will be TRUE; else it will be FALSE.
    // Setting "set_value" to FALSE will reverse the situation.
    // NOTE:  if CArgValue::HasValue() is TRUE, then AsBoolean() always TRUE.
    void AddFlag(const string& name,
                 const string& comment,
                 bool          set_value = true);

    // Mandatory positional::   arg_pos := <value>
    // NOTE:  (for all types of positional arguments)
    // The order is important! -- the N-th positional argument passed in
    // the cmd.-line will be matched against (and processed according to)
    // the N-th added named positional arg. description.
    // NOTE 2:  mandatory positional args always go first.
    void AddPositional(const string& name,
                       const string& comment,
                       EType         type,
                       TFlags        flags = 0);

    // Optional positional, without default value::   arg_pos_opt := [<value>]
    // NOTE:  see NOTE for AddPositional() above.
    void AddOptionalPositional(const string& name,
                               const string& comment,
                               EType         type,
                               TFlags        flags = 0);

    // Optional positional, with default value::   arg_pos_dflt := [<value>]
    // NOTE:  see NOTE for AddPositional() above.
    void AddDefaultPositional(const string& name,
                              const string& comment,
                              EType         type,
                              const string& default_value,
                              TFlags        flags = 0);

    // Provide description for the extra (unnamed positional) args.
    // By default, no extra args are allowed.
    // The name of this description is always empty string.
    // Names of the resulting arg.values will be:  "#1", "#2", ...
    // Throw an exception if the extra arg. description already exists.
    void AddExtra(unsigned      n_mandatory,
                  unsigned      n_optional,
                  const string& comment,
                  EType         type,
                  TFlags        flags = 0);

    // Impose an additional (user-defined) restriction on the argument value.
    // NOTE: "constraint" must be allocated by "new", and it must NOT be
    //       freed by "delete" after it has been passed to CArgDescriptions!
    // See "CArgAllow_***" classes below for some pre-defined constraints.
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

    // Check if the "name" is spelled correctly: it can contain only
    // alphanumeric characters and underscore ('_'), or be empty.
    static bool VerifyName(const string& name, bool extended = false);


private:
    typedef set< AutoPtr<CArgDesc> >  TArgs;
    typedef TArgs::iterator           TArgsI;
    typedef TArgs::const_iterator     TArgsCI;
    typedef /*deque*/vector<string>   TPosArgs;
    typedef list<string>              TKeyFlagArgs;

    TArgs        m_Args;        // assoc.map of arguments' name/descr
    TPosArgs     m_PosArgs;     // pos. args, ordered by position in cmd.-line
    TKeyFlagArgs m_KeyFlagArgs; // key/flag args, ordered in order of insertion
    unsigned     m_nExtra;      // # of mandatory extra args
    unsigned     m_nExtraOpt;   // # of optional  extra args

    // extra USAGE info
    string    m_UsageName;         // program name
    string    m_UsageDescription;  // program description
    bool      m_UsageSortArgs;     // sort alphabetically on usage printout
    SIZE_TYPE m_UsageWidth;        // max. length of a usage line
    bool      m_AutoHelp;          // special flag "-h" activated

    // internal methods
    TArgsI  x_Find(const string& name);
    TArgsCI x_Find(const string& name) const;
    void    x_AddDesc(CArgDesc& arg);
    void    x_PreCheck(void) const;
    void    x_CheckAutoHelp(const string& arg) const;
    bool    x_CreateArg(const string& arg1, bool have_arg2, const string& arg2,
                       unsigned* n_plain, CArgs& args) const;
    void    x_PostCheck(CArgs& args, unsigned n_plain) const;

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
        unsigned n_plain = kMax_UInt;
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




/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// Classes to describe additional (user-defined) constraints
// to impose upon the argument value.
//
//  CArgAllow -- abstract base class
//
//  CArgAllow_Symbols  -- symbol from a set of allowed symbols
//  CArgAllow_String   -- string to contain only allowed symbols 
//  CArgAllow_Strings  -- string from a set of allowed strings
//  CArgAllow_Integers -- integer value to fall within a given interval
//  CArgAllow_Doubles  -- floating-point value to fall within a given interval
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



// CArgAllow_Symbols::
// Argument to be exactly one symbol, and of the specified set of symbols.
//
// To allow only symbols 'a', 'b' and 'Z' for argument "MyArg":
//   SetConstraint("MyArg", new CArgAllow_Symbols("abZ"))
// To allow only printable symbols (according to "isprint()" from <ctype.h>):
//   SetConstraint("MyArg", new CArgAllow_Symbols(CArgAllow_Symbols::ePrint))

class CArgAllow_Symbols : public CArgAllow
{
public:
    enum ESymbolClass {
        // Standard character class from <ctype.h>:  isalpha(), isdigit(), etc.
        eAlnum, eAlpha, eCntrl, eDigit, eGraph,
        eLower, ePrint, ePunct, eSpace, eUpper,
        eXdigit,
        // As specified by user (using constructor with "string&")
        eUser
    };
    CArgAllow_Symbols(ESymbolClass symbol_class);
    CArgAllow_Symbols(const string& symbol_set);  // set eUser, use symbol_set
protected:
    virtual bool   Verify(const string& value) const;
    virtual string GetUsage(void) const;
    virtual ~CArgAllow_Symbols(void);

    ESymbolClass m_SymbolClass;
    string       m_SymbolSet;  // to use if  m_SymbolSet == eUser
};



// CArgAllow_String::
// Argument to be a string containing only allowed symbols.
//
// To allow string containing only symbols 'a', 'b' and 'Z' for arg "MyArg":
//   SetConstraint("MyArg", new CArgAllow_String("abZ"))
// To allow only numeric symbols (according to "isdigit()" from <ctype.h>):
//   SetConstraint("MyArg", new CArgAllow_String(CArgAllow_String::eDigit))

class CArgAllow_String : public CArgAllow_Symbols
{
public:
    CArgAllow_String(ESymbolClass symbol_class);
    CArgAllow_String(const string& symbol_set);  // set eUser, use symbol_set
protected:
    virtual bool Verify(const string& value) const;
    virtual string GetUsage(void) const;
};



// CArgAllow_Strings::
// Allow argument to have only particular string values.
//
// Use "Allow()" to add the allowed string values, can daisy-chain it:
//   SetConstraint("a", (new CArgAllow_Strings)->
//                       Allow("foo")->Allow("bar")->Allow("etc"))
// Use "operator,()" to shorten the notation:
//   SetConstraint("b", &(*new CArgAllow_Strings, "foo", "bar", "etc"))

class CArgAllow_Strings : public CArgAllow
{
public:
    CArgAllow_Strings(void);
    CArgAllow_Strings* Allow(const string& value);
    CArgAllow_Strings& operator,(const string& value) { return *Allow(value); }
protected:
    virtual bool   Verify(const string& value) const;
    virtual string GetUsage(void) const;
    virtual ~CArgAllow_Strings(void);
private:
    set<string> m_Strings;
};



// CArgAllow_Integers::
// Allow argument to have only integer values falling within given interval.
//
// Example:  SetConstraint("a2", new CArgAllow_Integers(-3, 34))

class CArgAllow_Integers : public CArgAllow
{
public:
    CArgAllow_Integers(int x_min, int x_max);
protected:
    virtual bool   Verify(const string& value) const;
    virtual string GetUsage(void) const;
private:
    int m_Min;
    int m_Max;
};



// CArgAllow_Doubles::
// Allow argument to have only integer values falling within given interval.
//
// Example:  SetConstraint("a2", new CArgAllow_Doubles(0.01, 0.99))

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
