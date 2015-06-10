#ifdef MISC_JSONWRAPP___JSONWRAPP_OLD__HPP
#ifdef __GNUC__
#  error "Both jsonwrapp_old.hpp and jsonwrapp.hpp are used; please use only one of them"
#endif // __GNUC__
#if NCBI_COMPILER_MSVC
#error "Both jsonwrapp_old.hpp and jsonwrapp.hpp are used; please use only one of them"
#endif 
#endif


#ifndef MISC_JSONWRAPP___JSONWRAPP__HPP
#define MISC_JSONWRAPP___JSONWRAPP__HPP

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
*  Government have not placed any restriction on its use or reproduction.
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
* Author: Andrei Gourianov
*
*   File Description:
*       Wrapper API to work with JSON data
*       http://www.ietf.org/rfc/rfc4627.txt
*
*   Internally, data of any type is stored in a universal container.
*   We define different API classes here for the sake of semantics only.
*
*   This implementation uses object adapter pattern, in which adapter contains
*   a pointer  to an instance of a class it wraps.
*   Please note, objects of classes defined here act like pointers,
*   ie, creating, or copying them does not create any data, their destruction
*   does not destroy any data either.
*
*   We prohibit creation of standalone JSON value object;
*   only document objects can be created and copied. That is,
*   all values are associated with a specific document only.
*   When a document is destroyed, all its values are destroyed as well.
*   So, to create a value, one should add it into a document (or into JSON
*   array or object) and get a proper adapter object.
* 
*   Classes that store JSON value:
*       CJson_ConstNode,   CJson_Node     -- base class;
*       CJson_ConstValue,  CJson_Value    -- primitive type data 
*                                            (string, number, boolean, null);
*       CJson_ConstArray,  CJson_Array    -- JSON array;
*       CJson_ConstObject, CJson_Object   -- JSON object;
*       CJson_Document -- serializable JSON data container - array or object.
*
*   Sequential access parsing event listener:
*       CJson_WalkHandler -- define your own class derived from this one.
*/

#include <corelib/ncbistr.hpp>
#include <corelib/ncbidbg.hpp>
#include <iterator>

#define RAPIDJSON_NOMEMBERITERATORCLASS
#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/filewritestream.h"
#include "rapidjson/error/en.h"


BEGIN_NCBI_SCOPE

class CJson_Document;
class CJson_ConstValue;
class CJson_Value;
class CJson_ConstArray;
class CJson_Array;
class CJson_ConstObject;
class CJson_Object;

/////////////////////////////////////////////////////////////////////////////
///
/// CJson_Node
///
/// Container for JSON value.
/// A JSON value must be an object, array, number, or string, or one of
/// the following three literal names: false null true.
/// The class provides basic access methods only.
/// To get access to value data, one should "get" (cast) it as
/// Value, Array or Object and use an appropriate API.

class CJson_ConstNode
{
protected:
    typedef rapidjson::Value _Impl;

public:
    typedef char TCharType;
    typedef ncbi::CStringUTF8 TStringType;
    typedef ncbi::CStringUTF8 TKeyType;

public:
    /// Value type
    enum EJsonType {
        eNull,      ///< null
        eBool,      ///< bool
        eNumber,    ///< number
        eString,    ///< string
        eArray,     ///< array
        eObject     ///< object
    };
    /// Get value type
    EJsonType GetType(void) const;

    bool IsNull(   void) const;
    bool IsValue(  void) const;
    bool IsArray(  void) const;
    bool IsObject( void) const;

    /// Get JSON value contents of the node
    CJson_ConstValue GetValue(void) const;

    /// Get JSON array contents of the node
    CJson_ConstArray GetArray(void) const;

    /// Get JSON object contents of the node
    CJson_ConstObject GetObject(void) const;

    /// Convert the contents of the node into string
    std::string ToString(void) const;

    ~CJson_ConstNode(void) {}
    /// Note: this does not copy Node data
    /// Instead, both Node objects will point to the same data
    CJson_ConstNode(const CJson_ConstNode& n);
    /// Note: this does not copy Node data
    /// Instead, both Node object will point to the same data
    CJson_ConstNode& operator=(const CJson_ConstNode& n);

    bool operator!=(const CJson_ConstNode& n) const;
    bool operator==(const CJson_ConstNode& n) const;

protected:
    CJson_ConstNode(void) : m_Impl(0) {
    }
    CJson_ConstNode(_Impl* impl) : m_Impl(impl) {
    }
    _Impl* m_Impl;
    static _Impl*& x_Impl(CJson_ConstNode& v){
        return v.m_Impl;
    }
    friend class CJson_Node;
    friend class CJson_ConstValue;
    friend class CJson_Array;
    friend class CJson_ConstArray;
    friend class CJson_ConstObject;
    friend class CJson_Object;
    friend class CJson_WalkHandler;
    friend class CJson_Document;
    friend class CJson_ConstObject_pair;
    friend class CJson_Object_pair;
};

/////////////////////////////////////////////////////////////////////////////

class CJson_Node : virtual public CJson_ConstNode
{
public:
    /// Erase node data and convert it into JSON NULL value
    CJson_Node& SetNull(void);

    /// Erase node data and convert it into JSON value
    CJson_Value ResetValue(void);

    /// Get JSON value contents of the node
    CJson_Value SetValue(void);

    /// Erase node data and convert it into JSON array
    CJson_Array ResetArray(void);

    /// Get JSON array contents of the node
    CJson_Array SetArray(void);

    /// Erase node data and convert it into JSON object
    CJson_Object ResetObject(void);

    /// Get JSON object contents of the node
    CJson_Object SetObject(void);

    ~CJson_Node(void) {}
    /// Note: this does not copy Node data
    /// Instead, both Node objects will point to the same data
    CJson_Node(const CJson_Node& n);
    /// Note: this does not copy Node data
    /// Instead, both Node objects will point to the same data
    CJson_Node& operator=(const CJson_Node& n);

    /// Copy Node contents data into this node
    CJson_Node& AssignCopy(const CJson_ConstNode& n);

protected:
    CJson_Node(void) {
    }
    CJson_Node(_Impl* impl) : CJson_ConstNode(impl) {
        m_Impl = impl;
    }
    friend class CJson_Value;
    friend class CJson_Array;
    friend class CJson_ConstArray;
    friend class CJson_Object;
    friend class CJson_ConstObject;
    friend class CJson_Document;
    friend class CJson_ConstObject_pair;
    friend class CJson_Object_pair;
};


/////////////////////////////////////////////////////////////////////////////
///
/// CJson_Value
///
/// Standard type JSON value.

class CJson_ConstValue : virtual public CJson_ConstNode
{
public:
    /// Test if value type is compatible with C++ type
    bool IsBool(   void) const;
    bool IsNumber( void) const;
    bool IsInt4(   void) const;
    bool IsUint4(  void) const;
    bool IsInt8(   void) const;
    bool IsUint8(  void) const;
    bool IsDouble( void) const;
    bool IsString( void) const;

    /// Get primitive value data
    bool   GetBool(   void) const;
    Int4   GetInt4(   void) const;
    Uint4  GetUint4(  void) const;
    Int8   GetInt8(   void) const;
    Uint8  GetUint8(  void) const;
    double GetDouble( void) const;
    TStringType GetString(void) const;
    size_t GetStringLength(void) const;

    ~CJson_ConstValue(void) {}
    /// Note: this does not copy Node data
    /// Instead, both Node objects will point to the same data
    CJson_ConstValue(const CJson_ConstValue& n);
    /// Note: this does not copy Node data
    /// Instead, both Node objects will point to the same data
    CJson_ConstValue& operator=(const CJson_ConstValue& n);

protected:
    CJson_ConstValue(void) {}
    CJson_ConstValue(_Impl* impl) : CJson_ConstNode(impl) {
        m_Impl = impl;
    }
    friend class CJson_ConstNode;
    friend class CJson_WalkHandler;
};

/////////////////////////////////////////////////////////////////////////////

class CJson_Value : public CJson_ConstValue, public CJson_Node
{
public:
    /// Set primitive value data
    CJson_Value& SetBool(   bool  value);
    CJson_Value& SetInt4(   Int4  value);
    CJson_Value& SetUint4( Uint4  value);
    CJson_Value& SetInt8(   Int8  value);
    CJson_Value& SetUint8( Uint8  value);
    CJson_Value& SetDouble(double value);
    CJson_Value& SetString(const TStringType& value);

    ~CJson_Value(void) {}
    /// Note: this does not copy Node data
    /// Instead, both Node objects will point to the same data
    CJson_Value(const CJson_Value& n);
    /// Note: this does not copy Node data
    /// Instead, both Node objects will point to the same data
    CJson_Value& operator=(const CJson_Value& n);

protected:
    CJson_Value(void) {
    }
    CJson_Value( _Impl* impl) : CJson_Node(impl) {
        m_Impl = impl;
    }
    friend class CJson_Node;
};


/////////////////////////////////////////////////////////////////////////////
///
/// CJson_Array
///
/// JSON array is an ordered sequence of zero or more values.
/// The class provides API to populate and explore a JSON array

class CJson_ConstArray : virtual public CJson_ConstNode
{
protected:
    typedef rapidjson::Value::ValueIterator      _ImplIterator;
    typedef rapidjson::Value::ConstValueIterator _ImplCIterator;

public:
    typedef CJson_ConstNode   value_type;
    typedef CJson_ConstNode&  const_reference;

    /// Random-access iterator to access const JSON array element.
    /// It is designed to resemble std::vector::const_iterator class.
    class const_iterator {
    public:
        typedef std::random_access_iterator_tag iterator_category;
        typedef CJson_ConstNode                 value_type;
        typedef std::ptrdiff_t                  difference_type;
        typedef std::ptrdiff_t                  distance_type;
        typedef CJson_ConstNode*                pointer;
        typedef CJson_ConstNode&                reference;

