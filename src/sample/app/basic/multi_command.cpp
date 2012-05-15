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
 * Authors:  Denis Vakatov, Andrei Gourianov, David McElhany
 *
 * File Description:
 *
 *  --- General ---
 *
 *  This program demonstrates how to set up and process command-line arguments
 *  for programs that support "command-based" command lines instead of, or in
 *  addition to, a "command-less" command line.
 *   -  A "command-based" command line begins with a "command" (a case-sensitive
 *      keyword), typically followed by other arguments. See the description of
 *      the "post" command below.
 *   -  A "command-less" command line doesn't contain such "commands".
 *   -  Programs that support command-based command lines can support any number
 *      of commands (each with its own set of supported arguments), and may
 *      optionally support a command-less command line as well.
 *
 *  To demonstrate setting up and processing command-based usage forms in the
 *  most clear and concise way, this program does not demonstrate the basics of
 *  command-less command-line argument processing. If you'd like to see how to
 *  set up optional and mandatory flags, keys, positional arguments, etc.,
 *  please see basic_sample.cpp.
 *
 *  It is assumed that the reader is familiar with the NCBI application
 *  framework class CNcbiApplication and with auto_ptr.
 *
 *  --- Program Invocation and Supported Features ---
 *
 *  Here are the command-based usage forms this program supports:
 *      multi_command list
 *      multi_command create <queue>
 *      multi_command post   <queue> [-imp importance] <message>
 *      multi_command query  [queue]
 *
 *  Here are some examples of possible command-based invocations:
 *      multi_command create myqueue-101
 *      multi_command list
 *      multi_command post   myqueue-101 -imp URGENT "Stop all operations!"
 *      multi_command query
 *
 *  With the "list" command, no additional arguments are necessary.
 *  With "create", the "queue" positional argument is required.
 *  With "query", the "queue" positional argument is optional.
 *
 *  The "post" command illustrates a new feature introduced with command-based
 *  command lines - opening arguments. Opening arguments are essentially
 *  identical to mandatory positional arguments except that opening arguments
 *  must precede optional arguments whereas mandatory positional arguments
 *  must follow them. Thus, opening arguments allow usage forms such as the
 *  "post" command in this program, which has an optional argument between
 *  mandatory arguments.
 *
 *  In addition to the commands listed above, this program also supports this
 *  this command-less usage form:
 *      multi_command [-v] <script_file>
 *
 *  Here is an example invocation using a command-less command line:
 *      multi_command -v routine_maint.cmds
 *
 *  --- Required Steps to Process Command-Based Command-Lines ---
 *
 *  At a high level, setting up a program to support a command-less command-line
 *  requires creating a CArgDescriptions object, adding argument descriptions
 *  to it, and passing it to SetupArgDescriptions().
 *
 *  Setting up a program to support command-based command lines is similar, but
 *  requires a CCommandArgDescriptions instead. The CCommandArgDescriptions
 *  class is derived from CArgDescriptions, so all the same functionality is
 *  available; however, the AddCommand() method of CCommandArgDescriptions
 *  allows you to create multiple CArgDescriptions objects (one for each
 *  command) in addition to the overall program description. Other command-
 *  specific features are also provided, such as command grouping and
 *  opening arguments.
 *
 *  Programs that support command-based command lines must execute these steps:
 *   1. Create a command descriptions object (class CCommandArgDescriptions)
 *      for the overall program description.
 *   2. Create argument descriptions objects (class CArgDescriptions) for each
 *      command.
 *   3. Add the actual argument descriptions to the argument descriptions
 *      objects using methods such as AddOpening(), AddPositional(), etc.
 *   4. Add each argument descriptions object to the overall command
 *      descriptions object.
 *   5. Determine which command was specified on the command line.
 *   6. Process the appropriate arguments for the given command.
 *
 *  --- Program Organization ---
 *
 *  The first four required steps above could be done in one long sequence of
 *  statements in the application's Init() function. However, this could become
 *  unwieldy for programs with many commands. Factoring those steps into
 *  dedicated "Init" functions for each command keeps the application's Init()
 *  function readable.
 *
 *  Similarly, the sixth required step could be implemented as an "if-else-if"
 *  type construct in the application's Run() function, with a branch for each
 *  command. However, a cleaner approach is to create a dedicated "Run"
 *  function for each command.
 *
 *  This program is arranged in essentially three sections (four if you include
 *  this rather long introductory comment):
 *   1. Command-specific "Init" and "Run" functions. This section comes first
 *      because pointers to these functions are used in the next section. You
 *      may want to read this section last.
 *   2. Constructs for simplifying the program - the command "metadata".
 *   3. The application's class definition.
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>

