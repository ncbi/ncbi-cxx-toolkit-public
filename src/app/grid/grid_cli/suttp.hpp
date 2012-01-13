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

#include <util/uttp.hpp>
#include <util/util_exception.hpp>
#include <connect/ncbi_pipe.hpp>

BEGIN_NCBI_SCOPE

class CSUTTPNode;

typedef CRef<CSUTTPNode> TSUTTPNodeRef;

typedef vector<TSUTTPNodeRef> TSUTTPNodeArray;
typedef map<string, TSUTTPNodeRef> TSUTTPNodeMap;

class CSUTTPNode : public CObject
{
public:
    enum ENodeType {
        eScalarNode,
        eArrayNode,
        eHashNode
    };

    ENodeType GetNodeType() const {return m_NodeType;}

    bool IsScalar() const {return m_NodeType == eScalarNode;}
    bool IsArray() const {return m_NodeType == eArrayNode;}
    bool IsHash() const {return m_NodeType == eHashNode;}

    const string& GetScalar() const;

    static TSUTTPNodeRef NewArrayNode();

    const TSUTTPNodeArray& GetArray() const;
    TSUTTPNodeArray& GetArray();

    void Push(CSUTTPNode* new_node);
    void Push(const string& scalar);

    static TSUTTPNodeRef NewHashNode();

    const TSUTTPNodeMap& GetHash() const;
    TSUTTPNodeMap& GetHash();

    void Set(const string& key, CSUTTPNode* value);
    void Set(const string& key, const string& value);

protected:
    CSUTTPNode(ENodeType node_type) : m_NodeType(node_type) {}

private:
    ENodeType m_NodeType;
};

struct SSUTTPScalarNodeImpl : public CSUTTPNode
{
    typedef string TScalar;

    SSUTTPScalarNodeImpl(const string& scalar) :
        CSUTTPNode(eScalarNode),
        m_Scalar(scalar)
    {
    }

    TScalar m_Scalar;
};

inline const string& CSUTTPNode::GetScalar() const
{
    _ASSERT(m_NodeType == eScalarNode);

    return static_cast<const SSUTTPScalarNodeImpl*>(this)->m_Scalar;
}

struct SSUTTPArrayNodeImpl : public CSUTTPNode
{
    SSUTTPArrayNodeImpl() : CSUTTPNode(eArrayNode) {}

    TSUTTPNodeArray m_Array;
};

inline TSUTTPNodeRef CSUTTPNode::NewArrayNode()
{
    return TSUTTPNodeRef(new SSUTTPArrayNodeImpl);
}

inline const TSUTTPNodeArray& CSUTTPNode::GetArray() const
{
    _ASSERT(m_NodeType == eArrayNode);

    return static_cast<const SSUTTPArrayNodeImpl*>(this)->m_Array;
}

inline TSUTTPNodeArray& CSUTTPNode::GetArray()
{
    _ASSERT(m_NodeType == eArrayNode);

    return static_cast<SSUTTPArrayNodeImpl*>(this)->m_Array;
}

inline void CSUTTPNode::Push(CSUTTPNode* new_node)
{
    GetArray().push_back(TSUTTPNodeRef(new_node));
}

inline void CSUTTPNode::Push(const string& scalar)
{
    Push(new SSUTTPScalarNodeImpl(scalar));
}

struct SSUTTPHashNodeImpl : public CSUTTPNode
{
    SSUTTPHashNodeImpl() : CSUTTPNode(eHashNode) {}

    TSUTTPNodeMap m_Hash;
};

inline TSUTTPNodeRef CSUTTPNode::NewHashNode()
{
    return TSUTTPNodeRef(new SSUTTPHashNodeImpl);
}

inline const TSUTTPNodeMap& CSUTTPNode::GetHash() const
{
    _ASSERT(m_NodeType == eHashNode);

    return static_cast<const SSUTTPHashNodeImpl*>(this)->m_Hash;
}

inline TSUTTPNodeMap& CSUTTPNode::GetHash()
{
    _ASSERT(m_NodeType == eHashNode);

    return static_cast<SSUTTPHashNodeImpl*>(this)->m_Hash;
}

inline void CSUTTPNode::Set(const string& key, CSUTTPNode* value)
{
    GetHash()[key] = TSUTTPNodeRef(value);
}

inline void CSUTTPNode::Set(const string& key, const string& value)
{
    Set(key, new SSUTTPScalarNodeImpl(value));
}

class CSUTTPReader
{
public:
    typedef list<TSUTTPNodeRef> TNodeStack;

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
        eHashKeyMustBeScalar = 25,
        eUnexpectedClosingBracket = 26,
        eUnknownControlSymbol = 27,
        eMessageTooLong = 28,
    };

    CSUTTPReader();
    EParsingEvent ProcessParsingEvents(CUTTPReader& reader);
    const CSUTTPNode* GetMessage() const;
    void Reset();

private:
    TNodeStack m_NodeStack;
    TSUTTPNodeRef m_CurrentNode;
    string m_CurrentChunk;
    string m_HashKey;
    bool m_ReadingChunk;
    bool m_HashValueIsExpected;
};

inline const CSUTTPNode* CSUTTPReader::GetMessage() const
{
    return m_CurrentNode.GetPointerOrNull();
}

class CSUTTPWriter
{
public:
    CSUTTPWriter(CPipe& pipe, CUTTPWriter& writer) :
        m_Pipe(pipe), m_UTTPWriter(writer)
    {
    }

    void SendMessage(const CSUTTPNode* root_node);

private:
    struct SOutputStackFrame {
        const CSUTTPNode* m_Node;
        TSUTTPNodeArray::const_iterator m_ArrayIterator;
        TSUTTPNodeMap::const_iterator m_HashIterator;
    };

    typedef list<SOutputStackFrame> TOutputStack;

    bool ContinueWithReply();
    void SendOutputBuffer();
    bool SendNode(const CSUTTPNode* node);

    CPipe& m_Pipe;
    CUTTPWriter& m_UTTPWriter;

    TOutputStack m_OutputStack;
    SOutputStackFrame m_CurrentOutputNode;
    bool m_SendHashValue;
};

END_NCBI_SCOPE

#endif /* UTIL___SUTTP__HPP */