        ~const_iterator(void) {}
        const_iterator(void);
        const_iterator(const const_iterator& vi);
        const_iterator& operator=(const const_iterator& vi);

        /// Comparison
        bool operator!=(const const_iterator& vi) const;
        bool operator==(const const_iterator& vi) const;
        bool operator<( const const_iterator& vi) const;
        bool operator<=(const const_iterator& vi) const;
        bool operator>( const const_iterator& vi) const;
        bool operator>=(const const_iterator& vi) const;

        /// Increment and decrement
        const_iterator& operator++(void);
        const_iterator& operator++(int);
        const_iterator& operator+=(int);
        const_iterator  operator+(int) const;
        const_iterator& operator--(void);
        const_iterator& operator--(int);
        const_iterator& operator-=(int);
        const_iterator  operator-(int) const;

        /// Dereference
        const CJson_ConstNode& operator*( void) const;
        const CJson_ConstNode* operator->(void) const;

        /// Distance
        distance_type operator-(const_iterator vi) const;
    protected:
        const_iterator(const _ImplCIterator vi);
        const_iterator(const _ImplIterator vi);
        _ImplIterator m_vi;
        mutable CJson_Node m_v;
        friend class CJson_ConstArray;
        friend class CJson_Array;
    };

    /// Random-access iterator to access non-const JSON array element.
    /// It is designed to resemble std::vector::iterator class.
    class iterator : public const_iterator {
    public:
        typedef std::random_access_iterator_tag iterator_category;
        typedef CJson_Node                      value_type;
        typedef std::ptrdiff_t                  difference_type;
        typedef std::ptrdiff_t                  distance_type;
        typedef CJson_Node*                     pointer;
        typedef CJson_Node&                     reference;

        ~iterator(void) {}
        iterator(void);
        iterator(const iterator& i);
        iterator& operator=(const iterator& vi);

        /// Increment and decrement
        iterator operator+(int) const;
        iterator operator-(int) const;

        /// Dereference
        CJson_Node& operator*(void) const;
        CJson_Node* operator->(void) const;

        /// Distance
        distance_type operator-(iterator vi) const;

    private:
        iterator(const _ImplIterator vi);
        friend class CJson_Array;
    };

public:

    /// Return the number of elements in the array
    size_t size(void) const;

    /// Return the number of elements that the array could contain without
    /// allocating more storage.
    size_t capacity(void) const;

    /// Test if the array is empty
    bool empty(void) const;

    /// Return a reference to the element at a specified location in the array
    /// If index is greater than or equal to the size of the array,
    /// the function throws std::out_of_range exception
    CJson_ConstNode at(size_t index) const;

    /// Return a reference to the element at a specified location in the array
    /// If index is greater than or equal to the size of the array,
    /// the result is undefined
    CJson_ConstNode operator[](size_t index) const;

    /// Return a reference to the first element in the array
    /// If the array is empty, the result is undefined
    CJson_ConstNode front(void) const;

    /// Return a reference to the last element of the array.
    /// If the array is empty, the result is undefined
    CJson_ConstNode back(void) const;

    /// Return a random-access iterator to the first element in the array
    const_iterator begin(void) const;

    /// Return a random-access iterator that points just beyond the end of
    /// the array
    const_iterator end(void) const;

    ~CJson_ConstArray(void) {}
    /// Note: this does not copy Node data
    /// Instead, both Node objects will point to the same data
    CJson_ConstArray(const CJson_ConstArray& n);
    /// Note: this does not copy Node data
    /// Instead, both Node objects will point to the same data
    CJson_ConstArray& operator=(const CJson_ConstArray& n);

protected:
    CJson_ConstArray(void)  {
    }
    CJson_ConstArray(_Impl* impl) : CJson_ConstNode(impl) {
        m_Impl = impl;
    }
    friend class CJson_ConstNode;
};

/////////////////////////////////////////////////////////////////////////////

class CJson_Array : public CJson_ConstArray, public CJson_Node
{
public:
    typedef CJson_Node        value_type;
    typedef CJson_Node&       reference;
    typedef CJson_ConstArray::const_iterator const_iterator;
    typedef CJson_ConstArray::iterator             iterator;

    /// Reserve a minimum length of storage for the array object
    void reserve(size_t count);

    /// Erase all elements of the array
    void clear(void);
    
    /// Remove an element
    iterator erase(const_iterator _where);

    /// Remove a range of elements
    iterator erase(const_iterator _first, const_iterator _last);

    /// Return a reference to the element at a specified location in the array
    /// If index is greater than or equal to the size of the array,
    /// the function throws std::out_of_range exception
    CJson_Node at(size_t index);

    /// Return a reference to the element at a specified location in the array
    /// If index is greater than or equal to the size of the array,
    /// the result is undefined
    CJson_Node operator[](size_t index);

    /// Return a reference to the first element in the array
    /// If the array is empty, the result is undefined
    CJson_Node front(void);

    /// Return a reference to the last element of the array.
    /// If the array is empty, the result is undefined
    CJson_Node back(void);

    /// Add null element to the end of the array.
    void push_back(void); //null value

    /// Add primitive type element to the end of the array.
#ifndef NCBI_COMPILER_WORKSHOP
    template <typename T> void push_back(const T&); // primitive and string
    template <typename T> void push_back(const T*);
#else
    void push_back(const bool& v);
    void push_back(const Int4& v);
    void push_back(const Uint4& v);
    void push_back(const Int8& v);
    void push_back(const Uint8& v);
    void push_back(const float& v);
    void push_back(const double& v);
    void push_back(const CJson_Node::TCharType* v);
    void push_back(const CJson_Node::TStringType& v);
    void push_back(const CJson_ConstNode& v);
#endif

    /// Add array type element to the end of the array.
    CJson_Array  push_back_array(void);

    /// Add object type element to the end of the array.
    CJson_Object push_back_object(void);

    /// Delete the element at the end of the array
    void pop_back(void);

    /// Return a random-access iterator to the first element in the array
    iterator begin(void) const;

    /// Return a random-access iterator that points just beyond the end of
    /// the array
    iterator end(void) const;

    ~CJson_Array(void) {}
    /// Note: this does not copy Node data
    /// Instead, both Node objects will point to the same data
    CJson_Array(const CJson_Array& n);
    /// Note: this does not copy Node data
    /// Instead, both Node objects will point to the same data
    CJson_Array& operator=(const CJson_Array& n);

protected:
    CJson_Array(void)  {
    }
    CJson_Array(_Impl* impl) : CJson_Node(impl) {
        m_Impl = impl;
    }
    friend class CJson_Node;
    friend class CJson_Object;
    template<class Type> class CProhibited {};
};

/////////////////////////////////////////////////////////////////////////////
///
/// CJson_ConstObject value type  - [name,value] pair.

class CJson_Object_pair;
class CJson_ConstObject_pair {
protected:
    typedef rapidjson::Value _Impl;
public:
    const CJson_Node::TCharType* name;
    const CJson_ConstNode value;

    ~CJson_ConstObject_pair(void) {}
    CJson_ConstObject_pair(void);
    CJson_ConstObject_pair(const CJson_Node::TCharType* _name, const _Impl& _value);
    CJson_ConstObject_pair(const CJson_Object_pair& p);
    CJson_ConstObject_pair& assign(
        const CJson_Node::TCharType* _name, const _Impl& _value);
};

/////////////////////////////////////////////////////////////////////////////
///
/// CJson_Object value type  - [name,value] pair.

class CJson_Object_pair {
protected:
    typedef rapidjson::Value _Impl;
public:
    const CJson_Node::TCharType* name;
    CJson_Node value;

    ~CJson_Object_pair(void) {}
    CJson_Object_pair(void);
    CJson_Object_pair(const CJson_Node::TCharType* _name, _Impl& _value);
    CJson_Object_pair& assign(const CJson_Node::TCharType* _name, _Impl& _value);
};

/////////////////////////////////////////////////////////////////////////////
///
/// CJson_Object
///
/// A JSON object is an unordered collection of name/value pairs.
/// The class provides API to populate and explore a JSON object

class CJson_ConstObject : virtual public CJson_ConstNode
{
protected:
    typedef rapidjson::Value::MemberIterator      _ImplIterator;
    typedef rapidjson::Value::ConstMemberIterator _ImplCIterator;

public:
    typedef CJson_ConstObject_pair  value_type;
    class iterator;

    /// Bidirectional iterator to access const JSON object element.
    /// It is designed to resemble std::map::const_iterator class.
    /// Dereferencing the iterator returns [name,value] pair.
    class const_iterator {
    public:
        typedef  CJson_ConstObject_pair pair;

        typedef std::bidirectional_iterator_tag           iterator_category;
        typedef CJson_ConstObject::const_iterator::pair   value_type;
        typedef std::ptrdiff_t                            difference_type;
        typedef std::ptrdiff_t                            distance_type;
        typedef CJson_ConstObject::const_iterator::pair*  pointer;
        typedef CJson_ConstObject::const_iterator::pair&  reference;

        ~const_iterator(void) {}
        const_iterator(void);
        const_iterator(const const_iterator& vi);
        const_iterator& operator=(const const_iterator& vi);
        const_iterator(const iterator& vi);
        const_iterator& operator=(const iterator& vi);

        /// Comparison
        bool operator!=(const const_iterator& vi) const;
        bool operator==(const const_iterator& vi) const;
        bool operator!=(const iterator& vi) const;
        bool operator==(const iterator& vi) const;

        /// Increment and decrement
        const_iterator& operator++(void);
        const_iterator& operator++(int);
        const_iterator& operator--(void);
        const_iterator& operator--(int);

        /// Dereference
        const pair& operator*(void) const;
        const pair* operator->(void) const;