#include <common/test_assert.h>  /* This header must go last, if used at all */

USING_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
//  "Init" and "Run" functions for each command
//
//  The "Init" functions add the command-specific argument descriptions.
//  These descriptions are used when parsing the command line and when
//  generating the USAGE statement.
//
//  The "Run" functions use the arguments set up by the corresponding "Init"
//  functions to execute the command-specific functionality.


//  Note: The "list" command doesn't have any additional arguments, so it
//  doesn't have an "Init" function.

static int s_RunList(const CArgs & args, ostream & os)
{
    // The "list" command might print a list of queue names.
    os << "Here is the list of queues: ..." << endl;
    return 0;
}


static void s_InitCreate(CArgDescriptions & arg_desc)
{
    // The "create" command requires the "queue" argument. Here it is being
    // added as an opening argument - a mandatory argument preceding any
    // other arguments.
    arg_desc.AddOpening("queue", "Queue name", CArgDescriptions::eString);

    // Note that because the "queue" argument isn't followed by any optional
    // arguments, it could just as well have been added as a positional
    // argument:
    //arg_desc.AddPositional("queue", "Queue name", CArgDescriptions::eString);
}

static int s_RunCreate(const CArgs & args, ostream & os)
{
    // The "create" command might create a queue with a given name.
    os << "Creating queue '" << args["queue"].AsString() << "'." << endl;
    return 0;
}


static void s_InitPost(CArgDescriptions & arg_desc)
{
    // The "post" command requires the "queue" opening argument.
    // AddOpening must be used in this case because the queue argument precedes
    // a non-positional argument.
    arg_desc.AddOpening("queue", "Queue name", CArgDescriptions::eString);

    // The "post" command allows the "imp" key and requires the "message"
    // positional argument.
    arg_desc.AddOptionalKey("imp", "Importance", "The message importance",
        CArgDescriptions::eString);
    arg_desc.AddPositional("message", "The message to post",
        CArgDescriptions::eString);
}

static int s_RunPost(const CArgs & args, ostream & os)
{
    // The "post" command might send a message to a queue, with an optional
    // importance value.
    os  << "Sending message '" << args["message"].AsString()
        << "' to queue '" << args["queue"].AsString() << "'";
    if ( args["imp"].HasValue() ) {
        os << " with importance '" << args["imp"].AsString() << "'." << endl;
    } else {
        os << " with default importance." << endl;
    }
    return 0;
}


static void s_InitQuery(CArgDescriptions & arg_desc)
{
    // The "query" command allows the "queue" argument. Because it is optional,
    // it must be a positional argument, not an opening argument.
    arg_desc.AddOptionalPositional("queue", "Queue name",
        CArgDescriptions::eString);
}

static int s_RunQuery(const CArgs & args, ostream & os)
{
    // The "query" command might print the messages for a given queue, or for
    // all queues if none was specified.
    if ( args["queue"].HasValue() ) {
        os << "Messages for queue '" << args["queue"].AsString() << "': ..."
            << endl;
    } else {
        os << "Messages for all queues: ..." << endl;
    }
    return 0;
}


// The following "Init" and "Run" functions are for the command-less usage
// form of the program, i.e.  multi_command [-v] <script_file>

static void s_CommandlessInit(CArgDescriptions & arg_desc)
{
    // Example arguments for a command-less usage form:
    arg_desc.AddFlag("v", "Verbose");
    arg_desc.AddPositional("script_file", "A file containing commands",
        CArgDescriptions::eString);
}

