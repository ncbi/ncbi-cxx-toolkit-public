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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Dmitry Kazimirov
 *
 * File Description: Declaration of the automation processor.
 *
 */

#ifndef AUTOMATION__HPP
#define AUTOMATION__HPP

#include "ns_cmd_impl.hpp"
#include "util.hpp"

BEGIN_NCBI_SCOPE

namespace NAutomation
{

class CAutomationException : public CException
{
public:
    enum EErrCode {
        eInvalidInput,
        eCommandProcessingError,
    };

    virtual const char* GetErrCodeString(void) const override
    {
        switch (GetErrCode()) {
        case eInvalidInput:
            return "eInvalidInput";
        case eCommandProcessingError:
            return "eCommandProcessingError";
        default:
            return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CAutomationException, CException);
};

struct SInputOutput
{
    CJsonIterator input;
    CJsonNode reply;

    SInputOutput(const CJsonNode& message) :
        input(message.Iterate()),
        reply(CJsonNode::NewArrayNode())
    {}
};

class CArgument
{
public:
    CArgument(CJsonIterator& input) :
        m_Value(input.GetNode())
    {}

    CArgument(string name, CJsonNode::ENodeType type) :
        m_Name(name), 
        m_TypeOrDefaultValue(type),
        m_Optional(false)
    {}

    template <typename TValue>
    CArgument(string name, const TValue& value) :
        m_Name(name), 
        m_TypeOrDefaultValue(value),
        m_Optional(true)
    {}

    CJsonNode Help();
    void Exec(const string& name, CJsonIterator& input);
    string Name() const { return m_Name; }
    CJsonNode Value() const { return m_Value; }

    // Shortcuts
    template <typename TInt = Int8>
    TInt   AsInteger() const { return static_cast<TInt>(m_Value.AsInteger()); }
    string AsString()  const { return                   m_Value.AsString();   }
    double AsDouble()  const { return                   m_Value.AsDouble();   }
    bool   AsBoolean() const { return                   m_Value.AsBoolean();  }

private:
    string m_Name;
    CJsonNode m_TypeOrDefaultValue;
    CJsonNode m_Value;
    bool m_Optional;
};

typedef initializer_list<CArgument> TArgsInit;

struct TArguments : vector<CArgument>
{
    TArguments() {}
    TArguments(TArgsInit args) : vector<CArgument>(args) {}

    const CArgument& operator[](const char* name) const
    {
        auto found = [&](const CArgument& arg) { return name == arg.Name(); };
        const auto result = find_if(begin(), end(), found);

        _ASSERT(result != end());
        return *result;
    }
};

struct SCommandImpl;
class CCommand;

typedef vector<CCommand> TCommands;
typedef function<void(const TArguments&, SInputOutput&, void*)> TCommandExecutor;
typedef function<void*(const string&, SInputOutput&, void*)> TCommandChecker;
typedef pair<TCommands, TCommandChecker> TCommandGroup;

class CCommand
{
public:
    CCommand(string name, TCommandExecutor exec, TArgsInit args = TArgsInit());
    CCommand(string name, TCommandExecutor exec, const char * const args);
    CCommand(string name, TCommands commands);
    CCommand(string name, TCommandGroup group);

    CJsonNode Help(CJsonIterator& input);
    void* Check(SInputOutput& io, void* data);
    bool Exec(SInputOutput& io, void* data);

private:
    string m_Name;
    shared_ptr<SCommandImpl> m_Impl;
};

struct SServerAddressToJson : public IExecToJson
{
    SServerAddressToJson(int which_part) : m_WhichPart(which_part) {}

    virtual CJsonNode ExecOn(CNetServer server);

    int m_WhichPart;
};

typedef Int8 TObjectID;

class CAutomationProc;

class CAutomationObject : public CObject
{
public:
    CAutomationObject(CAutomationProc* automation_proc) :
        m_AutomationProc(automation_proc)
    {
    }

    void SetID(TObjectID object_id) {m_Id = object_id;}

    TObjectID GetID() const {return m_Id;}

    virtual const string& GetType() const = 0;

    CAutomationProc* m_AutomationProc;

    template <class TDerived>
    static void* CheckCall(const string& name, SInputOutput& io, void* data);

    template <class TDerived>
    static void ExecNew(const TArguments& args, SInputOutput& io, void* data);

    template <class TClass, void(TClass::*Method)(const TArguments&, SInputOutput& io)>
    static void ExecMethod(const TArguments& args, SInputOutput& io, void* data);

protected:
    TObjectID m_Id;
};

struct SNetServiceBase : public CAutomationObject
{
    using TSelf = SNetServiceBase;

    SNetServiceBase(CAutomationProc* automation_proc) :
        CAutomationObject(automation_proc)
    {
    }

