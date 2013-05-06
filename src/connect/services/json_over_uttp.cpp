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
 * File Description: JSON to UTTP serializer/deserializer (implementation).
 *
 */

#include <ncbi_pch.hpp>

#include <connect/services/json_over_uttp.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE

struct SJsonNodeImpl : public CObject
{
    SJsonNodeImpl(CJsonNode::ENodeType node_type) : m_NodeType(node_type) {}

    CJsonNode::ENodeType m_NodeType;
};

struct SJsonObjectNodeImpl : public SJsonNodeImpl
{
    SJsonObjectNodeImpl() : SJsonNodeImpl(CJsonNode::eObject) {}

    CJsonNode::TObject m_Object;
};

struct SJsonArrayNodeImpl : public SJsonNodeImpl
{
    SJsonArrayNodeImpl() : SJsonNodeImpl(CJsonNode::eArray) {}

    CJsonNode::TArray m_Array;
};

struct SJsonStringNodeImpl : public SJsonNodeImpl
{
    SJsonStringNodeImpl(const string& str) :
        SJsonNodeImpl(CJsonNode::eString),
        m_String(str)
    {
    }

    string m_String;
};

struct SJsonFixedSizeNodeImpl : public SJsonNodeImpl
{
    SJsonFixedSizeNodeImpl(Int8 value) :
        SJsonNodeImpl(CJsonNode::eInteger),
        m_Integer(value)
    {
    }

    SJsonFixedSizeNodeImpl(double value) :
        SJsonNodeImpl(CJsonNode::eDouble),
        m_Double(value)
    {
    }

    SJsonFixedSizeNodeImpl(bool value) :
        SJsonNodeImpl(CJsonNode::eBoolean),
        m_Boolean(value)
    {
    }

    SJsonFixedSizeNodeImpl() : SJsonNodeImpl(CJsonNode::eNull)
    {
    }

    union {
        Int8 m_Integer;
        double m_Double;
        bool m_Boolean;
    };
};

CJsonNode::ENodeType CJsonNode::GetNodeType() const
{
    return m_Impl->m_NodeType;
}

CJsonNode CJsonNode::NewArrayNode()
{
    return new SJsonArrayNodeImpl;
}

const CJsonNode::TArray& CJsonNode::GetArray() const
{
    _ASSERT(m_Impl->m_NodeType == eArray);

    return static_cast<const SJsonArrayNodeImpl*>(
        m_Impl.GetPointerOrNull())->m_Array;
}

CJsonNode::TArray& CJsonNode::GetArray()
{
    _ASSERT(m_Impl->m_NodeType == eArray);

    return static_cast<SJsonArrayNodeImpl*>(
        m_Impl.GetPointerOrNull())->m_Array;
}

void CJsonNode::PushNode(CJsonNode::TInstance value)
{
    GetArray().push_back(value);
}

void CJsonNode::PushString(const string& value)
{
    PushNode(new SJsonStringNodeImpl(value));
}

void CJsonNode::PushInteger(Int8 value)
{
    PushNode(new SJsonFixedSizeNodeImpl(value));
}

void CJsonNode::PushDouble(double value)
{
    PushNode(new SJsonFixedSizeNodeImpl(value));
}

void CJsonNode::PushBoolean(bool value)
{
    PushNode(new SJsonFixedSizeNodeImpl(value));
}

void CJsonNode::PushNull()
{
    PushNode(new SJsonFixedSizeNodeImpl);
}

CJsonNode CJsonNode::NewObjectNode()
{
    return new SJsonObjectNodeImpl;
}

const CJsonNode::TObject& CJsonNode::GetObject() const
{
    _ASSERT(m_Impl->m_NodeType == eObject);

    return static_cast<const SJsonObjectNodeImpl*>(
        m_Impl.GetPointerOrNull())->m_Object;
}

CJsonNode::TObject& CJsonNode::GetObject()
{
    _ASSERT(m_Impl->m_NodeType == eObject);

    return static_cast<SJsonObjectNodeImpl*>(
        m_Impl.GetPointerOrNull())->m_Object;
}

void CJsonNode::SetNode(const string& key, CJsonNode::TInstance value)
{
    GetObject()[key] = value;
}

void CJsonNode::SetString(const string& key, const string& value)
{
    SetNode(key, new SJsonStringNodeImpl(value));
}

void CJsonNode::SetInteger(const string& key, Int8 value)
{
    SetNode(key, new SJsonFixedSizeNodeImpl(value));
}

void CJsonNode::SetDouble(const string& key, double value)
{
    SetNode(key, new SJsonFixedSizeNodeImpl(value));
}