static int s_CommandlessRun(const CArgs & args, ostream & os)
{
    // The command-less usage form would do something with the arguments.
    os << "Executing script file '" << args["script_file"].AsString() << "'."
        << endl;
    if ( args["v"].AsBoolean() ) {
        os << "    Blah, blah, blah..." << endl;
    }
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  Convenience constructs

// Function pointer types for the "Init" and "Run" functions.
typedef void (*FInit)(CArgDescriptions & arg_desc);
typedef int (*FRun)(const CArgs & args, ostream & os);

// A structure to collect all the metadata for a command.
struct SCmd {
    string  m_Name;     // the command's name - really, the command itself
    string  m_Desc;     // its description
    string  m_Alias;    // an alias
    FInit   m_Init;     // the command's "Init" function
    FRun    m_Run;      // the command's "Run" function
};

// Specify the metadata and function pointers for all commands.
//
// The following array of structures collects the basic information about all
// the commands used in this program in one place. Using an array of structures
// like this is not a requirement for processing command-based arguments, but
// it greatly simplifies both the specification of the metadata and the
// application's Init() method. Alternatively, you could repeat the required
// steps - currently in a for loop in Init() - for each command, supplying the
// metadata as needed in each repeated group of statements.

static SCmd s_Cmds[] = {
    //name      description        alias  "Init" function  "Run" function
    { "list",   "list entries",    "lst", NULL,            s_RunList   },
    { "create", "create an entry", "crt", s_InitCreate,    s_RunCreate },
    { "post",   "post an entry",   "pst", s_InitPost,      s_RunPost   },
    { "query",  "select entries",  "qry", s_InitQuery,     s_RunQuery  }
};
static size_t s_NumCmds = sizeof(s_Cmds) / sizeof(s_Cmds[0]);


/////////////////////////////////////////////////////////////////////////////
//  CMultiCommandApplication::

class CMultiCommandApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
};


// Set up the argument descriptions for all commands.
void CMultiCommandApplication::Init(void)
{
    // Create the program's overall command-line argument descriptions class.
    // This will be a container for both the command-less and command-based
    // usage forms. The eCommandOptional enum value lets the user decide whether
    // to use a command-less or command-based command line. If you use the
    // eCommandMandatory enum value, then a command-based command line must
    // be used.
    auto_ptr< CCommandArgDescriptions >     cmd_desc(
        new CCommandArgDescriptions(true, 0,
            CCommandArgDescriptions::eCommandOptional));

    // Specify the overall program USAGE context.
    cmd_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Demo program for command-based command-line "
                              "argument processing.", false);

    // Set up the argument descriptions for the command-less command line.
    s_CommandlessInit(*cmd_desc);

    // The following loop sets up the separate commands.
    for (size_t idx = 0; idx < s_NumCmds; ++idx) {

        // Create a container for this command's arguments. Argument
        // descriptions are allocated on the heap.
        auto_ptr< CArgDescriptions >    arg_desc(new CArgDescriptions());

        // Describe this command (the name parameter is not used for commands).
        // This description will be shown in the USAGE statement.
        arg_desc->SetUsageContext("", s_Cmds[idx].m_Desc);

        // Set up this command's argument descriptions (if it has any) using
        // its "Init" function.
        if ( s_Cmds[idx].m_Init != NULL ) {
            s_Cmds[idx].m_Init(*arg_desc);
        }

        // OPTIONAL: Add this command to a group of commands. Command groups
        // serve the sole purpose of organizing the commands in the USAGE
        // statement.
        //  if ( this_command_should_be_grouped ) {
        //      cmd_desc->SetCurrentCommandGroup(the_group_name);
        //  }

        // Add this command (and its argument descriptions) to the program's
        // overall argument descriptions object, 'cmd_desc'. Ownership is
        // transfered to 'cmd_desc' by calling the auto_ptr's release() method
        // and passing that to 'cmd_desc' via AddCommand().
        cmd_desc->AddCommand(s_Cmds[idx].m_Name, arg_desc.release(),
                             s_Cmds[idx].m_Alias);
    }

    // Finalize the setup for all of the program's argument descriptions
    // (also transferring ownership of the argument descriptions).
    //
    // Note: This is also where the supplied command line is checked, and if
    // it doesn't match a valid usage form, an exception will be thrown.
    SetupArgDescriptions(cmd_desc.release());
}


// Process the arguments obtained from the command line.
int CMultiCommandApplication::Run(void)
{
    // Get the specified command (if one is supplied).
    string command(GetArgs().GetCommand());

    // Check for the command-less usage form.
    if ( command == "" ) {
        return s_CommandlessRun(GetArgs(), cout);
    }

    // A command-based usage form was used, so call the command's "Run"
    // function.
    for (size_t idx = 0; idx < s_NumCmds; ++idx) {
        if ( command == s_Cmds[idx].m_Name ) {
            _ASSERT(s_Cmds[idx].m_Run != NULL); // empty "Run" is meaningless
            return s_Cmds[idx].m_Run(GetArgs(), cout);
        }
    }

    // Should never get here. If the command line is not valid, an exception
    // will be thrown in Init(). If it is valid, then it will either result in
    // s_CommandlessRun() or one of the command's "Run" functions being called.
    _ASSERT(false);
    return 1;
}


int main(int argc, const char * argv[])
{
    return CMultiCommandApplication().AppMain(argc, argv);
}
