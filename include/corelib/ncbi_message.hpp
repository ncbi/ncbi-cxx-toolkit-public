#ifndef CORELIB___NCBI_MESSAGE__HPP
#define CORELIB___NCBI_MESSAGE__HPP

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
 * Authors: Aleksey Grichenko
 *
 * File Description:   IMessage/IMessageListener interfaces
 *
 */

/// @file ncbi_message.hpp
///
/// IMessage/IMessageListener interfaces and basic implementations.
///

#include <corelib/ncbiobj.hpp>
#include <list>
#include <corelib/version.hpp>

/** @addtogroup MESSAGE
 *
 * @{
 */

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
///
/// IMessage::
///
/// Generic message interface to be used with IMessageListener to collect and
/// report error messages and progress.
///

class IMessage
{
public:
    virtual ~IMessage(void) {}

    /// Get text message.
    virtual string GetText(void) const = 0;

    /// Get message severity.
    virtual EDiagSev GetSeverity(void) const = 0;

    /// Get error code. Zero = not set.
    virtual int GetCode(void) const = 0;

    /// Get error subcode. Zero = not set.
    virtual int GetSubCode(void) const = 0;

    /// Create a copy of the message. The caller is responsible for
    /// destroying the copy.
    virtual IMessage* Clone(void) const = 0;

    /// Print the message and any additional information to the stream.
    virtual void Write(CNcbiOstream& /*out*/) const = 0;

    /// Get the whole composed message as string.
    /// The default implementation uses Write() to compose the string.
    virtual string Compose(void) const = 0;
};


inline
ostream& operator<<(CNcbiOstream& out, const IMessage& msg)
{
    msg.Write(out);
    return out;
}


/// Default IMessage implementation: text and severity only.
class NCBI_XNCBI_EXPORT CMessage_Base : public IMessage
{
public:
    CMessage_Base(const string& txt,
                  EDiagSev      sev,
                  int           err_code = 0,
                  int           sub_code = 0);

    virtual string GetText(void) const;
    virtual EDiagSev GetSeverity(void) const;
    virtual int GetCode(void) const;
    virtual int GetSubCode(void) const;
    virtual IMessage* Clone(void) const;
    virtual void Write(CNcbiOstream& out) const;
    virtual string Compose(void) const;

private:
    string   m_Text;
    EDiagSev m_Severity;
    int      m_ErrCode;
    int      m_SubCode;
};



/////////////////////////////////////////////////////////////////////////////
///
/// IMessageListener::
///
/// Interface for IMessage listener/collector.
///

class NCBI_XNCBI_EXPORT IMessageListener : public CObject
{
public:
    virtual ~IMessageListener(void) {}

    /// Result of PostXXXX() operation.
    enum EPostResult {
        eHandled,   ///< The message was successfully handled and will not be
                    ///< passed to other listeners installed with
                    ///< eListen_Unhandled flag.
        eUnhandled  ///< The message was not handled and should be passed to
                    ///< other listeners.
    };

    /// Post new message to the listener.
    virtual EPostResult PostMessage(const IMessage& /*message*/) = 0;

    /// Report progress.
    /// @param message
    ///   Text message explaining the current state.
    /// @param current
    ///   Current progress value.
    /// @param total
    ///   Max progress value.
    virtual EPostResult PostProgress(const string& /*message*/,
                                     Uint8         /*current*/,
                                     Uint8         /*total*/) = 0;

    /// Get a previously collected message.
    /// @param index
    ///   Index of the message, must be less than Count().
    virtual const IMessage& GetMessage(size_t /*index*/) const = 0;

    /// Get total number of collected messages.
    virtual size_t Count(void) const = 0;

    /// Clear all collected messages.
    virtual void Clear(void) = 0;

    /// Which messages should be passed to the listener
    enum EListenFlag {
        eListen_Unhandled, ///< Default flag. The listener only wants messages
                           ///< not handled by previous listeners.
        eListen_All        ///< The listener wants to see all messages, even
                           ///< those already handled.
    };

    /// Push a new listener to the stack in the current thread.
    /// @param listener
    ///   The listener to be installed.
    /// @param flag
    ///   Flag specifying if the new listener should receive all messages
    ///   or only those not handled by other listeners above the current.
    /// @return
    ///   Total number of listeners in the current thread's stack including
    ///   the new one. This number can be passed to PopListener() for more
    ///   reliable cleanup.
    /// @sa PopListener()
    static size_t PushListener(IMessageListener& listener,
                               EListenFlag flag = eListen_Unhandled);

    /// Remove listener(s) from the current thread's stack.
    /// @param depth
    ///   Index of the listener to be removed, as returned by PushListener().
    ///   If the current stack size is less than depth, no action is performed.
    ///   If the depth is zero (default), only the topmost listener is removed.
    /// @sa PushListener()
    static void PopListener(size_t depth = 0);

    /// Post the message to listener(s), if any. The message is posted
    /// to each listener starting from the top of the stack. After a
    /// listener returns eHandled, the message is posted only to the
    /// listeners which were installed with eListen_All flag.
    /// @return
    ///   eHandled if at least one listener has handled the message,
    ///   eUnhandled otherwise.
    /// @sa PostMessage()
    static EPostResult Post(const IMessage& message);

    /// Post the progress to listener(s), if any. The progress is posted
    /// to each listener starting from the top of the stack. After a
    /// listener returns eHandled, the progress is posted only to the
    /// listeners which were installed with eListen_All flag.
    /// @return
    ///   eHandled if at least one listener has handled the event,
    ///   eUnhandled otherwise.
    /// @sa PostProgress()
    static EPostResult Post(const string& message,
                            Uint8         current,
                            Uint8         total);
};


/// Default implementation of IMessageListener: collects all messages
/// posted.
class NCBI_XNCBI_EXPORT CMessageListener_Base : public IMessageListener
{
public:
    virtual EPostResult PostMessage(const IMessage& message);
    virtual EPostResult PostProgress(const string& message,
                                     Uint8         current,
                                     Uint8         total);
    virtual const IMessage& GetMessage(size_t index) const;
    virtual size_t Count(void) const;
    virtual void Clear(void);
private:
    typedef vector< AutoPtr<IMessage> > TMessages;

    TMessages m_Messages;
};


/* @} */

END_NCBI_SCOPE

#endif  /* CORELIB___NCBI_MESSAGE__HPP */
