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
#include <set>

BEGIN_NCBI_SCOPE

class CJsonParser
{
public:
    CJsonParser(CJsonNode::TParseFlags flags) : m_Flags(flags) {}

    CJsonNode ParseObject(const string& ns_output)
    {
        m_Ch = (m_NSOutput = ns_output).c_str();

        return ParseObject('\0');
    }

    CJsonNode ParseArray(const string& ns_output)
    {
        m_Ch = (m_NSOutput = ns_output).c_str();

        return ParseArray('\0');
    }

    CJsonNode ParseJSON(const string& json);

private:
    size_t GetRemainder() const
    {
        return m_NSOutput.length() - (m_Ch - m_NSOutput.data());
    }

    size_t GetPosition() const
    {
        return m_Ch - m_NSOutput.data() + 1;
    }

    string ParseString(size_t max_len);
    Int8 ParseInt(size_t len);
    double ParseDouble(size_t len);
    bool MoreNodes();

    CJsonNode ParseObject(char closing_char);
    CJsonNode ParseArray(char closing_char);
    CJsonNode ParseValue();

    string m_NSOutput;
    const char* m_Ch;
    CJsonNode::TParseFlags m_Flags;
};

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

typedef CNetRef<SJsonNodeImpl> TJsonNodeRef;

struct SJsonObjectElement {
    SJsonObjectElement(const string& key, SJsonNodeImpl* node_impl) :
        m_Key(key),
        m_Node(node_impl)
    {
    }

    bool operator <(const SJsonObjectElement& right_hand) const
    {
        return m_Key < right_hand.m_Key;
    }

    string m_Key;
    TJsonNodeRef m_Node;
    size_t m_Order;
};

struct SObjectElementLessOrder {
    bool operator ()(const SJsonObjectElement* left_hand,
            const SJsonObjectElement* right_hand) const
    {
        return left_hand->m_Order < right_hand->m_Order;
    }
};

typedef set<SJsonObjectElement> TJsonObjectElements;
typedef set<SJsonObjectElement*,
        SObjectElementLessOrder> TJsonObjectElementOrder;
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
                "Cannot call the " << operation <<
                " method for " << GetTypeName() << " node; " <<
                GetTypeName(required_type) << " node is required");
    }
}

struct SJsonObjectNodeImpl : public SJsonNodeImpl
{
    SJsonObjectNodeImpl() :
        SJsonNodeImpl(CJsonNode::eObject),
        m_NextElementOrder(0)
    {
    }

