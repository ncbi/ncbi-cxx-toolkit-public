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

#ifndef PYTHONPP_DICT_H
#define PYTHONPP_DICT_H

#include "pythonpp_object.hpp"

BEGIN_NCBI_SCOPE

namespace pythonpp
{

template<class T> class CDictHelper;

template<class T>
class CDictProxy
{
protected:
    CDictHelper<T>& s;  //< the dictionary
    CObject key;        //< item key
    T value;            //< value

public:
    CDictProxy<T> (CDictHelper<T>& map, const std::string& k)
    : s(map), value()
    {
        key = CString(k);
        if(map.HasKey(key)) value = map.GetItem(key);
    };

    CDictProxy<T> (CDictHelper<T>& map, const CObject& k)
        : s(map), key(k), value()
    {
        if(map.HasKey(key)) value = map.GetItem(key);
    };

    ~CDictProxy<T>(void)
    {
    }

    // CDictHelper<T> stuff
    // lvalue
    CDictProxy<T>& operator=(const CDictProxy<T>& other)
    {
        if ( this != &other) {
            value = other.value;
            s.SetItem(key, other.value);
        }
        return *this;
    };

    CDictProxy<T>& operator= (const T& ob)
    {
        value = ob;
        s.SetItem (key, ob);
        return *this;
    }

    operator T&(void)
    {
        return value;
    }

    operator const T&(void) const
    {
        return value;
    }
}; // end of CDictProxy


template<class T>
class CDictHelper : public CObject
{
protected:
    CDictHelper(void)
    {
    }

public:
    typedef size_t size_type;
    typedef CObject key_type;
    typedef CDictProxy<T> data_type;
    typedef std::pair< const T, T > value_type;
    typedef std::pair< const T, CDictProxy<T> > reference;
    typedef const std::pair< const T, const T > const_reference;
    typedef std::pair< const T, CDictProxy<T> > pointer;

public:
    // Constructors

    CDictHelper(PyObject *obj, EOwnership ownership = eAcquireOwnership)
    : CObject(obj, ownership)
    {
        // !!! Somebody should do a type-check here !!!
    }

    CDictHelper(const CObject& obj)
    : CObject(obj)
    {
        // !!! Somebody should do a type-check here !!!
    }

    virtual ~CDictHelper(void)
    {
    }

    CDictHelper& operator= (const CObject& rhs)
    {
        return (*this = *rhs);
    }

    CDictHelper& operator= (PyObject* rhsp)
    {
        if ( Get() != rhsp) {
            Set (rhsp);
        }
        return *this;
    }

public:
    void clear (void)
    {
        CList k = Keys();
        for(CList::iterator i = k.begin(); i != k.end(); ++i)
        {
            DelItem(*i);
        }
    }

    // !!!
    virtual size_type size() const
    {
        return PyMapping_Length (Get());
    }

    // Element Access
    T operator [] (const string& key) const
    {
        return GetItem(key);
    }

    T operator [] (const CObject& key) const
    {
        return GetItem(key);
    }

    CDictProxy<T> operator [] (const string& key)
    {
        return CDictProxy<T>(*this, key);
    }

    CDictProxy<T> operator [] (const CObject& key)
    {
        return CDictProxy<T>(*this, key);
    }

    int GetLength (void) const
    {
        return PyMapping_Length (Get());
    }

    bool HasKey (const string& s) const
    {
        return PyMapping_HasKeyString ( Get(), const_cast<char*>( s.c_str() ) ) != 0;
    }

    bool HasKey (const CObject& s) const
    {
        return PyMapping_HasKey (Get(), s.Get()) != 0;
    }

    // !!!
    virtual T GetItem (const string& s) const
    {
        return T(
            PyMapping_GetItemString (Get(), const_cast<char*>(s.c_str())),
            eTakeOwnership
        );
    }

    // !!!
    virtual T GetItem (const CObject& s) const
    {
        return T(
            PyObject_GetItem (Get(), s.Get()),
            eTakeOwnership
        );
    }

    // !!!
    virtual void SetItem (const string& key, const CObject& obj)
    {
        if (PyMapping_SetItemString (Get(), const_cast<char*>(key.c_str()), obj) == -1) {
            throw CSystemError("SetItem");
        }
    }

    // !!!
    virtual void SetItem (const CObject& key, const CObject& obj)
    {
        if (PyObject_SetItem (Get(), key, obj) == -1) {
            throw CSystemError("SetItem");
        }
    }

    // !!!
    virtual void DelItem (const string& s)
    {
        if (PyMapping_DelItemString (Get(), const_cast<char*>(s.c_str())) == -1) {
            throw CSystemError("DelItem");
        }
    }

    // !!!
    virtual void DelItem (const CObject& obj)
    {
        if (PyMapping_DelItem (Get(), obj) == -1) {
            throw CSystemError("DelItem");
        }
    }

