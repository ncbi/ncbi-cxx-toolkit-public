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
* Author: Sergey Sikorskiy
*
* File Description: Tiny Python API wrappers
*
* Status: *Initial*
*
* ===========================================================================
*/

#ifndef PYTHONPP_SEQ_H
#define PYTHONPP_SEQ_H

#include "pythonpp_object.hpp"

BEGIN_NCBI_SCOPE

namespace pythonpp
{

template<class T> class CSequnceHelper;

template<class T>
class CSequnceProxy
{
protected:
    CSequnceHelper<T>& s;   //< the sequence
    int offset;             //< item number
    T value;                //< value

public:

    CSequnceProxy (CSequnceHelper<T>& seq, int j)
    : s(seq), offset(j), value (s.GetItem(j))
    {
    }

    CSequnceProxy (const CSequnceProxy<T>& range)
    : s(range.s), offset(range.offset), value(range.value)
    {
    }

    CSequnceProxy (CObject& obj)
    : s(dynamic_cast< CSequnceHelper<T>&>(obj))
    , offset( NULL )
    , value(s.getItem(offset))
    {
    }

    ~CSequnceProxy(void)
    {
    }

    operator const T&(void) const
    {
        return value;
    }

    operator T&(void)
    {
        return value;
    }

    CSequnceProxy<T>& operator=(const CSequnceProxy<T>& rhs)
    {
        value = rhs.value;
        s.SetItem(offset, value);
        return *this;
    }

    CSequnceProxy<T>& operator=(const T& obj)
    {
        value = obj;
        s.SetItem(offset, value);
        return *this;
    }

}; // end of CSequnceProxy


template<class T>
class CSequnceHelper : public CObject
{
public:
    typedef size_t size_type;
    typedef CSequnceProxy<T> reference;
    typedef T const_reference;
    typedef CSequnceProxy<T>* pointer;
    typedef int difference_type;
    typedef T value_type;

public:
//        virtual size_type max_size() const
//        {
//            return std::string::npos; // ?
//        }

//        virtual size_type capacity() const
//        {
//            return size();
//        }

//        virtual void swap(CSequnceHelper<T>& c)
//        {
//            CSequnceHelper<T> temp = c;
//            c = ptr();
//            set(temp.ptr());
//        }

    // Function name made STL compatible ...
    // !!! Temporary hack. This function should not be virdual. 1/12/2005 2:43PM ...
    virtual size_type size () const
    {
        return PySequence_Length (Get());
    }

public:
        /// ???
//        explicit CSequnceHelper<T> ()
//            :Object(PyTuple_New(0), true)
//        {
//            validate();
//        }

    CSequnceHelper(PyObject* obj, EOwnership ownership = eAcquireOwnership)
    : CObject(obj, ownership)
    {
        // !!! Somebody should do a type-check here !!!
    }

    CSequnceHelper (const CObject& obj)
    : CObject(obj)
    {
        // !!! Somebody should do a type-check here !!!
    }

    // Assignment acquires new ownership of pointer

    CSequnceHelper& operator= (const CObject& rhs)
    {
        return (*this = *rhs);
    }
    virtual ~CSequnceHelper(void)
    {
    }

//        CSequnceHelper<T>& operator= (PyObject* obj)
//        {
//            if ( Get() != obj) {
//                Set (obj);
//            }
//            return *this;
//        }

    size_type GetLength (void) const
    {
        return PySequence_Length (Get());
    }

    // Element access
    const T operator[] (int index) const
    {
        return GetItem(index);
    }

    CSequnceProxy<T> operator [] (int index)
    {
        return CSequnceProxy<T>(*this, index);
    }

    T GetItem (int i) const
    {
        PyObject* obj = PySequence_GetItem (Get(), i);
        if (obj == NULL) {
            CError::Check();
        }
        return T(obj, eTakeOwnership);
    }

    // !!! Temporary hack. This function should not be virdual. 1/12/2005 2:39PM ...
    virtual void SetItem (int i, const T& obj)
    {
        if (PySequence_SetItem (Get(), i, obj) == -1)
        {
            throw CSystemError("Cannot set item with a sequence");
        }
        IncRefCount(obj);
    }

