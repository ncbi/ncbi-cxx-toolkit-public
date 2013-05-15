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

#include <corelib/ncbistre.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE

const char* CJsonException::GetErrCodeString() const
{
    switch (GetErrCode()) {
    case eInvalidNodeType:
        return "eInvalidNodeType";
    case eIndexOutOfRange:
        return "eIndexOutOfRange";
    case eKeyNotFound:
        return "eKeyNotFound";
    default:
        return CException::GetErrCodeString();
    }
}

typedef CRef<SJsonNodeImpl,
    CNetComponentCounterLocker<SJsonNodeImpl> > TJsonNodeRef;

typedef map<string, TJsonNodeRef> TJsonNodeMap;
typedef vector<TJsonNodeRef> TJsonNodeVector;

struct SJsonObjectNodeImpl;
struct SJsonArrayNodeImpl;

struct SJsonNodeImpl : public CObject
{
    SJsonNodeImpl(CJsonNode::ENodeType node_type) : m_NodeType(node_type) {}

    static const char* GetTypeName(CJsonNode::ENodeType node_type);
    const char* GetTypeName() const {return GetTypeName(m_NodeType);}

    void VerifyType(const char* operation,
            CJsonNode::ENodeType required_type) const;

    const SJsonObjectNodeImpl* GetObjectNodeImpl(const char* operation) const;
    SJsonObjectNodeImpl* GetObjectNodeImpl(const char* operation);

    const SJsonArrayNodeImpl* GetArrayNodeImpl(const char* operation) const;
    SJsonArrayNodeImpl* GetArrayNodeImpl(const char* operation);

    CJsonNode::ENodeType m_NodeType;
};

const char* SJsonNodeImpl::GetTypeName(CJsonNode::ENodeType node_type)
{
    switch (node_type) {
    case CJsonNode::eObject:
        return "an object";
    case CJsonNode::eArray:
        return "an array";
    case CJsonNode::eString:
        return "a string";
    case CJsonNode::eInteger:
        return "an integer";
    case CJsonNode::eDouble:
        return "a floating point";
    case CJsonNode::eBoolean:
        return "a boolean";
    default: /* case CJsonNode::eNull: */
        return "a null";
    }
}

void SJsonNodeImpl::VerifyType(const char* operation,
        CJsonNode::ENodeType required_type) const
{
    if (m_NodeType != required_type) {
        NCBI_THROW_FMT(CJsonException, eInvalidNodeType,
                "Cannot apply " << operation <<
                " to " << GetTypeName() << " node: " <<
                GetTypeName(required_type) << " node is required");
    }
}

struct SJsonObjectNodeImpl : public SJsonNodeImpl
{
    SJsonObjectNodeImpl() : SJsonNodeImpl(CJsonNode::eObject) {}

    TJsonNodeMap m_Object;
};

inline const SJsonObjectNodeImpl* SJsonNodeImpl::GetObjectNodeImpl(
        const char* operation) const
{
    VerifyType(operation, CJsonNode::eObject);

    return static_cast<const SJsonObjectNodeImpl*>(this);
}

inline SJsonObjectNodeImpl* SJsonNodeImpl::GetObjectNodeImpl(
        const char* operation)
{
    VerifyType(operation, CJsonNode::eObject);

    return static_cast<SJsonObjectNodeImpl*>(this);
}

struct SJsonArrayNodeImpl : public SJsonNodeImpl
{
    SJsonArrayNodeImpl() : SJsonNodeImpl(CJsonNode::eArray) {}

    void VerifyIndexBounds(const char* operation, size_t index) const;

    TJsonNodeVector m_Array;
};

inline const SJsonArrayNodeImpl* SJsonNodeImpl::GetArrayNodeImpl(
        const char* operation) const
{
    VerifyType(operation, CJsonNode::eArray);

    return static_cast<const SJsonArrayNodeImpl*>(this);
}

inline SJsonArrayNodeImpl* SJsonNodeImpl::GetArrayNodeImpl(
        const char* operation)
{
    VerifyType(operation, CJsonNode::eArray);

    return static_cast<SJsonArrayNodeImpl*>(this);
}