    TJsonObjectElements m_Elements;
    TJsonObjectElementOrder m_ElementOrder;
    size_t m_NextElementOrder;
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

CJsonNode::CJsonNode(const string& value)
    : m_Impl(new SJsonStringNodeImpl(value))
{
}

CJsonNode::CJsonNode(const char* value)
    : m_Impl(new SJsonStringNodeImpl(value))
{
}

CJsonNode::CJsonNode(int value)
    : CJsonNode(new SJsonFixedSizeNodeImpl(Int8(value)))
{
}

CJsonNode::CJsonNode(Int8 value)
    : CJsonNode(new SJsonFixedSizeNodeImpl(value))
{
}

CJsonNode::CJsonNode(double value)
    : CJsonNode(new SJsonFixedSizeNodeImpl(value))
{
}

CJsonNode::CJsonNode(bool value)
    : CJsonNode(new SJsonFixedSizeNodeImpl(value))
{
}

CJsonNode CJsonNode::NewNullNode()
{
    return new SJsonFixedSizeNodeImpl();
}

CJsonNode CJsonNode::GuessType(const CTempString& value)
{
    const char* ch = value.begin();
    const char* end = value.end();

    switch (*ch) {
    case '"':
    case '\'':
        return NewStringNode(NStr::ParseQuoted(value));

    case '-':
        if (++ch >= end || !isdigit(*ch))
            return NewStringNode(value);

    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        do
            if (++ch >= end)
                return NewIntegerNode(NStr::StringToInt8(value));
        while (isdigit(*ch));

        switch (*ch) {
        case '.':
            if (++ch == end || !isdigit(*ch))
                return NewStringNode(value);
            for (;;) {
                if (++ch == end)
                    return NewDoubleNode(NStr::StringToDouble(value));

                if (!isdigit(*ch)) {
                    if (*ch == 'E' || *ch == 'e')
                        break;

                    return NewStringNode(value);
                }
            }
            /* FALL THROUGH */

        case 'E':
        case 'e':
            if (++ch < end && (*ch == '-' || *ch == '+' ?
                    ++ch < end && isdigit(*ch) : isdigit(*ch)))
                do
                    if (++ch == end)
                        return NewDoubleNode(NStr::StringToDouble(value));
                while (isdigit(*ch));
            /* FALL THROUGH */

        default:
            return NewStringNode(value);
        }
    }

    return NStr::CompareNocase(value, "false") == 0 ? NewBooleanNode(false) :
            NStr::CompareNocase(value, "true") == 0 ? NewBooleanNode(true) :
            NStr::CompareNocase(value, "none") == 0 ? NewNullNode() :
            NewStringNode(value);
}

CJsonNode::TInstance s_CreateImpl(CJsonNode::ENodeType type)
{
    switch (type)
    {
    case CJsonNode::eObject:    return new SJsonObjectNodeImpl;
    case CJsonNode::eArray:     return new SJsonArrayNodeImpl;
    case CJsonNode::eString:    return new SJsonStringNodeImpl(string());
    case CJsonNode::eInteger:   return new SJsonFixedSizeNodeImpl(Int8());
    case CJsonNode::eDouble:    return new SJsonFixedSizeNodeImpl(double());
    case CJsonNode::eBoolean:   return new SJsonFixedSizeNodeImpl(bool());
    case CJsonNode::eNull:      return new SJsonFixedSizeNodeImpl();
    }

    NCBI_THROW_FMT(CJsonException, eInvalidNodeType, "Unknown type: " << type);
    return nullptr; // Not reached
}

CJsonNode::CJsonNode(ENodeType type) :
    m_Impl(s_CreateImpl(type))
{
}

CJsonNode::ENodeType CJsonNode::GetNodeType() const
{
    return m_Impl->m_NodeType;
}

string CJsonNode::GetTypeName() const
{
    return m_Impl->GetTypeName();
}

struct SJsonObjectKeyIterator : public SJsonIteratorImpl
{
    SJsonObjectKeyIterator(SJsonObjectNodeImpl* container) :
        m_Container(container),
        m_Iterator(container->m_Elements.begin())
    {
    }

    virtual SJsonNodeImpl* GetNode() const;
    virtual string GetKey() const;
    virtual bool Next();
    virtual bool IsValid() const;

    CNetRef<SJsonObjectNodeImpl> m_Container;
    TJsonObjectElements::iterator m_Iterator;
};

SJsonNodeImpl* SJsonObjectKeyIterator::GetNode() const
{
    return const_cast<SJsonNodeImpl*>(m_Iterator->m_Node.GetPointerOrNull());
}

string SJsonObjectKeyIterator::GetKey() const
{
    return m_Iterator->m_Key;
}

bool SJsonObjectKeyIterator::Next()
{
    _ASSERT(IsValid());

    return ++m_Iterator != m_Container->m_Elements.end();
}

bool SJsonObjectKeyIterator::IsValid() const
{
    return m_Iterator != const_cast<TJsonObjectElements&>(
            m_Container->m_Elements).end();
}

struct SJsonObjectElementOrderIterator : public SJsonIteratorImpl
{
    SJsonObjectElementOrderIterator(SJsonObjectNodeImpl* container) :
        m_Container(container),
        m_Iterator(container->m_ElementOrder.begin())
    {
    }

    virtual SJsonNodeImpl* GetNode() const;
    virtual string GetKey() const;
    virtual bool Next();
    virtual bool IsValid() const;

