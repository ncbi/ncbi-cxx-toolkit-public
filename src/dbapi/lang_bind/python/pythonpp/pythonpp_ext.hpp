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

#ifndef PYTHONPP_EXT_H
#define PYTHONPP_EXT_H

#include "pythonpp_seq.hpp"

BEGIN_NCBI_SCOPE

namespace pythonpp
{

extern "C"
{
    typedef PyObject* (*TMethodVarArgsHandler)( PyObject* self, PyObject* args );
    typedef PyObject* (*TMethodKeywordHandler)( PyObject* self, PyObject* args, PyObject* dict );
};

//////////////////////////////////////////////////////////////////////////
/// Introduces constructor methods for a python PyMethodDef structure ...
struct SMethodDef : public PyMethodDef
{
public:
    SMethodDef(void)
    {
        ml_name = 0;
        ml_meth = 0;
        ml_flags = 0;
        ml_doc = 0;
    }
    SMethodDef(const char* name, PyCFunction func, int flags = 1, const char* doc = NULL)
    {
        ml_name = const_cast<char*>( name );
        ml_meth = func;
        ml_flags = flags;
        ml_doc = const_cast<char*>( doc );
    }
    SMethodDef(const SMethodDef& other)
    {
        ml_name = other.ml_name;
        ml_meth = other.ml_meth;
        ml_flags = other.ml_flags;
        ml_doc = other.ml_doc;
    }

