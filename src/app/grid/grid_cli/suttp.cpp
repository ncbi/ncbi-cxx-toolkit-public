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

#include "suttp.hpp"

USING_NCBI_SCOPE;

CSUTTPReader::CSUTTPReader() :
    m_CurrentNode(CSUTTPNode::NewArrayNode()),
    m_ReadingChunk(false),
    m_HashValueIsExpected(false)
{
}

CSUTTPReader::EParsingEvent
    CSUTTPReader::ProcessParsingEvents(CUTTPReader& reader)
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
            if (m_CurrentNode->IsArray())
                m_CurrentNode->Push(m_CurrentChunk);
            else // The current node is eHashNode.
                if (m_HashValueIsExpected) {
                    m_HashValueIsExpected = false;
                    m_CurrentNode->Set(m_HashKey, m_CurrentChunk);
                } else {
                    m_HashValueIsExpected = true;
                    m_HashKey = m_CurrentChunk;
                }
            break;

        case CUTTPReader::eControlSymbol:
            {
                if (m_ReadingChunk)
                    return eChunkContinuationExpected;

                char control_symbol = *reader.GetChunkPart();
                if (control_symbol == '\n') {
                    if (m_NodeStack.size() != 0)
                        return eUnexpectedEOM;
                    if (reader.GetNextEvent() != CUTTPReader::eEndOfBuffer)
                        return eExtraCharsPastEOM;
                    return eEndOfMessage;
                }

                switch (control_symbol) {
                case '[':
                case '{':
                    {
                        TSUTTPNodeRef new_node(control_symbol == '[' ?
                            CSUTTPNode::NewArrayNode() :
                            CSUTTPNode::NewHashNode());
                        if (m_CurrentNode->IsArray())
                            m_CurrentNode->Push(new_node);
                        else // The current node is eHashNode.
                            if (m_HashValueIsExpected) {
                                m_HashValueIsExpected = false;
                                m_CurrentNode->Set(m_HashKey, new_node);
                            } else
                                return eHashKeyMustBeScalar;
                        m_NodeStack.push_back(m_CurrentNode);
                        m_CurrentNode = new_node;
                    }
                    break;

                case ']':
                case '}':
                    if (m_NodeStack.size() == 0 || (control_symbol == ']') ^
                            m_CurrentNode->IsArray())
                        return eUnexpectedClosingBracket;
                    m_CurrentNode = m_NodeStack.back();
                    m_NodeStack.pop_back();
                    break;

                default:
                    return eUnknownControlSymbol;
                }
            }
            break;

        case CUTTPReader::eEndOfBuffer:
            return eNextBuffer;

        default: // case CUTTPReader::eFormatError:
            return eUTTPFormatError;
        }
}

void CSUTTPReader::Reset()
{
    m_CurrentNode = CSUTTPNode::NewArrayNode();
}

void CSUTTPWriter::SendMessage(const CSUTTPNode* root_node)
{
    _ASSERT(m_OutputStack.empty());

    m_CurrentOutputNode.m_Node = root_node;
    m_CurrentOutputNode.m_ArrayIterator = root_node->GetArray().begin();

    m_SendHashValue = false;

    while (ContinueWithReply())
        SendOutputBuffer();

    SendOutputBuffer();

    //m_Pipe.Flush();
}

bool CSUTTPWriter::ContinueWithReply()
{
    for (;;)
        if (m_CurrentOutputNode.m_Node->IsArray()) {
            if (m_CurrentOutputNode.m_ArrayIterator ==
                    m_CurrentOutputNode.m_Node->GetArray().end()) {
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
        } else { // The current node is eHashNode
            if (m_CurrentOutputNode.m_HashIterator ==
                    m_CurrentOutputNode.m_Node->GetHash().end()) {
                m_CurrentOutputNode = m_OutputStack.back();
                m_OutputStack.pop_back();
                if (!m_UTTPWriter.SendControlSymbol('}'))
                    return true;
            } else {
                if (!m_SendHashValue) {
                    string key(m_CurrentOutputNode.m_HashIterator->first);
                    if (!m_UTTPWriter.SendChunk(
                            key.data(), key.length(), false)) {
                        m_SendHashValue = true;
                        return true;
                    }
                }

                m_SendHashValue = false;

                if (!SendNode(m_CurrentOutputNode.m_HashIterator++->second))
                    return true;
            }
        }
}

void CSUTTPWriter::SendOutputBuffer()
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

bool CSUTTPWriter::SendNode(const CSUTTPNode* node)
{
    switch (node->GetNodeType()) {
    case CSUTTPNode::eScalarNode:
        {
            string scalar(node->GetScalar());
            if (!m_UTTPWriter.SendChunk(scalar.data(), scalar.length(), false))
                return false;
        }
        break;

    case CSUTTPNode::eArrayNode:
        m_OutputStack.push_back(m_CurrentOutputNode);
        m_CurrentOutputNode.m_Node = node;
        m_CurrentOutputNode.m_ArrayIterator = node->GetArray().begin();
        if (!m_UTTPWriter.SendControlSymbol('['))
            return false;
        break;

    case CSUTTPNode::eHashNode:
        m_OutputStack.push_back(m_CurrentOutputNode);
        m_CurrentOutputNode.m_Node = node;
        m_CurrentOutputNode.m_HashIterator = node->GetHash().begin();
        m_SendHashValue = false;
        if (!m_UTTPWriter.SendControlSymbol('{'))
            return false;
    }

    return true;
}