    protected:
        const_iterator(const _ImplCIterator vi);
        const_iterator(const _ImplIterator vi);
        _ImplIterator m_vi;
        mutable pair m_pvi;
        friend class CJson_ConstObject;
        friend class CJson_Object;
        friend class iterator;
    };

    /// Bidirectional iterator to access non-const JSON object element.
    /// It is designed to resemble std::map::iterator class.
    /// Dereferencing the iterator returns [name,value] pair.
    class iterator {
    public:
        typedef CJson_Object_pair pair;

        typedef std::bidirectional_iterator_tag     iterator_category;
        typedef CJson_ConstObject::iterator::pair   value_type;
        typedef std::ptrdiff_t                      difference_type;
        typedef std::ptrdiff_t                      distance_type;
        typedef CJson_ConstObject::iterator::pair*  pointer;
        typedef CJson_ConstObject::iterator::pair&  reference;

        ~iterator(void) {}
        iterator(void);
        iterator(const iterator& i);
        iterator& operator=(const iterator& vi);

        /// Comparison
        bool operator!=(const iterator& vi) const;
        bool operator==(const iterator& vi) const;
        bool operator!=(const const_iterator& vi) const;
        bool operator==(const const_iterator& vi) const;

        /// Increment and decrement
        iterator& operator++(void);
        iterator& operator++(int);
        iterator& operator--(void);
        iterator& operator--(int);

        /// Dereference
        pair& operator*(void) const;
        pair* operator->(void) const;

    private:
        iterator(const _ImplIterator vi);
        _ImplIterator m_vi;
        mutable pair m_pvi;
        friend class CJson_ConstObject;
        friend class CJson_Object;
        friend class const_iterator;
    };

    /// Return the number of elements in the object
    size_t size(void) const;

    /// Test if the object is empty
    bool  empty(void) const;

    /// Access an element with a given name.
    /// If such element was not found, the function throws std::out_of_range exception
    CJson_ConstNode at(const CJson_Node::TKeyType& name) const;

    /// Access an element with a given name.
    /// If such element does not exist in this object, the result is undefined.
    CJson_ConstNode operator[](const CJson_Node::TKeyType& name) const;

    /// Return an iterator that points to the first element in the object
    const_iterator begin(void) const;

    /// Return an iterator that points to the location after the last element.
    const_iterator end(void) const;

    /// Return an iterator that points to the location of the element.
    const_iterator find(const CJson_Node::TKeyType& name) const;

    /// Test if an element with this name exists in the object
    bool has(const CJson_Node::TKeyType& name) const;

    ~CJson_ConstObject(void) {}
    /// Note: this does not copy Node data
    /// Instead, both Node objects will point to the same data
    CJson_ConstObject(const CJson_ConstObject& v);
    /// Note: this does not copy Node data
    /// Instead, both Node objects will point to the same data
    CJson_ConstObject& operator=(const CJson_ConstObject& v);

protected:
    CJson_ConstObject(void) {
    }
    CJson_ConstObject( _Impl* impl) : CJson_ConstNode(impl) {
        m_Impl = impl;
    }
    friend class CJson_ConstNode;
};

/////////////////////////////////////////////////////////////////////////////

class CJson_Object : public CJson_ConstObject, public CJson_Node
{
public:
    typedef CJson_ConstObject::const_iterator const_iterator;
    typedef CJson_ConstObject::iterator             iterator;
    typedef CJson_Object_pair                       value_type;

    /// Erase all elements of the object
    void clear(void);

    /// Remove an element with a given name from the object
    /// Returns the number of elements that have been removed
    size_t erase(const CJson_Node::TKeyType& name);

    /// Remove an element
    iterator erase(const_iterator _where);

    /// Remove a range of elements
    iterator erase(const_iterator _first, const_iterator _last);

    /// Access an element with a given name.
    /// If such element was not found, the function throws std::out_of_range exception
    CJson_Node at(const CJson_Node::TKeyType& name);

    /// Access an element with a given name.
    /// If such element does not exist in this object, it will be added.
    CJson_Node operator[](const CJson_Node::TKeyType& name);

    /// Insert null element into the object
    void insert(const CJson_Node::TKeyType& name);

    /// Insert primitive type element into the object
    template <typename T> void insert(const CJson_Node::TKeyType& name, const T&);
    template <typename T> void insert(const CJson_Node::TKeyType& name, const T*);

#ifdef NCBI_COMPILER_WORKSHOP
    void insert(const CJson_Node::TKeyType& name,
                const CJson_Node::TStringType& value);
#endif

    /// Insert array type element into the object
    CJson_Array  insert_array( const CJson_Node::TKeyType& name);

    /// Insert object type element into the object
    CJson_Object insert_object(const CJson_Node::TKeyType& name);

    /// Return an iterator that points to the first element in the object
    iterator begin(void) const;

    /// Return an iterator that points to the location after the last element.
    iterator end(void) const;

    /// Return an iterator that points to the location of the element.
    iterator find(const CJson_Node::TKeyType& name) const;

    ~CJson_Object(void) {}
    /// Note: this does not copy Node data
    /// Instead, both Node objects will point to the same data
    CJson_Object(const CJson_Object& v);
    /// Note: this does not copy Node data
    /// Instead, both Node objects will point to the same data
    CJson_Object& operator=(const CJson_Object& v);

protected:
    CJson_Object(void) {
    }
    CJson_Object(_Impl* impl) : CJson_Node(impl) {
        m_Impl = impl;
    }
    friend class CJson_Node;
    friend class CJson_Array;
    template<class Type> class CProhibited {};
};

/////////////////////////////////////////////////////////////////////////////
///
/// CJson_WalkHandler
///
/// Sequential access parsing event listener.
/// Provides a mechanism for reading data from a JSON file or document
/// as a series of events.

class CJson_WalkHandler : public rapidjson::BaseReaderHandler<>
{
public:
    CJson_WalkHandler(void);
    virtual ~CJson_WalkHandler(void) {}

    /// Begin reading object contents
    ///
    /// @param name
    ///   Name of this object in the parent object, or empty string
    /// if this object has no parent.
    virtual bool BeginObject(const CJson_Node::TKeyType& /*name*/) {
        return true;
    }
    
    /// Begin reading object member
    ///
    /// Right after this event, there can be one of the following only:
    /// EndObject, BeginObject, BeginArray, or PlainMemberValue.
    ///
    /// @param name
    ///   Name of this object in the parent object, or empty string
    ///   if this object has no parent.
    /// @param member
    ///   Member name
    virtual bool BeginObjectMember(const CJson_Node::TKeyType& /*name*/,
                                   const CJson_Node::TKeyType& /*member*/) {
        return true;
    }
    
    /// Primitive type data has been read
    ///
    /// @param name
    ///   Name of this object in the parent object, or empty string
    ///   if this object has no parent.
    /// @param member
    ///   Member name
    /// @param value
    ///   JSON value
    virtual bool PlainMemberValue(const CJson_Node::TKeyType& /*name*/,
                                  const CJson_Node::TKeyType& /*member*/,
                                  const CJson_ConstValue& /*value*/) {
        return true;
    }

    /// End reading object contents
    ///
    /// @param name
    ///   Name of this object in the parent object, or empty string
    ///   if this object has no parent.
    virtual bool EndObject(const CJson_Node::TKeyType& /*name*/) {
        return true;
    }


    /// Begin reading array contents
    ///
    /// @param name
    ///   Name of this array in the parent object, or empty string
    ///   if this array has no parent.
    virtual bool BeginArray(const CJson_Node::TKeyType& /*name*/) {
        return true;
    }

    /// Begin reading array element
    ///
    /// Right after this event, there can be one of the following only:
    /// EndArray, BeginObject, BeginArray, or PlainElementValue.
    ///
    /// @param name
    ///   Name of this array in the parent object, or empty string
    ///   if this array has no parent.
    /// @param index
    ///   Index of the array element
    virtual bool BeginArrayElement(const CJson_Node::TKeyType& /*name*/,
                                   size_t /*index*/) {
        return true;
    }

    /// Primitive type data has been read
    ///
    /// @param name
    ///   Name of this array in the parent object, or empty string
    ///   if this array has no parent.
    /// @param member
    ///   Index of the array element
    /// @param value
    ///   JSON value
    virtual bool PlainElementValue(const CJson_Node::TKeyType& /*name*/,
                                   size_t /*index*/,
                                   const CJson_ConstValue& /*value*/) {
        return true;
    }

    /// End reading array contents
    ///
    /// @param name
    ///   Name of this array in the parent object, or empty string
    ///   if this array has no parent.
    virtual bool EndArray(const CJson_Node::TKeyType& /*name*/) {
        return true;
    }

    /// Return current stack path as string
    /// For example:  "/root/obj2/arr[3]"
    CJson_Node::TKeyType GetCurrentJPath(void) const;
    
    /// Convert data, starting at the current parsing position, into
    /// a document object.
    /// This method may be called from BeginObject or BeginArray only.
    bool Read(CJson_Document& doc);

private:
    bool x_Notify(const rapidjson::Value& v);
    bool x_BeginObjectOrArray(bool object_type);
    void x_EndObjectOrArray(void);

public:
    // The following functions are named this way because rapidjson requires so
    bool Null();
    bool Bool(bool v);
    bool Int(int v);
    bool Uint(unsigned v);
    bool Int64(int64_t v);
    bool Uint64(uint64_t v);
    bool Double(double v);
    bool String(const Ch* buf, rapidjson::SizeType sz, bool c);
    bool Key(   const Ch* buf, rapidjson::SizeType sz, bool c);
    bool StartObject();
    bool EndObject(rapidjson::SizeType sz);
    bool StartArray();
    bool EndArray(rapidjson::SizeType sz);

private:
    void x_SetSource(std::istream* in) {m_in=in;}
    std::istream* m_in;                       // Input stream
    std::vector<bool> m_object_type;          // Object (true), or array (false)
    std::vector<size_t> m_index;              // array element index
    std::vector<CJson_Node::TKeyType> m_name; // object member name