void SJsonArrayNodeImpl::VerifyIndexBounds(
        const char* operation, size_t index) const
{
    if (m_Array.size() <= index) {
        NCBI_THROW_FMT(CJsonException, eIndexOutOfRange,
                operation << ": index " << index <<
                " is out of range (array size is " <<
                m_Array.size() << ')');
    }
}

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

CJsonNode CJsonNode::NewObjectNode()
{
    return new SJsonObjectNodeImpl;
}

CJsonNode CJsonNode::NewArrayNode()
{
    return new SJsonArrayNodeImpl;
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

CJsonNode::ENodeType CJsonNode::GetNodeType() const
{
    return m_Impl->m_NodeType;
}

struct SJsonObjectIterator : public SJsonIteratorImpl
{
    SJsonObjectIterator(SJsonObjectNodeImpl* container) :
        m_Container(container),
        m_Iterator(container->m_Object.begin())
    {
    }

    virtual SJsonNodeImpl* GetNode() const;
    virtual const string& GetKey() const;
    virtual bool Next();
    virtual bool IsValid() const;

    CRef<SJsonObjectNodeImpl,
            CNetComponentCounterLocker<SJsonObjectNodeImpl> > m_Container;
    TJsonNodeMap::iterator m_Iterator;
};

SJsonNodeImpl* SJsonObjectIterator::GetNode() const
{
    return m_Iterator->second;
}

const string& SJsonObjectIterator::GetKey() const
{
    return m_Iterator->first;
}

bool SJsonObjectIterator::Next()
{
    _ASSERT(IsValid());

    return ++m_Iterator != m_Container->m_Object.end();
}

bool SJsonObjectIterator::IsValid() const
{
    return m_Iterator != const_cast<TJsonNodeMap&>(m_Container->m_Object).end();
}

struct SJsonArrayIterator : public SJsonIteratorImpl
{
    SJsonArrayIterator(SJsonArrayNodeImpl* container) :
        m_Container(container),
        m_Iterator(container->m_Array.begin())
    {
    }

    virtual SJsonNodeImpl* GetNode() const;
    virtual const string& GetKey() const;
    virtual bool Next();
    virtual bool IsValid() const;

    CRef<SJsonArrayNodeImpl,
            CNetComponentCounterLocker<SJsonArrayNodeImpl> > m_Container;
    TJsonNodeVector::iterator m_Iterator;
};

SJsonNodeImpl* SJsonArrayIterator::GetNode() const
{
    return *m_Iterator;
}

const string& SJsonArrayIterator::GetKey() const
{
    NCBI_THROW(CJsonException, eInvalidNodeType,
            "Cannot get a key for an array iterator");
}

bool SJsonArrayIterator::Next()
{
    _ASSERT(IsValid());

    return ++m_Iterator != m_Container->m_Array.end();
}

bool SJsonArrayIterator::IsValid() const
{
    return m_Iterator != m_Container->m_Array.end();
}

SJsonIteratorImpl* CJsonNode::Iterate() const
{
    switch (m_Impl->m_NodeType) {
    case CJsonNode::eObject:
        return new SJsonObjectIterator(const_cast<SJsonObjectNodeImpl*>(
                static_cast<const SJsonObjectNodeImpl*>(
                        m_Impl.GetPointerOrNull())));
    case CJsonNode::eArray:
        return new SJsonArrayIterator(const_cast<SJsonArrayNodeImpl*>(
                static_cast<const SJsonArrayNodeImpl*>(
                        m_Impl.GetPointerOrNull())));
    default:
        NCBI_THROW(CJsonException, eInvalidNodeType,
                "Cannot iterate a scalar type");
    }
}

size_t CJsonNode::GetSize() const
{
    switch (m_Impl->m_NodeType) {
    case CJsonNode::eObject:
        return static_cast<const SJsonObjectNodeImpl*>(
                m_Impl.GetPointerOrNull())->m_Object.size();
    case CJsonNode::eArray:
        return static_cast<const SJsonArrayNodeImpl*>(
                m_Impl.GetPointerOrNull())->m_Array.size();
    default:
        NCBI_THROW(CJsonException, eInvalidNodeType,
                "GetSize() requires a container type");
    }
}

void CJsonNode::AppendString(const string& value)
{
    Append(new SJsonStringNodeImpl(value));
}

void CJsonNode::AppendInteger(Int8 value)
{
    Append(new SJsonFixedSizeNodeImpl(value));
}

void CJsonNode::AppendDouble(double value)
{
    Append(new SJsonFixedSizeNodeImpl(value));
}

void CJsonNode::AppendBoolean(bool value)
{
    Append(new SJsonFixedSizeNodeImpl(value));
}

void CJsonNode::AppendNull()
{
    Append(new SJsonFixedSizeNodeImpl);
}

void CJsonNode::Append(CJsonNode::TInstance value)
{
    m_Impl->GetArrayNodeImpl("Append()")->
            m_Array.push_back(TJsonNodeRef(value));
}

void CJsonNode::SetAtIndex(size_t index, CJsonNode::TInstance value)
{
    SJsonArrayNodeImpl* impl(m_Impl->GetArrayNodeImpl("SetAtIndex()"));

    impl->VerifyIndexBounds("SetAtIndex()", index);

    impl->m_Array[index] = value;
}

void CJsonNode::DeleteAtIndex(size_t index)
{
    SJsonArrayNodeImpl* impl(m_Impl->GetArrayNodeImpl("DeleteAtIndex()"));

    impl->VerifyIndexBounds("DeleteAtIndex()", index);

    impl->m_Array.erase(impl->m_Array.begin() + index);
}

CJsonNode CJsonNode::GetAtIndex(size_t index) const
{
    const SJsonArrayNodeImpl* impl(m_Impl->GetArrayNodeImpl("GetAtIndex()"));

    impl->VerifyIndexBounds("GetAtIndex()", index);

    return const_cast<SJsonNodeImpl*>(impl->m_Array[index].GetPointerOrNull());
}

void CJsonNode::SetString(const string& key, const string& value)
{
    SetByKey(key, new SJsonStringNodeImpl(value));
}

void CJsonNode::SetInteger(const string& key, Int8 value)
{
    SetByKey(key, new SJsonFixedSizeNodeImpl(value));
}

void CJsonNode::SetDouble(const string& key, double value)
{
    SetByKey(key, new SJsonFixedSizeNodeImpl(value));
}

void CJsonNode::SetBoolean(const string& key, bool value)
{
    SetByKey(key, new SJsonFixedSizeNodeImpl(value));
}

void CJsonNode::SetNull(const string& key)
{
    SetByKey(key, new SJsonFixedSizeNodeImpl);
}

void CJsonNode::SetByKey(const string& key, CJsonNode::TInstance value)
{
    m_Impl->GetObjectNodeImpl("SetByKey()")->m_Object[key] = value;
}

void CJsonNode::DeleteByKey(const string& key)
{
    m_Impl->GetObjectNodeImpl("DeleteByKey()")->m_Object.erase(key);
}

bool CJsonNode::HasKey(const string& key) const
{
    const SJsonObjectNodeImpl* impl(m_Impl->GetObjectNodeImpl("HasKey()"));

    return impl->m_Object.find(key) != impl->m_Object.end();
}

CJsonNode CJsonNode::GetByKey(const string& key) const
{
    const SJsonObjectNodeImpl* impl(m_Impl->GetObjectNodeImpl("GetByKey()"));

    TJsonNodeMap::const_iterator it(impl->m_Object.find(key));

    if (it == impl->m_Object.end()) {
        NCBI_THROW_FMT(CJsonException, eKeyNotFound,
                "GetByKey(): no such key \"" << key << '\"');
    }

    return const_cast<SJsonNodeImpl*>(it->second.GetPointerOrNull());
}

const string& CJsonNode::AsString() const
{
    m_Impl->VerifyType("AsString()", eString);

    return static_cast<const SJsonStringNodeImpl*>(
        m_Impl.GetPointerOrNull())->m_String;
}

Int8 CJsonNode::AsInteger() const
{
    if (m_Impl->m_NodeType == eDouble)
        return (Int8) static_cast<const SJsonFixedSizeNodeImpl*>(
            m_Impl.GetPointerOrNull())->m_Double;

    m_Impl->VerifyType("AsInteger()", eInteger);

    return static_cast<const SJsonFixedSizeNodeImpl*>(
        m_Impl.GetPointerOrNull())->m_Integer;
}

double CJsonNode::AsDouble() const
{
    if (m_Impl->m_NodeType == eInteger)
        return (double) static_cast<const SJsonFixedSizeNodeImpl*>(
            m_Impl.GetPointerOrNull())->m_Integer;

    m_Impl->VerifyType("AsDouble()", eDouble);

    return static_cast<const SJsonFixedSizeNodeImpl*>(
        m_Impl.GetPointerOrNull())->m_Double;
}

bool CJsonNode::AsBoolean() const
{
    m_Impl->VerifyType("AsBoolean()", eBoolean);

    return static_cast<const SJsonFixedSizeNodeImpl*>(
        m_Impl.GetPointerOrNull())->m_Boolean;
}

static void s_Repr_Value(CNcbiOstrstream& oss, const CJsonNode& node);

static void s_Repr_Object(CNcbiOstrstream& oss, const CJsonNode& node)
{
    CJsonIterator it = node.Iterate();
    if (it) {
        oss << '"' << it.GetKey() << "\": ";
        s_Repr_Value(oss, *it);
        while (++it) {
            oss << ", \"" << it.GetKey() << "\": ";
            s_Repr_Value(oss, *it);
        }
    }
}

static void s_Repr_Array(CNcbiOstrstream& oss, const CJsonNode& node)
{
    CJsonIterator it = node.Iterate();
    if (it) {
        s_Repr_Value(oss, *it);
        while (++it) {
            oss << ", ";
            s_Repr_Value(oss, *it);
        }
    }
}

static void s_Repr_Value(CNcbiOstrstream& oss, const CJsonNode& node)
{
    switch (node.GetNodeType()) {
    case CJsonNode::eObject:
        oss << '{';
        s_Repr_Object(oss, node);
        oss << '}';
        break;
    case CJsonNode::eArray:
        oss << '[';
        s_Repr_Array(oss, node);
        oss << ']';
        break;
    case CJsonNode::eString:
        oss << '"' << NStr::PrintableString(node.AsString()) << '"';
        break;
    case CJsonNode::eInteger:
        oss << node.AsInteger();
        break;
    case CJsonNode::eDouble:
        oss << node.AsDouble();
        break;
    case CJsonNode::eBoolean:
        oss << (node.AsBoolean() ? "true" : "false");
        break;
    default: /* case CJsonNode::eNull: */
        oss << "null";
    }
}

string CJsonNode::Repr(TReprFlags flags) const
{
    CNcbiOstrstream oss;

    switch (GetNodeType()) {
    case CJsonNode::eObject:
        if (flags & fOmitOutermostBrackets)
            s_Repr_Object(oss, *this);
        else {
            oss << '{';
            s_Repr_Object(oss, *this);
            oss << '}';
        }
        break;
    case CJsonNode::eArray:
        if (flags & fOmitOutermostBrackets)
            s_Repr_Array(oss, *this);
        else {
            oss << '[';
            s_Repr_Array(oss, *this);
            oss << ']';
        }
        break;
    case CJsonNode::eString:
        if (flags & fVerbatimIfString)
            return (static_cast<const SJsonStringNodeImpl*>(
                    m_Impl.GetPointerOrNull())->m_String);
        /* FALL THROUGH */
    default:
        s_Repr_Value(oss, *this);
    }

    return CNcbiOstrstreamToString(oss);
}

const char* CJsonOverUTTPException::GetErrCodeString() const
{
    switch (GetErrCode()) {
    case eUTTPFormatError:
        return "eUTTPFormatError";
    case eChunkContinuationExpected:
        return "eChunkContinuationExpected";
    case eUnexpectedEOM:
        return "eUnexpectedEOM";
    case eUnexpectedTrailingToken:
        return "eUnexpectedTrailingToken";
    case eObjectKeyMustBeString:
        return "eObjectKeyMustBeString";
    case eUnexpectedClosingBracket:
        return "eUnexpectedClosingBracket";
    case eUnknownControlSymbol:
        return "eUnknownControlSymbol";
    default:
        return CException::GetErrCodeString();
    }
}

CJsonOverUTTPReader::CJsonOverUTTPReader() :
    m_State(eExpectNextToken),
    m_HashValueIsExpected(false)
{
}

#ifdef WORDS_BIGENDIAN
#define DOUBLE_PREFIX 'D'
#else
#define DOUBLE_PREFIX 'd'
#endif

bool CJsonOverUTTPReader::ReadMessage(CUTTPReader& reader)
{
    for (;;)
        switch (reader.GetNextEvent()) {
        case CUTTPReader::eChunkPart:
            switch (m_State) {
            case eExpectNextToken:
                m_State = eReadingString;
                m_CurrentChunk.assign(reader.GetChunkPart(),
                        reader.GetChunkPartSize());
                break;
            case eReadingString:
                m_CurrentChunk.append(reader.GetChunkPart(),
                        reader.GetChunkPartSize());
                break;
            case eReadingDouble:
                // The underlying transport protocol guarantees
                // that m_Double boundaries will not be exceeded.
                memcpy(m_DoublePtr, reader.GetChunkPart(),
                        reader.GetChunkPartSize());
                m_DoublePtr += reader.GetChunkPartSize();
                break;
            default: /* case eMessageComplete: */
                goto ThrowUnexpectedTrailingToken;
            }
            break;

        case CUTTPReader::eChunk:
            switch (m_State) {
            case eExpectNextToken:
                m_CurrentChunk.assign(reader.GetChunkPart(),
                        reader.GetChunkPartSize());
                break;
            case eReadingString:
                m_State = eExpectNextToken;
                m_CurrentChunk.append(reader.GetChunkPart(),
                        reader.GetChunkPartSize());
                break;
            case eReadingDouble:
                m_State = eExpectNextToken;
                memcpy(m_DoublePtr, reader.GetChunkPart(),
                        reader.GetChunkPartSize());
                if (m_DoubleEndianness != DOUBLE_PREFIX)
                    reverse(reinterpret_cast<char*>(&m_Double),
                            reinterpret_cast<char*>(&m_Double + 1));
                if (!x_AddNewNode(CJsonNode::NewDoubleNode(m_Double)))
                    m_State = eMessageComplete;
                continue;
            default: /* case eMessageComplete: */
                goto ThrowUnexpectedTrailingToken;
            }
            if (!m_CurrentNode) {
                m_CurrentNode = CJsonNode::NewStringNode(m_CurrentChunk);
                m_State = eMessageComplete;
            } else if (m_HashValueIsExpected) {
                m_HashValueIsExpected = false;
                m_CurrentNode.SetString(m_HashKey, m_CurrentChunk);
            } else
                // The current node is either a JSON object or an array,
                // because if it was a non-container node, m_State would
                // be eMessageComplete.
                if (m_CurrentNode.IsArray())
                    m_CurrentNode.AppendString(m_CurrentChunk);
                else {
                    m_HashKey = m_CurrentChunk;
                    m_HashValueIsExpected = true;
                }
            break;

        case CUTTPReader::eControlSymbol:
            {
                char control_symbol = reader.GetControlSymbol();

                if (control_symbol == '\n') {
                    if (m_State != eMessageComplete) {
                        NCBI_THROW(CJsonOverUTTPException, eUnexpectedEOM,
                                "JSON-over-UTTP: Unexpected end of message");
                    }
                    return true;
                }

                if (m_State != eExpectNextToken)
                    goto ThrowChunkContinuationExpected;

                switch (control_symbol) {
                case '[':
                case '{':
                    {
                        CJsonNode new_node(control_symbol == '[' ?
                            CJsonNode::NewArrayNode() :
                            CJsonNode::NewObjectNode());
                        if (x_AddNewNode(new_node)) {
                            m_NodeStack.push_back(m_CurrentNode);
                            m_CurrentNode = new_node;
                        }
                    }
                    break;

                case ']':
                case '}':
                    if (!m_CurrentNode ||
                            (control_symbol == ']') ^ m_CurrentNode.IsArray()) {
                        NCBI_THROW(CJsonOverUTTPException, eUnexpectedClosingBracket,
                                "JSON-over-UTTP: Unexpected closing bracket");
                    }
                    if (m_NodeStack.empty())
                        m_State = eMessageComplete;
                    else {
                        m_CurrentNode = m_NodeStack.back();
                        m_NodeStack.pop_back();
                    }
                    break;

                case 'D':
                case 'd':
                    switch (reader.ReadRawData(sizeof(double))) {
                    default: /* case CUTTPReader::eEndOfBuffer: */
                        m_State = eReadingDouble;
                        m_DoubleEndianness = control_symbol;
                        m_DoublePtr = reinterpret_cast<char*>(&m_Double);
                        return false;
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
                            memcpy(&m_Double, reader.GetChunkPart(),
                                    sizeof(double));
                        else {
                            // Copy the entire chunk to
                            // m_Double in reverse order.
                            const char* src = reader.GetChunkPart();
                            char* dst = reinterpret_cast<char*>(&m_Double + 1);
                            int count = sizeof(double);

                            do
                                *--dst = *src++;
                            while (--count > 0);
                        }

                        if (!x_AddNewNode(CJsonNode::NewDoubleNode(m_Double)))
                            m_State = eMessageComplete;
                    }
                    break;

                case 'Y':
                    if (!x_AddNewNode(CJsonNode::NewBooleanNode(true)))
                        m_State = eMessageComplete;
                    break;

                case 'N':
                    if (!x_AddNewNode(CJsonNode::NewBooleanNode(false)))
                        m_State = eMessageComplete;
                    break;

                case 'U':
                    if (!x_AddNewNode(CJsonNode::NewNullNode()))
                        m_State = eMessageComplete;
                    break;

                default:
                    NCBI_THROW_FMT(CJsonOverUTTPException,
                            eUnknownControlSymbol,
                            "JSON-over-UTTP: Unknown control symbol '" <<
                            NStr::PrintableString(CTempString(&control_symbol,
                                    1)) << '\'');
                }
            }
            break;

        case CUTTPReader::eNumber:
            switch (m_State) {
            case eExpectNextToken:
                if (!x_AddNewNode(CJsonNode::NewIntegerNode(reader.GetNumber())))
                    m_State = eMessageComplete;
                break;
            case eMessageComplete:
                goto ThrowUnexpectedTrailingToken;
            default:
                goto ThrowChunkContinuationExpected;
            }
            break;

        case CUTTPReader::eEndOfBuffer:
            return false;

        default: /* case CUTTPReader::eFormatError: */
            NCBI_THROW(CJsonOverUTTPException, eUTTPFormatError,
                    "JSON-over-UTTP: UTTP format error");
        }

ThrowUnexpectedTrailingToken:
    NCBI_THROW(CJsonOverUTTPException, eUnexpectedTrailingToken,
            "JSON-over-UTTP: Received a token while expected EOM");

ThrowChunkContinuationExpected:
    NCBI_THROW(CJsonOverUTTPException, eChunkContinuationExpected,
            "JSON-over-UTTP: Chunk continuation expected");
}

void CJsonOverUTTPReader::Reset()
{
    m_State = eExpectNextToken;
    m_NodeStack.clear();
    m_CurrentNode = NULL;
    m_HashValueIsExpected = false;
}

bool CJsonOverUTTPReader::x_AddNewNode(CJsonNode::TInstance new_node)
{
    if (!m_CurrentNode) {
        m_CurrentNode = new_node;
        return false;
    } else if (m_HashValueIsExpected) {
        m_HashValueIsExpected = false;
        m_CurrentNode.SetByKey(m_HashKey, new_node);
    } else
        // The current node is either a JSON object or an array,
        // because if it was a non-container node, this method
        // would not be called.
        if (m_CurrentNode.IsArray())
            m_CurrentNode.Append(new_node);
        else {
            NCBI_THROW(CJsonOverUTTPException, eObjectKeyMustBeString,
                    "JSON-over-UTTP: Invalid object key type");
        }
    return true;
}

bool CJsonOverUTTPWriter::WriteMessage(const CJsonNode& root_node)
{
    _ASSERT(!m_CurrentOutputNode.m_Node && m_OutputStack.empty());

    m_SendHashValue = false;

    return x_SendNode(root_node) && CompleteMessage();
}

bool CJsonOverUTTPWriter::CompleteMessage()
{
    while (m_CurrentOutputNode.m_Node) {
        switch (m_CurrentOutputNode.m_Node.GetNodeType()) {
        case CJsonNode::eArray:
            if (!m_CurrentOutputNode.m_Iterator) {
                x_PopNode();
                if (!m_UTTPWriter.SendControlSymbol(']'))
                    return false;
            } else {
                CJsonIterator it(m_CurrentOutputNode.m_Iterator);
                if (!x_SendNode(*it)) {
                    it.Next();
                    return false;
                }
                it.Next();
            }
            break;

        case CJsonNode::eObject:
            if (!m_CurrentOutputNode.m_Iterator) {
                x_PopNode();
                if (!m_UTTPWriter.SendControlSymbol('}'))
                    return false;
            } else {
                if (!m_SendHashValue) {
                    string key(m_CurrentOutputNode.m_Iterator.GetKey());
                    if (!m_UTTPWriter.SendChunk(
                            key.data(), key.length(), false)) {
                        m_SendHashValue = true;
                        return false;
                    }
                } else
                    m_SendHashValue = false;

                CJsonIterator it(m_CurrentOutputNode.m_Iterator);
                if (!x_SendNode(*it)) {
                    it.Next();
                    return false;
                }
                it.Next();
            }
            break;

        default: /* case CJsonNode::eDouble: */
            x_PopNode();
            if (!m_UTTPWriter.SendRawData(&m_Double, sizeof(m_Double)))
                return false;
        }
    }

    return m_UTTPWriter.SendControlSymbol('\n');
}

bool CJsonOverUTTPWriter::x_SendNode(const CJsonNode& node)
{
    switch (node.GetNodeType()) {
    case CJsonNode::eObject:
        x_PushNode(node);
        m_CurrentOutputNode.m_Iterator = node.Iterate();
        m_SendHashValue = false;
        return m_UTTPWriter.SendControlSymbol('{');

    case CJsonNode::eArray:
        x_PushNode(node);
        m_CurrentOutputNode.m_Iterator = node.Iterate();
        return m_UTTPWriter.SendControlSymbol('[');

    case CJsonNode::eString:
        {
            string str(node.AsString());
            return m_UTTPWriter.SendChunk(str.data(), str.length(), false);
        }

    case CJsonNode::eInteger:
        return m_UTTPWriter.SendNumber(node.AsInteger());

    case CJsonNode::eDouble:
        m_Double = node.AsDouble();
        if (!m_UTTPWriter.SendControlSymbol(DOUBLE_PREFIX)) {
            x_PushNode(node);
            return false;
        }
        return m_UTTPWriter.SendRawData(&m_Double, sizeof(m_Double));

    case CJsonNode::eBoolean:
        return m_UTTPWriter.SendControlSymbol(node.AsBoolean() ? 'Y' : 'N');

    default: /* case CJsonNode::eNull: */
        return m_UTTPWriter.SendControlSymbol('U');
    }
}

void CJsonOverUTTPWriter::x_PushNode(const CJsonNode& node)
{
    if (!m_CurrentOutputNode.m_Node);
        m_OutputStack.push_back(m_CurrentOutputNode);
    m_CurrentOutputNode.m_Node = node;
}

void CJsonOverUTTPWriter::x_PopNode()
{
    _ASSERT(m_CurrentOutputNode.m_Node);

    if (m_OutputStack.empty())
        m_CurrentOutputNode.m_Node = NULL;
    else {
        m_CurrentOutputNode = m_OutputStack.back();
        m_OutputStack.pop_back();
    }
}

END_NCBI_SCOPE