    virtual CNetService GetService() = 0;
    CNetServer GetServer() { return GetService().Iterate().GetServer(); }

    void ExecGetName(const TArguments& args, SInputOutput& io);
    void ExecGetAddress(const TArguments& args, SInputOutput& io);

    static TCommands CallCommands();
};

struct SNetService : public SNetServiceBase
{
    using TSelf = SNetService;

    SNetService(CAutomationProc* automation_proc) :
        SNetServiceBase(automation_proc)
    {
    }

    void ExecServerInfo(const TArguments& args, SInputOutput& io);
    void ExecExec(const TArguments& args, SInputOutput& io);

    static TCommands CallCommands();
};

typedef CRef<CAutomationObject> TAutomationObjectRef;

class IMessageSender
{
public:
    virtual void InputMessage(const CJsonNode&) {}
    virtual void OutputMessage(const CJsonNode&) = 0;

    virtual ~IMessageSender() {}
};

class CAutomationProc
{
public:
    CAutomationProc(IMessageSender* message_sender);

    bool Process(const CJsonNode& message);

    void SendWarning(const string& warn_msg, TAutomationObjectRef source);

    void SendError(const CTempString& error_message);

    TObjectID AddObject(TAutomationObjectRef new_object);

    TAutomationObjectRef ReturnNetCacheServerObject(
            CNetICacheClient::TInstance ic_api,
            CNetServer::TInstance server);

    TAutomationObjectRef ReturnNetScheduleServerObject(
            CNetScheduleAPI::TInstance ns_api,
            CNetServer::TInstance server);

    TAutomationObjectRef ReturnNetStorageServerObject(
            CNetStorageAdmin::TInstance nst_api,
            CNetServer::TInstance server);

    static void ExecAllowXSite(const TArguments& args, SInputOutput& io, void* data);

    TAutomationObjectRef& ObjectIdToRef(TObjectID object_id);

private:
    CJsonNode ProcessMessage(const CJsonNode& message);

    IMessageSender* m_MessageSender;

    vector<TAutomationObjectRef> m_ObjectByIndex;

    CJsonNode m_OKNode;
    CJsonNode m_ErrNode;
    CJsonNode m_WarnNode;

    static void ExecExit(const TArguments& args, SInputOutput& io, void* data);
    static void ExecDel(const TArguments& args, SInputOutput& io, void* data);
    static void ExecVersion(const TArguments& args, SInputOutput& io, void* data);
    static void ExecWhatIs(const TArguments& args, SInputOutput& io, void* data);
    static void ExecEcho(const TArguments& args, SInputOutput& io, void* data);
    static void ExecSleep(const TArguments& args, SInputOutput& io, void* data);
    static void ExecSetContext(const TArguments& args, SInputOutput& io, void* data);

    static TCommands Commands();
    static TCommands CallCommands();
    static TCommands NewCommands();
};

inline TObjectID CAutomationProc::AddObject(TAutomationObjectRef new_object)
{
    TObjectID new_object_id = m_ObjectByIndex.size();
    new_object->SetID(new_object_id);
    m_ObjectByIndex.push_back(new_object);
    return new_object_id;
}

template <class TDerived>
void* CAutomationObject::CheckCall(const string& name, SInputOutput& io, void* data)
{
    _ASSERT(data);

    auto that = static_cast<CAutomationProc*>(data);
    auto& input = io.input;

    if (!input) {
        NCBI_THROW_FMT(CAutomationException, eInvalidInput, name << ": insufficient number of arguments");
    }

    auto object_id = input.GetNode().AsInteger();
    auto& object_ref = that->ObjectIdToRef(object_id);

    if (name != object_ref->GetType()) return nullptr;

    ++input;
    return object_ref.GetPointer();
}

template <class TDerived>
void CAutomationObject::ExecNew(const TArguments& args, SInputOutput& io, void* data)
{
    _ASSERT(data);

    auto that = static_cast<CAutomationProc*>(data);
    auto& reply = io.reply;

    CRef<CAutomationObject> new_object;

    try {
        new_object.Reset(TDerived::Create(args, that));
    }
    catch (CException& e) {
        NCBI_THROW_FMT(CAutomationException, eCommandProcessingError,
                "Error in '" << TDerived::kName << "' constructor: " << e.GetMsg());
    }

    auto id = that->AddObject(new_object);
    reply.AppendInteger(id);
}

template <class TClass, void(TClass::*Method)(const TArguments&, SInputOutput& io)>
void CAutomationObject::ExecMethod(const TArguments& args, SInputOutput& io, void* data)
{
    _ASSERT(data);

    auto that = static_cast<TClass*>(data);
    (that->*Method)(args, io);
}

}

END_NCBI_SCOPE

#endif // AUTOMATION__HPP
