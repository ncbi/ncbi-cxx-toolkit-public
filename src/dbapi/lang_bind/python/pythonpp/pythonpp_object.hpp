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

#ifndef PYTHONPP_OBJECT_H
#define PYTHONPP_OBJECT_H

#include "pythonpp_error.hpp"

BEGIN_NCBI_SCOPE

namespace pythonpp
{

enum EOwnership {eTakeOwnership, eAcquireOwnership};
enum EOwnershipFuture {eOwned, eAcquired, eBorrowed};

class CType;
class CString;

// Strong-typed operation ...
inline PyObject* IncRefCount(PyObject* obj)
{
    Py_INCREF(obj);
    return obj;
}
inline PyObject* DecRefCount(PyObject* obj)
{
    Py_DECREF(obj);
    return obj;
}

// PyObject
// PyVarObject
class CObject
{
// ???
///* Generic operations on objects */
//PyAPI_FUNC(CInt) PyObject_Print(PyObject *, FILE *, int);
//PyAPI_FUNC(void) _PyObject_Dump(PyObject *);
//#ifdef Py_USING_UNICODE
//PyAPI_FUNC(PyObject *) PyObject_Unicode(PyObject *);
//#endif
//PyAPI_FUNC(PyObject *) PyObject_RichCompare(PyObject *, PyObject *, int);
//PyAPI_FUNC(CInt) PyObject_RichCompareBool(PyObject *, PyObject *, int);
//PyAPI_FUNC(PyObject *) PyObject_GetAttr(PyObject *, PyObject *);
//PyAPI_FUNC(CInt) PyObject_HasAttr(PyObject *, PyObject *);
//PyAPI_FUNC(PyObject **) _PyObject_GetDictPtr(PyObject *);
//PyAPI_FUNC(PyObject *) PyObject_SelfIter(PyObject *);
//PyAPI_FUNC(PyObject *) PyObject_GenericGetAttr(PyObject *, PyObject *);
//PyAPI_FUNC(CInt) PyObject_GenericSetAttr(PyObject *,
//                          PyObject *, PyObject *);
//PyAPI_FUNC(CInt) PyObject_Not(PyObject *);
//PyAPI_FUNC(CInt) PyCallable_Check(PyObject *);
//PyAPI_FUNC(CInt) PyNumber_Coerce(PyObject **, PyObject **);
//PyAPI_FUNC(CInt) PyNumber_CoerceEx(PyObject **, PyObject **);

//PyAPI_FUNC(void) PyObject_ClearWeakRefs(PyObject *);

///* A slot function whose address we need to compare */
//extern int _PyObject_SlotCompare(PyObject *, PyObject *);

public:

public:
    /// Creates a python None object
    CObject(void)
    : m_PyObject(Py_None)
    // , m_Ownership(eOwned)
    {
        IncRefCount(Get());
    }
    explicit CObject(PyObject* obj, EOwnership ownership = eAcquireOwnership)
    : m_PyObject(obj)
    // , m_Ownership(eAcquireOwnership ? eAcquired : eOwned)  // !!! Currently this parameter does not much value of "ownership"
    {
        _ASSERT(Get());
        if ( ownership == eAcquireOwnership ) {
            IncRefCount(Get());
        }
    }
    CObject(const CObject& obj)
    : m_PyObject(obj)
    // , m_Ownership(eOwned)
    {
        _ASSERT(Get());
        IncRefCount(Get());
    }
    ~CObject(void)
    {
        Release();
    }

    CObject& operator= (const CObject& obj)
    {
        if (this != &obj)
        {
            Set(obj);
        }
        return *this;
    }

