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

#include "netcomponent.hpp"

#include <util/uttp.hpp>
#include <util/util_exception.hpp>

BEGIN_NCBI_SCOPE

/// Exception class for use by CJsonNode.
class NCBI_XCONNECT_EXPORT CJsonException : public CException
{
public:
    enum EErrCode {
        eInvalidNodeType,   ///< Operation is not valid for this node type
        eIndexOutOfRange,   ///< JSON array index exceeds the maximum index
        eKeyNotFound,       ///< No such key in the object node
    };

    virtual const char* GetErrCodeString() const;

    NCBI_EXCEPTION_DEFAULT(CJsonException, CException);
};

///< @internal
struct SJsonNodeImpl;

/// This interface should not be used directly.
/// @internal
/// @see CJsonIterator
struct NCBI_XCONNECT_EXPORT SJsonIteratorImpl : public CObject
{
    virtual SJsonNodeImpl* GetNode() const = 0;
    virtual string GetKey() const = 0;
    virtual bool Next() = 0;
    virtual bool IsValid() const = 0;
};

/// JSON node abstraction.
class NCBI_XCONNECT_EXPORT CJsonNode
{
    NCBI_NET_COMPONENT(JsonNode);

    /// Create a new JSON object node.
    static CJsonNode NewObjectNode();

    /// Create a new JSON array node.
    static CJsonNode NewArrayNode();

    /// Create a new JSON string node.
    static CJsonNode NewStringNode(const string& value) { return value; }

    /// Create a new JSON integer node.
    static CJsonNode NewIntegerNode(Int8 value) { return value; }

    /// Create a new JSON double node.
    static CJsonNode NewDoubleNode(double value) { return value; }

    /// Create a new JSON boolean node.
    static CJsonNode NewBooleanNode(bool value) { return value; }

    /// Create a new JSON null node.
    static CJsonNode NewNullNode();

    /// Guess the type of a JSON scalar from the string
    /// representation of its value and initialize a new
    /// node with this value.
    static CJsonNode GuessType(const CTempString& value);

    /// Create new JSON string node.
    CJsonNode(const string& value);
    CJsonNode(const char* value);

    /// Create new JSON integer node.
    CJsonNode(int value);
    CJsonNode(Int8 value);

    /// Create new JSON double node.
    CJsonNode(double value);

    /// Create new JSON boolean node.
    CJsonNode(bool value);

    /// JSON node type.
    enum ENodeType {
        eObject,
        eArray,
        eString,
        eInteger,
        eDouble,
        eBoolean,
        eNull
    };

    /// Create new JSON node (type depends on the argument)
    CJsonNode(ENodeType type);

    /// Return a ENodeType constant identifying the node type.
    ENodeType GetNodeType() const;

    /// Return a string identifying the node type.
    string GetTypeName() const;

    /// Return true for a JSON object. Return false otherwise.
    bool IsObject() const;

    /// Return true for a JSON array. Return false otherwise.
    bool IsArray() const;

    /// Return true for a string node. Return false otherwise.
    bool IsString() const;

    /// Return true for an integer node. Return false otherwise.
    bool IsInteger() const;

    /// Return true for a double node. Return false otherwise.
    bool IsDouble() const;

    /// Return true for a boolean node. Return false otherwise.
    bool IsBoolean() const;

    /// Return true for a null node. Return false otherwise.
    bool IsNull() const;

    /// Return true if the node is either an object or an array.
    bool IsContainer() const;

    /// Return true if the node is any of the JSON scalar types.
    bool IsScalar() const;

    /// Different modes of array and object iteration.
    enum EIterationMode {
        eNatural,   ///< Iterate array elements in ascending order
                    ///< of their indices; iterate object elements
                    ///< in the order they were added.
        eOrdered,   ///< Iterate object elements in lexicographic
                    ///< order of their keys.
        eFlatten,   ///< Iterate nested containers as if it were a
                    ///< single JSON object.
    };

    /// For a container node (that is, either an array or an object),
    /// begin iteration over its elements. The returned value must
    /// be used to initialize a CJsonIterator object.
    /// @see CJsonIterator
    SJsonIteratorImpl* Iterate(EIterationMode mode = eNatural) const;

    /// For a container node (that is, either an array or
    /// an object), return the number of elements in the container.
    size_t GetSize() const;

    /// For an array node, add a string node at the end of the array.
    void AppendString(const string& value);

    /// For an array node, add a integer node at the end of the array.
    void AppendInteger(Int8 value);

    /// For an array node, add a floating point node at the end of the array.
    void AppendDouble(double value);

    /// For an array node, add a boolean node at the end of the array.
    void AppendBoolean(bool value);