    CSequnceHelper<T> Repeat (int count) const
    {
        return CSequnceHelper<T> (PySequence_Repeat (Get(), count), eTakeOwnership);
    }

    CSequnceHelper<T> Concat (const CSequnceHelper<T>& other) const
    {
        return CSequnceHelper<T> (PySequence_Concat(Get(), *other), eTakeOwnership);
    }

    // more STL compatability
    const T front (void) const
    {
        return GetItem(0);
    }

    CSequnceProxy<T> front(void)
    {
        return CSequnceProxy<T>(this, 0);
    }

    const T back (void) const
    {
        return GetItem(size() - 1);
    }

    CSequnceProxy<T> back(void)
    {
        return CSequnceProxy<T>(this, size() - 1);
    }

    void VerifyLength(size_type required_size) const
    {
        if ( size() != required_size )
        {
            throw CIndexError ("Unexpected CSequnceHelper<T> length.");
        }
    }

    void VerifyLength(size_type min_size, size_type max_size) const
    {
        size_type n = size();
        if (n < min_size || n > max_size)
        {
            throw CIndexError ("Unexpected CSequnceHelper<T> length.");
        }
    }

public:
    static bool HasSameType(PyObject* obj)
    {
        return PySequence_Check (obj) == 1;
    }

public:
    // Iterators ...

    class iterator
    // : public std::iterator<std::random_access_iterator_tag, CSequnceProxy<T>, int>
    {
    protected:
        friend class CSequnceHelper<T>;
        CSequnceHelper<T>* seq;
        int position;

    public:
        ~iterator (void)
        {
        }

        iterator (void)
        : seq( 0 )
        , position( 0 )
        {
        }

        iterator (CSequnceHelper<T>* s, int where)
        : seq( s )
        , position( where )
        {
        }

        iterator (const iterator& other)
        : seq( other.seq )
        , position( other.position )
        {
        }

        bool eql (const iterator& other) const
        {
            return (*seq == *other.seq) && (position == other.position);
        }

        bool neq (const iterator& other) const
        {
            return (*seq != *other.seq) || (position != other.position);
        }

        bool lss (const iterator& other) const
        {
            return (position < other.position);
        }

        bool gtr (const iterator& other) const
        {
            return (position > other.position);
        }

        bool leq (const iterator& other) const
        {
            return (position <= other.position);
        }

        bool geq (const iterator& other) const
        {
            return (position >= other.position);
        }

        CSequnceProxy<T> operator*(void)
        {
            return CSequnceProxy<T>(*seq, position);
        }

        CSequnceProxy<T> operator[] (int i)
        {
            return CSequnceProxy<T>(*seq, position + i);
        }

        iterator& operator=(const iterator& other)
        {
            if ( this != &other ) {
                seq = other.seq;
                position = other.position;
            }
            return *this;
        }

        iterator operator+(int n) const
        {
            return iterator(seq, position + n);
        }

        iterator operator-(int n) const
        {
            return iterator(seq, position - n);
        }

        iterator& operator+=(int n)
        {
            position = position + n;
            return *this;
        }

        iterator& operator-=(int n)
        {
            position = position - n;
            return *this;
        }

        int operator-(const iterator& other) const
        {
            if (*seq != *other.seq) {
                throw CSystemError("CSequnceHelper<T>::iterator comparison error");
            }
            return position - other.position;
        }

        // prefix ++
        iterator& operator++ (void)
        {
            ++position;
            return *this;
        }
        // postfix ++
        iterator operator++ (int)
        {
            return iterator(seq, position++);
        }
        // prefix --
        iterator& operator-- (void)
        {
            --position;
            return *this;
        }
        // postfix --
        iterator operator-- (int)
        {
            return iterator(seq, position--);
        }
    };    // end of class CSequnceHelper<T>::iterator

    iterator begin (void)
    {
        return iterator(this, 0);
    }

    iterator end (void)
    {
        return iterator(this, GetLength());
    }