    CObject& operator= (PyObject* obj)
    {
        if (Get() != obj) {
            Set (obj);
        }
        return *this;
    }

public:
    // Implicit conversion to the PyObject* type
    operator PyObject* (void) const
    {
        return m_PyObject;
    }
    PyObject* Get(void) const
    {
        return m_PyObject;
    }
    /// Not exception-safe this time
    void Set(PyObject* obj, EOwnership ownership = eAcquireOwnership)
    {
        _ASSERT(obj);

        Release();
        m_PyObject = obj;
        if ( ownership == eAcquireOwnership )
        {
            IncRefCount(*this);
        }
    }
    void Release(void)
    {
        // PyAPI_FUNC(void) IncRefCount(PyObject *);
        // PyAPI_FUNC(void) Py_DecRef(PyObject *);
        Py_XDECREF(m_PyObject);
        m_PyObject = NULL;
    }

public:
    // Atributes ...
    CObject GetAttr (const std::string& name) const
    {
        PyObject* obj = PyObject_GetAttrString(Get(), const_cast<char*>(name.c_str()));
        if ( !obj ) {
            throw CAttributeError("Attribute does not exist");
        }
        return CObject(obj, eTakeOwnership);
    }
    void SetAttr(const std::string& name, const CObject& value)
    {
        if(PyObject_SetAttrString (Get(), const_cast<char*>(name.c_str()), value.Get()) == -1) {
            throw CAttributeError("SetAttr failed");
        }
    }
    void DelAttr (const std::string& name)
    {
        if(PyObject_DelAttrString (Get(), const_cast<char*>(name.c_str())) == -1) {
            throw CAttributeError("DelAttr failed");
        }
    }
    bool HasAttr (const std::string& name) const
    {
        return PyObject_HasAttrString (Get(), const_cast<char*>(name.c_str())) != 0;
    }

public:
    // Itemss ...
    CObject GetItem (const CObject& key) const
    {
        PyObject* obj = PyObject_GetItem(Get(), key.Get());
        if ( !obj ) {
            throw CKeyError("Item does not exist");
        }
        return CObject(obj, eTakeOwnership);
    }
    /* Do not delete this ...
    void SetItem(const CObject& key, const CObject& value)
    {
        if(PyObject_SetItem (Get(), key.Get(), value.Get()) == -1) {
            throw CKeyError ("SetItem failed");
        }
    }
    */
    void DelItem (const CObject& key)
    {
        if(PyObject_DelItem (Get(), key.Get()) == -1) {
            throw CKeyError ("DelItem failed");
        }
    }

public:
    long GetHashValue (void) const
    {
        long value = PyObject_Hash (Get());

        if ( value == -1) {
            throw CSystemError("Invalid hash value");
        }
        return value;
    }

public:
    CType GetType(void) const;
    // CString GetString(void) const;
    // CString GetRepresentation() const;
    // std::string as_string(void) const;

public:
    // Equality and comparison based on PyObject_Compare
    bool operator==(const CObject& obj) const
    {
        int result = PyObject_Compare (Get(), obj);
        CError::Check();
        return result == 0;
    }
    bool operator!=(const CObject& obj) const
    {
        int result = PyObject_Compare (Get(), obj);
        CError::Check();
        return result != 0;
    }
    bool operator>=(const CObject& obj) const
    {
        int result = PyObject_Compare (Get(), obj);
        CError::Check();
        return result >= 0;
    }
    bool operator<=(const CObject& obj) const
    {
        int result = PyObject_Compare (Get(), obj);
        CError::Check();
        return result <= 0;
    }
    bool operator<(const CObject& obj) const
    {
        int result = PyObject_Compare (Get(), obj);
        CError::Check();
        return result < 0;
    }
    bool operator>(const CObject& obj) const
    {
        int result = PyObject_Compare (Get(), obj);
        CError::Check();
        return result > 0;
    }

protected:
    bool IsNumeric (void) const
    {
        return PyNumber_Check (Get()) != 0;
    }
    bool IsSequence (void) const
    {
        return PySequence_Check (Get()) != 0;
    }
    bool IsTrue (void) const
    {
        return PyObject_IsTrue (Get()) != 0;
    }
    bool IsObjectType (const CType& t) const;

private:
    PyObject*   m_PyObject;
    // EOwnershipFuture  m_Ownership;
};

class CNone : public CObject
{
public:
    CNone(void)
    : CObject(Py_None)
    {
    }
    CNone(const CObject& obj)
    : CObject(obj)
    {
        if ( !HasExactSameType(obj) ) {
            throw CTypeError("Invalid conversion");
        }
    }

    CNone& operator= (const CNone& obj)
    {
        if ( this != &obj) {
            Set(obj);
        }
        return *this;
    }
    CNone& operator= (const CObject& obj)
    {
        if ( this != &obj) {
            if ( !HasExactSameType(obj) ) {
                throw CTypeError("Invalid conversion");
            }
            Set(obj);
        }
        return *this;
    }

public:
    static bool HasSameType(PyObject* obj)
    {
        //  Py_None is an object of undefined type ...
        // So, we can compare only objects themelf ...
        return obj == Py_None;
    }
    static bool HasExactSameType(PyObject* obj)
    {
        //  Py_None is an object of undefined type ...
        // So, we can compare only objects themelf ...
        return obj == Py_None;
    }
};

// PyTypeObject
// PyHeapTypeObject
class CType : public CObject
{
// int PyType_HasFeature(  PyObject *o, int feature)
// int PyType_IS_GC(   PyObject *o)
// int PyType_IsSubtype(   PyTypeObject *a, PyTypeObject *b)
// PyAPI_FUNC(PyObject *) PyType_GenericAlloc(PyTypeObject *, int);
// PyAPI_FUNC(PyObject *) PyType_GenericNew(PyTypeObject *, PyObject *, PyObject *);
// PyAPI_FUNC(CInt) PyType_Ready(PyTypeObject *);

public:
    CType(PyObject* obj, EOwnership ownership = eAcquireOwnership)
    : CObject(obj, ownership)
    {
    }
    CType(const CObject& obj)
    : CObject(obj)
    {
        if ( !HasExactSameType(obj) ) {
            throw CTypeError("Invalid conversion");
        }
    }
    CType(const CType& obj)
    : CObject(obj)
    {
    }

    CType& operator= (const CObject& obj)
    {
        if ( this != &obj) {
            if ( !HasExactSameType(obj) ) {
                throw CTypeError("Invalid conversion");
            }
            Set(obj);
        }
        return *this;
    }

