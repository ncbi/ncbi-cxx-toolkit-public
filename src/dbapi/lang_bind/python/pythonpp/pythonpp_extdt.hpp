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

#ifndef PYTHONPP_EXTDT_H
#define PYTHONPP_EXTDT_H

#include "pythonpp_object.hpp"

BEGIN_NCBI_SCOPE

namespace pythonpp
{

// PyBuffer_Type
class CBuffer : public CObject
{
// int PyBuffer_Check( PyObject *p)
//PyAPI_FUNC(PyObject *) PyBuffer_FromObject(PyObject *base, int offset, int size);
//PyAPI_FUNC(PyObject *) PyBuffer_FromReadWriteObject(PyObject *base, int offset, int size);
//PyAPI_FUNC(PyObject *) PyBuffer_FromMemory(void *ptr, int size);
//PyAPI_FUNC(PyObject *) PyBuffer_FromReadWriteMemory(void *ptr, int size);
//PyAPI_FUNC(PyObject *) PyBuffer_New(CInt size);

public:
protected:
private:
};

// PyCellObject
class CCell : public CObject
{
//PyAPI_FUNC(PyObject *) PyCell_New(PyObject *);
//PyAPI_FUNC(PyObject *) PyCell_Get(PyObject *);
//PyAPI_FUNC(CInt) PyCell_Set(PyObject *, PyObject *);
// int PyCell_Check(   ob)
// PyObject* PyCell_GET(   PyObject *cell)
// void PyCell_SET(    PyObject *cell, PyObject *value)

public:
protected:
private:
};

// PyRange_Type
class CRange : public CObject
{
//PyAPI_FUNC(PyObject *) PyRange_New(long, long, long, int);

public:
protected:
private:
};

// PySlice_Type
class CSlice : public CObject
{
//PyAPI_FUNC(PyObject *) PySlice_New(PyObject* start, PyObject* stop,
//                                  PyObject* step);
//PyAPI_FUNC(CInt) PySlice_GetIndices(PySliceObject *r, int length,
//                                  int *start, int *stop, int *step);
//PyAPI_FUNC(CInt) PySlice_GetIndicesEx(PySliceObject *r, int length,
//                    int *start, int *stop,
//                    int *step, int *slicelength);

public:
public:
    static bool HasExactSameType(PyObject* obj)
    {
        return PySlice_Check (obj);
    }
    static bool HasSameType(PyObject* obj)
    {
        return PySlice_Check (obj);
    }
};

class CCalable : public CObject
{
public:
    CCalable(PyObject* obj, EOwnership ownership = eAcquireOwnership)
    : CObject(obj, ownership)
    {
    }
    CCalable(const CObject& obj)
    {
        if ( !HasExactSameType(obj) ) {
            throw CTypeError("Invalid conversion");
        }
        Set(obj);
    }
    CCalable(const CCalable& obj)
    : CObject(obj)
    {
    }

public:
    // Assign operators ...
    CCalable& operator= (const CObject& obj)
    {
        if ( this != &obj ) {
            if ( !HasExactSameType(obj) ) {
                throw CTypeError("Invalid conversion");
            }
            Set(obj);
        }
        return *this;
    }
    CCalable& operator= (PyObject* obj)
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
    /// Call
    CObject Apply(void) const
    {
        PyObject* tmp_obj = PyObject_CallObject(Get(), NULL);
        if ( !tmp_obj ) {
            throw CSystemError("PyObject_CallObject error");
        }
        return CObject(tmp_obj, eTakeOwnership);
    }
    /// Call
    CObject Apply(const CTuple& args) const
    {
        PyObject* tmp_obj = PyObject_CallObject(Get(), args.Get());
        if ( !tmp_obj ) {
            throw CSystemError("PyObject_CallObject error");
        }
        return CObject(tmp_obj, eTakeOwnership);
    }
    /// Call with keywords
    CObject Apply(const CTuple& args, const CDict& key_words) const
    {
        PyObject* tmp_obj = PyEval_CallObjectWithKeywords( Get(), args.Get(), key_words.Get() );
        if ( !tmp_obj ) {
            throw CSystemError("PyEval_CallObjectWithKeywords error");
        }
        return CObject(tmp_obj, eTakeOwnership);
    }

public:
    static bool HasExactSameType(PyObject* obj)
    {
        return PyCallable_Check (obj) != 0;
    }
};

// PyModule_Type
class CModule : public CObject
{
//PyAPI_FUNC(void) _PyModule_Clear(PyObject *);
// int PyModule_AddObject( PyObject *module, char *name, PyObject *value)
// int PyModule_AddIntConstant( PyObject *module, char *name, long value)
// int PyModule_AddStringConstant( PyObject *module, char *name, char *value)

public:
    CModule(const char* name)
    : CObject(PyModule_New(const_cast<char*>(name)), false)
    {
    }
    CModule(PyObject* obj, EOwnership ownership = eAcquireOwnership)
    : CObject(obj, ownership)
    {
        if ( !HasExactSameType(obj) ) {
            throw CTypeError("Invalid conversion");
        }
    }
    CModule(const CModule& obj)
    : CObject(obj)
    {
        if ( !HasExactSameType(obj) ) {
            throw CTypeError("Invalid conversion");
        }
    }
    CModule(const std::string& name)
    // !!! This is strange .....
    : CObject(PyImport_AddModule( const_cast<char *>(name.c_str()) ), true)
    {
    }

public:
    CDict GetDict()
    {
        // PyModule_GetDict returns borrowed reference !!!
        return CDict(PyModule_GetDict(Get()));
    }
    string GetName(void) const
    {
        const char* tmp_str = PyModule_GetName(Get());

        CError::check(tmp_str);
        return tmp_str;
    }
    string GetFileName(void) const
    {
        const char* tmp_str = PyModule_GetFilename(Get());

        CError::check(tmp_str);
        return tmp_str;
    }

public:
    static bool HasExactSameType(PyObject* obj)
    {
        return PyModule_CheckExact(obj);
    }
    static bool HasSameType(PyObject* obj)
    {
        return PyModule_Check(obj);
    }
};

class CDescr : public CObject
{
// PyAPI_FUNC(PyObject *) PyDescr_NewGetSet(PyTypeObject *, struct PyGetSetDef *);
// PyAPI_FUNC(PyObject *) PyDescr_NewMember(PyTypeObject *, struct PyMemberDef *);
// PyAPI_FUNC(PyObject *) PyDescr_NewMethod(PyTypeObject *, PyMethodDef *);
// PyAPI_FUNC(PyObject *) PyDescr_NewWrapper(PyTypeObject *, struct wrapperbase *, void *);
// PyAPI_FUNC(PyObject *) PyDescr_NewClassMethod(PyTypeObject *, PyMethodDef *);
// int PyDescr_IsData( PyObject *descr)
// PyObject* PyWrapper_New(    PyObject *, PyObject *)

public:
protected:
private:
};

// PySeqIter_Type
class CSeqIterator : public CObject
{
// PyAPI_FUNC(PyObject *) PySeqIter_New(PyObject *);

public:
public:
    static bool HasExactSameType(PyObject* obj)
    {
        return PySeqIter_Check(obj);
    }
    static bool HasSameType(PyObject* obj)
    {
        return PySeqIter_Check(obj);
    }
};

// PyCallIter_Type
class CCallIterator : public CObject
{
// PyAPI_FUNC(PyObject *) PyCallIter_New(PyObject *, PyObject *);

public:
public:
    static bool HasExactSameType(PyObject* obj)
    {
        return PyCallIter_Check(obj);
    }
    static bool HasSameType(PyObject* obj)
    {
        return PyCallIter_Check(obj);
    }
};

class CGenerator : public CObject
{
public:
//        CGenerator(PyFrameObject* frame)
//        : CObject(PyGen_New(frame), false)
//        {
//            // ??? IncRefCount(frame);
//        }

public:
    static bool HasExactSameType(PyObject* obj)
    {
        return PyGen_CheckExact(obj);
    }
    static bool HasSameType(PyObject* obj)
    {
        return PyGen_Check(obj);
    }
};


class CCObject : public CObject
{
// PyObject* PyCObject_FromVoidPtr(    void* cobj, void (*destr)(void *))
// PyObject* PyCObject_FromVoidPtrAndDesc( void* cobj, void* desc, void (*destr)(void *, void *))
// void* PyCObject_AsVoidPtr(  PyObject* self)
// void* PyCObject_GetDesc(    PyObject* self)
// int PyCObject_SetVoidPtr(   PyObject* self, void* cobj)

public:
public:
    static bool HasExactSameType(PyObject* obj)
    {
        return PyCObject_Check(obj);
    }
    static bool HasSameType(PyObject* obj)
    {
        return PyCObject_Check(obj);
    }
};

class CWeakRef : public CObject
{
// int PyWeakref_CheckRef( obj)
// int PyWeakref_CheckProxy(   obj)
// PyObject* PyWeakref_NewRef( PyObject *ob, PyObject *callback)
// PyObject* PyWeakref_NewProxy(   PyObject *ob, PyObject *callback)
// PyObject* PyWeakref_GetObject(  PyObject *ref)
// PyObject* PyWeakref_GET_OBJECT( PyObject *ref)

public:
public:
    static bool HasExactSameType(PyObject* obj)
    {
        return PyWeakref_Check(obj);
    }
    static bool HasSameType(PyObject* obj)
    {
        return PyWeakref_Check(obj);
    }
};

class CMethod : public CObject
{
// PyObject* PyMethod_New( PyObject *func. PyObject *self, PyObject *class)
// PyObject* PyMethod_Class(   PyObject *meth)
// PyObject* PyMethod_GET_CLASS(   PyObject *meth)
// PyObject* PyMethod_Function(    PyObject *meth)
// PyObject* PyMethod_GET_FUNCTION(    PyObject *meth)
// PyObject* PyMethod_Self(    PyObject *meth)
// PyObject* PyMethod_GET_SELF(    PyObject *meth)

public:
public:
    static bool HasExactSameType(PyObject* obj)
    {
        return PyMethod_Check(obj);
    }
    static bool HasSameType(PyObject* obj)
    {
        return PyMethod_Check(obj);
    }
};

// PyFunction_Type
// PyClassMethod_Type
// PyStaticMethod_Type
class CFunction : public CObject
{
//PyAPI_FUNC(PyObject *) PyFunction_New(PyObject *, PyObject *);
//PyAPI_FUNC(PyObject *) PyFunction_GetCode(PyObject *);
//PyAPI_FUNC(PyObject *) PyFunction_GetGlobals(PyObject *);
//PyAPI_FUNC(PyObject *) PyFunction_GetModule(PyObject *);
//PyAPI_FUNC(PyObject *) PyFunction_GetDefaults(PyObject *);
//PyAPI_FUNC(int) PyFunction_SetDefaults(PyObject *, PyObject *);
//PyAPI_FUNC(PyObject *) PyFunction_GetClosure(PyObject *);
//PyAPI_FUNC(int) PyFunction_SetClosure(PyObject *, PyObject *);

//PyAPI_FUNC(PyObject *) PyClassMethod_New(PyObject *);
//PyAPI_FUNC(PyObject *) PyStaticMethod_New(PyObject *);

public:
public:
    static bool HasExactSameType(PyObject* obj)
    {
        return PyFunction_Check(obj);
    }
    static bool HasSameType(PyObject* obj)
    {
        return PyFunction_Check(obj);
    }
};

class CInstance : public CObject
{
// PyObject* PyInstance_New(   PyObject *class, PyObject *arg, PyObject *kw)
// PyObject* PyInstance_NewRaw(    PyObject *class, PyObject *dict)

public:
public:
    static bool HasExactSameType(PyObject* obj)
    {
        return PyInstance_Check(obj);
    }
    static bool HasSameType(PyObject* obj)
    {
        return PyInstance_Check(obj);
    }
};

}                                       // namespace pythonpp

END_NCBI_SCOPE

#endif                                  // PYTHONPP_EXTDT_H

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