    /// For an array node, add a null node at the end of the array.
    void AppendNull();

    /// For an array node, add a new element at the end of the array.
    void Append(CJsonNode::TInstance value);

    /// For an array node, insert a new element at the specified position.
    void InsertAt(size_t index, CJsonNode::TInstance value);

    /// For an array node, set a new value for an existing element.
    /// Throw an exception if the index is out of range.
    void SetAt(size_t index, CJsonNode::TInstance value);

    /// Delete an element located at the specified index from a JSON array.
    /// Throw an exception if the index is out of range.
    void DeleteAt(size_t index);

    /// Return a JSON array element at the specified index.
    /// Throw an exception if the index is out of range.
    CJsonNode GetAt(size_t index) const;

    /// Set a JSON object element to the specified string value.
    void SetString(const string& key, const string& value);

    /// Set a JSON object element to the specified integer value.
    void SetInteger(const string& key, Int8 value);

    /// Set a JSON object element to the specified floating point value.
    void SetDouble(const string& key, double value);

    /// Set a JSON object element to the specified boolean value.
    void SetBoolean(const string& key, bool value);

    /// Set a JSON object element to the specified null value.
    void SetNull(const string& key);

    /// For a JSON object node, insert a new element or update
    /// an existing element.
    void SetByKey(const string& key, CJsonNode::TInstance value);

    /// Delete an element referred to by the specified key from a JSON object.
    void DeleteByKey(const string& key);

    /// Check if an object node has an element accessible by
    /// the specified key.
    bool HasKey(const string& key) const;

    /// For a JSON object node, return the value associated with
    /// the specified key. Throw an exception if there is no
    /// such key in this object.
    CJsonNode GetByKey(const string& key) const;

    /// For a JSON object node, return the value associated with
    /// the specified key. Return NULL if there is no such key
    /// in this object.
    CJsonNode GetByKeyOrNull(const string& key) const;

    /// For a JSON object node, return the string referred to
    /// by the specified key. Throw an exception if the key
    /// does not exist or refers to a non-string node.
    string GetString(const string& key) const;

    /// For a JSON object node, return the integer referred to
    /// by the specified key. Throw an exception if the key
    /// does not exist or refers to a non-numeric node.
    Int8 GetInteger(const string& key) const;

    /// For a JSON object node, return the floating point number
    /// referred to by the specified key. Throw an exception
    /// if the key does not exist or refers to a non-numeric node.
    double GetDouble(const string& key) const;

    /// For a JSON object node, return the boolean referred to
    /// by the specified key. Throw an exception if the key
    /// does not exist or refers to a non-boolean node.
    bool GetBoolean(const string& key) const;

    /// Provided that this is a string node, return
    /// the string value of this node.
    const string AsString() const;

    /// Provided that this is a numeric node (that is, either
    /// an integer or a floating point node), return the value
    /// of this node as an integer number.
    Int8 AsInteger() const;

    /// Provided that this is a numeric node (that is, either
    /// a floating point or an integer node), return the value
    /// of this node as a floating point number.
    double AsDouble() const;

    /// Provided that this is a boolean node, return
    /// the boolean value of this node.
    bool AsBoolean() const;

    /// String representation flags.
    enum EReprFlags {
        fVerbatimIfString = 1 << 0,
        fOmitOutermostBrackets = 1 << 1,
    };
    /// Binary OR of EReprFlags.
    typedef int TReprFlags;

    /// Return a string representation of this node.
    string Repr(TReprFlags flags = 0) const;

    static CJsonNode ParseObject(const string& json);
    static CJsonNode ParseArray(const string& json);
    static CJsonNode ParseJSON(const string& json);
};

/// Iterator for JSON arrays and objects.
/// @see CJsonNode::Iterate()
class NCBI_XCONNECT_EXPORT CJsonIterator
{
    NCBI_NET_COMPONENT(JsonIterator);

    /// Return the value of the current element.
    CJsonNode GetNode() const;

    /// When iterating over a JSON object, return
    /// the key of the current element.
    string GetKey() const;

    /// Skip to the next element if there is one, in which
    /// case TRUE is returned. Otherwise, return FALSE.
    bool Next();

    /// Return true if this iterator is still valid.
    bool IsValid() const;

    /// An alternative way to get the value of the current element.
    CJsonNode operator *() const;

    /// An operator version of Next().
    CJsonIterator& operator ++();

    /// An operator version of IsValid().
    operator bool() const;

    /// An operator version of IsValid().
    operator bool();
};

inline bool CJsonNode::IsObject() const
{
    return GetNodeType() == eObject;
}

inline bool CJsonNode::IsArray() const
{
    return GetNodeType() == eArray;
}

