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
 *                   Transfer Protocol (implementation).
 *
 */

#include <ncbi_pch.hpp>

#include "json_over_uttp.hpp"

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
    SJsonFixedSizeNodeImpl(CJsonNode::TNumber number) :
        SJsonNodeImpl(CJsonNode::eNumber),
        m_Number(number)
    {
    }

    SJsonFixedSizeNodeImpl(bool boolean) :
        SJsonNodeImpl(CJsonNode::eBoolean),
        m_Boolean(boolean)
    {
    }

    SJsonFixedSizeNodeImpl() : SJsonNodeImpl(CJsonNode::eNull)
    {
    }

    union {
        CJsonNode::TNumber m_Number;
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

void CJsonNode::PushNumber(CJsonNode::TNumber value)
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

void CJsonNode::SetNumber(const string& key, CJsonNode::TNumber value)
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

CJsonNode CJsonNode::NewNumberNode(const CJsonNode::TNumber value)
{
    return new SJsonFixedSizeNodeImpl(value);
}

CJsonNode CJsonNode::NewBooleanNode(const bool value)
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

CJsonNode::TNumber CJsonNode::GetNumber() const
{
    _ASSERT(m_Impl->m_NodeType == eNumber);

    return static_cast<const SJsonFixedSizeNodeImpl*>(
        m_Impl.GetPointerOrNull())->m_Number;
}

bool CJsonNode::GetBoolean() const
{
    _ASSERT(m_Impl->m_NodeType == eBoolean);

    return static_cast<const SJsonFixedSizeNodeImpl*>(
        m_Impl.GetPointerOrNull())->m_Boolean;
}

CJsonOverUTTPReader::CJsonOverUTTPReader() :
    m_CurrentNode(CJsonNode::NewArrayNode()),
    m_ReadingChunk(false),
    m_HashValueIsExpected(false)
{
}

CJsonOverUTTPReader::EParsingEvent
    CJsonOverUTTPReader::ProcessParsingEvents(CUTTPReader& reader)
{
    for (;;)
        switch (reader.GetNextEvent()) {
        case CUTTPReader::eChunkPart:
            if (m_ReadingChunk)
                m_CurrentChunk.append(reader.GetChunkPart(),
                    reader.GetChunkPartSize());
            else {
                m_ReadingChunk = true;
                m_CurrentChunk.assign(reader.GetChunkPart(),
                    reader.GetChunkPartSize());
            }
            break;

        case CUTTPReader::eChunk:
            if (m_ReadingChunk) {
                m_ReadingChunk = false;
                m_CurrentChunk.append(reader.GetChunkPart(),
                    reader.GetChunkPartSize());
            } else
                m_CurrentChunk.assign(reader.GetChunkPart(),
                    reader.GetChunkPartSize());
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
                if (m_ReadingChunk)
                    return eChunkContinuationExpected;

                char control_symbol = reader.GetControlSymbol();

                switch (control_symbol) {
                case '\n':
                    if (m_NodeStack.size() != 0)
                        return eUnexpectedEOM;
                    if (reader.GetNextEvent() != CUTTPReader::eEndOfBuffer)
                        return eExtraCharsPastEOM;
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
            if (m_ReadingChunk)
                return eChunkContinuationExpected;
            if (m_CurrentNode.IsArray())
                m_CurrentNode.PushNumber(reader.GetNumber());
            else // The current node is eObject.
                if (m_HashValueIsExpected) {
                    m_HashValueIsExpected = false;
                    m_CurrentNode.SetNumber(m_HashKey, reader.GetNumber());
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

void CJsonOverUTTPWriter::SendMessage(const CJsonNode& root_node)
{
    _ASSERT(m_OutputStack.empty());

    m_CurrentOutputNode.m_Node = root_node;
    m_CurrentOutputNode.m_ArrayIterator =
        m_CurrentOutputNode.m_Node.GetArray().begin();

    m_SendHashValue = false;

    while (ContinueWithReply())
        SendOutputBuffer();

    SendOutputBuffer();
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
        } else { // The current node is eObject.
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
        }
}

void CJsonOverUTTPWriter::SendOutputBuffer()
{
    const char* output_buffer;
    size_t output_buffer_size;
    size_t bytes_written;

    do {
        m_UTTPWriter.GetOutputBuffer(&output_buffer, &output_buffer_size);
        for (;;) {
            if (m_Pipe.Write(output_buffer, output_buffer_size,
                    &bytes_written) != eIO_Success) {
                NCBI_THROW(CIOException, eWrite,
                    "Error while writing to the pipe");
            }
            if (bytes_written == output_buffer_size)
                break;
            output_buffer += bytes_written;
            output_buffer_size -= bytes_written;
        }
    } while (m_UTTPWriter.NextOutputBuffer());
}

bool CJsonOverUTTPWriter::SendNode(const CJsonNode& node)
{
    switch (node.GetNodeType()) {
    case CJsonNode::eObject:
        m_OutputStack.push_back(m_CurrentOutputNode);
        m_CurrentOutputNode.m_Node = node;
        m_CurrentOutputNode.m_ObjectIterator = node.GetObject().begin();
        m_SendHashValue = false;
        if (!m_UTTPWriter.SendControlSymbol('{'))
            return false;
        break;

    case CJsonNode::eArray:
        m_OutputStack.push_back(m_CurrentOutputNode);
        m_CurrentOutputNode.m_Node = node;
        m_CurrentOutputNode.m_ArrayIterator = node.GetArray().begin();
        if (!m_UTTPWriter.SendControlSymbol('['))
            return false;
        break;

    case CJsonNode::eString:
        {
            string str(node.GetString());
            if (!m_UTTPWriter.SendChunk(str.data(), str.length(), false))
                return false;
        }
        break;

    case CJsonNode::eNumber:
        if (!m_UTTPWriter.SendNumber(node.GetNumber()))
            return false;
        break;

    case CJsonNode::eBoolean:
        if (!m_UTTPWriter.SendControlSymbol(node.GetBoolean() ? 'Y' : 'N'))
            return false;
        break;

    case CJsonNode::eNull:
        if (!m_UTTPWriter.SendControlSymbol('U'))
            return false;
    }

    return true;
}

END_NCBI_SCOPE