    friend class CJson_Document;
};

/////////////////////////////////////////////////////////////////////////////
///
/// CJson_Document
///
/// Serializable, copyable container for JSON data.

class CJson_Document : public CJson_Node
{
    typedef rapidjson::Document _DocImpl;

public:
    CJson_Document(CJson_Node::EJsonType type = CJson_Node::eObject);
    /// Create document by parsing UTF8 string
    CJson_Document(const TStringType& v);
    /// Copy another document contents into this document
    CJson_Document(const CJson_Document& v);
    /// Copy another document contents into this document
    CJson_Document& operator=(const CJson_Document& v);
    /// Copy another Node contents into this document
    CJson_Document(const CJson_ConstNode& v);
    /// Copy another Node contents into this document
    CJson_Document& operator=(const CJson_ConstNode& v);

    ~CJson_Document(void) {
    }

    /// Read JSON data from a UTF8 string
    bool ParseString(const TStringType& v);

    /// Read JSON data from a stream
    bool Read(std::istream& in);

    /// Read JSON data from a file
    bool Read(const std::string& filename) {
        std::ifstream in(filename.c_str());
        return Read(in);
    }

    /// Test if the most recent read was successful
    bool ReadSucceeded(void);

    /// Get most recent read error
    std::string GetReadError(void) const;

    /// Write JSON data into a stream
    void Write(std::ostream& out) const;

    /// Write JSON data into a file
    void Write(const std::string& filename) const {
        std::ofstream out(filename.c_str());
        Write(out);
    }

    /// Traverse the document contents
    void Walk(CJson_WalkHandler& walk) const;

    /// Traverse the JSON data stream contents
    static void Walk(std::istream& in, CJson_WalkHandler& walk);


private:
    _DocImpl m_DocImpl;
};


/////////////////////////////////////////////////////////////////////////////

/// Extraction operator for JSON document
inline std::istream& operator>>(std::istream& is, CJson_Document& d) {
    if (!d.Read(is)) {
        is.setstate(std::ios::failbit);
    }
    return is;
}

/// Insertion operator for JSON document
inline std::ostream& operator<<(std::ostream& os, const CJson_Document& d)
{
    d.Write(os);
    return os;
}

/// Insertion operator for JSON node
inline std::ostream& operator<<(std::ostream& os, const CJson_ConstNode& v)
{
    return operator<<(os, CJson_Document(v)); 
}



/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// inline implementations

#if defined(NCBI_COMPILER_GCC) && !defined(__clang__)
#if (NCBI_COMPILER_VERSION == 442) || (NCBI_COMPILER_VERSION == 443)
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif
#endif

#define JSONWRAPP_TO_NCBIUTF8(v)        (v)

// workarounds to make it compile
#if defined(NCBI_COMPILER_GCC) && (NCBI_COMPILER_VERSION < 442) && !defined(__clang__)
#  define JSONWRAPP_MAKENODE(v) CJson_Node(v)
#else
#  define JSONWRAPP_MAKENODE(v) (v)
#endif

// --------------------------------------------------------------------------
// CJson_Node methods

inline CJson_ConstNode::CJson_ConstNode(const CJson_ConstNode& n)
    :  m_Impl(n.m_Impl) {
}
inline CJson_ConstNode&
CJson_ConstNode::operator=(const CJson_ConstNode& n) {
    m_Impl = n.m_Impl; return *this;
}
inline bool
CJson_ConstNode::operator==(const CJson_ConstNode& n) const
{
    return m_Impl->operator==(*n.m_Impl);
}
inline bool
CJson_ConstNode::operator!=(const CJson_ConstNode& n) const
{
    return m_Impl->operator!=(*n.m_Impl);
}

inline CJson_Node::CJson_Node(const CJson_Node& n)
    :  CJson_ConstNode(n) {
}
inline CJson_Node&
CJson_Node::operator=(const CJson_Node& n) {
    CJson_ConstNode::operator=(n); return *this;
}
inline CJson_Node&
CJson_Node::AssignCopy(const CJson_ConstNode& n) {
    m_Impl->CopyFrom( *n.m_Impl, *m_Impl->GetValueAllocator());
    return *this;
}


inline CJson_Node::EJsonType
CJson_ConstNode::GetType(void) const {
    switch (m_Impl->GetType()) {
    default:
    case rapidjson::kNullType:   break;
    case rapidjson::kFalseType:
    case rapidjson::kTrueType:   return eBool;
    case rapidjson::kObjectType: return eObject;
    case rapidjson::kArrayType:  return eArray;
    case rapidjson::kStringType: return eString;
    case rapidjson::kNumberType: return eNumber;
    }
    return eNull;
}

inline bool CJson_ConstNode::IsNull(void) const {
    return m_Impl->IsNull();
}
inline bool CJson_ConstNode::IsValue(void) const {
    return !IsObject() && !IsArray();
}
inline bool CJson_ConstNode::IsArray(void) const {
    return m_Impl->IsArray();
}
inline bool CJson_ConstNode::IsObject(void) const {
    return m_Impl->IsObject();
}
inline CJson_Node& CJson_Node::SetNull(void) {
    m_Impl->SetNull( ); return *this;
}

inline CJson_Value CJson_Node::ResetValue(void) {
    m_Impl->SetNull();
    return CJson_Value(m_Impl);
}
inline CJson_Value CJson_Node::SetValue(void) {
    _ASSERT(IsValue());
    return CJson_Value(m_Impl);
}
inline CJson_ConstValue CJson_ConstNode::GetValue(void) const {
    _ASSERT(IsValue());
    return CJson_ConstValue(m_Impl);
}
inline CJson_Array CJson_Node::ResetArray(void) {
    m_Impl->SetArray();
    return CJson_Array(m_Impl);
}
inline CJson_Array CJson_Node::SetArray(void) {
    _ASSERT(IsArray());
    return CJson_Array(m_Impl);
}
inline CJson_ConstArray CJson_ConstNode::GetArray(void) const {
    _ASSERT(IsArray());
    return CJson_ConstArray(m_Impl);
}
inline CJson_Object CJson_Node::ResetObject(void) {
    m_Impl->SetObject();
    return CJson_Object(m_Impl);
}
inline CJson_Object CJson_Node::SetObject(void) {
    _ASSERT(IsObject());
    return CJson_Object(m_Impl);
}
inline CJson_ConstObject CJson_ConstNode::GetObject(void) const {
    _ASSERT(IsObject());
    return CJson_ConstObject(m_Impl);
}
inline std::string
CJson_ConstNode::ToString(void) const {
    ncbi::CNcbiOstrstream os;
    rapidjson::CppOStream ofs(os);
    rapidjson::PrettyWriter<rapidjson::CppOStream> writer(ofs);
    m_Impl->Accept(writer);
    return std::string( ncbi::CNcbiOstrstreamToString(os) );
}

// --------------------------------------------------------------------------
// CJson_Value methods

inline CJson_ConstValue::CJson_ConstValue( const CJson_ConstValue& n)
    :  CJson_ConstNode(n) {
    m_Impl = n.m_Impl;
}
inline CJson_ConstValue&
CJson_ConstValue::operator=(const CJson_ConstValue& n) {
    CJson_ConstNode::operator=(n); return *this;
}
inline CJson_Value::CJson_Value( const CJson_Value& n)
    :  CJson_Node(n) {
    m_Impl = n.m_Impl;
}
inline CJson_Value&
CJson_Value::operator=(const CJson_Value& n) {
    CJson_Node::operator=(n); return *this;
}

inline bool CJson_ConstValue::IsBool(void) const {
    return m_Impl->IsBool();
}
inline bool CJson_ConstValue::IsNumber(void) const {
    return m_Impl->IsNumber();
}
inline bool CJson_ConstValue::IsInt4(void) const {
    return m_Impl->IsInt();
}
inline bool CJson_ConstValue::IsUint4(void) const {
    return m_Impl->IsUint();
}
inline bool CJson_ConstValue::IsInt8(void) const {
    return m_Impl->IsInt64();
}
inline bool CJson_ConstValue::IsUint8(void) const {
    return m_Impl->IsUint64();
}
inline bool CJson_ConstValue::IsDouble(void) const {
    return m_Impl->IsDouble();
}
inline bool CJson_ConstValue::IsString(void) const {
    return m_Impl->IsString();
}

inline bool   CJson_ConstValue::GetBool(void) const {
    return m_Impl->GetBool();
}
inline Int4   CJson_ConstValue::GetInt4(void) const {
    return m_Impl->GetInt();
}
inline Uint4  CJson_ConstValue::GetUint4(void) const {
    return m_Impl->GetUint();
}
inline Int8   CJson_ConstValue::GetInt8(void) const {
    return m_Impl->GetInt64();
}
inline Uint8  CJson_ConstValue::GetUint8(void) const {
    return m_Impl->GetUint64();
}
inline double CJson_ConstValue::GetDouble(void) const {
    return m_Impl->GetDouble();
}
inline CJson_Node::TStringType
CJson_ConstValue::GetString(void) const {
    return JSONWRAPP_TO_NCBIUTF8(m_Impl->GetString());
}
inline size_t CJson_ConstValue::GetStringLength(void) const {
    return m_Impl->GetStringLength();
}