    SMethodDef& operator = (const SMethodDef& other)
    {
        if (this != &other) {
            ml_name = other.ml_name;
            ml_meth = other.ml_meth;
            ml_flags = other.ml_flags;
            ml_doc = other.ml_doc;
        }
        return *this;
    }
};

extern "C" void DoNotDeallocate( void* )
{
}

//////////////////////////////////////////////////////////////////////////
extern "C"
void standard_dealloc( PyObject* obj )
{
    PyMem_DEL( obj );
}

//////////////////////////////////////////////////////////////////////////
class CExtObjectBase;

// PyTypeObject is inherited from a PyVarObject (C-kind of inheritance) ...
class CExtType : public PyTypeObject
{
public:
    CExtType(
        size_t basic_size,
        destructor dr = standard_dealloc)
    {
        BasicInit();

        ob_type = &PyType_Type;
        tp_basicsize = basic_size;
        tp_dealloc = dr;
        // Py_TPFLAGS_BASETYPE - means that the type is subtypable ...
        tp_flags = Py_TPFLAGS_DEFAULT;

        // Finalize the type object including setting type of the new type
        // object; doing it here is required for portability to Windows
        // without requiring C++.
        // !!! This code does not work currently !!!
        // if ( PyType_Ready(this) == -1 ) {
        //     throw CError("Cannot initialyze a type object");
        // }
    }

public:
    PyTypeObject* GetPyTypeObject( void )
    {
        return this;;
    }
    void SetName( const char* name )
    {
        tp_name = const_cast<char*>( name );
    }
    void SetDescription( const char* descr )
    {
        tp_doc = const_cast<char*>( descr );
    }

public:
    void SupportGetAttr( getattrfunc func )
    {
        tp_getattr = func;
    }

private:
    void BasicInit( void )
    {
#ifdef Py_TRACE_REFS
        _ob_next = NULL;
        _ob_prev = NULL;
#endif
        ob_refcnt = 1;
        ob_type = NULL;

        ob_size = 0;

        tp_name = NULL;

        tp_basicsize = 0;
        tp_itemsize = 0;

        // Methods to implement standard operations
        tp_dealloc = NULL;
        tp_print = NULL;

        tp_getattr = NULL;              // This field is deprecated.
        tp_setattr = NULL;              // This field is deprecated.

        tp_compare = NULL;
        tp_repr = NULL;

        // Method suites for standard classes
        tp_as_number = NULL;
        tp_as_sequence = NULL;
        tp_as_mapping =  NULL;

        // More standard operations (here for binary compatibility)
        tp_hash = NULL;
        tp_call = NULL;
        tp_str = NULL;

        tp_getattro = NULL;
        tp_setattro = NULL;

        // Functions to access object as input/output buffer
        tp_as_buffer = NULL;

        // Flags to define presence of optional/expanded features
        tp_flags = 0L;

        // Documentation string
        tp_doc = NULL;

        // call function for all accessible objects
        tp_traverse = 0L;

        // delete references to contained objects
        tp_clear = 0L;

        // rich comparisons
        tp_richcompare = 0L;

        // weak reference enabler
        tp_weaklistoffset = 0L;

        // Iterators
        tp_iter = 0L;
        tp_iternext = 0L;

        // Attribute descriptor and subclassing stuff
        tp_methods = NULL;  // Object's method table ( same as retturned by GetMethodHndlList() )
        tp_members = NULL;
        tp_getset = NULL;
        tp_base = NULL;

        tp_dict = NULL;
        tp_descr_get = NULL;
        tp_descr_set = NULL;

        tp_dictoffset = 0;

        tp_init = NULL;
        tp_alloc = NULL;
        tp_new = NULL;
        tp_free = NULL; // Low-level free-memory routine
        tp_is_gc = NULL; // For PyObject_IS_GC
        tp_bases = NULL;
        tp_mro = NULL; // method resolution order
        tp_cache = NULL;
        tp_subclasses = NULL;
        tp_weaklist = NULL;
        tp_del = NULL;

#ifdef COUNT_ALLOCS
        tp_alloc = 0;
        tp_free = 0;
        tp_maxalloc = 0;
        tp_next = 0;
#endif
    }
};

//////////////////////////////////////////////////////////////////////////
namespace bind
{

enum EBindType { eReadOnly, eReadWrite };

class CBase
{
public:
    CBase( EBindType type = eReadOnly )
    : m_Type( type )
    {
    }
    virtual ~CBase(void)
    {
    }

public:
    bool IsReadOnly(void) const
    {
        return m_Type == eReadOnly;
    }
    virtual PyObject* Get(void) const = 0;
    void Set( PyObject* value ) const
    {
        if ( IsReadOnly() ) {
            throw CError("Read-only property");
        }
        SetInternal( value );
    }

protected:
    virtual void SetInternal( PyObject* value ) const
    {
    }

private:
    EBindType   m_Type;
};

class CLong : public CBase
{
public:
    CLong(long& value, EBindType type = eReadOnly )
    : CBase( type )
    , m_Value( &value )
    {
    }
    virtual ~CLong(void)
    {
    }

public:
    virtual PyObject* Get(void) const
    {
        return PyLong_FromLong( *m_Value );
    }

protected:
    virtual void SetInternal( PyObject* value ) const
    {
        long tmp_value = PyLong_AsLong( value );
        CError::Check();
        *m_Value = tmp_value;
    }

private:
    long* const m_Value;
};

class CString : public CBase
{
public:
    CString(string& value, EBindType type = eReadOnly )
    : CBase( type )
    , m_Value( &value )
    {
    }
    virtual ~CString(void)
    {
    }

public:
    virtual PyObject* Get(void) const
    {
        return PyString_FromString( m_Value->c_str() );
    }

protected:
    virtual void SetInternal( PyObject* value ) const
    {
        string tmp_value = string( PyString_AsString( value ), static_cast<size_t>( PyString_Size( value ) ) );
        CError::Check();
        *m_Value = tmp_value;
    }

private:
    string* const m_Value;
};

template <class T>
class CObject : public CBase
{
public:
    typedef PyObject* (T::*TGetFunc)( void ) const;
    typedef void (T::*TSetFunc)( PyObject* value );

public:
    CObject( T& obj, TGetFunc get, TSetFunc set = NULL )
    : CBase( set == NULL ? eReadOnly : eReadWrite )
    , m_Obj( &obj )
    , m_GetFunc( get )
    , m_SetFunc( set )
    {
    }
    virtual ~CObject(void)
    {
    }

public:
    virtual PyObject* Get(void) const
    {
        return (m_Obj->*m_GetFunc)();
    }

protected:
    virtual void SetInternal( PyObject* value ) const
    {
        (m_Obj->*m_SetFunc)( value );
    }

private:
    T* const    m_Obj;
    TGetFunc    m_GetFunc;
    TSetFunc    m_SetFunc;
};

}

//////////////////////////////////////////////////////////////////////////
template <class T>
class CExtObject : public PyObject
{
public:
    CExtObject(void)
    {
        // This is not an appropriate place for initialization ....
//            PyObject_INIT( this, GetType().GetPyTypeObject() );
    }