    // Queries
    CList Keys (void) const
    {
        return CList(PyMapping_Keys(Get()), eTakeOwnership);
    }

    CList Values (void) const
    {
        return CList(PyMapping_Values(Get()), eTakeOwnership);
    }

    CList Items (void) const
    {
        return CList(PyMapping_Items(Get()), eTakeOwnership);
    }

public:
    static bool HasSameType(PyObject* obj)
    {
        return PyMapping_Check (obj) != 0;
    }

public:
    // Iterator ...
    // Future development ...

};  // end of CDictHelper<T>

typedef CDictHelper<CObject> CDictBase;

// PyDict_Type
class CDict : public CDictBase
{
// PyAPI_FUNC(PyObject *) PyDictProxy_New(PyObject *);
//PyAPI_FUNC(void) PyDict_Clear(PyObject *mp);
//PyAPI_FUNC(CInt) PyDict_Contains(PyObject *mp, PyObject *key);
//PyAPI_FUNC(PyObject *) PyDict_Copy(PyObject *mp);


//PyAPI_FUNC(CInt) PyDict_Next(PyObject *mp, int *pos, PyObject **key, PyObject **value);
//PyAPI_FUNC(CInt) PyDict_Update(PyObject *mp, PyObject *other);
//PyAPI_FUNC(CInt) PyDict_MergeFromSeq2(PyObject *d, PyObject *seq2, int override);

public:
    CDict(PyObject* obj, EOwnership ownership = eAcquireOwnership)
    : CDictBase(obj, ownership)
    {
    }
    CDict(const CObject& obj)
    {
        if ( !HasExactSameType(obj) ) {
            throw CTypeError("Invalid conversion");
        }
        Set(obj);
    }
    CDict(const CDict& obj)
    : CDictBase(obj)
    {
    }
    CDict(void)
    : CDictBase(PyDict_New(), eTakeOwnership)
    {
    }

public:
    // Assign operators ...
    CDict& operator= (const CObject& obj)
    {
        if ( this != &obj ) {
            if ( !HasExactSameType(obj) ) {
                throw CTypeError("Invalid conversion");
            }
            Set(obj);
        }
        return *this;
    }
    CDict& operator= (PyObject* obj)
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
    // Function name is made STL compatible ...
    size_type size(void) const
    {
        return PyDict_Size(Get());
    }
    template<class O>
    void Merge(const CDictHelper<O>& other, bool override = true)
    {
        if( PyDict_Merge ( Get(), other, (override ? 1 : 0) ) == -1 ) {
            throw CSystemError("Merge");
        }
    }

public:
    // Queries
    CList Keys (void) const
    {
        return CList(PyDict_Keys(Get()), eTakeOwnership);
    }

    CList Values (void) const
    { // each returned item is a (key, value) pair
        return CList(PyDict_Values(Get()), eTakeOwnership);
    }

    CList Items (void) const
    {
        return CList(PyDict_Items(Get()), eTakeOwnership);
    }

    CObject GetItem (const string& s) const
    {
        return CObject(
            PyDict_GetItemString (Get(), s.c_str()),
            eTakeOwnership
        );
    }
    CObject GetItem (const CObject& obj) const
    {
        return CObject(
            PyDict_GetItem (Get(), obj),
            eTakeOwnership
        );
    }
    void SetItem (const string& key, const CObject& obj)
    {
        if (PyDict_SetItemString (Get(), key.c_str(), obj) == -1) {
            throw CSystemError("SetItem");
        }
    }
    void SetItem (const CObject& key, const CObject& obj)
    {
        if (PyDict_SetItem (Get(), key, obj) == -1) {
            throw CSystemError("SetItem");
        }
    }

    void DelItem (const string& key)
    {
        if (PyDict_DelItemString (Get(), const_cast<char*>(key.c_str())) == -1) {
            throw CSystemError("DelItem");
        }
    }
    void DelItem (const CObject& key)
    {
        if (PyDict_DelItem (Get(), key) == -1) {
            throw CSystemError("DelItem");
        }
    }

public:
    static bool HasSameType(PyObject* obj)
    {
        return PyDict_Check(obj);
    }
    static bool HasExactSameType(PyObject* obj)
    {
        return PyDict_CheckExact(obj);
    }
};

}                                       // namespace pythonpp

END_NCBI_SCOPE

#endif                                  // PYTHONPP_DICT_H

/* ===========================================================================
*
* $Log$
* Revision 1.3  2005/02/10 17:43:56  ssikorsk
* Changed: more 'precise' exception types
*
* Revision 1.2  2005/01/27 18:50:03  ssikorsk
* Fixed: a bug with transactions
* Added: python 'transaction' object
*
* Revision 1.1  2005/01/18 19:26:07  ssikorsk
* Initial version of a Python DBAPI module
*
*
* ===========================================================================
*/
