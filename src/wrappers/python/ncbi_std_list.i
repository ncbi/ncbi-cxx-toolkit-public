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
 * Authors:  Josh Cherry
 *
 * File Description:  Support for SWIG wrapping of std::list
 *
 */


%include ncbi_std_helpers.i


CATCH_OUT_OF_RANGE(std::list::pop)


%{
#include <list>
%}

namespace std {

    template<class T> class list {

        %typemap(in) list<T> (std::list<T>* vec) {
            if (SWIG_ConvertPtr($input, (void**) &vec, $&1_descriptor, 0)
                != -1
                && vec) {
                $1 = *vec;
            } else if (IsListOrTuple($input)) {
                unsigned int size = SizeListOrTuple($input);
                for (unsigned int i = 0; i < size; ++i) {
                    PyObject* pobj = PySequence_GetItem($input, i);
                    T* obj;
                    if (SWIG_ConvertPtr(pobj, (void **) &obj,
                        $descriptor(T*), 0) != -1
                        && obj) {
                        $1.push_back(*obj);
                        Py_DECREF(pobj);
                    } else {
                        Py_DECREF(pobj);
                        PyErr_SetString(PyExc_TypeError,
                                        "list<" #T "> expected");
                        SWIG_fail;
                    }
                }
            } else {
                PyErr_SetString(PyExc_TypeError,
                                "list<" #T "> expected");
                SWIG_fail;
            }
        }
        %typemap(in) const list<T>* (std::list<T>* vec, std::list<T> tmp),
                     const list<T>& (std::list<T>* vec, std::list<T> tmp) {
            if (SWIG_ConvertPtr($input, (void**) &vec, $&1_descriptor, 0)
                != -1
                && vec) {
                $1 = vec;
            } else if (IsListOrTuple($input)) {
                unsigned int size = SizeListOrTuple($input);
                $1 = &tmp;
                for (unsigned int i = 0; i < size; ++i) {
                    PyObject* pobj = PySequence_GetItem($input, i);
                    T* obj;
                    if (SWIG_ConvertPtr(pobj, (void **) &obj,
                        $descriptor(T*), 0) != -1
                        && obj) {
                        tmp.push_back(*obj);
                        Py_DECREF(pobj);
                    } else {
                        Py_DECREF(pobj);
                        PyErr_SetString(PyExc_TypeError,
                                        "list<" #T "> expected");
                        SWIG_fail;
                    }
                }
            } else {
                PyErr_SetString(PyExc_TypeError,
                                "list<" #T "> expected");
                SWIG_fail;
            }
        }

        %typemap(out) list<T> {
            $result = PyList_New(0);
            for (list<T >::iterator it = $1.begin();
                 it != $1.end(); ++it) {
                T* ptr = new T(*it);
                PyList_Append($result, SWIG_NewPointerObj((void *) ptr,
                              $descriptor(T *), 1));
            }
        }

        %typecheck(SWIG_TYPECHECK_LIST) list<T> {
            if (IsListOrTuple($input)) {
                if (SizeListOrTuple($input) == 0) {
                    $1 = 1;
                } else {
                    PyObject* obj = PySequence_GetItem($input, 0);
                    $1 = CheckPtrForbidNull(obj, $descriptor(T*));
                    Py_DECREF(obj);
                }
            } else {
                $1 = CheckPtrForbidNull($input, $&1_descriptor);
            }
        }

        %typecheck(SWIG_TYPECHECK_LIST) const list<T>*, const list<T>& {
            if (IsListOrTuple($input)) {
                if (SizeListOrTuple($input) == 0) {
                    $1 = 1;
                } else {
                    PyObject* obj = PySequence_GetItem($input, 0);
                    $1 = CheckPtrForbidNull(obj, $descriptor(T*));
                    Py_DECREF(obj);
                }
            } else {
                $1 = CheckPtrForbidNull($input, $1_descriptor);
            }
        }

    public:
        list(void);
        list(unsigned int size, const T& value);
        list(const list<T>& other);

        void clear(void);
        unsigned int size(void);
        void push_back(const T& item);
        //T pop_back(void);

        %extend {
            bool __nonzero__(void) {
                return !self->empty();
            }
            unsigned int __len__(void) {return self->size();}
            void append(const T& item) {self->push_back(item);}

            T pop(void) {
                if (self->empty()) {
                    throw std::out_of_range("attempted pop from empty list");
                }
                T rv = self->back();
                self->pop_back();
                return rv;
            }

            std::list<T > __getslice__(int i, int j) {
                int size = self->size();
                AdjustSlice(i, j, size);
                std::list<T > rv;
                std::list<T >::iterator start = self->begin();
                advance(start, i);
                std::list<T >::iterator end = start;
                advance(end, j - i);
                rv.insert(rv.begin(), start, end);
                return rv;
            }

        }  // %extend
    };  // class



    // Specialization macro

    %define specialize_std_list(T, CHECK, TO_CPP, FROM_CPP)
    template<> class list<T > {

        %typemap(in) list<T > (std::list<T >* vec) {
            if (SWIG_ConvertPtr($input, (void**) &vec, $&1_descriptor, 0)
                != -1
                && vec) {
                $1 = *vec;
            } else if (IsListOrTuple($input)) {
                unsigned int size = SizeListOrTuple($input);
                for (unsigned int i = 0; i < size; ++i) {
                    PyObject* pobj = PySequence_GetItem($input, i);
                    if (CHECK(pobj)) {
                        // cast is necessary here for enums
                        $1.push_back(static_cast<T >(TO_CPP(pobj)));
                        Py_DECREF(pobj);
                    } else {
                        Py_DECREF(pobj);
                        PyErr_SetString(PyExc_TypeError,
                                        "list<" #T "> expected");
                        SWIG_fail;
                    }
                }
            } else {
                PyErr_SetString(PyExc_TypeError,
                                "list<" #T "> expected");
                SWIG_fail;
            }
        }
        %typemap(in) const list<T >* (std::list<T >* vec, std::list<T > tmp),
                     const list<T >& (std::list<T >* vec, std::list<T > tmp) {
            if (SWIG_ConvertPtr($input, (void**) &vec, $&1_descriptor, 0)
                != -1
                && vec) {
                $1 = vec;
            } else if (IsListOrTuple($input)) {
                unsigned int size = SizeListOrTuple($input);
                $1 = &tmp;
                for (unsigned int i = 0; i < size; ++i) {
                    PyObject* pobj = PySequence_GetItem($input, i);
                    if (CHECK(pobj)) {
                        // cast is necessary here for enums
                        tmp.push_back(static_cast<T >(TO_CPP(pobj)));
                        Py_DECREF(pobj);
                    } else {
                        Py_DECREF(pobj);
                        PyErr_SetString(PyExc_TypeError,
                                        "list<" #T "> expected");
                        SWIG_fail;
                    }
                }
            } else {
                PyErr_SetString(PyExc_TypeError,
                                "list<" #T "> expected");
                SWIG_fail;
            }
        }

        %typemap(out) list<T > {
            $result = PyList_New(0);
            for (list<T >::iterator it = $1.begin();
                 it != $1.end();  ++it) {
                T* ptr = new T(*it);
                PyList_Append($result, FROM_CPP(*it));
            }
        }

        %typecheck(SWIG_TYPECHECK_LIST) list<T > {
            if (IsListOrTuple($input)) {
                if (SizeListOrTuple($input) == 0) {
                    $1 = 1;
                } else {
                    PyObject* obj = PySequence_GetItem($input, 0);
                    $1 = CHECK(obj) ? 1 : 0;
                    Py_DECREF(obj);
                }
            } else {
                $1 = CheckPtrForbidNull($input, $&1_descriptor);
            }
        }

        %typecheck(SWIG_TYPECHECK_LIST) const list<T >*, const list<T >& {
            if (IsListOrTuple($input)) {
                if (SizeListOrTuple($input) == 0) {
                    $1 = 1;
                } else {
                    PyObject* obj = PySequence_GetItem($input, 0);
                    $1 = CHECK(obj) ? 1 : 0;
                    Py_DECREF(obj);
                }
            } else {
                $1 = CheckPtrForbidNull($input, $1_descriptor);
            }
        }

    public:
        list(unsigned int size = 0);   // requires default ctor
        list(unsigned int size, const T& value);
        list(const list<T >& other);

        void clear(void);
        unsigned int size(void);
        void push_back(const T& item);
        //T pop_back(void);

        %extend {
            bool __nonzero__(void) {
                return !self->empty();
            }
            unsigned int __len__(void) {return self->size();}
            void append(const T& item) {self->push_back(item);}

            T pop(void) {
                if (self->empty()) {
                    throw std::out_of_range("attempted pop from empty list");
                }
                T rv = self->back();
                self->pop_back();
                return rv;
            }

            std::list<T > __getslice__(int i, int j) {
                int size = self->size();
                AdjustSlice(i, j, size);
                std::list<T > rv;
                std::list<T >::iterator start = self->begin();
                advance(start, i);
                std::list<T >::iterator end = start;
                advance(end, j - i);
                rv.insert(rv.begin(), start, end);
                return rv;
            }

        }  // %extend
    };  // class
    %enddef


    // Specializations
    specialize_std_list(int,
                        PyInt_Check, PyInt_AsLong, PyInt_FromLong);
    specialize_std_list(unsigned int,
                        PyInt_Check, PyInt_AsLong, PyInt_FromLong);
    specialize_std_list(short,
                        PyInt_Check, PyInt_AsLong, PyInt_FromLong);
    specialize_std_list(unsigned short,
                        PyInt_Check, PyInt_AsLong, PyInt_FromLong);
    specialize_std_list(long,
                        PyLong_Check, PyLong_AsLong, PyLong_FromLong);
    specialize_std_list(unsigned long,
                        PyLong_Check,
                        PyLong_AsUnsignedLong, PyLong_FromUnsignedLong);

    specialize_std_list(char,
                        PyInt_Check, PyInt_AsLong, PyInt_FromLong);
    specialize_std_list(unsigned char,
                        PyInt_Check, PyInt_AsLong, PyInt_FromLong);

    specialize_std_list(bool,
                        PyInt_Check, PyInt_AsLong, SWIG_From_bool);

    specialize_std_list(double,
                        DoubleCheck,
                        DoubleToCpp, PyFloat_FromDouble);
    specialize_std_list(float,
                        DoubleCheck,
                        DoubleToCpp, PyFloat_FromDouble);

    // Could do better here (allow std::string proxies in lists/tuples)?
    specialize_std_list(std::string,
                        PyString_Check,
                        StringToCpp, StringFromCpp);


}  // namespace std


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/05/11 21:30:44  jcherry
 * Initial version
 *
 * ===========================================================================
 */