    CType& operator= (PyObject* obj)
    {
        if ( Get() != obj ) {
            Set(obj);
        }
        return *this;
    }

public:
    static bool HasSameType(PyObject* obj)
    {
        return PyType_Check (obj);
    }
    static bool HasExactSameType(PyObject* obj)
    {
        return PyType_CheckExact (obj);
    }
};

///////////////////////////////////////////////////////////////////////////
// Numeric interface
inline CObject operator+ (const CObject& a)
{
    PyObject* tmp_obj = PyNumber_Positive(a.Get());
    if ( !tmp_obj ) {
        throw CArithmeticError("PyNumber_Positive");
    }
    return CObject(tmp_obj, eTakeOwnership);
}
inline CObject operator- (const CObject& a)
{
    PyObject* tmp_obj = PyNumber_Negative(a.Get());
    if ( !tmp_obj ) {
        throw CArithmeticError("PyNumber_Negative");
    }
    return CObject(tmp_obj, eTakeOwnership);
}

inline CObject abs(const CObject& a)
{
    PyObject* tmp_obj = PyNumber_Absolute(a.Get());
    if ( !tmp_obj ) {
        throw CArithmeticError("PyNumber_Absolute");
    }
    return CObject(tmp_obj, eTakeOwnership);
}

inline std::pair<CObject, CObject> coerce(const CObject& a, const CObject& b)
{
    PyObject* p1;
    PyObject* p2;
    p1 = a.Get();
    p2 = b.Get();
    if(PyNumber_Coerce(&p1, &p2) == -1) {
        throw CArithmeticError("PyNumber_Coerce");
    }
    return std::pair<CObject, CObject>(CObject(p1, eTakeOwnership), CObject(p2, eTakeOwnership));
}

inline CObject operator+ (const CObject& a, const CObject& b)
{
    PyObject* tmp_obj = PyNumber_Add(a.Get(), b.Get());
    if ( !tmp_obj ) {
        throw CArithmeticError("PyNumber_Add");
    }
    return CObject(tmp_obj, eTakeOwnership);
}

inline CObject operator- (const CObject& a, const CObject& b)
{
    PyObject* tmp_obj = PyNumber_Subtract(a.Get(), b.Get());
    if ( !tmp_obj ) {
        throw CArithmeticError("PyNumber_Subtract");
    }
    return CObject(tmp_obj, eTakeOwnership);
}

inline CObject operator* (const CObject& a, const CObject& b)
{
    PyObject* tmp_obj = PyNumber_Multiply(a.Get(), b.Get());
    if ( !tmp_obj ) {
        throw CArithmeticError("PyNumber_Multiply");
    }
    return CObject(tmp_obj, eTakeOwnership);
}

inline CObject operator/ (const CObject& a, const CObject& b)
{
    PyObject* tmp_obj = PyNumber_Divide(a.Get(), b.Get());
    if ( !tmp_obj ) {
        throw CArithmeticError("PyNumber_Divide");
    }
    return CObject(tmp_obj, eTakeOwnership);
}

inline CObject operator% (const CObject& a, const CObject& b)
{
    PyObject* tmp_obj = PyNumber_Remainder(a.Get(), b.Get());
    if ( !tmp_obj ) {
        throw CArithmeticError("PyNumber_Remainder");
    }
    return CObject(tmp_obj, eTakeOwnership);
}

//////////////////////////////////////////////////////////////////////////
inline
CType
CObject::GetType(void) const
{
    // ???
    PyObject* obj = PyObject_Type (Get());
    if ( !obj ) {
        throw CTypeError("Type does not exist");
    }
    return CType(obj, eTakeOwnership);
}

/* Do not delete this code ...
inline
CString
CObject::GetString(void) const
{
    // ???
    PyObject* obj = PyObject_Str (Get());
    if ( !obj ) {
        throw CTypeError("Unable to convert an object to a string");
    }
    return CString(obj, eTakeOwnership);
}

inline
CString
CObject::GetRepresentation() const
{
    // ???
    PyObject* obj = PyObject_Repr(Get());
    if ( !obj ) {
        throw CTypeError("Unable to convert an object to a representation");
    }
    return CString(obj, eTakeOwnership);
}

inline
std::string
CObject::as_string(void) const
{
    return static_cast<std::string>(GetString());
}
*/

inline
bool
CObject::IsObjectType (const CType& t) const
{
    return GetType().Get() == t.Get();
}

}
                                       // namespace pythonpp
END_NCBI_SCOPE

#endif                                  // PYTHONPP_OBJECT_H

/* ===========================================================================
*
* $Log$
* Revision 1.5  2005/02/10 17:43:56  ssikorsk
* Changed: more 'precise' exception types
*
* Revision 1.4  2005/02/08 19:19:35  ssikorsk
* A lot of improvements
*
* Revision 1.3  2005/01/27 18:50:03  ssikorsk
* Fixed: a bug with transactions
* Added: python 'transaction' object
*
* Revision 1.2  2005/01/21 15:50:18  ssikorsk
* Fixed: build errors with GCC 2.95.
*
* Revision 1.1  2005/01/18 19:26:07  ssikorsk
* Initial version of a Python DBAPI module
*
*
* ===========================================================================
*/
