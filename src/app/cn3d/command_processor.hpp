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
* Authors:  Paul Thiessen
*
* File Description:
*       module to process external commands (e.g. from file messenger)
*
* ===========================================================================
*/

#ifndef COMMAND_PROCESSOR__HPP
#define COMMAND_PROCESSOR__HPP

#include <corelib/ncbistd.hpp>

#include <string>

#include "file_messaging.hpp"


BEGIN_SCOPE(Cn3D)

// utility macros for setting up command processing functions

// data in and out
#define DECLARE_PARAMS \
        const std::string& command, const std::string& dataIn, \
        ncbi::MessageResponder::ReplyStatus *status, std::string *dataOut

#define PASS_PARAMS command, dataIn, status, dataOut

// implementation of functions to handle commands
#define DECLARE_COMMAND_FUNCTION(name) \
    void DoCommand_##name ( DECLARE_PARAMS )

#define IMPLEMENT_COMMAND_FUNCTION(name) \
    void CommandProcessor::DoCommand_##name ( DECLARE_PARAMS )

#define PROCESS_IF_COMMAND_IS(name) \
    do { \
        if (command == #name ) { \
            DoCommand_##name ( PASS_PARAMS ); \
            return; \
        } \
    } while (0)

#define ADD_REPLY_ERROR(msg) \
    do { \
        *status = MessageResponder::REPLY_ERROR; \
        *dataOut += msg; \
        *dataOut += '\n'; \
    } while (0)


class StructureWindow;

class CommandProcessor
{
private:
    StructureWindow *structureWindow;

public:
    CommandProcessor(StructureWindow *sw) : structureWindow(sw) { }
    ~CommandProcessor(void) { }

    // main command interface: pass in a command, and get an immediate reply
    void ProcessCommand(DECLARE_PARAMS);

    // command functions
    DECLARE_COMMAND_FUNCTION(Highlight);
    DECLARE_COMMAND_FUNCTION(LoadFile);
    DECLARE_COMMAND_FUNCTION(Exit);
};

END_SCOPE(Cn3D)

#endif // COMMAND_PROCESSOR__HPP

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.5  2004/02/19 17:04:53  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.4  2003/09/02 19:34:52  thiessen
* implement Exit message
*
* Revision 1.3  2003/07/10 13:47:22  thiessen
* add LoadFile command
*
* Revision 1.2  2003/03/20 20:33:51  thiessen
* implement Highlight command
*
* Revision 1.1  2003/03/14 19:23:06  thiessen
* add CommandProcessor to handle file-message commands; fixes for GCC 2.9
*
*/