    ~CExtObject(void)
    {
    }

public:
    typedef CObject (T::*TMethodVarArgsFunc)( const CTuple& args );
    /// Workaround for GCC 2.95
    struct SFunct
    {
        SFunct(const TMethodVarArgsFunc& funct)
        : m_Funct(funct)
        {
        }

        operator TMethodVarArgsFunc(void) const
        {
            return m_Funct;
        }

        TMethodVarArgsFunc m_Funct;
    };

    // A helper class for generating functions ...
    template<size_t N = 0>
    class CClass
    {
    public:
        CClass(void)
        {
            // Finalyze a method definition ...
            GetMethodHndlList().push_back( SMethodDef() );
        }

    public:
        CClass<N + 1> Def(const char* name, TMethodVarArgsFunc func, const char* doc = 0)
        {
            TMethodHndlList& hndl_list = GetMethodHndlList();

            // Prepare data for python ...
            if (hndl_list.size() < ePosition) {
                hndl_list.resize(ePosition);
            }
            hndl_list[ePosition] = SMethodDef(name, HandleMethodVarArgs, METH_VARARGS, doc);

            // Prepare data for our handler ...
            GetMethodList().push_back(func);

            return CClass<N + 1>();
        }

        static PyObject* HandleMethodVarArgs( PyObject* self, PyObject* args )
        {
            const TMethodVarArgsFunc func = GetMethodList()[ePosition];
            T* obj = static_cast<T*>(self);

            try {
                const CTuple args_tuple( args );

                return IncRefCount((obj->*func)(args_tuple));
            }
            catch(const CError&) {
                // An error message is already set by a previosly raised exception ...
                return NULL;
            }
            catch(...) {
                CError::SetString("Unknown error during executing of a method");
            }
            // NULL means "error".
            return NULL;
        }

    private:
        enum EPosition {ePosition = N};
    };

    template <size_t N> friend class CClass;

    static void Declare(const char* name, const char* descr = 0)
    {
        CExtType& type = GetType();

        type.SetName(name);
        if ( descr ) {
            type.SetDescription(descr);
        }
        type.SupportGetAttr(GetAttrImpl);
    }

    static CClass<1> Def(const char* name, TMethodVarArgsFunc func, const char* doc = 0)
    {
        return CClass<0>().Def(name, func, doc);
    }

public:
    // Return a python object type.
    static CExtType& GetType(void)
    {
        static CExtType obj_type( sizeof(T), deallocator );

        return obj_type;
    }

    // Just delete an object ...
    static void deallocator ( PyObject* obj )
    {
        delete static_cast<T*>( obj );
    }

protected:
    static void PrepareForPython(CExtObject<T>* self)
    {
        // Borrowed reference.
        PyObject_Init( self, GetType().GetPyTypeObject() );
    }

protected:
    typedef map<string, AutoPtr<bind::CBase> > TAttrList;
    TAttrList m_AttrList;

    void ROAttr( const string& name, long& value )
    {
        m_AttrList[ name ] = new bind::CLong( value );
    }
    void ROAttr( const string& name, string& value )
    {
        m_AttrList[ name ] = new bind::CString( value );
    }

    void RWAttr( const string& name, long& value )
    {
        m_AttrList[ name ] = new bind::CLong( value, bind::eReadWrite );
    }
    void RWAttr( const string& name, string& value )
    {
        m_AttrList[ name ] = new bind::CString( value, bind::eReadWrite );
    }

private:
    typedef vector<SMethodDef>  TMethodHndlList;
    static TMethodHndlList      sm_MethodHndlList;

    static TMethodHndlList& GetMethodHndlList(void)
    {
        return sm_MethodHndlList;
    }

    static PyObject* GetAttrImpl( PyObject* self, char* name )
    {
        _ASSERT( self != NULL );
        CExtObject<T>* obj_ptr = static_cast<CExtObject<T>* >( self );
        TAttrList::const_iterator citer = obj_ptr->m_AttrList.find( name );

        if ( citer != obj_ptr->m_AttrList.end() ) {
            // Return an attribute value ...
            return citer->second->Get();
        }

        // Classic python implementation ...
        // It will do a linear search within the m_MethodHndlList table ...
        return Py_FindMethod( &GetMethodHndlList().front(), self, name );
    }

private:
    typedef vector<SFunct>  TMethodList;
    static TMethodList      sm_MethodList;

