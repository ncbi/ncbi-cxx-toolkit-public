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
*       file-based messaging system
*
* ===========================================================================
*/

#ifdef _MSC_VER
#pragma warning(disable:4018)   // disable signed/unsigned mismatch warning in MSVC
#endif

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbi_system.hpp>
#include <util/stream_utils.hpp>

#include <memory>

#include "file_messaging.hpp"


BEGIN_NCBI_SCOPE

// diagnostic streams
#define TRACEMSG(stream) ERR_POST(Trace << stream)
#define INFOMSG(stream) ERR_POST(Info << stream)
#define WARNINGMSG(stream) ERR_POST(Warning << stream)
#define ERRORMSG(stream) ERR_POST(Error << stream)
#define FATALMSG(stream) ERR_POST(Fatal << stream)


FileMessenger::FileMessenger(FileMessagingManager *parentManager,
    const std::string& messageFilename, MessageResponder *responderObject, bool isReadOnly) :
        manager(parentManager), responder(responderObject),
        messageFile(messageFilename), lockFile(string(messageFilename) + ".lock"),
        lastKnownSize(0), readOnly(isReadOnly)
{
    TRACEMSG("monitoring message file " << messageFilename);
}

static fstream * CreateLock(const CDirEntry& lockFile)
{
    if (lockFile.Exists()) {
        TRACEMSG("unable to establish a lock - lock file exists already");
        return NULL;
    }

    auto_ptr<fstream> lockStream(CFile::CreateTmpFile(lockFile.GetPath(), CFile::eText, CFile::eAllowRead));
    if (lockStream.get() == NULL || !(*lockStream)) {
        TRACEMSG("unable to establish a lock - cannot create lock file");
        return NULL;
    }
    char lockWord[4];
    lockStream->seekg(0);
    if (CStreamUtils::Readsome(*lockStream, lockWord, 4) == 4 &&
            lockWord[0] == 'L' && lockWord[1] == 'O' &&
            lockWord[2] == 'C' && lockWord[3] == 'K') {
        ERRORMSG("lock file opened for writing but apparently already LOCKed!");
        return NULL;
    }
    lockStream->seekg(0);
    lockStream->write("LOCK", 4);
    lockStream->flush();
    TRACEMSG("lock file established: " << lockFile.GetPath());
    return lockStream.release();
}

FileMessenger::~FileMessenger(void)
{
    // sanity check to make sure each command issued received a reply
    bool okay = false;
    CommandOriginators::const_iterator c, ce = commandsSent.end();
    TargetApp2Command::const_iterator a, ae;
    if (commandsSent.size() == repliesReceived.size()) {
        for (c=commandsSent.begin(); c!=ce; ++c) {
            CommandReplies::const_iterator r = repliesReceived.find(c->first);
            if (r == repliesReceived.end())
                break;
            for (a=c->second.begin(), ae=c->second.end(); a!=ae; ++a) {
                if (r->second.find(a->first) == r->second.end())
                    break;
            }
            if (a != ae) break;
        }
        if (c == ce) okay = true;
    }
    if (!okay) WARNINGMSG("FileMessenger: did not receive a reply to all commands sent!");

    // last-minute attempt to write any pending commands to the file
    if (pendingCommands.size() > 0) {
        auto_ptr<fstream> lockStream(CreateLock(lockFile.GetPath()));
        if (lockStream.get() == NULL) {
            int nTries = 1;
            do {
                SleepSec(1);
                lockStream.reset(CreateLock(lockFile));
                ++nTries;
            } while (lockStream.get() == NULL && nTries <= 30);
        }
        if (lockStream.get() != NULL)
            SendPendingCommands();
        else
            ERRORMSG("Timeout occurred when attempting to flush pending commands to file");
    }

    // sanity check to make sure each command received was sent a reply
    okay = false;
    ce = commandsReceived.end();
    if (commandsReceived.size() == repliesSent.size()) {
        for (c=commandsReceived.begin(); c!=ce; ++c) {
            CommandReplies::const_iterator r = repliesSent.find(c->first);
            if (r == repliesSent.end())
                break;
            for (a=c->second.begin(), ae=c->second.end(); a!=ae; ++a) {
                if (r->second.find(a->first) == r->second.end())
                    break;
            }
            if (a != ae) break;
        }
        if (c == ce) okay = true;
    }
    if (!okay) ERRORMSG("FileMessenger: did not send a reply to all commands received!");
}