inline bool CJsonNode::IsString() const
{
    return GetNodeType() == eString;
}

inline bool CJsonNode::IsInteger() const
{
    return GetNodeType() == eInteger;
}

inline bool CJsonNode::IsDouble() const
{
    return GetNodeType() == eDouble;
}

inline bool CJsonNode::IsBoolean() const
{
    return GetNodeType() == eBoolean;
}

inline bool CJsonNode::IsNull() const
{
    return GetNodeType() == eNull;
}

inline bool CJsonNode::IsContainer() const
{
    return GetNodeType() <= eArray;
}

inline bool CJsonNode::IsScalar() const
{
    return GetNodeType() > eArray;
}

inline CJsonNode CJsonNode::GetByKey(const string& key) const
{
    CJsonNode node(GetByKeyOrNull(key));

    if (node)
        return node;

    NCBI_THROW_FMT(CJsonException, eKeyNotFound,
            "GetByKey(): no such key \"" << key << '\"');
}

inline string CJsonNode::GetString(const string& key) const
{
    return GetByKey(key).AsString();
}

inline Int8 CJsonNode::GetInteger(const string& key) const
{
    return GetByKey(key).AsInteger();
}

inline double CJsonNode::GetDouble(const string& key) const
{
    return GetByKey(key).AsDouble();
}

inline bool CJsonNode::GetBoolean(const string& key) const
{
    return GetByKey(key).AsBoolean();
}

inline CJsonNode CJsonIterator::GetNode() const
{
    return m_Impl->GetNode();
}

inline string CJsonIterator::GetKey() const
{
    return m_Impl->GetKey();
}

inline bool CJsonIterator::Next()
{
    return m_Impl->Next();
}

inline bool CJsonIterator::IsValid() const
{
    return m_Impl->IsValid();
}

inline CJsonNode CJsonIterator::operator *() const
{
    return GetNode();
}

inline CJsonIterator& CJsonIterator::operator ++()
{
    m_Impl->Next();
    return *this;
}

inline CJsonIterator::operator bool() const
{
    return m_Impl->IsValid();
}

inline CJsonIterator::operator bool()
{
    return m_Impl->IsValid();
}

/// Exception class for use by CJsonNode.
class NCBI_XCONNECT_EXPORT CJsonOverUTTPException : public CException
{
public:
    enum EErrCode {
        eUTTPFormatError,
        eChunkContinuationExpected,
        eUnexpectedEOM,
        eUnexpectedTrailingToken,
        eObjectKeyMustBeString,
        eUnexpectedClosingBracket,
        eUnknownControlSymbol,
    };

    virtual const char* GetErrCodeString() const;

    NCBI_EXCEPTION_DEFAULT(CJsonOverUTTPException, CException);
};

class NCBI_XCONNECT_EXPORT CJsonOverUTTPReader
{
public:
    typedef list<CJsonNode> TNodeStack;

    CJsonOverUTTPReader();
    bool ReadMessage(CUTTPReader& reader);
    const CJsonNode GetMessage() const;
    void Reset();

private:
    bool x_AddNewNode(CJsonNode::TInstance new_node);

    enum {
        eExpectNextToken,
        eReadingString,
        eReadingDouble,
        eMessageComplete
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

class NCBI_XCONNECT_EXPORT CJsonOverUTTPWriter
{
public:
    CJsonOverUTTPWriter(CUTTPWriter& writer) : m_UTTPWriter(writer)
    {
    }

    bool WriteMessage(const CJsonNode& root_node);
    bool CompleteMessage();
    void GetOutputBuffer(const char** output_buffer,
            size_t* output_buffer_size);
    bool NextOutputBuffer();

private:
    struct SOutputStackFrame {
        CJsonNode m_Node;
        CJsonIterator m_Iterator;
    };

    typedef list<SOutputStackFrame> TOutputStack;

    bool x_SendNode(const CJsonNode& node);
    void x_PushNode(const CJsonNode& node);
    void x_PopNode();

    CUTTPWriter& m_UTTPWriter;

    TOutputStack m_OutputStack;
    SOutputStackFrame m_CurrentOutputNode;
    double m_Double;
    bool m_SendHashValue;
};

inline void CJsonOverUTTPWriter::GetOutputBuffer(
        const char** output_buffer, size_t* output_buffer_size)
{
    m_UTTPWriter.GetOutputBuffer(output_buffer, output_buffer_size);
}

inline bool CJsonOverUTTPWriter::NextOutputBuffer()
{
    return m_UTTPWriter.NextOutputBuffer();
}

END_NCBI_SCOPE

#endif /* JSON_OVER_UTTP__HPP */