inline CJson_Value& CJson_Value::SetBool(bool  value) {
    m_Impl->SetBool(  value); return *this;
}
inline CJson_Value& CJson_Value::SetInt4(Int4 value) {
    m_Impl->SetInt(   value); return *this;
}
inline CJson_Value& CJson_Value::SetUint4(Uint4 value) {
    m_Impl->SetUint(  value); return *this;
}
inline CJson_Value& CJson_Value::SetInt8(Int8 value) {
    m_Impl->SetInt64( value); return *this;
}
inline CJson_Value& CJson_Value::SetUint8(Uint8 value) {
    m_Impl->SetUint64(value); return *this;
}
inline CJson_Value& CJson_Value::SetDouble(double value) {
    m_Impl->SetDouble(value); return *this;
}
inline CJson_Value& CJson_Value::SetString(const CJson_Node::TStringType& value) {
    m_Impl->SetString(value.c_str(), *(m_Impl->GetValueAllocator())); return *this;
}

// --------------------------------------------------------------------------
// CJson_Array methods
inline CJson_ConstArray::CJson_ConstArray( const CJson_ConstArray& n)
    :  CJson_ConstNode(n) {
    m_Impl = n.m_Impl;
}
inline CJson_ConstArray&
CJson_ConstArray::operator=(const CJson_ConstArray& n) {
    CJson_ConstNode::operator=(n); return *this;
}
inline CJson_Array::CJson_Array( const CJson_Array& n)
    :  CJson_Node(n) {
    m_Impl = n.m_Impl;
}
inline CJson_Array&
CJson_Array::operator=(const CJson_Array& n) {
    CJson_Node::operator=(n); return *this;
}
inline void CJson_Array::reserve(size_t count) {
    m_Impl->Reserve(count, *(m_Impl->GetValueAllocator()));
}
inline void CJson_Array::clear(void) {
    m_Impl->Clear();
}
inline CJson_Array::iterator CJson_Array::erase(CJson_ConstArray::const_iterator _where) {
    return CJson_Array::iterator( m_Impl->Erase( _where.m_vi));
}
inline CJson_Array::iterator
CJson_Array::erase(CJson_ConstArray::const_iterator _first, CJson_ConstArray::const_iterator _last) {
    return CJson_Array::iterator( m_Impl->Erase( _first.m_vi, _last.m_vi));
}
inline size_t CJson_ConstArray::size(void) const {
    return m_Impl->Size();
}
inline size_t CJson_ConstArray::capacity(void) const {
    return m_Impl->Capacity();
}
inline bool CJson_ConstArray::empty(void) const {
    return m_Impl->Empty();
}
inline CJson_ConstNode CJson_ConstArray::at(size_t index) const {
    if (index >= size()) {
        throw std::out_of_range("array index out of range");
    }
    return operator[](index);
}
inline CJson_Node CJson_Array::at(size_t index) {
    if (index >= size()) {
        throw std::out_of_range("array index out of range");
    }
    return operator[](index);
}
inline CJson_ConstNode CJson_ConstArray::operator[](size_t index) const {
    return CJson_ConstNode(&(m_Impl->operator[](index)));
}
inline CJson_Node CJson_Array::operator[](size_t index) {
    return CJson_Node(&(m_Impl->operator[](index)));
}
inline CJson_ConstNode CJson_ConstArray::front(void) const {
    return operator[](0);
}
inline CJson_Node CJson_Array::front(void) {
    return operator[](0);
}
inline CJson_ConstNode CJson_ConstArray::back(void) const {
    return operator[](size()-1);
}
inline CJson_Node CJson_Array::back(void) {
    return operator[](size()-1);
}

// Implicit conversions are prohibited
#ifndef NCBI_COMPILER_WORKSHOP
// this may fail to compile
//template <typename T> void CJson_Array::push_back(T) =delete;
// this will compile:
template <typename T> inline void CJson_Array::push_back(const T&) {
    CProhibited<T>::Implicit_conversions_are_prohibited();
}
template <typename T> inline void CJson_Array::push_back(const T*) {
    CProhibited<T>::Implicit_conversions_are_prohibited();
}
#define JSW_EMPTY_TEMPLATE template<>
#else
#define JSW_EMPTY_TEMPLATE
#endif
inline void CJson_Array::push_back(void) {
    rapidjson::Value sv(m_Impl->GetValueAllocator());
    m_Impl->PushBack( sv, *(m_Impl->GetValueAllocator()));
}
JSW_EMPTY_TEMPLATE inline void CJson_Array::push_back(const bool& v) {
    rapidjson::Value sv(m_Impl->GetValueAllocator());
    m_Impl->PushBack( sv.SetBool(v), *(m_Impl->GetValueAllocator()));
}
JSW_EMPTY_TEMPLATE inline void CJson_Array::push_back(const Int4& v) {
    rapidjson::Value sv(m_Impl->GetValueAllocator());
    m_Impl->PushBack( sv.SetInt(v), *(m_Impl->GetValueAllocator()));
}
JSW_EMPTY_TEMPLATE inline void CJson_Array::push_back(const Uint4& v) {
    rapidjson::Value sv(m_Impl->GetValueAllocator());
    m_Impl->PushBack( sv.SetUint(v), *(m_Impl->GetValueAllocator()));
}
JSW_EMPTY_TEMPLATE inline void CJson_Array::push_back(const Int8& v) {
    rapidjson::Value sv(m_Impl->GetValueAllocator());
    m_Impl->PushBack( sv.SetInt64(v), *(m_Impl->GetValueAllocator()));
}
JSW_EMPTY_TEMPLATE inline void CJson_Array::push_back(const Uint8& v) {
    rapidjson::Value sv(m_Impl->GetValueAllocator());
    m_Impl->PushBack( sv.SetUint64(v), *(m_Impl->GetValueAllocator()));
}
JSW_EMPTY_TEMPLATE inline void CJson_Array::push_back(const float& v) {
    rapidjson::Value sv(m_Impl->GetValueAllocator());
    m_Impl->PushBack( sv.SetDouble(v), *(m_Impl->GetValueAllocator()));
}
JSW_EMPTY_TEMPLATE inline void CJson_Array::push_back(const double& v) {
    rapidjson::Value sv(m_Impl->GetValueAllocator());
    m_Impl->PushBack( sv.SetDouble(v), *(m_Impl->GetValueAllocator()));
}
JSW_EMPTY_TEMPLATE inline void CJson_Array::push_back(
    const CJson_Node::TCharType* v) {
    rapidjson::Value sv(v, *(m_Impl->GetValueAllocator()));
    m_Impl->PushBack( sv, *(m_Impl->GetValueAllocator()));
}
JSW_EMPTY_TEMPLATE inline void CJson_Array::push_back(
    const CJson_Node::TStringType& value) {
    push_back(value.c_str());
}
JSW_EMPTY_TEMPLATE inline void CJson_Array::push_back(const CJson_ConstNode& v) {
    rapidjson::Value t(m_Impl->GetValueAllocator());
    t.CopyFrom( *v.m_Impl, *t.GetValueAllocator());
    m_Impl->PushBack( t, *(m_Impl->GetValueAllocator()));
}
JSW_EMPTY_TEMPLATE inline void CJson_Array::push_back(const CJson_Node& v) {
    push_back<CJson_ConstNode>(v);
}
JSW_EMPTY_TEMPLATE inline void CJson_Array::push_back(const CJson_ConstArray& v) {
    push_back<CJson_ConstNode>(v);
}
JSW_EMPTY_TEMPLATE inline void CJson_Array::push_back(const CJson_Array& v) {
    push_back<CJson_ConstNode>(v);
}
JSW_EMPTY_TEMPLATE inline void CJson_Array::push_back(const CJson_ConstObject& v) {
    push_back<CJson_ConstNode>(v);
}
JSW_EMPTY_TEMPLATE inline void CJson_Array::push_back(const CJson_Object& v) {
    push_back<CJson_ConstNode>(v);
}
#undef JSW_EMPTY_TEMPLATE

inline CJson_Array CJson_Array::push_back_array(void) {
    rapidjson::Value sv(m_Impl->GetValueAllocator());
    m_Impl->PushBack( sv.SetArray(), *(m_Impl->GetValueAllocator()));
    return back().SetArray();
}

inline CJson_Object CJson_Array::push_back_object(void) {
    rapidjson::Value sv(m_Impl->GetValueAllocator());
    m_Impl->PushBack( sv.SetObject(), *(m_Impl->GetValueAllocator()));
    return back().SetObject();
}

inline void CJson_Array::pop_back(void) {
    m_Impl->PopBack();
}

inline CJson_ConstArray::const_iterator
CJson_ConstArray::begin(void) const {
    return const_iterator(m_Impl->Begin());
}
inline CJson_ConstArray::const_iterator
CJson_ConstArray::end(void) const {
    return const_iterator(m_Impl->End());
}
inline CJson_Array::iterator CJson_Array::begin(void) const {
    return iterator(m_Impl->Begin());
}
inline CJson_Array::iterator CJson_Array::end(void) const {
    return iterator(m_Impl->End());
}

// --------------------------------------------------------------------------
// CJson_Array::const_iterator