    CNetRef<SJsonObjectNodeImpl> m_Container;
    TJsonObjectElementOrder::iterator m_Iterator;
};

SJsonNodeImpl* SJsonObjectElementOrderIterator::GetNode() const
{
    return (*m_Iterator)->m_Node;
}

string SJsonObjectElementOrderIterator::GetKey() const
{
    return (*m_Iterator)->m_Key;
}

bool SJsonObjectElementOrderIterator::Next()
{
    _ASSERT(IsValid());

    return ++m_Iterator != m_Container->m_ElementOrder.end();
}

bool SJsonObjectElementOrderIterator::IsValid() const
{
    return m_Iterator != const_cast<TJsonObjectElementOrder&>(
            m_Container->m_ElementOrder).end();
}

struct SJsonArrayIterator : public SJsonIteratorImpl
{
    SJsonArrayIterator(SJsonArrayNodeImpl* container) :
        m_Container(container),
        m_Iterator(container->m_Array.begin())
    {
    }

    virtual SJsonNodeImpl* GetNode() const;
    virtual string GetKey() const;
    virtual bool Next();
    virtual bool IsValid() const;

    CNetRef<SJsonArrayNodeImpl> m_Container;
    TJsonNodeVector::iterator m_Iterator;
};

SJsonNodeImpl* SJsonArrayIterator::GetNode() const
{
    return *m_Iterator;
}

string SJsonArrayIterator::GetKey() const
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

struct SFlattenIterator : public SJsonIteratorImpl
{
    struct SFrame {
        CJsonIterator m_Iterator;
        string m_Path;
        size_t m_Index;

        void Advance()
        {
            m_Iterator.Next();
            if (m_Index != (size_t) -1)
                ++m_Index;
        }

        string MakePath() const;
    };

    SFlattenIterator(const CJsonNode& container)
    {
        m_CurrentFrame.m_Iterator = container.Iterate();
        m_CurrentFrame.m_Index = container.IsObject() ? (size_t) -1 : 0;
        x_DepthFirstSearchForScalar();
    }

    virtual SJsonNodeImpl* GetNode() const;
    virtual string GetKey() const;
    virtual bool Next();
    virtual bool IsValid() const;

    bool x_DepthFirstSearchForScalar();