    class const_iterator // : public std::iterator<std::random_access_iterator_tag, CObject, int>
    {
    protected:
        friend class CSequnceHelper<T>;
        const CSequnceHelper<T>* seq;
        int position;

    public:
        ~const_iterator (void)
        {
        }

        const_iterator (void)
        : seq( 0 )
        , position( 0 )
        {
        }

        const_iterator (const CSequnceHelper<T>* s, int where)
        : seq( s )
        , position( where )
        {
        }

        const_iterator(const const_iterator& other)
        : seq( other.seq )
        , position( other.position )
        {
        }

        const T operator*(void) const
        {
            return seq->GetItem(position);
        }

        const T operator[] (int i) const
        {
            return seq->GetItem(position + i);
        }

        const_iterator& operator=(const const_iterator& other)
        {
            if ( this != &other ) {
                seq = other.seq;
                position = other.position;
            }
            return *this;
        }

        const_iterator operator+(int n) const
        {
            return const_iterator(seq, position + n);
        }

        bool operator == (const const_iterator& other) const
        {
            return (*seq == *other.seq) && (position == other.position);
        }

        bool operator != (const const_iterator& other) const
        {
            return (*seq != *other.seq) || (position != other.position);
        }

        bool lss (const const_iterator& other) const
        {
            return (position < other.position);
        }

        bool gtr (const const_iterator& other) const
        {
            return (position > other.position);
        }

        bool leq (const const_iterator& other) const
        {
            return (position <= other.position);
        }

        bool geq (const const_iterator& other) const
        {
            return (position >= other.position);
        }

        const_iterator operator-(int n)
        {
            return const_iterator(seq, position - n);
        }

        const_iterator& operator+=(int n)
        {
            position = position + n;
            return *this;
        }

        const_iterator& operator-=(int n)
        {
            position = position - n;
            return *this;
        }

        int operator-(const const_iterator& other) const
        {
            if (*seq != *other.seq) {
                throw CRuntimeError ("CSequnceHelper<T>::const_iterator::- error");
            }
            return position - other.position;
        }

        // prefix ++
        const_iterator& operator++ (void)
        {
            ++position;
            return *this;
        }
        // postfix ++
        const_iterator operator++ (int)
        {
            return const_iterator(seq, position++);
        }
        // prefix --
        const_iterator& operator-- ()
        {
            --position;
            return *this;
        }
        // postfix --
        const_iterator operator-- (int)
        {
            return const_iterator(seq, position--);
        }
    };    // end of class CSequnceHelper<T>::const_iterator

    const_iterator begin (void) const
    {
        return const_iterator(this, 0);
    }

