#ifndef JSON_OVER_UTTP__HPP
#define JSON_OVER_UTTP__HPP

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
 * File Description: JSON to UTTP serializer/deserializer (declarations).
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

    enum ENodeType {
        eObject,
        eArray,
        eString,
        eInteger,
        eDouble,
        eBoolean,
        eNull
    };

    ENodeType GetNodeType() const;

    bool IsObject() const {return GetNodeType() == eObject;}
    bool IsArray() const {return GetNodeType() == eArray;}
    bool IsString() const {return GetNodeType() == eString;}
    bool IsInteger() const {return GetNodeType() == eInteger;}
    bool IsDouble() const {return GetNodeType() == eDouble;}
    bool IsBoolean() const {return GetNodeType() == eBoolean;}
    bool IsNull() const {return GetNodeType() == eNull;}

    static CJsonNode NewArrayNode();

    const TArray& GetArray() const;
    TArray& GetArray();

    CJsonNode& operator [](size_t index) {return GetArray()[index];}
    const CJsonNode& operator [](size_t index) const {return GetArray()[index];}

    void PushNode(CJsonNode::TInstance value);
    void PushString(const string& value);
    void PushInteger(Int8 value);
    void PushDouble(double value);
    void PushBoolean(bool value);
    void PushNull();

    static CJsonNode NewObjectNode();

    const CJsonNode::TObject& GetObject() const;
    CJsonNode::TObject& GetObject();

    CJsonNode& operator [](const string& key) {return GetObject()[key];}

    void SetNode(const string& key, CJsonNode::TInstance value);
    void SetString(const string& key, const string& value);
    void SetInteger(const string& key, Int8 value);
    void SetDouble(const string& key, double value);
    void SetBoolean(const string& key, bool value);
    void SetNull(const string& key);

    static CJsonNode NewStringNode(const string& value);
    static CJsonNode NewIntegerNode(Int8 value);
    static CJsonNode NewDoubleNode(double value);
    static CJsonNode NewBooleanNode(bool value);
    static CJsonNode NewNullNode();

    const string& GetString() const;
    Int8 GetInteger() const;
    double GetDouble() const;
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
    enum {
        eInitialState,
        eReadingString,
        eReadingDouble
    } m_State;
    TNodeStack m_NodeStack;
    CJsonNode m_CurrentNode;
    string m_CurrentChunk;
    double m_Double;
    char* m_DoublePtr;
    char m_DoubleEndianness;
    string m_HashKey;
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
    double m_Double;
    bool m_SendHashValue;
};

END_NCBI_SCOPE

#endif /* JSON_OVER_UTTP__HPP */