void FileMessenger::SendCommand(const std::string& targetApp, unsigned long id,
        const std::string& command, const std::string& data)
{
    if (readOnly) {
        WARNINGMSG("command '" << command << "' to " << targetApp
            << " received but not written to read-only message file");
        return;
    }

    // check against record of commands already sent
    CommandOriginators::iterator c = commandsSent.find(id);
    if (c != commandsSent.end() && c->second.find(targetApp) != c->second.end()) {
        ERRORMSG("Already sent command " << id << " to " << targetApp << '!');
        return;
    }
    commandsSent[id][targetApp] = command;

    // create a new CommandInfo on the queue - will actually be sent later
    pendingCommands.resize(pendingCommands.size() + 1);
    pendingCommands.back().to = targetApp;
    pendingCommands.back().id = id;
    pendingCommands.back().command = command;
    pendingCommands.back().data = data;
}

void FileMessenger::SendReply(const std::string& targetApp, unsigned long id,
        MessageResponder::ReplyStatus status, const std::string& data)
{
    // check against record of commands already received and replies already sent
    CommandOriginators::iterator c = commandsReceived.find(id);
    if (c == commandsReceived.end() || c->second.find(targetApp) == c->second.end()) {
        ERRORMSG("Can't reply; have not received command " << id << " from " << targetApp << '!');
        return;
    }
    CommandReplies::iterator r = repliesSent.find(id);
    if (r != repliesSent.end() && r->second.find(targetApp) != r->second.end()) {
        ERRORMSG("Already sent reply " << id << " to " << targetApp << '!');
        return;
    }
    repliesSent[id][targetApp] = status;

    if (readOnly) {
        TRACEMSG("reply " << id << " to " << targetApp
            << " logged but not written to read-only message file");
    } else {
        // create a new CommandInfo on the queue - will actually be sent later
        pendingCommands.resize(pendingCommands.size() + 1);
        pendingCommands.back().to = targetApp;
        pendingCommands.back().id = id;
        switch (status) {
            case MessageResponder::REPLY_OKAY:
                pendingCommands.back().command = "OKAY"; break;
            case MessageResponder::REPLY_ERROR:
                pendingCommands.back().command = "ERROR"; break;
            default:
                ERRORMSG("Unknown reply status " << status << '!');
                pendingCommands.back().command = "ERROR"; break;
        }
        pendingCommands.back().data = data;
    }
}

void FileMessenger::PollMessageFile(void)
{
    // skip all checking if message file doesn't exist or lock file is already present
    if (!messageFile.Exists() || !messageFile.IsFile() || lockFile.Exists())
        return;

    // check to see if we need to read file's contents
    CFile mf(messageFile.GetPath());
    Int8 messageFileSize = mf.GetLength();
    if (messageFileSize < 0) {
        ERRORMSG("Couldn't get message file size!");
        return;
    }
    bool needToRead = (messageFileSize > lastKnownSize);

    // only continue if have new commands to receive, or have pending commands to send
    if (!needToRead && pendingCommands.size() == 0)
        return;

    TRACEMSG("message file: " << messageFile.GetPath());
//    TRACEMSG("current size: " << (long) messageFileSize);
//    TRACEMSG("last known size: " << (long) lastKnownSize);
    if (needToRead) TRACEMSG("message file has grown since last read");
    if (pendingCommands.size() > 0) TRACEMSG("has pending commands to send");

    // since we're going to read or write the file, establish a lock now
    auto_ptr<fstream> lockStream(CreateLock(lockFile));
    if (lockStream.get() == NULL)
        return; // try again later, so program isn't locked during wait

    // first read any new commands from the file
    if (needToRead) ReceiveCommands();

    // then send any pending commands
    if (pendingCommands.size() > 0)
        SendPendingCommands();

    // now update the size stamp to current size so we don't unnecessarily read in any commands just sent
    lastKnownSize = mf.GetLength();
    if (lastKnownSize < 0) {
        ERRORMSG("Couldn't get message file size!");
        lastKnownSize = 0;
    }

    // lock file automatically deleted upon return...
}