    static TMethodList& GetMethodList(void)
    {
        return sm_MethodList;
    }
};

template <class T> typename CExtObject<T>::TMethodHndlList CExtObject<T>::sm_MethodHndlList;
template <class T> typename CExtObject<T>::TMethodList CExtObject<T>::sm_MethodList;

//////////////////////////////////////////////////////////////////////////
template <class T>
class CExtModule : public PyObject
{
public:
    CExtModule(const char* name, const char* descr = 0)
    : m_Module(NULL)
    {
        m_Module = Py_InitModule4(
            const_cast<char*>(name),
            &GetMethodHndlList().front(),
            const_cast<char*>(descr),
            this,
            PYTHON_API_VERSION);
        if ( !m_Module ) {
            throw CError("Cannot initialyze module");
        }
    }
    ~CExtModule(void)
    {
    }

public:
    typedef CObject (T::*TMethodVarArgsFunc)( const CTuple& args );
    /// Workaround for GCC 2.95
    struct SFunct
    {
        SFunct(const TMethodVarArgsFunc& funct)
        : m_Funct(funct)
        {
        }

        operator TMethodVarArgsFunc(void) const
        {
            return m_Funct;
        }

        TMethodVarArgsFunc m_Funct;
    };

    // A helper class for generating functions ...
    template<size_t N = 0>
    class CClass
    {
    public:
        CClass(void)
        {
            // Finalyze method definition ...
            GetMethodHndlList().push_back( SMethodDef() );
        }

    public:
        CClass<N + 1> Def(const char* name, TMethodVarArgsFunc func, const char* doc = 0)
        {
            // Prepare data for python ...
            GetMethodHndlList()[ePosition] = SMethodDef(name, HandleMethodVarArgs, METH_VARARGS, doc);

            // Prepare data for our handler ...
            GetMethodList().push_back(func);

            return CClass<N + 1>();
        }
        static PyObject* HandleMethodVarArgs( PyObject* self, PyObject* args )
        {
            const TMethodVarArgsFunc func = GetMethodList()[ePosition];
            T* obj = static_cast<T*>(self);

            try {
                const CTuple args_tuple( args );

                return IncRefCount((obj->*func)(args_tuple));
            }
            catch(const CError&) {
                // An error message is already set by an exception ...
                return NULL;
            }
            catch(...) {
                CError::SetString("Unknown error during executing of a method");
            }
            // NULL means "error".
            return NULL;
        }

    private:
        enum {ePosition = N};
    };

    static CClass<1> Def(const char* name, TMethodVarArgsFunc func, const char* doc = 0)
    {
        return CClass<0>().Def(name, func, doc);
    }

public:
    // Return a python object type.
    static CExtType& GetType(void)
    {
        static CExtType obj_type( sizeof(T), deallocator );

        return obj_type;
    }

    // Just delete an object ...
    static void deallocator ( PyObject* obj )
    {
        delete static_cast<T*>( obj );
    }

protected:
    static void PrepareForPython(CExtModule<T>* self)
    {
        // Borrowed reference.
        PyObject_Init( self, GetType().GetPyTypeObject() );
    }

private:
    typedef vector<SMethodDef>  TMethodHndlList;
    static TMethodHndlList      sm_MethodHndlList;

    static TMethodHndlList& GetMethodHndlList(void)
    {
        return sm_MethodHndlList;
    }

    static PyObject* GetAttrImpl( PyObject* self, char* name )
    {
        // Classic python implementation ...
        // It will do a linear search within the m_MethodHndlList table ...
        return Py_FindMethod(&GetMethodHndlList().front(), self, name);
    }

private:
    typedef vector<SFunct>  TMethodList;
    static TMethodList      sm_MethodList;