inline CJson_ConstArray::const_iterator::const_iterator(void)
    : m_vi(0), m_v(JSONWRAPP_MAKENODE(0)) {
}
inline CJson_ConstArray::const_iterator::const_iterator(
    const CJson_ConstArray::const_iterator& i) : m_vi(i.m_vi), m_v(i.m_v) {
}
inline CJson_ConstArray::const_iterator&
CJson_ConstArray::const_iterator::operator=(
    const CJson_ConstArray::const_iterator& vi) {
    m_vi = vi.m_vi; return *this;
}
inline bool
CJson_ConstArray::const_iterator::operator!=(
    const CJson_ConstArray::const_iterator& vi) const {
    return m_vi != vi.m_vi;
}
inline bool
CJson_ConstArray::const_iterator::operator==(
    const CJson_ConstArray::const_iterator& vi) const {
    return m_vi == vi.m_vi;
}
inline bool
CJson_ConstArray::const_iterator::operator<(
    const CJson_ConstArray::const_iterator& vi) const {
    return m_vi < vi.m_vi;
}
inline bool
CJson_ConstArray::const_iterator::operator<=(
    const CJson_ConstArray::const_iterator& vi) const {
    return m_vi <= vi.m_vi;
}
inline bool
CJson_ConstArray::const_iterator::operator>(
    const CJson_ConstArray::const_iterator& vi) const {
    return m_vi > vi.m_vi;
}
inline bool
CJson_ConstArray::const_iterator::operator>=(
    const CJson_ConstArray::const_iterator& vi) const {
    return m_vi >= vi.m_vi;
}
inline CJson_ConstArray::const_iterator&
CJson_ConstArray::const_iterator::operator++(void) {
    ++m_vi; return *this;
}
inline CJson_ConstArray::const_iterator&
CJson_ConstArray::const_iterator::operator++(int) {
    ++m_vi; return *this;
}
inline CJson_ConstArray::const_iterator&
CJson_ConstArray::const_iterator::operator+=(int i) {
    m_vi += i; return *this;
}
inline CJson_ConstArray::const_iterator
CJson_ConstArray::const_iterator::operator+(int i) const {
    return const_iterator(m_vi + i);
}
inline CJson_ConstArray::const_iterator&
CJson_ConstArray::const_iterator::operator--(void) {
    --m_vi; return *this;
}
inline CJson_ConstArray::const_iterator&
CJson_ConstArray::const_iterator::operator--(int) {
    --m_vi; return *this;
}
inline CJson_ConstArray::const_iterator&
CJson_ConstArray::const_iterator::operator-=(int i) {
    m_vi -= i; return *this;
}
inline CJson_ConstArray::const_iterator
CJson_ConstArray::const_iterator::operator-(int i) const {
    return const_iterator(m_vi - i);
}
inline const CJson_ConstNode&
CJson_ConstArray::const_iterator::operator*(void) const {
    x_Impl(m_v) = m_vi; return m_v;
}
inline const CJson_ConstNode*
CJson_Array::const_iterator::operator->(void) const {
    x_Impl(m_v) = m_vi; return &m_v;
}
inline CJson_ConstArray::const_iterator::distance_type
CJson_ConstArray::const_iterator::operator-(CJson_ConstArray::const_iterator vi) const {
    return m_vi - vi.m_vi;
}

inline CJson_ConstArray::const_iterator::const_iterator(
    const CJson_ConstArray::_ImplCIterator vi)
    : m_vi(const_cast<CJson_ConstArray::_ImplIterator>(vi)),
      m_v(JSONWRAPP_MAKENODE(0)) {
}
inline CJson_ConstArray::const_iterator::const_iterator(
    const CJson_ConstArray::_ImplIterator vi)
    : m_vi(vi), m_v(JSONWRAPP_MAKENODE(0)) {
}
// --------------------------------------------------------------------------
// CJson_Array::iterator

inline CJson_ConstArray::iterator::iterator(void) {
}
inline CJson_ConstArray::iterator::iterator(const CJson_Array::iterator& i)
    : const_iterator(i) {
}
inline CJson_ConstArray::iterator&
CJson_ConstArray::iterator::operator=(const CJson_Array::iterator& vi) {
    const_iterator::operator=(vi); return *this;
}
inline CJson_ConstArray::iterator
CJson_Array::iterator::operator+(int i) const {
    return iterator(m_vi + i);
}
inline CJson_ConstArray::iterator
CJson_Array::iterator::operator-(int i) const {
    return iterator(m_vi - i);
}
inline CJson_Node&
CJson_ConstArray::iterator::operator*(void) const {
    x_Impl(m_v) = m_vi; return m_v;
}
inline CJson_Node*
CJson_ConstArray::iterator::operator->(void) const {
    x_Impl(m_v) = m_vi; return &m_v;
}
inline CJson_ConstArray::iterator::distance_type
CJson_ConstArray::iterator::operator-(CJson_ConstArray::iterator vi) const {
    return m_vi - vi.m_vi;
}
inline CJson_ConstArray::iterator::iterator(
    const CJson_ConstArray::_ImplIterator vi) : const_iterator(vi) {
}

// --------------------------------------------------------------------------
// CJson_Object methods

inline CJson_ConstObject::CJson_ConstObject( const CJson_ConstObject& n)
    :  CJson_ConstNode(n) {
    m_Impl = n.m_Impl;
}
inline CJson_ConstObject&
CJson_ConstObject::operator=(const CJson_ConstObject& n) {
    CJson_ConstNode::operator=(n); return *this;
}
inline CJson_Object::CJson_Object( const CJson_Object& n)
    :  CJson_Node(n) {
    m_Impl = n.m_Impl;
}
inline CJson_Object&
CJson_Object::operator=(const CJson_Object& n) {
    CJson_Node::operator=(n); return *this;
}
inline void CJson_Object::clear(void) {
    m_Impl->RemoveAllMembers();
}
inline size_t CJson_Object::erase(const CJson_Node::TKeyType& name) {
    return m_Impl->RemoveMember(name.c_str()) ? 1 : 0;
}
inline CJson_Object::iterator CJson_Object::erase(CJson_ConstObject::const_iterator _where) {
    return CJson_Object::iterator( m_Impl->EraseMember( _where.m_vi));
}
inline CJson_Object::iterator
CJson_Object::erase(CJson_ConstObject::const_iterator _first, CJson_ConstObject::const_iterator _last) {
    return CJson_Object::iterator( m_Impl->EraseMember( _first.m_vi, _last.m_vi));
}
inline size_t CJson_ConstObject::size(void) const {
    return m_Impl->MemberCount();
}
inline bool CJson_ConstObject::empty(void) const {
    return m_Impl->ObjectEmpty();
}
inline CJson_ConstNode
CJson_ConstObject::at(const CJson_Node::TKeyType& name) const {
    if (!has(name)) {
        throw std::out_of_range(name + " object member not found");
    }
    return CJson_ConstNode(&(m_Impl->operator[](name.c_str())));
}
inline CJson_Node
CJson_Object::at(const CJson_Node::TKeyType& name) {
    if (!has(name)) {
        throw std::out_of_range(name + " object member not found");
    }
    return CJson_Object(&(m_Impl->operator[](name.c_str())));
}
inline CJson_ConstNode
CJson_ConstObject::operator[](const CJson_Node::TKeyType& name) const {
    return CJson_ConstNode(&(m_Impl->operator[](name.c_str())));
}
inline CJson_Node
CJson_Object::operator[](const CJson_Node::TKeyType& name) {
    if (!has(name)) {
        insert(name);
    }
    return CJson_Node(&(m_Impl->operator[](name.c_str())));
}