static const string COMMAND_END = "### END COMMAND ###";

// returns true if some data was read (up to eol) before eof
static bool ReadSingleLine(CNcbiIfstream& inStream, string *str)
{
    str->erase();
    CT_CHAR_TYPE ch;
    do {
        ch = inStream.get();
        if (inStream.bad() || inStream.fail() || CT_EQ_INT_TYPE(CT_TO_INT_TYPE(ch), CT_EOF))
            return false;
        else if (CT_EQ_INT_TYPE(CT_TO_INT_TYPE(ch), CT_TO_INT_TYPE('\n')))
            break;
        else
            *str += CT_TO_CHAR_TYPE(ch);
    } while (1);
    return true;
}

// must be called only after lock is established!
void FileMessenger::ReceiveCommands(void)
{
    TRACEMSG("receiving commands...");

    auto_ptr<CNcbiIfstream> inStream(new ncbi::CNcbiIfstream(
        messageFile.GetPath().c_str(), IOS_BASE::in));
    if (!(*inStream)) {
        ERRORMSG("cannot open message file for reading!");
        return;
    }

#define GET_EXPECTED_LINE \
    if (!ReadSingleLine(*inStream, &line)) { \
        ERRORMSG("unexpected EOF!"); \
        return; \
    }

#define SKIP_THROUGH_END_OF_COMMAND \
    do { \
        if (!ReadSingleLine(*inStream, &line)) { \
            ERRORMSG("no end-of-command marker found before EOF!"); \
            return; \
        } \
        if (line == COMMAND_END) break; \
    } while (1)

#define GET_ITEM(ident) \
    item = string(ident); \
    if (line.substr(0, item.size()) != item) { \
        ERRORMSG("Line does not begin with expected '" << item << "'!"); \
        return; \
    } \
    item = line.substr(item.size());

    string line, item, from;
    CommandInfo command;
    do {

        // get To: (ignore if not this app)
        if (!ReadSingleLine(*inStream, &line) || line.size() == 0) {
            return;
        }

        GET_ITEM("To: ")
        if (item != manager->applicationName) {
            SKIP_THROUGH_END_OF_COMMAND;
            continue;
        }

        // get From:
        GET_EXPECTED_LINE
        GET_ITEM("From: ")
        from = item;

        // get ID:
        GET_EXPECTED_LINE
        GET_ITEM("ID: ")
        char *endptr;
        command.id = strtoul(item.c_str(), &endptr, 10);
        if (endptr == item.c_str()) {
            ERRORMSG("Bad " << line << '!');
            return;
        }

        // get command or reply
        GET_EXPECTED_LINE
        if (line.substr(0, 9) != "Command: " && line.substr(0, 7) != "Reply: ") {
            ERRORMSG("Line does not begin with expected 'Command: ' or 'Reply: '!");
            return;
        }
        bool isCommand = (line.substr(0, 9) == "Command: ");
        command.command = line.substr(isCommand ? 9 : 7);

        // skip commands/replies already read
        if (isCommand) {
            CommandOriginators::iterator c = commandsReceived.find(command.id);
            if (c != commandsReceived.end() && c->second.find(from) != c->second.end()) {
                SKIP_THROUGH_END_OF_COMMAND;
                continue;
            }
        } else {    // reply
            CommandReplies::iterator r = repliesReceived.find(command.id);
            if (r != repliesReceived.end() && r->second.find(from) != r->second.end()) {
                SKIP_THROUGH_END_OF_COMMAND;
                continue;
            }
        }

        // get data (all lines up to end marker)
        command.data.erase();
        do {
            GET_EXPECTED_LINE
            if (line == COMMAND_END) break;
            command.data += line;
            command.data += '\n';
        } while (1);

        // process new commands/replies
        if (isCommand) {
            commandsReceived[command.id][from] = command.command;
            TRACEMSG("processing command " << command.id << " from " << from << ": " << command.command);
            // command received callback
            responder->ReceivedCommand(from, command.id, command.command, command.data);

        } else {    // reply
            MessageResponder::ReplyStatus status = MessageResponder::REPLY_ERROR;
            if (command.command == "OKAY")
                status = MessageResponder::REPLY_OKAY;
            else if (command.command != "ERROR")
                ERRORMSG("Unknown reply status " << command.command << '!');
            repliesReceived[command.id][from] = status;
            TRACEMSG("processing reply " << command.id << " from " << from);
            // reply received callback
            responder->ReceivedReply(from, command.id, status, command.data);
        }

    } while (1);
}