    const_iterator end (void) const
    {
        return const_iterator(this, GetLength());
    }
};

typedef CSequnceHelper<CObject> CSequence;

class CList;

// PyTuple_Type
class CTuple : public CSequence
{
//PyAPI_FUNC(CInt) _PyTuple_Resize(PyObject **, int);
//PyAPI_FUNC(PyObject *) PyTuple_Pack(CInt, ...);
// int PyTuple_GET_SIZE(   PyObject *p)
// PyObject* PyTuple_GET_ITEM( PyObject *p, int pos)
// void PyTuple_SET_ITEM(  PyObject *p, int pos, PyObject *o)

public:
    CTuple(PyObject* obj, EOwnership ownership = eAcquireOwnership)
    : CSequence(obj, ownership)
    {
        if ( !HasExactSameType(obj) ) {
            throw CTypeError("Invalid conversion");
        }
    }
    CTuple(const CObject& obj)
    : CSequence(obj)
    {
        if ( !HasExactSameType(obj) ) {
            throw CTypeError("Invalid conversion");
        }
    }
    CTuple(const CTuple& obj)
    : CSequence(obj)
    {
    }
    CTuple(const CList& obj);
    /// Create a CTuple of size "size" and initialize it with python None
    CTuple(size_t size = 0)
    : CSequence(PyTuple_New (size), eTakeOwnership)
    {
        // *** WARNING *** PyTuple_SetItem does not increment the new item's reference
        // count, but does decrement the reference count of the item it replaces,
        // if not nil.  It does *decrement* the reference count if it is *not*
        // inserted in the tuple.

        for ( size_t i = 0; i < size; ++i ) {
            if ( PyTuple_SetItem (Get(), i, Py_None) == -1 ) {
                IncRefCount(Py_None);
                throw CSystemError("PyTuple_SetItem error");
            }
            IncRefCount(Py_None);
        }
    }

public:
    // Assign operators ...
    CTuple& operator= (const CObject& obj)
    {
        if ( this != &obj ) {
            if ( !HasExactSameType(obj) ) {
                throw CTypeError("Invalid conversion");
            }
            Set(obj);
        }
        return *this;
    }
    CTuple& operator= (PyObject* obj)
    {
        if ( Get() != obj ) {
            if ( !HasExactSameType(obj) ) {
                throw CTypeError("Invalid conversion");
            }
            Set(obj);
        }
        return *this;
    }

public:
    void SetItem (int offset, const CObject& obj)
    {
        // PyTuple_SetItem does not increment the new item's reference
        // count, but does decrement the reference count of the item it
        // replaces.
        // It does *decrement* the reference count if it is *not*
        // inserted in the tuple.

        if ( PyTuple_SetItem (Get(), offset, obj) == -1 ) {
            IncRefCount(obj);
            throw CSystemError("");
        }
        IncRefCount(obj);
    }
    CObject GetItem(int offset)
    {
        PyObject* obj = PyTuple_GetItem(Get(), offset);
        if ( !obj ) {
            throw CSystemError("");
        }
        return CObject(obj);
    }

    // Fast version. Acquires ownership of obj ...
    void SetItemFast (int offset, PyObject* obj)
    {
        if(PyTuple_SetItem (Get(), offset, obj) == -1 ) {
            IncRefCount(obj);
            throw CSystemError("");
        }
    }
    // Fast version. Does not increment a counter ...
    PyObject* GetItemFast(int offset)
    {
        return PyTuple_GetItem(Get(), offset);
    }

    CTuple GetSlice (int i, int j) const
    {
        return CTuple(PyTuple_GetSlice(Get(), i, j), eTakeOwnership);
    }
    size_type size(void) const
    {
        return PyTuple_Size(Get());
    }

public:
    static bool HasExactSameType(PyObject* obj)
    {
        return PyTuple_CheckExact(obj);
    }
    static bool HasSameType(PyObject* obj)
    {
        return PyTuple_Check (obj);
    }
};

// PyList_Type
class CList : public CSequence
{
// PyAPI_FUNC(PyObject *) _PyList_Extend(PyListObject *, PyObject *);
// int PyList_GET_SIZE(    PyObject *list)
// PyObject* PyList_GET_ITEM(  PyObject *list, int i)
// void PyList_SET_ITEM(   PyObject *list, int i, PyObject *o)

public:
    CList(PyObject* obj, EOwnership ownership = eAcquireOwnership)
    : CSequence(obj, ownership)
    {
        if ( !HasExactSameType(obj) ) {
            throw CTypeError("Invalid conversion");
        }
    }
    CList(const CObject& obj)
    : CSequence(obj)
    {
        if ( !HasExactSameType(obj) ) {
            throw CTypeError("Invalid conversion");
        }
    }
    CList(const CList& obj)
    : CSequence(obj)
    {
    }
    /// Create a CTuple of size "size" and initialize it with python None
    CList(size_t size = 0)
    : CSequence(PyList_New (size), eTakeOwnership)
    {
        for ( size_t i = 0; i < size; ++i ) {
            if ( PyList_SetItem (Get(), i, Py_None) != 0 ) {
                throw CSystemError("");
            }
            IncRefCount(Py_None);
        }
    }

public:
    // Assign operators ...
    CList& operator= (const CObject& obj)
    {
        if ( this != &obj ) {
            if ( !HasExactSameType(obj) ) {
                throw CTypeError("Invalid conversion");
            }
            Set(obj);
        }
        return *this;
    }
    CList& operator= (PyObject* obj)
    {
        if ( Get() != obj ) {
            if ( !HasExactSameType(obj) ) {
                throw CTypeError("Invalid conversion");
            }
            Set(obj);
        }
        return *this;
    }

public:
    void SetItem (int offset, const CObject& obj)
    {
        // PyList_SetItem does not increment the new item's reference
        // count, but does decrement the reference count of the item it replaces,
        // if not nil.  It does *decrement* the reference count if it is *not*
        // inserted in the list.

        if ( PyList_SetItem (Get(), offset, obj) == -1 ) {
            IncRefCount(obj);
            throw CSystemError("");
        }
        IncRefCount(obj);
    }
    CObject GetItem(int offset)
    {
        PyObject* obj = PyList_GetItem(Get(), offset);
        if ( !obj ) {
            throw CSystemError("");
        }
        return CObject(obj);
    }