inline void CJson_Object::insert(const CJson_Node::TKeyType& name) {
    rapidjson::Value::AllocatorType& a = *(m_Impl->GetValueAllocator());
    rapidjson::Value sv_name(name.c_str(), a);
    rapidjson::Value sv_value(&a);
    m_Impl->AddMember( sv_name, sv_value, a);
}
// Implicit conversions are prohibited
// this may fail to compile
//template <typename T> void CJson_Object::insert(const std::string& , T) =delete;
// this will compile:
template <typename T> inline void CJson_Object::insert(
    const CJson_Node::TKeyType& , const T&) {
    CProhibited<T>::Implicit_conversions_are_prohibited();
}
template <typename T> inline void CJson_Object::insert(
    const CJson_Node::TKeyType& , const T*) {
    CProhibited<T>::Implicit_conversions_are_prohibited();
}
template<> inline void
CJson_Object::insert(const CJson_Node::TKeyType& name, const bool& v) {
    rapidjson::Value::AllocatorType& a = *(m_Impl->GetValueAllocator());
    rapidjson::Value sv_name(name.c_str(), a);
    rapidjson::Value sv_value(&a);
    m_Impl->AddMember( sv_name, sv_value.SetBool(v), a);
}
template<> inline void
CJson_Object::insert(const CJson_Node::TKeyType& name, const Int4& v) {
    rapidjson::Value::AllocatorType& a = *(m_Impl->GetValueAllocator());
    rapidjson::Value sv_name(name.c_str(), a);
    rapidjson::Value sv_value(&a);
    m_Impl->AddMember( sv_name, sv_value.SetInt(v), a);
}
template<> inline void
CJson_Object::insert(const CJson_Node::TKeyType& name, const Uint4& v) {
    rapidjson::Value::AllocatorType& a = *(m_Impl->GetValueAllocator());
    rapidjson::Value sv_name(name.c_str(), a);
    rapidjson::Value sv_value(&a);
    m_Impl->AddMember( sv_name, sv_value.SetUint(v), a);
}
template<> inline void
CJson_Object::insert(const CJson_Node::TKeyType& name, const Int8& v) {
    rapidjson::Value::AllocatorType& a = *(m_Impl->GetValueAllocator());
    rapidjson::Value sv_name(name.c_str(), a);
    rapidjson::Value sv_value(&a);
    m_Impl->AddMember( sv_name, sv_value.SetInt64(v), a);
}
template<> inline void
CJson_Object::insert(const CJson_Node::TKeyType& name, const Uint8& v) {
    rapidjson::Value::AllocatorType& a = *(m_Impl->GetValueAllocator());
    rapidjson::Value sv_name(name.c_str(), a);
    rapidjson::Value sv_value(&a);
    m_Impl->AddMember( sv_name, sv_value.SetUint64(v), a);
}
template<> inline void
CJson_Object::insert(const CJson_Node::TKeyType& name, const float& v) {
    rapidjson::Value::AllocatorType& a = *(m_Impl->GetValueAllocator());
    rapidjson::Value sv_name(name.c_str(), a);
    rapidjson::Value sv_value(&a);
    m_Impl->AddMember( sv_name, sv_value.SetDouble(v), a);
}
template<> inline void
CJson_Object::insert(const CJson_Node::TKeyType& name, const double& v) {
    rapidjson::Value::AllocatorType& a = *(m_Impl->GetValueAllocator());
    rapidjson::Value sv_name(name.c_str(), a);
    rapidjson::Value sv_value(&a);
    m_Impl->AddMember( sv_name, sv_value.SetDouble(v), a);
}
template<> inline void
CJson_Object::insert(const CJson_Node::TKeyType& name,
                     const CJson_Node::TCharType* value) {
    rapidjson::Value::AllocatorType& a = *(m_Impl->GetValueAllocator());
    rapidjson::Value sv_name(name.c_str(), a);
    rapidjson::Value sv_value(value, a);
    m_Impl->AddMember( sv_name, sv_value, a);
}
#ifndef NCBI_COMPILER_WORKSHOP
template<>
#endif
inline void
CJson_Object::insert(const CJson_Node::TKeyType& name,
                     const CJson_Node::TStringType& value) {
    insert(name, value.c_str());
}
template<> inline void
CJson_Object::insert(const CJson_Node::TKeyType& name, const CJson_ConstNode& v) {
    rapidjson::Value::AllocatorType& a = *(m_Impl->GetValueAllocator());
    rapidjson::Value sv_name(name.c_str(), a);
    rapidjson::Value sv_value(&a);
    sv_value.CopyFrom( *v.m_Impl, a);
    m_Impl->AddMember( sv_name, sv_value, a);
}
template<> inline void
CJson_Object::insert(const CJson_Node::TKeyType& name, const CJson_Node& v) {
    insert<CJson_ConstNode>(name, v);
}
template<> inline void
CJson_Object::insert(const CJson_Node::TKeyType& name, const CJson_ConstArray& v) {
    insert<CJson_ConstNode>(name, v);
}
template<> inline void
CJson_Object::insert(const CJson_Node::TKeyType& name, const CJson_Array& v) {
    insert<CJson_ConstNode>(name, v);
}
template<> inline void
CJson_Object::insert(const CJson_Node::TKeyType& name, const CJson_ConstObject& v) {
    insert<CJson_ConstNode>(name, v);
}
template<> inline void
CJson_Object::insert(const CJson_Node::TKeyType& name, const CJson_Object& v) {
    insert<CJson_ConstNode>(name, v);
}
inline CJson_Array
CJson_Object::insert_array(const CJson_Node::TKeyType& name) {
    rapidjson::Value::AllocatorType& a = *(m_Impl->GetValueAllocator());
    rapidjson::Value sv_name(name.c_str(), a);
    rapidjson::Value sv_value(&a);
    m_Impl->AddMember( sv_name, sv_value.SetArray(), a);
    return  operator[](name).SetArray();
}
inline CJson_Object
CJson_Object::insert_object(const CJson_Node::TKeyType& name) {
    rapidjson::Value::AllocatorType& a = *(m_Impl->GetValueAllocator());
    rapidjson::Value sv_name(name.c_str(), a);
    rapidjson::Value sv_value(&a);
    m_Impl->AddMember( sv_name, sv_value.SetObject(), a);
    return operator[](name).SetObject();
}
inline bool
CJson_ConstObject::has(const CJson_Node::TKeyType& name) const {
    return m_Impl->HasMember(name.c_str());
}


inline CJson_ConstObject::const_iterator
CJson_ConstObject::begin(void) const {
    return const_iterator(m_Impl->MemberBegin());
}
inline CJson_ConstObject::const_iterator
CJson_ConstObject::end(void) const {
    return const_iterator(m_Impl->MemberEnd());
}
inline CJson_ConstObject::const_iterator
CJson_ConstObject::find(const CJson_Node::TKeyType& name) const {
    _ImplCIterator m = m_Impl->FindMember(name.c_str());
    return m ? const_iterator(m) : end();
}

inline CJson_Object::iterator
CJson_Object::begin(void) const {
    return iterator(m_Impl->MemberBegin());
}
inline CJson_Object::iterator
CJson_Object::end(void) const {
    return iterator(m_Impl->MemberEnd());
}
inline CJson_Object::iterator
CJson_Object::find(const CJson_Node::TKeyType& name) const {
    _ImplIterator m = m_Impl->FindMember(name.c_str());
    return m ? iterator(m) : end();
}


// --------------------------------------------------------------------------
// CJson_Object::const_iterator

inline CJson_ConstObject::const_iterator::const_iterator(void)
    : m_vi(0) {
}
inline CJson_ConstObject::const_iterator::const_iterator(
    const CJson_ConstObject::const_iterator& i)
    : m_vi(i.m_vi) {
}
inline CJson_ConstObject::const_iterator::const_iterator(
    const CJson_ConstObject::iterator& i)
    : m_vi(i.m_vi) {
}
inline CJson_ConstObject::const_iterator&
CJson_ConstObject::const_iterator::operator++(void) {
    ++m_vi; return *this;
}
inline CJson_ConstObject::const_iterator&
CJson_ConstObject::const_iterator::operator++(int) {
    ++m_vi; return *this;
}
inline CJson_ConstObject::const_iterator&
CJson_ConstObject::const_iterator::operator--(void) {
    --m_vi; return *this;
}
inline CJson_ConstObject::const_iterator&
CJson_ConstObject::const_iterator::operator--(int) {
    --m_vi; return *this;
}
inline CJson_ConstObject::const_iterator&
CJson_ConstObject::const_iterator::operator=(
    const CJson_ConstObject::const_iterator& vi) {
    m_vi = vi.m_vi; return *this;
}
inline CJson_ConstObject::const_iterator&
CJson_ConstObject::const_iterator::operator=(
    const CJson_ConstObject::iterator& vi) {
    m_vi = vi.m_vi; return *this;
}
inline bool
CJson_ConstObject::const_iterator::operator!=(
    const CJson_ConstObject::const_iterator& vi) const {
    return m_vi != vi.m_vi;
}
inline bool
CJson_ConstObject::const_iterator::operator==(
    const CJson_ConstObject::const_iterator& vi) const {
    return m_vi == vi.m_vi;
}
inline bool
CJson_ConstObject::const_iterator::operator!=(
    const CJson_ConstObject::iterator& vi) const {
    return m_vi != vi.m_vi;
}
inline bool
CJson_ConstObject::const_iterator::operator==(
    const CJson_ConstObject::iterator& vi) const {
    return m_vi == vi.m_vi;
}

inline CJson_ConstObject_pair::CJson_ConstObject_pair(void)
    : name(0), value(JSONWRAPP_MAKENODE(0)) {
}
inline CJson_ConstObject_pair::CJson_ConstObject_pair(
    const CJson_Node::TCharType* _name, const _Impl& _value)
    : name(_name), value(JSONWRAPP_MAKENODE(const_cast<_Impl*>(&_value))) {
}
inline CJson_ConstObject_pair::CJson_ConstObject_pair(const CJson_Object_pair& p)
    : name(p.name), value(JSONWRAPP_MAKENODE(const_cast<_Impl*>(p.value.m_Impl))) {
}
inline CJson_ConstObject_pair&
CJson_ConstObject_pair::assign(
    const CJson_Node::TCharType* _name, const _Impl& _value) {
    this->~CJson_ConstObject_pair();
    new (this) CJson_ConstObject_pair(_name, _value);
    return *this;
}

inline const CJson_ConstObject::const_iterator::pair&
CJson_ConstObject::const_iterator::operator*(void) const {
    return m_pvi.assign(m_vi->name.GetString(), m_vi->value);
}
inline const CJson_ConstObject::const_iterator::pair*
CJson_ConstObject::const_iterator::operator->(void) const {
    return &(m_pvi.assign(m_vi->name.GetString(), m_vi->value));
}

inline CJson_ConstObject::const_iterator::const_iterator(
    const CJson_ConstObject::_ImplCIterator vi)
    : m_vi(const_cast<CJson_Object::_ImplIterator>(vi)) {
}
inline CJson_ConstObject::const_iterator::const_iterator(
    const CJson_ConstObject::_ImplIterator vi)
    : m_vi(vi) {
}

// --------------------------------------------------------------------------
// CJson_Object::iterator

inline CJson_ConstObject::iterator::iterator(void) {
}
inline CJson_ConstObject::iterator::iterator(
    const CJson_ConstObject::iterator& i) : m_vi(i.m_vi) {
}
inline CJson_ConstObject::iterator&
CJson_ConstObject::iterator::operator=(const CJson_ConstObject::iterator& vi) {
    m_vi = vi.m_vi; return *this;
}
inline bool
CJson_ConstObject::iterator::operator!=(
    const CJson_ConstObject::iterator& vi) const {
    return m_vi != vi.m_vi;
}
inline bool
CJson_ConstObject::iterator::operator==(
    const CJson_ConstObject::iterator& vi) const {
    return m_vi == vi.m_vi;
}
inline bool
CJson_ConstObject::iterator::operator!=(
    const CJson_ConstObject::const_iterator& vi) const {
    return m_vi != vi.m_vi;
}
inline bool
CJson_ConstObject::iterator::operator==(
    const CJson_ConstObject::const_iterator& vi) const {
    return m_vi == vi.m_vi;
}
inline CJson_ConstObject::iterator&
CJson_ConstObject::iterator::operator++(void) {
    ++m_vi; return *this;
}
inline CJson_ConstObject::iterator&
CJson_ConstObject::iterator::operator++(int) {
    ++m_vi; return *this;
}
inline CJson_ConstObject::iterator&
CJson_ConstObject::iterator::operator--(void) {
    --m_vi; return *this;
}
inline CJson_ConstObject::iterator&
CJson_ConstObject::iterator::operator--(int) {
    --m_vi; return *this;
}
inline CJson_ConstObject::iterator::pair&
CJson_ConstObject::iterator::operator*(void) const {
    return m_pvi.assign(m_vi->name.GetString(), m_vi->value);
}
inline CJson_ConstObject::iterator::pair*
CJson_ConstObject::iterator::operator->(void) const {
    return &(m_pvi.assign(m_vi->name.GetString(), m_vi->value));
}