    static TMethodList& GetMethodList(void)
    {
        return sm_MethodList;
    }

private:
    PyObject* m_Module;
};

template <class T> typename CExtModule<T>::TMethodHndlList CExtModule<T>::sm_MethodHndlList;
template <class T> typename CExtModule<T>::TMethodList CExtModule<T>::sm_MethodList;


///////////////////////////////////////////////////////////////////////////////
// New development
///////////////////////////////////////////////////////////////////////////////

// An attempt to wrap a module ...
class CModuleExt
{
public:
    static void Declare(const string& name, PyMethodDef* methods);

public:
    static const string& GetName(void)
    {
        return m_Name;
    }
    static PyObject* GetPyModule(void)
    {
        return m_Module;
    }
    static void AddConstValue( const string& name, PyObject* value );
    static void AddConst( const string& name, const string& value );
    static void AddConst( const string& name, long value );

private:
    static string m_Name;
    static PyObject* m_Module;
};

string CModuleExt::m_Name;
PyObject* CModuleExt::m_Module = NULL;

void
CModuleExt::Declare(const string& name, PyMethodDef* methods)
{
    _ASSERT( m_Module == NULL );

    m_Name = name;
    m_Module = Py_InitModule( const_cast<char*>( name.c_str() ), methods );
    CError::Check(m_Module);
}

void
CModuleExt::AddConstValue( const string& name, PyObject* value )
{
    CError::Check( value );
    if ( PyModule_AddObject(m_Module, const_cast<char*>(name.c_str()), value ) == -1 ) {
        throw CError("Failed to add a constant value to a module");
    }
}

void
CModuleExt::AddConst( const string& name, const string& value )
{
    PyObject* py_value = PyString_FromString( value.c_str() );
    CError::Check( py_value );
    AddConstValue( name, py_value );
}

void
CModuleExt::AddConst( const string& name, long value )
{
    PyObject* py_value = PyLong_FromLong( value );
    CError::Check( py_value );
    AddConstValue( name, py_value );
}

///////////////////////////////////
// User-defined exceptions
///////////////////////////////////

template <class T, class B = CStandardError>
class CUserError : public B
{
public:
    CUserError(const string& msg)
    : B( msg, GetPyException() )
    {
    }

protected:
    CUserError(const string& msg, PyObject* err_type)
    : B(msg, err_type)
    {
    }

public:
    static void Declare(const string& name)
    {
        _ASSERT( m_Exception == NULL );
        _ASSERT( CModuleExt::GetPyModule() );
        const string full_name = CModuleExt::GetName() + "." + name;
        m_Exception = PyErr_NewException( const_cast<char*>(full_name.c_str()), B::GetPyException(), NULL );
        CError::Check( m_Exception );
        if ( PyModule_AddObject( CModuleExt::GetPyModule(), const_cast<char*>(name.c_str()), m_Exception ) == -1 ) {
            throw CError( "Unable to add an object to a module" );
        }
    }

protected:
    static PyObject* GetPyException(void)
    {
        _ASSERT( m_Exception );
        return m_Exception;
    }

private:
    static PyObject* m_Exception;
};

template <class T, class B> PyObject* CUserError<T, B>::m_Exception(NULL);


}                                       // namespace pythonpp

END_NCBI_SCOPE

#endif                                  // PYTHONPP_EXT_H

/* ===========================================================================
*
* $Log$
* Revision 1.7  2005/02/08 19:19:34  ssikorsk
* A lot of improvements
*
* Revision 1.6  2005/01/27 18:50:03  ssikorsk
* Fixed: a bug with transactions
* Added: python 'transaction' object
*
* Revision 1.5  2005/01/21 15:50:18  ssikorsk
* Fixed: build errors with GCC 2.95.
*
* Revision 1.4  2005/01/19 16:26:13  ssikorsk
* Fixed: declared template static members
*
* Revision 1.3  2005/01/18 22:26:18  ssikorsk
* Fixed: ucko's fix was rolled back because of a logical error.
* Explanation: we must have a separate m_MethodList member for each
* CExtObject instantiation, not one for all CExtObject's instantiations.
*
* Revision 1.2  2005/01/18 21:31:05  ucko
* Tweak to fix build errors with GCC 2.95.
*
* Revision 1.1  2005/01/18 19:26:07  ssikorsk
* Initial version of a Python DBAPI module
*
* ===========================================================================
*/
