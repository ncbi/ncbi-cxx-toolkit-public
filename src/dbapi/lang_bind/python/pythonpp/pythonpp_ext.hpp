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
class CExtType : PyTypeObject
{
public:
    CExtType(
        size_t basic_size,
        int itemsize,
        destructor dr = standard_dealloc)
    {
#ifdef Py_TRACE_REFS
        _ob_next = NULL;
        _ob_prev = NULL;
#endif
        ob_refcnt = 1;
        ob_type = NULL;

        ob_type = &PyType_Type;
        ob_size = 0;
        tp_name = NULL;
        tp_basicsize = basic_size;
        tp_itemsize = itemsize;
        tp_dealloc = dr;
        tp_print = NULL;

        tp_getattr = NULL;
        tp_setattr = NULL;

        tp_compare = NULL;
        tp_repr = NULL;

        tp_as_number = NULL;
        tp_as_sequence = NULL;
        tp_as_mapping =  NULL;

        tp_hash = NULL;
        tp_call = NULL;
        tp_str = NULL;

        tp_getattro = NULL;
        tp_setattro = NULL;

        tp_as_buffer = NULL;

        tp_flags = 0L;

        tp_doc = NULL;

        tp_traverse = 0L;
        tp_clear = 0L;

        tp_richcompare = 0L;
        tp_weaklistoffset = 0L;

#ifdef COUNT_ALLOCS
        tp_alloc = 0;
        tp_free = 0;
        tp_maxalloc = 0;
        tp_next = 0;
#endif
    }

public:
    PyTypeObject* GetPyTypeObject(void)
    {
        return this;;
    }
    void SetName(const char* name)
    {
        tp_name = const_cast<char*>( name );
    }
    void SetDescription(const char* descr)
    {
        tp_doc = const_cast<char*>( descr );
    }

public:
    void SupportGetAttr(getattrfunc func)
    {
        tp_getattr = func;
    }
};

/* Experimental ....
//////////////////////////////////////////////////////////////////////////
class CExtBase : public PyObject
{
public:
    ~CExtBase(void)
    {
        //_ASSERT( ob_refcnt == 0 );
        assert( ob_refcnt == 0 );
    }

protected:
    typedef vector<SMethodDef> TMethodHndlList;

    static TMethodHndlList& GetMethodHndlList(void)
    {
        static TMethodHndlList m_MethodHndlList;

        return m_MethodHndlList;
    }

protected:
    static PyObject* GetAttrImpl( PyObject* self, char* name )
    {
        // Classic python implementation ...
        // It will do a linear search with the m_MethodHndlList table ...
        return Py_FindMethod(&GetMethodHndlList().front(), self, name);
    }
};

//////////////////////////////////////////////////////////////////////////
template <class T>
class CExtension : public CExtBase
{
protected:
    typedef CObject (T::*TMethodVarArgsFunc)( const CTuple& args );

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
        static CClass<N + 1> def(const char* name, TMethodVarArgsFunc func, const char* doc = 0)
        {
            // Prepare data for python ...
            GetMethodHndlList()[N] = SMethodDef(name, HandleMethodVarArgs, METH_VARARGS, doc);

            // Prepare data for our handler ...
            GetMethodList().push_back(func);

            return CClass<N + 1>();
        }

        static PyObject* HandleMethodVarArgs( PyObject* self, PyObject* args )
        {
            TMethodVarArgsFunc func = GetMethodList()[ePosition];
            T* obj = static_cast<T*>(self);

            try {
                const CTuple args_tuple( args );

                return IncRefCount((obj->*func)(args_tuple));
            } catch(...) {
                // Just ignore it ...
            }
            // NULL means "error".
            return NULL;
        }
    };

private:
    typedef vector<TMethodVarArgsFunc> TMethodList;
    static TMethodList sm_MethodList;

    static TMethodList& GetMethodList(void)
    {
        return Sm_MethodList;
    }
};

template <class T> typename CExtension<T>::TMethodList CExtension<T>::sm_MethodList;

*/

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
            } catch(...) {
                // Just ignore it ...
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

protected:
    static void PrepareForPython(CExtObject<T>* self)
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
        // It will do a linear search with the m_MethodHndlList table ...
        return Py_FindMethod( &GetMethodHndlList().front(), self, name );
    }

private:
    // Return a python object type.
    static CExtType& GetType(void)
    {
        static CExtType obj_type( sizeof(T), 0, deallocator );

        return obj_type;
    }

    // Just delete an object ...
    static void deallocator ( PyObject* obj )
    {
        delete static_cast<T*>( obj );
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

public:
    typedef CObject (T::*TMethodVarArgsFunc)( const CTuple& args );

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
        static CClass<N + 1> def(const char* name, TMethodVarArgsFunc func, const char* doc = 0)
        {
            // Prepare data for python ...
            GetMethodHndlList()[N] = SMethodDef(name, HandleMethodVarArgs, METH_VARARGS, doc);

            // Prepare data for our handler ...
            GetMethodList().push_back(func);

            return CClass<N + 1>();
        }

        static PyObject* HandleMethodVarArgs( PyObject* self, PyObject* args )
        {
            TMethodVarArgsFunc func = GetMethodList()[N];
            T* obj = static_cast<T*>(self);
            CTuple args_tuple( args );

            CObject result = (obj->*func)(args_tuple);
            Py_INCREF(result.Get());

            return result.Get();
        }

    private:
        enum {ePosition = N};
    };

    static CClass<1> def(const char* name, TMethodVarArgsFunc func, const char* doc = 0)
    {
        return CClass<0>().def(name, func, doc);
    }

private:
    typedef vector<SMethodDef> TMethodHndlList;

    static TMethodHndlList& GetMethodHndlList(void)
    {
        static TMethodHndlList m_MethodHndlList;

        return m_MethodHndlList;
    }

    static PyObject* GetAttrImpl( PyObject* self, char* name )
    {
        // Classic python implementation ...
        // It will do a linear search with the m_MethodHndlList table ...
        return Py_FindMethod(&GetMethodHndlList().front(), self, name);
    }

private:
    typedef vector<TMethodVarArgsFunc> TMethodList;

    static TMethodList& GetMethodList(void)
    {
        static TMethodList m_MethodList;

        return m_MethodList;
    }

private:
    PyObject* m_Module;
};

}                                       // namespace pythonpp

END_NCBI_SCOPE

#endif                                  // PYTHONPP_EXT_H

/* ===========================================================================
*
* $Log$
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
*
* ===========================================================================
*/