    // Fast version. Acquires ownership of obj ...
    void SetItemFast (int offset, PyObject* obj)
    {
        if(PyList_SetItem (Get(), offset, obj) != 0) {
            throw CSystemError("");
        }
    }
    // Fast version. Does not increment a counter ...
    PyObject* GetItemFast(int offset)
    {
        return PyList_GetItem(Get(), offset);
    }

public:
    CList GetSlice (int i, int j) const
    {
        return CList (PyList_GetSlice (Get(), i, j), eTakeOwnership);
    }
    void SetSlice (int i, int j, const CObject& obj)
    {
        if(PyList_SetSlice (Get(), i, j, obj) != 0) {
            throw CSystemError("");
        }
    }

public:
    void Append (const CObject& obj)
    {
        if(PyList_Append (Get(), obj) == -1) {
            throw CSystemError("");
        }
    }
    void Insert (int i, const CObject& obj)
    {
        if(PyList_Insert (Get(), i, obj) == -1) {
            throw CSystemError("");
        }
    }
    void Sort (void)
    {
        if(PyList_Sort(Get()) == -1) {
            throw CSystemError("");
        }
    }
    void Reverse (void)
    {
        if(PyList_Reverse(Get()) == -1) {
            throw CSystemError("");
        }
    }

public:
    // Function name is made STL compatible ...
    size_type size(void) const
    {
        return PyList_Size(Get());
    }

public:
    static bool HasExactSameType(PyObject* obj)
    {
        return PyList_CheckExact (obj);
    }
    static bool HasSameType(PyObject* obj)
    {
        return PyList_Check (obj);
    }
};

// PySet_Type
class CSet : public CObject
{
public:
public:
    static bool HasSameType(PyObject* obj)
    {
        return PyAnySet_Check(obj);
    }
    static bool HasExactSameType(PyObject* obj)
    {
        return PyAnySet_Check(obj);
    }
};

// PyFrozenSet_Type
class CFrozenSet : public CObject
{
public:
public:
    static bool HasSameType(PyObject* obj)
    {
        return PyAnySet_Check(obj);
    }
    static bool HasExactSameType(PyObject* obj)
    {
        return PyFrozenSet_CheckExact(obj);
    }
};

//////////////////////////////////////////////////////////////////////////
inline
CTuple::CTuple(const CList& obj)
: CSequence(PyList_AsTuple(obj), eTakeOwnership)
{
}

}                                       // namespace pythonpp

END_NCBI_SCOPE

#endif                                  // PYTHONPP_SEQ_H

/* ===========================================================================
*
* $Log$
* Revision 1.4  2005/02/10 17:43:56  ssikorsk
* Changed: more 'precise' exception types
*
* Revision 1.3  2005/01/27 18:50:03  ssikorsk
* Fixed: a bug with transactions
* Added: python 'transaction' object
*
* Revision 1.2  2005/01/18 21:31:38  ucko
* Don't inherit from std::iterator<>, which GCC 2.95 lacks.
*
* Revision 1.1  2005/01/18 19:26:08  ssikorsk
* Initial version of a Python DBAPI module
*
*
* ===========================================================================
*/