void CJsonNode::SetBoolean(const string& key, bool value)
{
    SetNode(key, new SJsonFixedSizeNodeImpl(value));
}

void CJsonNode::SetNull(const string& key)
{
    SetNode(key, new SJsonFixedSizeNodeImpl);
}

CJsonNode CJsonNode::NewStringNode(const string& value)
{
    return new SJsonStringNodeImpl(value);
}

CJsonNode CJsonNode::NewIntegerNode(Int8 value)
{
    return new SJsonFixedSizeNodeImpl(value);
}

CJsonNode CJsonNode::NewDoubleNode(double value)
{
    return new SJsonFixedSizeNodeImpl(value);
}

CJsonNode CJsonNode::NewBooleanNode(bool value)
{
    return new SJsonFixedSizeNodeImpl(value);
}

CJsonNode CJsonNode::NewNullNode()
{
    return new SJsonFixedSizeNodeImpl();
}

const string& CJsonNode::GetString() const
{
    _ASSERT(m_Impl->m_NodeType == eString);

    return static_cast<const SJsonStringNodeImpl*>(
        m_Impl.GetPointerOrNull())->m_String;
}

Int8 CJsonNode::GetInteger() const
{
    if (m_Impl->m_NodeType == eDouble)
        return (Int8) static_cast<const SJsonFixedSizeNodeImpl*>(
            m_Impl.GetPointerOrNull())->m_Double;

    _ASSERT(m_Impl->m_NodeType == eInteger);

    return static_cast<const SJsonFixedSizeNodeImpl*>(
        m_Impl.GetPointerOrNull())->m_Integer;
}

double CJsonNode::GetDouble() const
{
    if (m_Impl->m_NodeType == eInteger)
        return (double) static_cast<const SJsonFixedSizeNodeImpl*>(
            m_Impl.GetPointerOrNull())->m_Integer;

    _ASSERT(m_Impl->m_NodeType == eDouble);

    return static_cast<const SJsonFixedSizeNodeImpl*>(
        m_Impl.GetPointerOrNull())->m_Double;
}

bool CJsonNode::GetBoolean() const
{
    _ASSERT(m_Impl->m_NodeType == eBoolean);

    return static_cast<const SJsonFixedSizeNodeImpl*>(
        m_Impl.GetPointerOrNull())->m_Boolean;
}

CJsonOverUTTPReader::CJsonOverUTTPReader() :
    m_State(eInitialState),
    m_CurrentNode(CJsonNode::NewArrayNode()),
    m_HashValueIsExpected(false)
{
}

#ifdef WORDS_BIGENDIAN
#define DOUBLE_PREFIX 'D'
#else
#define DOUBLE_PREFIX 'd'
#endif