    SFrame m_CurrentFrame;
    vector<SFrame> m_IteratorStack;
};

string SFlattenIterator::SFrame::MakePath() const
{
    if (m_Index == (size_t) -1) {
        if (m_Path.empty())
            return m_Iterator.GetKey();

        string path(m_Path + '.');
        path += m_Iterator.GetKey();
        return path;
    } else {
        string index_str(NStr::NumericToString(m_Index));

        if (m_Path.empty())
            return index_str;

        string path(m_Path + '.');
        path += index_str;
        return path;
    }
}

SJsonNodeImpl* SFlattenIterator::GetNode() const
{
    return m_CurrentFrame.m_Iterator.GetNode();
}

string SFlattenIterator::GetKey() const
{
    return m_CurrentFrame.MakePath();
}

bool SFlattenIterator::Next()
{
    _ASSERT(m_CurrentFrame.m_Iterator.IsValid());

    m_CurrentFrame.Advance();

    return x_DepthFirstSearchForScalar();
}

bool SFlattenIterator::IsValid() const
{
    return m_CurrentFrame.m_Iterator.IsValid();
}

bool SFlattenIterator::x_DepthFirstSearchForScalar()
{
    for (;;) {
        while (m_CurrentFrame.m_Iterator.IsValid()) {
            CJsonNode node(m_CurrentFrame.m_Iterator.GetNode());

            switch (node.GetNodeType()) {
            case CJsonNode::eObject:
                m_IteratorStack.push_back(m_CurrentFrame);

                m_CurrentFrame.m_Path = m_CurrentFrame.MakePath();
                m_CurrentFrame.m_Index = (size_t) -1;
                break;

            case CJsonNode::eArray:
                m_IteratorStack.push_back(m_CurrentFrame);

                m_CurrentFrame.m_Path = m_CurrentFrame.MakePath();
                m_CurrentFrame.m_Index = 0;
                break;

            default: /* Scalar type */
                return true;
            }

            m_CurrentFrame.m_Iterator = node.Iterate();
        }

        if (m_IteratorStack.empty())
            return false;

        m_CurrentFrame = m_IteratorStack.back();
        m_IteratorStack.pop_back();

        m_CurrentFrame.Advance();
    }
}

SJsonIteratorImpl* CJsonNode::Iterate(EIterationMode mode) const
{
    switch (m_Impl->m_NodeType) {
    case CJsonNode::eObject:
        switch (mode) {
        default: /* case eNatural: */
            return new SJsonObjectElementOrderIterator(
                    const_cast<SJsonObjectNodeImpl*>(
                            static_cast<const SJsonObjectNodeImpl*>(
                                    m_Impl.GetPointerOrNull())));

        case eOrdered:
            return new SJsonObjectKeyIterator(const_cast<SJsonObjectNodeImpl*>(
                    static_cast<const SJsonObjectNodeImpl*>(
                            m_Impl.GetPointerOrNull())));
        case eFlatten:
            return new SFlattenIterator(*this);
        }

    case CJsonNode::eArray:
        if (mode == eFlatten)
            return new SFlattenIterator(*this);
        else
            return new SJsonArrayIterator(const_cast<SJsonArrayNodeImpl*>(
                    static_cast<const SJsonArrayNodeImpl*>(
                            m_Impl.GetPointerOrNull())));

    default:
        NCBI_THROW(CJsonException, eInvalidNodeType,
                "Cannot iterate a non-container type");
    }
}

size_t CJsonNode::GetSize() const
{
    switch (m_Impl->m_NodeType) {
    case CJsonNode::eObject:
        return static_cast<const SJsonObjectNodeImpl*>(
                m_Impl.GetPointerOrNull())->m_Elements.size();
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

void CJsonNode::InsertAt(size_t index, CJsonNode::TInstance value)
{
    SJsonArrayNodeImpl* impl(m_Impl->GetArrayNodeImpl("SetAt()"));

    impl->VerifyIndexBounds("InsertAt()", index);

    impl->m_Array.insert(impl->m_Array.begin() + index, TJsonNodeRef(value));
}

void CJsonNode::SetAt(size_t index, CJsonNode::TInstance value)
{
    SJsonArrayNodeImpl* impl(m_Impl->GetArrayNodeImpl("SetAt()"));

    impl->VerifyIndexBounds("SetAt()", index);

    impl->m_Array[index] = value;
}

void CJsonNode::DeleteAt(size_t index)
{
    SJsonArrayNodeImpl* impl(m_Impl->GetArrayNodeImpl("DeleteAt()"));

    impl->VerifyIndexBounds("DeleteAt()", index);

    impl->m_Array.erase(impl->m_Array.begin() + index);
}

CJsonNode CJsonNode::GetAt(size_t index) const
{
    const SJsonArrayNodeImpl* impl(m_Impl->GetArrayNodeImpl("GetAt()"));

    impl->VerifyIndexBounds("GetAt()", index);

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
    SJsonObjectNodeImpl* impl(m_Impl->GetObjectNodeImpl("SetByKey()"));

    pair<TJsonObjectElements::iterator, bool> insertion =
            impl->m_Elements.insert(SJsonObjectElement(key, NULL));

    SJsonObjectElement* element =
            &const_cast<SJsonObjectElement&>(*insertion.first);

    element->m_Node = value;

    if (insertion.second) {
        element->m_Order = impl->m_NextElementOrder++;
        impl->m_ElementOrder.insert(element);
    }
}

void CJsonNode::DeleteByKey(const string& key)
{
    SJsonObjectNodeImpl* impl(m_Impl->GetObjectNodeImpl("DeleteByKey()"));

    TJsonObjectElements::iterator it =
            impl->m_Elements.find(SJsonObjectElement(key, NULL));

    if (it != impl->m_Elements.end()) {
        impl->m_ElementOrder.erase(&const_cast<SJsonObjectElement&>(*it));
        impl->m_Elements.erase(it);
    }
}

bool CJsonNode::HasKey(const string& key) const
{
    const SJsonObjectNodeImpl* impl(m_Impl->GetObjectNodeImpl("HasKey()"));

    return impl->m_Elements.find(SJsonObjectElement(key, NULL)) !=
            impl->m_Elements.end();
}

CJsonNode CJsonNode::GetByKeyOrNull(const string& key) const
{
    const SJsonObjectNodeImpl* impl(m_Impl->GetObjectNodeImpl("GetByKey()"));

    TJsonObjectElements::const_iterator it =
            impl->m_Elements.find(SJsonObjectElement(key, NULL));

    if (it == impl->m_Elements.end())
        return CJsonNode();

    return const_cast<SJsonNodeImpl*>(it->m_Node.GetPointerOrNull());
}

const string CJsonNode::AsString() const
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

static void s_Repr_Value(string& os, const CJsonNode& node,
                         CJsonNode::TReprFlags flags);

static void s_Repr_Object(string& os, const CJsonNode& node,
                          CJsonNode::TReprFlags flags)
{
    CJsonIterator it = node.Iterate();
    if (it) {
        os.append(1, '"')
          .append(it.GetKey())
          .append("\": ");
        s_Repr_Value(os, *it, flags);
        while (++it) {
            os.append(", \"")
              .append(it.GetKey())
              .append("\": ");
            s_Repr_Value(os, *it, flags);
        }
    }
}

static void s_Repr_Array(string& os, const CJsonNode& node,
                         CJsonNode::TReprFlags flags)
{
    CJsonIterator it = node.Iterate();
    if (it) {
        s_Repr_Value(os, *it, flags);
        while (++it) {
            os.append(", ");
            s_Repr_Value(os, *it, flags);
        }
    }
}

static void s_Repr_Value(string& os, const CJsonNode& node,
                         CJsonNode::TReprFlags flags)
{
    switch (node.GetNodeType()) {
    case CJsonNode::eObject:
        os.append(1, '{');
        s_Repr_Object(os, node, flags);
        os.append(1, '}');
        break;
    case CJsonNode::eArray:
        os.append(1, '[');
        s_Repr_Array(os, node, flags);
        os.append(1, ']');
        break;
    case CJsonNode::eString:
        if (flags & CJsonNode::fStandardJson) {
            os.append(NStr::JsonEncode(node.AsString(), NStr::eJsonEnc_Quoted));
        } else {
            os.append(1, '"');
            os.append(NStr::PrintableString(node.AsString()));
            os.append(1, '"');
        }
        break;
    case CJsonNode::eInteger:
        os.append(NStr::NumericToString(node.AsInteger()));
        break;
    case CJsonNode::eDouble:
        os.append(NStr::DoubleToString(node.AsDouble()));
        break;
    case CJsonNode::eBoolean:
        os.append(node.AsBoolean() ? "true" : "false");
        break;
    default: /* case CJsonNode::eNull: */
        os.append("null");
    }
}

string CJsonNode::Repr(TReprFlags flags) const
{
    string          os;

    switch (GetNodeType()) {
    case CJsonNode::eObject:
        if (flags & fOmitOutermostBrackets)
            s_Repr_Object(os, *this, flags);
        else {
            os.append(1, '{');
            s_Repr_Object(os, *this, flags);
            os.append(1, '}');
        }
        break;
    case CJsonNode::eArray:
        if (flags & fOmitOutermostBrackets)
            s_Repr_Array(os, *this, flags);
        else {
            os.append(1, '[');
            s_Repr_Array(os, *this, flags);
            os.append(1, ']');
        }
        break;
    case CJsonNode::eString:
        if (flags & fVerbatimIfString)
            return (static_cast<const SJsonStringNodeImpl*>(
                    m_Impl.GetPointerOrNull())->m_String);
        /* FALL THROUGH */
    default:
        s_Repr_Value(os, *this, flags);
    }

    return os;
}

#define INVALID_FORMAT_ERROR() \
    NCBI_THROW2(CStringException, eFormat, \
            (*m_Ch == '\0' ? "Unexpected end of output" : \
                    "Syntax error in structured output"), \
            GetPosition())

CJsonNode CJsonParser::ParseJSON(const string& json)
{
    m_Ch = (m_NSOutput = json).c_str();

    while (isspace((unsigned char) *m_Ch))
        ++m_Ch;

    CJsonNode root;

    switch (*m_Ch) {
    case '[':
        ++m_Ch;
        root = ParseArray(']');
        break;

    case '{':
        ++m_Ch;
        root = ParseObject('}');
        break;

    default:
        INVALID_FORMAT_ERROR();
    }

    while (isspace((unsigned char) *m_Ch))
        ++m_Ch;

    if (*m_Ch != '\0') {
        INVALID_FORMAT_ERROR();
    }

    return root;
}

string CJsonParser::ParseString(size_t max_len)
{
    const bool standard_json = m_Flags & CJsonNode::fStandardJson;
    auto str = CTempString(m_Ch, max_len);
    size_t len;
    auto val = standard_json ? NStr::JsonDecode(std::move(str), &len) : NStr::ParseQuoted(std::move(str), &len);

    m_Ch += len;
    return val;
}

Int8 CJsonParser::ParseInt(size_t len)
{
    Int8 val = NStr::StringToInt8(CTempString(m_Ch, len));

    if (*m_Ch == '-') {
        ++m_Ch;
        --len;
    }
    if (*m_Ch == '0' && len > 1) {
        NCBI_THROW2(CStringException, eFormat,
                "Leading zeros are not allowed", GetPosition());
    }

    m_Ch += len;
    return val;
}

double CJsonParser::ParseDouble(size_t len)
{
    double val = NStr::StringToDouble(CTempString(m_Ch, len));

    m_Ch += len;
    return val;
}

bool CJsonParser::MoreNodes()
{
    while (isspace((unsigned char) *m_Ch))
        ++m_Ch;
    if (*m_Ch != ',')
        return false;
    while (isspace((unsigned char) *++m_Ch))
        ;
    return true;
}

CJsonNode CJsonParser::ParseObject(char closing_char)
{
    CJsonNode result(CJsonNode::NewObjectNode());

    while (isspace((unsigned char) *m_Ch))
        ++m_Ch;

    if (*m_Ch == closing_char) {
        ++m_Ch;
        return result;
    }

    while (*m_Ch == '\'' || *m_Ch == '"') {
        // New attribute/value pair
        string attr_name(ParseString(GetRemainder()));

        while (isspace((unsigned char) *m_Ch))
            ++m_Ch;
        if (*m_Ch == ':' || *m_Ch == '=')
            while (isspace((unsigned char) *++m_Ch))
                ;

        result.SetByKey(attr_name, ParseValue());

        if (!MoreNodes()) {
            if (*m_Ch != closing_char)
                break;
            ++m_Ch;
            return result;
        }
    }

    INVALID_FORMAT_ERROR();
}

CJsonNode CJsonParser::ParseArray(char closing_char)
{
    CJsonNode result(CJsonNode::NewArrayNode());

    while (isspace((unsigned char) *m_Ch))
        ++m_Ch;

    if (*m_Ch == closing_char) {
        ++m_Ch;
        return result;
    }

    do
        result.Append(ParseValue());
    while (MoreNodes());

    if (*m_Ch == closing_char) {
        ++m_Ch;
        return result;
    }

    INVALID_FORMAT_ERROR();
}

CJsonNode CJsonParser::ParseValue()
{
    size_t max_len = GetRemainder();
    size_t len = 0;

    switch (*m_Ch) {
    /* Array */
    case '[':
        ++m_Ch;
        return ParseArray(']');

    /* Object */
    case '{':
        ++m_Ch;
        return ParseObject('}');

    /* String */
    case '\'':
    case '"':
        return CJsonNode::NewStringNode(ParseString(max_len));

    /* Number */
    case '-':
        // Check that there's at least one digit after the minus sign.
        if (max_len <= 1 || !isdigit((unsigned char) m_Ch[1])) {
            ++m_Ch;
            break;
        }
        len = 1;

    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        // Skim through the integer part.
        do
            if (++len >= max_len)
                return CJsonNode::NewIntegerNode(ParseInt(len));
        while (isdigit((unsigned char) m_Ch[len]));

        // Stumbled upon a non-digit character -- check
        // if it's a fraction part or an exponent part.
        switch (m_Ch[len]) {
        case '.':
            if (++len == max_len || !isdigit((unsigned char) m_Ch[len])) {
                NCBI_THROW2(CStringException, eFormat,
                        "At least one digit after the decimal "
                        "point is required", GetPosition());
            }
            for (;;) {
                if (++len == max_len)
                    return CJsonNode::NewDoubleNode(ParseDouble(len));

                if (!isdigit((unsigned char) m_Ch[len])) {
                    if (m_Ch[len] == 'E' || m_Ch[len] == 'e')
                        break;

                    return CJsonNode::NewDoubleNode(ParseDouble(len));
                }
            }
            /* FALL THROUGH */

        case 'E':
        case 'e':
            if (++len == max_len ||
                    (m_Ch[len] == '-' || m_Ch[len] == '+' ?
                            ++len == max_len ||
                                    !isdigit((unsigned char) m_Ch[len]) :
                            !isdigit((unsigned char) m_Ch[len]))) {
                m_Ch += len;
                NCBI_THROW2(CStringException, eFormat,
                        "Invalid exponent specification", GetPosition());
            }
            while (++len < max_len && isdigit((unsigned char) m_Ch[len]))
                ;
            return CJsonNode::NewDoubleNode(ParseDouble(len));

        default:
            return CJsonNode::NewIntegerNode(ParseInt(len));
        }

    /* Constant */
    case 'F': case 'f': case 'N': case 'n':
    case 'T': case 't': case 'Y': case 'y':
        while (len <= max_len && isalpha((unsigned char) m_Ch[len]))
            ++len;

        {
            CTempString val(m_Ch, len);
            m_Ch += len;
            return val == "null" ? CJsonNode::NewNullNode() :
                CJsonNode::NewBooleanNode(NStr::StringToBool(val));
        }
    }

    INVALID_FORMAT_ERROR();
}

CJsonNode CJsonNode::ParseObject(const string& json, TParseFlags flags)
{
    return CJsonParser(flags).ParseObject(json);
}

CJsonNode CJsonNode::ParseArray(const string& json, TParseFlags flags)
{
    return CJsonParser(flags).ParseArray(json);
}

CJsonNode CJsonNode::ParseJSON(const string& json, TParseFlags flags)
{
    return CJsonParser(flags).ParseJSON(json);
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

#define EOM_MARKER '\n'

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

                if (control_symbol == EOM_MARKER) {
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
        // because if it was a non-container node then this method
        // wouldn't have been called.
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
            if (!m_CurrentOutputNode.m_Iterator) { // TODO surround in a loop
                x_PopNode();
                if (!m_UTTPWriter.SendControlSymbol('}'))
                    return false;
            } else {
                if (!m_SendHashValue) {
                    m_Key = m_CurrentOutputNode.m_Iterator.GetKey();
                    if (!m_UTTPWriter.SendChunk(
                            m_Key.data(), m_Key.length(), false)) {
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

    m_UTTPWriter.SendControlSymbol(EOM_MARKER);
    return true;
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
            const string& str(static_cast<const SJsonStringNodeImpl*>(
                (const SJsonNodeImpl*) node)->m_String);
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
    if (m_CurrentOutputNode.m_Node)
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