inline CJson_Object_pair::CJson_Object_pair(void)
    : name(0), value(JSONWRAPP_MAKENODE(0)) {
}
inline CJson_Object_pair::CJson_Object_pair(
    const CJson_Node::TCharType* _name, _Impl& _value)
    : name(_name), value(JSONWRAPP_MAKENODE(&_value)) {
}
inline CJson_Object_pair&
CJson_Object_pair::assign(
    const CJson_Node::TCharType* _name, _Impl& _value) {
    this->~CJson_Object_pair();
    new (this) CJson_Object_pair(_name, _value);
    return *this;
}

inline CJson_ConstObject::iterator::iterator(
    const CJson_Object::_ImplIterator vi)
    : m_vi(vi) {
}

// --------------------------------------------------------------------------
// CJson_WalkHandler methods

inline CJson_WalkHandler::CJson_WalkHandler(void)
    : m_in(0) {
    m_object_type.push_back(true); m_index.push_back(size_t(-1));
    m_name.push_back(kEmptyStr);
}

inline bool CJson_WalkHandler::x_Notify(const rapidjson::Value& v) {
    bool ret = true;
    if (m_object_type.back()) {
        ret = BeginObjectMember(m_name[m_name.size()-2], m_name.back());
        if (ret) {
            ret = PlainMemberValue( m_name[m_name.size()-2], m_name.back(),
                                    const_cast<CJson_Node::_Impl*>(&v));
        }
        return ret;
    }
    ret = BeginArrayElement(m_name[m_name.size()-2], m_index.back());
    if (ret) {
        PlainElementValue(m_name[m_name.size()-2], m_index.back(),
                          const_cast<CJson_Node::_Impl*>(&v));
    }
    ++(m_index.back());
    return ret;
}
inline bool CJson_WalkHandler::x_BeginObjectOrArray(bool object_type) {
    bool ret = true;
    if (m_object_type.size() > 1) {
        if (m_object_type.back()) {
            ret = BeginObjectMember(m_name[m_name.size()-2], m_name.back());
        } else {
            ret = BeginArrayElement(m_name[m_name.size()-2], m_index.back());
        }
    }
    if (ret) {
        m_object_type.push_back(object_type); m_index.push_back(size_t(-1));
        m_name.push_back(kEmptyStr);
    }
    return ret;
}
inline void CJson_WalkHandler::x_EndObjectOrArray(void) {
    m_object_type.pop_back(); m_index.pop_back(); m_name.pop_back();
    if (!m_object_type.back()) {
        ++(m_index.back());
    }
}
inline bool CJson_WalkHandler::Null() {
    return x_Notify( (rapidjson::Value::AllocatorType*)0);
}
inline bool CJson_WalkHandler::Bool(bool v) {
    rapidjson::Value jv(v);
    return x_Notify(jv);
}
inline bool CJson_WalkHandler::Int(int v) { 
    rapidjson::Value jv(v);
    return x_Notify(jv);
}
inline bool CJson_WalkHandler::Uint(unsigned v) { 
    rapidjson::Value jv(v);
    return x_Notify(jv);
}
inline bool CJson_WalkHandler::Int64(int64_t v) { 
    rapidjson::Value jv(v);
    return x_Notify(jv);
}
inline bool CJson_WalkHandler::Uint64(uint64_t v) { 
    rapidjson::Value jv(v);
    return x_Notify(jv);
}
inline bool CJson_WalkHandler::Double(double v) { 
    rapidjson::Value jv(v);
    return x_Notify(jv);
}
inline bool CJson_WalkHandler::String(const Ch* buf,
    rapidjson::SizeType sz, bool) {
    rapidjson::Value jv(buf,sz);
    return x_Notify(jv);
}
inline bool CJson_WalkHandler::Key(const Ch* buf,
    rapidjson::SizeType sz, bool c) {
    m_name.back().assign(buf, sz);
    return true;
}

inline bool CJson_WalkHandler::StartObject() { 
    if (!x_BeginObjectOrArray(true)) {
        return false;
    }
    return BeginObject(m_name[m_name.size()-2]);
}
inline bool CJson_WalkHandler::EndObject(rapidjson::SizeType) { 
    m_name.back().clear();
    bool ret = EndObject(m_name[m_name.size()-2]);
    x_EndObjectOrArray();
    return ret;
}
inline bool CJson_WalkHandler::StartArray() { 
    if (!x_BeginObjectOrArray(false)) {
        return false;
    }
    bool ret = BeginArray(m_name[m_name.size()-2]);
    m_index.back() = 0;
    return ret;
}
inline bool CJson_WalkHandler::EndArray(rapidjson::SizeType) { 
    m_index.back() = size_t(-1);
    bool ret = EndArray(m_name[m_name.size()-2]);
    x_EndObjectOrArray();
    return ret;
}

inline CJson_Node::TKeyType
CJson_WalkHandler::GetCurrentJPath(void) const {
    std::vector<bool>::const_iterator t = m_object_type.begin();
    std::vector<bool>::const_iterator te = m_object_type.end();
    std::vector<size_t>::const_iterator i = m_index.begin();
    std::vector<CJson_Node::TKeyType>::const_iterator n = m_name.begin();
    CJson_Node::TKeyType path;
    for ( ++t, ++i, ++n; t != te; ++t, ++i, ++n) {
        if (*t) {
            path += JSONWRAPP_TO_NCBIUTF8("/");
            path += JSONWRAPP_TO_NCBIUTF8(*n);
        } else  if (*i != size_t(-1)) {
            path += JSONWRAPP_TO_NCBIUTF8("[");
            path += JSONWRAPP_TO_NCBIUTF8(ncbi::NStr::NumericToString(*i));
            path += JSONWRAPP_TO_NCBIUTF8("]");
        }
    }
    return path;
}

inline bool CJson_WalkHandler::Read(CJson_Document& doc) {
    bool b = false;
    if (m_in) {
        m_in->unget();
        b = doc.Read(*m_in);
        m_in->unget();
    }
    return b;
}

// --------------------------------------------------------------------------
// CJson_Document methods

inline CJson_Document::CJson_Document( CJson_Value::EJsonType type) {
    switch (type) {
    default:
    case CJson_Node::eObject: m_DocImpl.SetObject();    break;
    case CJson_Node::eArray:  m_DocImpl.SetArray();     break;
    case CJson_Node::eNull:   m_DocImpl.SetNull();      break;
    case CJson_Node::eBool:   m_DocImpl.SetBool(false); break;
    case CJson_Node::eNumber: m_DocImpl.SetInt(0);      break;
    case CJson_Node::eString: m_DocImpl.SetString(kEmptyCStr,0);  break;
    }
    m_Impl = &m_DocImpl;
}
inline CJson_Document::CJson_Document(const CJson_ConstNode::TStringType& v) {
    m_DocImpl.Parse<rapidjson::kParseStopWhenDoneFlag, rapidjson::UTF8<> >(v.c_str());
    m_Impl = &m_DocImpl;
}
inline CJson_Document::CJson_Document(const CJson_Document& v) {
    m_DocImpl.CopyFrom(*v.m_Impl, m_DocImpl.GetAllocator());
    m_Impl = &m_DocImpl;
}
inline CJson_Document& CJson_Document::operator=(const CJson_Document& v) {
    m_DocImpl.CopyFrom(*v.m_Impl, m_DocImpl.GetAllocator());
    return *this;
}
inline CJson_Document::CJson_Document(const CJson_ConstNode& v) {
    m_DocImpl.CopyFrom(*v.m_Impl, m_DocImpl.GetAllocator());
    m_Impl = &m_DocImpl;
}
inline CJson_Document& CJson_Document::operator=(const CJson_ConstNode& v) {
    m_DocImpl.CopyFrom(*v.m_Impl, m_DocImpl.GetAllocator());
    return *this;
}

inline bool CJson_Document::ParseString(const CJson_ConstNode::TStringType& v) {
    m_DocImpl.Parse<rapidjson::kParseStopWhenDoneFlag, rapidjson::UTF8<> >(v.c_str());
    return  !m_DocImpl.HasParseError();
}

inline bool CJson_Document::Read(std::istream& in) {
    rapidjson::CppIStream ifs(in);
    m_DocImpl.ParseStream<rapidjson::kParseStopWhenDoneFlag>(ifs);
    return  !m_DocImpl.HasParseError();
}

inline bool CJson_Document::ReadSucceeded(void) {
    return !m_DocImpl.HasParseError();
}
inline std::string CJson_Document::GetReadError() const {
    return rapidjson::GetParseError_En(m_DocImpl.GetParseError());
}

inline void CJson_Document::Write(std::ostream& out) const {
    rapidjson::CppOStream ofs(out);
    rapidjson::PrettyWriter<rapidjson::CppOStream> writer(ofs);
    m_DocImpl.Accept(writer);
}

inline void CJson_Document::Walk(CJson_WalkHandler& walk) const {
    walk.x_SetSource(0);
    m_DocImpl.Accept(walk);
}

inline void CJson_Document::Walk(std::istream& in,
                                 CJson_WalkHandler& walk) {
    walk.x_SetSource(&in);
    rapidjson::CppIStream ifs(in);
    rapidjson::Reader rdr;
    rdr.Parse<rapidjson::kParseStopWhenDoneFlag>(ifs,walk);
}

END_NCBI_SCOPE

#endif  /* MISC_JSONWRAPP___JSONWRAPP__HPP */


