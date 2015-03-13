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

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/stream_utils.hpp>
#include <serial/serial.hpp>
#include <serial/objostr.hpp>
#include <serial/objostrxml.hpp>
#include <serial/objistr.hpp>

#include <memory>

#include "file_messaging.hpp"


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

// diagnostic streams
#define TRACEMSG(stream) ERR_POST(Trace << stream)
#define INFOMSG(stream) ERR_POST(Info << stream)
#define WARNINGMSG(stream) ERR_POST(Warning << stream)
#define ERRORMSG(stream) ERR_POST(Error << stream)
#define FATALMSG(stream) ERR_FATAL(stream)


FileMessenger::FileMessenger(FileMessagingManager *parentManager,
    const std::string& messageFilename, MessageResponder *responderObject, bool isReadOnly) :
        manager(parentManager),
        messageFile(messageFilename), lockFile(string(messageFilename) + ".lock"),
        responder(responderObject), readOnly(isReadOnly), lastKnownSize(0)
{
    TRACEMSG("monitoring message file " << messageFilename);
}

static CPIDGuard * CreateLock(const CDirEntry& lockFile)
{
    CPIDGuard* guard = NULL;
    try {
        guard = new CPIDGuard(lockFile.GetPath());
        TRACEMSG("FileMessenger:  lock file established: " << lockFile.GetPath());
    } catch (CPIDGuardException& pidge) {

        if (pidge.GetErrCode() == CPIDGuardException::eStillRunning) {
            TRACEMSG("FileMessenger:  unable to establish a lock for new PID - old PID is still running\n" << pidge.ReportThis());
        } else if (pidge.GetErrCode() == CPIDGuardException::eWrite) {
            TRACEMSG("FileMessenger:  write to PID-guarded file failed\n" << pidge.ReportThis());
        } else {
            TRACEMSG("FileMessenger:  unknown Toolkit PID-guard failure\n" << pidge.ReportAll());
        }

        delete guard;  //  calls CPIDGuard::Release()
        guard = NULL;

    } catch (...) {
        TRACEMSG("FileMessenger:  unknown exception while creating lock");
        delete guard;  //  calls CPIDGuard::Release()
        guard = NULL;
    }

    return guard;
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
        auto_ptr<CPIDGuard> lockStream(CreateLock(lockFile));
        if (lockStream.get() == NULL) {
            int nTries = 1;
            do {
                SleepSec(1);
                lockStream.reset(CreateLock(lockFile));
                ++nTries;
            } while (lockStream.get() == NULL && nTries <= 30);
        }
        if (lockStream.get() != NULL) {
            SendPendingCommands();
        } 
        else
            ERRORMSG("Timeout occurred when attempting to flush pending commands to file");

        //  CPIDGuard pointer cleaned up when auto_ptr goes out of scope.
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
    auto_ptr<CPIDGuard> lockStream(CreateLock(lockFile));
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

    //  CPIDGuard pointer cleaned up when auto_ptr goes out of scope.
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

bool SeqIdToIdentifier(const CRef < ncbi::objects::CSeq_id >& seqID, string& identifier)
{
    if (seqID.Empty())
        return false;
    try {
        CNcbiOstrstream oss;
        auto_ptr < CObjectOStream > osa(CObjectOStream::Open(eSerial_Xml, oss, eNoOwnership));
        osa->SetUseIndentation(false);
        CObjectOStreamXml *osx = dynamic_cast<CObjectOStreamXml*>(osa.get());
        if (osx)
            osx->SetReferenceDTD(false);
        *osa << *seqID;
        identifier = CNcbiOstrstreamToString(oss);
        NStr::ReplaceInPlace(identifier, "\n", "");
        NStr::ReplaceInPlace(identifier, "\r", "");
        NStr::ReplaceInPlace(identifier, "\t", "");
        if (NStr::StartsWith(identifier, "<?")) {
            SIZE_TYPE endb = NStr::Find(identifier, "?>");
            identifier = identifier.substr(endb + 2);
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool IdentifierToSeqId(const string& identifier, CRef < ncbi::objects::CSeq_id >& seqID)
{
    try {
        CNcbiIstrstream iss(identifier.data(), identifier.size());
        auto_ptr < CObjectIStream > isa(CObjectIStream::Open(eSerial_Xml, iss, eNoOwnership));
        seqID.Reset(new CSeq_id());
        *isa >> *seqID;
        return true;
    } catch (...) {
        return false;
    }
}

END_NCBI_SCOPE
