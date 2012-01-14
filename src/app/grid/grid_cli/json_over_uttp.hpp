#ifndef UTIL___SUTTP__HPP
#define UTIL___SUTTP__HPP

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
 * Authors: Dmitry Kazimirov
 *
 * File Description: Structured Untyped Token
 *                   Transfer Protocol (declarations).
 *
 */

#include <connect/services/netcomponent.hpp>

#include <connect/ncbi_pipe.hpp>

#include <util/uttp.hpp>
#include <util/util_exception.hpp>

BEGIN_NCBI_SCOPE

struct SJsonNodeImpl;

class CJsonNode
{
    NCBI_NET_COMPONENT(JsonNode);

    typedef map<string, CJsonNode> TObject;
    typedef vector<CJsonNode> TArray;
    typedef Int8 TNumber;

    enum ENodeType {
        eObject,
        eArray,
        eString,
        eNumber,
        eBoolean,
        eNull
    };

    ENodeType GetNodeType() const;

    bool IsObject() const {return GetNodeType() == eObject;}
    bool IsArray() const {return GetNodeType() == eArray;}
    bool IsString() const {return GetNodeType() == eString;}
    bool IsNumber() const {return GetNodeType() == eNumber;}
    bool IsBoolean() const {return GetNodeType() == eBoolean;}
    bool IsNull() const {return GetNodeType() == eNull;}

    static CJsonNode NewArrayNode();

    const TArray& GetArray() const;
    TArray& GetArray();

    void PushNode(CJsonNode::TInstance value);
    void PushString(const string& value);
    void PushNumber(TNumber value);
    void PushBoolean(bool value);
    void PushNull();

    static CJsonNode NewObjectNode();

    const CJsonNode::TObject& GetObject() const;
    CJsonNode::TObject& GetObject();

    void SetNode(const string& key, CJsonNode::TInstance value);
    void SetString(const string& key, const string& value);
    void SetNumber(const string& key, TNumber value);
    void SetBoolean(const string& key, bool value);
    void SetNull(const string& key);

    const string& GetString() const;
    TNumber GetNumber() const;
    bool GetBoolean() const;
};

class CJsonOverUTTPReader
{
public:
    typedef list<CJsonNode> TNodeStack;

    enum EParsingEvent {
        // These two constants indicate success and are only
        // used internally.
        eEndOfMessage,
        eNextBuffer,

        // These constants indicate different parsing error
        // conditions and are used as process return codes.
        eParsingError = 20,
        eUTTPFormatError = 21,
        eChunkContinuationExpected = 22,
        eUnexpectedEOM = 23,
        eExtraCharsPastEOM = 24,
        eHashKeyMustBeString = 25,
        eUnexpectedClosingBracket = 26,
        eUnknownControlSymbol = 27,
        eMessageTooLong = 28,
    };

    CJsonOverUTTPReader();
    EParsingEvent ProcessParsingEvents(CUTTPReader& reader);
    const CJsonNode GetMessage() const;
    void Reset();

private:
    TNodeStack m_NodeStack;
    CJsonNode m_CurrentNode;
    string m_CurrentChunk;
    string m_HashKey;
    bool m_ReadingChunk;
    bool m_HashValueIsExpected;
};

inline const CJsonNode CJsonOverUTTPReader::GetMessage() const
{
    return m_CurrentNode;
}

class CJsonOverUTTPWriter
{
public:
    CJsonOverUTTPWriter(CPipe& pipe, CUTTPWriter& writer) :
        m_Pipe(pipe), m_UTTPWriter(writer)
    {
    }

    void SendMessage(const CJsonNode& root_node);

private:
    struct SOutputStackFrame {
        CJsonNode m_Node;
        CJsonNode::TArray::const_iterator m_ArrayIterator;
        CJsonNode::TObject::const_iterator m_ObjectIterator;
    };

    typedef list<SOutputStackFrame> TOutputStack;

    bool ContinueWithReply();
    void SendOutputBuffer();
    bool SendNode(const CJsonNode& node);

    CPipe& m_Pipe;
    CUTTPWriter& m_UTTPWriter;

    TOutputStack m_OutputStack;
    SOutputStackFrame m_CurrentOutputNode;
    bool m_SendHashValue;
};

END_NCBI_SCOPE

#endif /* UTIL___SUTTP__HPP */