// must be called only after lock is established!
void FileMessenger::SendPendingCommands(void)
{
    TRACEMSG("sending commands...");
    if (pendingCommands.size() == 0)
        return;

    auto_ptr<CNcbiOfstream> outStream(new ncbi::CNcbiOfstream(
        messageFile.GetPath().c_str(), IOS_BASE::out | IOS_BASE::app));
    if (!(*outStream)) {
        ERRORMSG("cannot open message file for writing!");
        return;
    }

    CommandList::iterator c, ce = pendingCommands.end();
    for (c=pendingCommands.begin(); c!=ce; ++c) {
        // dump the command to the file, noting different syntax for replies
        bool isReply = (c->command == "OKAY" || c->command == "ERROR");
        *outStream
            << "To: " << c->to << '\n'
            << "From: " << manager->applicationName << '\n'
            << "ID: " << c->id << '\n'
            << (isReply ? "Reply: " : "Command: ") << c->command << '\n';
        if (c->data.size() > 0) {
            *outStream << c->data;
            if (c->data[c->data.size() - 1] != '\n')    // append \n if data doesn't end with one
                *outStream << '\n';
        }
        *outStream << COMMAND_END << '\n';
        outStream->flush();
        TRACEMSG("sent " << (isReply ? "reply " : "command ") << c->id << " to " << c->to);
    }
    pendingCommands.clear();
}


FileMessagingManager::FileMessagingManager(const std::string& appName):
    applicationName(appName)
{
}

FileMessagingManager::~FileMessagingManager(void)
{
    FileMessengerList::iterator m, me = messengers.end();
    for (m=messengers.begin(); m!=me; ++m)
        delete *m;
}

FileMessenger * FileMessagingManager::CreateNewFileMessenger(
    const std::string& messageFilename, MessageResponder *responderObject, bool readOnly)
{
    if (!responderObject) {
        ERRORMSG("CreateNewFileMessenger() - got NULL responderObject!");
        return NULL;
    }
    FileMessenger *newMessenger = new FileMessenger(this, messageFilename, responderObject, readOnly);
    messengers.push_back(newMessenger);
    return newMessenger;
}

void FileMessagingManager::DeleteFileMessenger(FileMessenger *messenger)
{
    FileMessengerList::iterator f, fe = messengers.end();
    for (f=messengers.begin(); f!=fe; ++f) {
        if (*f == messenger) {
            delete *f;
            messengers.erase(f);
            return;
        }
    }
    ERRORMSG("DeleteFileMessenger() - given FileMessenger* not created by this FileMessagingManager!");
}

void FileMessagingManager::PollMessageFiles(void)
{
    FileMessengerList::iterator f, fe = messengers.end();
    for (f=messengers.begin(); f!=fe; ++f)
        (*f)->PollMessageFile();
}

END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.11  2004/05/21 21:41:39  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.10  2004/03/15 18:25:36  thiessen
* prefer prefix vs. postfix ++/-- operators
*
* Revision 1.9  2004/02/19 17:04:56  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.8  2003/10/20 23:03:33  thiessen
* send pending commands before messenger is destroyed
*
* Revision 1.7  2003/10/02 18:45:22  thiessen
* make non-reply message warning only
*
* Revision 1.6  2003/09/22 17:33:12  thiessen
* add AlignmentChanged flag; flush message file; check row order of repeats
*
* Revision 1.5  2003/07/10 18:47:29  thiessen
* add CDTree->Select command
*
* Revision 1.4  2003/03/19 14:44:36  thiessen
* fix char/traits problem
*
* Revision 1.3  2003/03/14 19:22:59  thiessen
* add CommandProcessor to handle file-message commands; fixes for GCC 2.9
*
* Revision 1.2  2003/03/13 18:55:04  thiessen
* add messenger destroy function
*
* Revision 1.1  2003/03/13 14:26:18  thiessen
* add file_messaging module; split cn3d_main_wxwin into cn3d_app, cn3d_glcanvas, structure_window, cn3d_tools
*
*/