CJsonOverUTTPReader::EParsingEvent
    CJsonOverUTTPReader::ProcessParsingEvents(CUTTPReader& reader)
{
    for (;;)
        switch (reader.GetNextEvent()) {
        case CUTTPReader::eChunkPart:
            switch (m_State) {
            case eInitialState:
                m_State = eReadingString;
                m_CurrentChunk.assign(reader.GetChunkPart(),
                        reader.GetChunkPartSize());
                break;
            case eReadingString:
                m_CurrentChunk.append(reader.GetChunkPart(),
                        reader.GetChunkPartSize());
                break;
            default: /* case eReadingDouble: */
                memcpy(m_DoublePtr, reader.GetChunkPart(),
                        reader.GetChunkPartSize());
                m_DoublePtr += reader.GetChunkPartSize();
            }
            break;

        case CUTTPReader::eChunk:
            switch (m_State) {
            case eInitialState:
                m_CurrentChunk.assign(reader.GetChunkPart(),
                        reader.GetChunkPartSize());
                break;
            case eReadingString:
                m_State = eInitialState;
                m_CurrentChunk.append(reader.GetChunkPart(),
                        reader.GetChunkPartSize());
                break;
            default: /* case eReadingDouble: */
                m_State = eInitialState;
                memcpy(m_DoublePtr, reader.GetChunkPart(),
                        reader.GetChunkPartSize());
                if (m_DoubleEndianness != DOUBLE_PREFIX)
                    reverse(reinterpret_cast<char*>(&m_Double),
                            reinterpret_cast<char*>(&m_Double + 1));
                if (m_CurrentNode.IsArray())
                    m_CurrentNode.PushDouble(m_Double);
                else // The current node is eObject.
                    if (m_HashValueIsExpected) {
                        m_HashValueIsExpected = false;
                        m_CurrentNode.SetDouble(m_HashKey, m_Double);
                    } else
                        return eHashKeyMustBeString;
                continue;
            }
            if (m_CurrentNode.IsArray())
                m_CurrentNode.PushString(m_CurrentChunk);
            else // The current node is eObject.
                if (m_HashValueIsExpected) {
                    m_HashValueIsExpected = false;
                    m_CurrentNode.SetString(m_HashKey, m_CurrentChunk);
                } else {
                    m_HashValueIsExpected = true;
                    m_HashKey = m_CurrentChunk;
                }
            break;

        case CUTTPReader::eControlSymbol:
            {
                if (m_State != eInitialState)
                    return eChunkContinuationExpected;

                char control_symbol = reader.GetControlSymbol();

                switch (control_symbol) {
                case '\n':
                    if (m_NodeStack.size() != 0)
                        return eUnexpectedEOM;
                    return eEndOfMessage;

                case '[':
                case '{':
                    {
                        CJsonNode new_node(control_symbol == '[' ?
                            CJsonNode::NewArrayNode() :
                            CJsonNode::NewObjectNode());
                        if (m_CurrentNode.IsArray())
                            m_CurrentNode.PushNode(new_node);
                        else // The current node is eObject.
                            if (m_HashValueIsExpected) {
                                m_HashValueIsExpected = false;
                                m_CurrentNode.SetNode(m_HashKey, new_node);
                            } else
                                return eHashKeyMustBeString;
                        m_NodeStack.push_back(m_CurrentNode);
                        m_CurrentNode = new_node;
                    }
                    break;

                case ']':
                case '}':
                    if (m_NodeStack.size() == 0 || (control_symbol == ']') ^
                            m_CurrentNode.IsArray())
                        return eUnexpectedClosingBracket;
                    m_CurrentNode = m_NodeStack.back();
                    m_NodeStack.pop_back();
                    break;

                case 'D':
                case 'd':
                    switch (reader.ReadRawData(sizeof(double))) {
                    default: /* case CUTTPReader::eEndOfBuffer: */
                        m_State = eReadingDouble;
                        m_DoubleEndianness = control_symbol;
                        m_DoublePtr = reinterpret_cast<char*>(&m_Double);
                        return eNextBuffer;
                    case CUTTPReader::eChunkPart:
                        m_State = eReadingDouble;
                        m_DoubleEndianness = control_symbol;
                        memcpy(&m_Double, reader.GetChunkPart(),
                                reader.GetChunkPartSize());
                        m_DoublePtr = reinterpret_cast<char*>(&m_Double) +
                                reader.GetChunkPartSize();
                        break;
                    case CUTTPReader::eChunk:
                        _ASSERT(reader.GetChunkPartSize() == sizeof(double));

                        if (control_symbol == DOUBLE_PREFIX)
                            memcpy(&m_Double, reader.GetChunkPart(), sizeof(double));
                        else {
                            const char* src = reader.GetChunkPart();
                            char* dst = reinterpret_cast<char*>(&m_Double + 1);
                            int count = sizeof(double);

                            do
                                *--dst = *src++;
                            while (--count > 0);
                        }

                        if (m_CurrentNode.IsArray())
                            m_CurrentNode.PushDouble(m_Double);
                        else // The current node is eObject.
                            if (m_HashValueIsExpected) {
                                m_HashValueIsExpected = false;
                                m_CurrentNode.SetDouble(m_HashKey, m_Double);
                            } else
                                return eHashKeyMustBeString;
                    }
                    break;

                case 'Y':
                case 'N':
                    {
                        bool boolean = control_symbol == 'Y';

                        if (m_CurrentNode.IsArray())
                            m_CurrentNode.PushBoolean(boolean);
                        else // The current node is eObject.
                            if (m_HashValueIsExpected) {
                                m_HashValueIsExpected = false;
                                m_CurrentNode.SetBoolean(m_HashKey, boolean);
                            } else
                                return eHashKeyMustBeString;
                    }
                    break;

                case 'U':
                    if (m_CurrentNode.IsArray())
                        m_CurrentNode.PushNull();
                    else // The current node is eObject.
                        if (m_HashValueIsExpected) {
                            m_HashValueIsExpected = false;
                            m_CurrentNode.SetNull(m_HashKey);
                        } else
                            return eHashKeyMustBeString;
                    break;

                default:
                    return eUnknownControlSymbol;
                }
            }
            break;

        case CUTTPReader::eNumber:
            if (m_State != eInitialState)
                return eChunkContinuationExpected;
            if (m_CurrentNode.IsArray())
                m_CurrentNode.PushInteger(reader.GetNumber());
            else // The current node is eObject.
                if (m_HashValueIsExpected) {
                    m_HashValueIsExpected = false;
                    m_CurrentNode.SetInteger(m_HashKey, reader.GetNumber());
                } else
                    return eHashKeyMustBeString;
            break;

        case CUTTPReader::eEndOfBuffer:
            return eNextBuffer;

        default: // case CUTTPReader::eFormatError:
            return eUTTPFormatError;
        }
}

void CJsonOverUTTPReader::Reset()
{
    m_CurrentNode = CJsonNode::NewArrayNode();
}

void CJsonOverUTTPWriter::SetOutputMessage(const CJsonNode& root_node)
{
    _ASSERT(m_OutputStack.empty());

    m_CurrentOutputNode.m_Node = root_node;
    m_CurrentOutputNode.m_ArrayIterator =
        m_CurrentOutputNode.m_Node.GetArray().begin();

    m_SendHashValue = false;
}

bool CJsonOverUTTPWriter::ContinueWithReply()
{
    for (;;)
        if (m_CurrentOutputNode.m_Node.IsArray()) {
            if (m_CurrentOutputNode.m_ArrayIterator ==
                    m_CurrentOutputNode.m_Node.GetArray().end()) {
                if (m_OutputStack.empty()) {
                    m_UTTPWriter.SendControlSymbol('\n');
                    return false;
                }
                m_CurrentOutputNode = m_OutputStack.back();
                m_OutputStack.pop_back();
                if (!m_UTTPWriter.SendControlSymbol(']'))
                    return true;
            } else
                if (!SendNode(*m_CurrentOutputNode.m_ArrayIterator++))
                    return true;
        } else if (m_CurrentOutputNode.m_Node.IsObject()) {
            if (m_CurrentOutputNode.m_ObjectIterator ==
                    m_CurrentOutputNode.m_Node.GetObject().end()) {
                m_CurrentOutputNode = m_OutputStack.back();
                m_OutputStack.pop_back();
                if (!m_UTTPWriter.SendControlSymbol('}'))
                    return true;
            } else {
                if (!m_SendHashValue) {
                    string key(m_CurrentOutputNode.m_ObjectIterator->first);
                    if (!m_UTTPWriter.SendChunk(
                            key.data(), key.length(), false)) {
                        m_SendHashValue = true;
                        return true;
                    }
                }

                m_SendHashValue = false;

                if (!SendNode(m_CurrentOutputNode.m_ObjectIterator++->second))
                    return true;
            }
        } else {
            _ASSERT(m_CurrentOutputNode.m_Node.IsDouble());

            m_OutputStack.pop_back();
            if (!m_UTTPWriter.SendRawData(&m_Double, sizeof(m_Double)))
                return true;
        }
}

bool CJsonOverUTTPWriter::SendNode(const CJsonNode& node)
{
    switch (node.GetNodeType()) {
    case CJsonNode::eObject:
        m_OutputStack.push_back(m_CurrentOutputNode);
        m_CurrentOutputNode.m_Node = node;
        m_CurrentOutputNode.m_ObjectIterator = node.GetObject().begin();
        m_SendHashValue = false;
        return m_UTTPWriter.SendControlSymbol('{');

    case CJsonNode::eArray:
        m_OutputStack.push_back(m_CurrentOutputNode);
        m_CurrentOutputNode.m_Node = node;
        m_CurrentOutputNode.m_ArrayIterator = node.GetArray().begin();
        return m_UTTPWriter.SendControlSymbol('[');

    case CJsonNode::eString:
        {
            string str(node.GetString());
            return m_UTTPWriter.SendChunk(str.data(), str.length(), false);
        }

    case CJsonNode::eInteger:
        return m_UTTPWriter.SendNumber(node.GetInteger());

    case CJsonNode::eDouble:
        m_Double = node.GetDouble();
        if (!m_UTTPWriter.SendControlSymbol(DOUBLE_PREFIX)) {
            m_OutputStack.push_back(m_CurrentOutputNode);
            m_CurrentOutputNode.m_Node = node;
            return false;
        }
        return m_UTTPWriter.SendRawData(&m_Double, sizeof(m_Double));

    case CJsonNode::eBoolean:
        return m_UTTPWriter.SendControlSymbol(node.GetBoolean() ? 'Y' : 'N');

    default: /* case CJsonNode::eNull: */
        return m_UTTPWriter.SendControlSymbol('U');
    }
}

END_NCBI_SCOPE
