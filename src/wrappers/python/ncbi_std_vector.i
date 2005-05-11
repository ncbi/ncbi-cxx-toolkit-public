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
 * File Description:  Support for SWIG wrapping of std::vector
 *
 */


%include ncbi_std_helpers.i


CATCH_OUT_OF_RANGE(std::vector::__getitem__)
CATCH_OUT_OF_RANGE(std::vector::__setitem__)
CATCH_OUT_OF_RANGE(std::vector::__delitem__)
CATCH_OUT_OF_RANGE(std::vector::pop)


%{
#include <vector>
%}

namespace std {

    template<class T> class vector {

        %typemap(in) vector<T> (std::vector<T>* vec) {
            if (SWIG_ConvertPtr($input, (void**) &vec, $&1_descriptor, 0)
                != -1
                && vec) {
                $1 = *vec;
            } else if (IsListOrTuple($input)) {
                unsigned int size = SizeListOrTuple($input);
                $1.reserve(size);
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
                                        "vector<" #T "> expected");
                        SWIG_fail;
                    }
                }
            } else {
                PyErr_SetString(PyExc_TypeError,
                                "vector<" #T "> expected");
                SWIG_fail;
            }
        }
        %typemap(in) const vector<T>* (std::vector<T>* vec, std::vector<T> tmp),
                     const vector<T>& (std::vector<T>* vec, std::vector<T> tmp) {
            if (SWIG_ConvertPtr($input, (void**) &vec, $&1_descriptor, 0)
                != -1
                && vec) {
                $1 = vec;
            } else if (IsListOrTuple($input)) {
                unsigned int size = SizeListOrTuple($input);
                $1 = &tmp;
                tmp.reserve(size);
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
                                        "vector<" #T "> expected");
                        SWIG_fail;
                    }
                }
            } else {
                PyErr_SetString(PyExc_TypeError,
                                "vector<" #T "> expected");
                SWIG_fail;
            }
        }

        %typemap(out) vector<T> {
            $result = PyList_New($1.size());
            for (unsigned int i = 0; i < $1.size(); ++i) {
                T* obj = new T($1[i]);
                PyList_SetItem($result, i,
                               SWIG_NewPointerObj((void *) obj,
                                                  $descriptor(T*), 1));
            }
        }

        %typecheck(SWIG_TYPECHECK_VECTOR) vector<T> {
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

        %typecheck(SWIG_TYPECHECK_VECTOR) const vector<T>*, const vector<T>& {
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
        vector(void);
        vector(unsigned int size, const T& value);
        vector(const vector<T>& other);

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
                    throw std::out_of_range("attempted pop from empty vector");
                }
                T rv = self->back();
                self->pop_back();
                return rv;
            }

            T& __getitem__(int i) {
                int size = self->size();
                AdjustIndex(i, size);
                if (i < 0 || i >= size) {
                    throw std::out_of_range("index out of range");
                } else {
                    return (*self)[i];
                }
            }

            std::vector<T> __getslice__(int i, int j) {
                int size = self->size();
                AdjustSlice(i, j, size);
                std::vector<T > rv;
                rv.reserve(j - i);
                rv.insert(rv.begin(), self->begin() + i, self->begin() + j);
                return rv;
            }

            void __setitem__(int i, const T& value) {
                int size = self->size();
                AdjustIndex(i, size);
                if (i < 0 || i >= size) {
                    throw std::out_of_range("index out of range");
                } else {
                    (*self)[i] = value;
                }
            }

            void __setslice__(int i, int j, const std::vector<T>& values) {
                int size = self->size();
                AdjustSlice(i, j, size);
                if (values.size() == j - i) {
                    std::copy(values.begin(), values.end(),
                              self->begin() + i);
                } else {
                    self->erase(self->begin() + i, self->begin() + j);
                    if (i < self->size()) {
                        self->insert(self->begin() + i,
                                     values.begin(), values.end());
                    } else {
                        self->insert(self->end(),
                                     values.begin(), values.end());
                    }
                }
            }

            void __delitem__(int i) {
                int size = self->size();
                AdjustIndex(i, size);
                if (i < 0 || i >= size) {
                    throw std::out_of_range("index out of range");
                } else {
                    self->erase(self->begin() + i);
                }
            }

            void __delslice__(int i, int j) {
                int size = self->size();
                AdjustSlice(i, j, size);
                self->erase(self->begin() + i, self->begin() + j);
            }
        }  // %extend
    };  // class



    // Specialization macro

    %define specialize_std_vector(T, CHECK, TO_CPP, FROM_CPP)
    template<> class vector<T > {

        %typemap(in) vector<T > (std::vector<T >* vec) {
            if (SWIG_ConvertPtr($input, (void**) &vec, $&1_descriptor, 0)
                != -1
                && vec) {
                $1 = *vec;
            } else if (IsListOrTuple($input)) {
                unsigned int size = SizeListOrTuple($input);
                $1.reserve(size);
                for (unsigned int i = 0; i < size; ++i) {
                    PyObject* pobj = PySequence_GetItem($input, i);
                    if (CHECK(pobj)) {
                        // cast is necessary here for enums
                        $1.push_back(static_cast<T >(TO_CPP(pobj)));
                        Py_DECREF(pobj);
                    } else {
                        Py_DECREF(pobj);
                        PyErr_SetString(PyExc_TypeError,
                                        "vector<" #T "> expected");
                        SWIG_fail;
                    }
                }
            } else {
                PyErr_SetString(PyExc_TypeError,
                                "vector<" #T "> expected");
                SWIG_fail;
            }
        }
        %typemap(in) const vector<T >* (std::vector<T >* vec, std::vector<T > tmp),
                     const vector<T >& (std::vector<T >* vec, std::vector<T > tmp) {
            if (SWIG_ConvertPtr($input, (void**) &vec, $&1_descriptor, 0)
                != -1
                && vec) {
                $1 = vec;
            } else if (IsListOrTuple($input)) {
                unsigned int size = SizeListOrTuple($input);
                $1 = &tmp;
                tmp.reserve(size);
                for (unsigned int i = 0; i < size; ++i) {
                    PyObject* pobj = PySequence_GetItem($input, i);
                    if (CHECK(pobj)) {
                        // cast is necessary here for enums
                        tmp.push_back(static_cast<T >(TO_CPP(pobj)));
                        Py_DECREF(pobj);
                    } else {
                        Py_DECREF(pobj);
                        PyErr_SetString(PyExc_TypeError,
                                        "vector<" #T "> expected");
                        SWIG_fail;
                    }
                }
            } else {
                PyErr_SetString(PyExc_TypeError,
                                "vector<" #T "> expected");
                SWIG_fail;
            }
        }

        %typemap(out) vector<T > {
            $result = PyList_New($1.size());
            for (unsigned int i = 0; i < $1.size(); ++i) {
                PyList_SetItem($result, i, FROM_CPP($1[i]));
            }
        }

        %typecheck(SWIG_TYPECHECK_VECTOR) vector<T > {
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

        %typecheck(SWIG_TYPECHECK_VECTOR) const vector<T >*, const vector<T >& {
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
        vector(unsigned int size = 0);   // requires default ctor
        vector(unsigned int size, const T& value);
        vector(const vector<T >& other);

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
                    throw std::out_of_range("attempted pop from empty vector");
                }
                T rv = self->back();
                self->pop_back();
                return rv;
            }

            T __getitem__(int i) {
                int size = self->size();
                AdjustIndex(i, size);
                if (i < 0 || i >= size) {
                    throw std::out_of_range("index out of range");
                } else {
                    return (*self)[i];
                }
            }

            std::vector<T > __getslice__(int i, int j) {
                int size = self->size();
                AdjustSlice(i, j, size);
                std::vector<T > rv;
                rv.reserve(j - i);
                rv.insert(rv.begin(), self->begin() + i, self->begin() + j);
                return rv;
            }

            void __setitem__(int i, T value) {
                int size = self->size();
                AdjustIndex(i, size);
                if (i < 0 || i >= size) {
                    throw std::out_of_range("index out of range");
                } else {
                    (*self)[i] = value;
                }
            }

            void __setslice__(int i, int j, const std::vector<T >& values) {
                int size = self->size();
                AdjustSlice(i, j, size);
                if (values.size() == j - i) {
                    std::copy(values.begin(), values.end(),
                              self->begin() + i);
                } else {
                    self->erase(self->begin() + i, self->begin() + j);
                    if (i < self->size()) {
                        self->insert(self->begin() + i,
                                     values.begin(), values.end());
                    } else {
                        self->insert(self->end(),
                                     values.begin(), values.end());
                    }
                }
            }

            void __delitem__(int i) {
                int size = self->size();
                AdjustIndex(i, size);
                if (i < 0 || i >= size) {
                    throw std::out_of_range("index out of range");
                } else {
                    self->erase(self->begin() + i);
                }
            }

            void __delslice__(int i, int j) {
                int size = self->size();
                AdjustSlice(i, j, size);
                self->erase(self->begin() + i, self->begin() + j);
            }

            // similar to std::string::data
            T* data() {
                return &(*self)[0];
            }

        }  // %extend
    };  // class
    %enddef

    // vector<>::data doesn't work with special vector<bool>
    %ignore vector<bool>::data;

    // Specializations
    specialize_std_vector(int,
                          PyInt_Check, PyInt_AsLong, PyInt_FromLong);
    specialize_std_vector(unsigned int,
                          PyInt_Check, PyInt_AsLong, PyInt_FromLong);
    specialize_std_vector(short,
                          PyInt_Check, PyInt_AsLong, PyInt_FromLong);
    specialize_std_vector(unsigned short,
                          PyInt_Check, PyInt_AsLong, PyInt_FromLong);
    specialize_std_vector(long,
                          PyLong_Check, PyLong_AsLong, PyLong_FromLong);
    specialize_std_vector(unsigned long,
                          PyLong_Check,
                          PyLong_AsUnsignedLong, PyLong_FromUnsignedLong);

    specialize_std_vector(char,
                          PyInt_Check, PyInt_AsLong, PyInt_FromLong);
    specialize_std_vector(unsigned char,
                          PyInt_Check, PyInt_AsLong, PyInt_FromLong);

    specialize_std_vector(bool,
                          PyInt_Check, PyInt_AsLong, SWIG_From_bool);

    specialize_std_vector(double,
                          DoubleCheck,
                          DoubleToCpp, PyFloat_FromDouble);
    specialize_std_vector(float,
                          DoubleCheck,
                          DoubleToCpp, PyFloat_FromDouble);

    // Could do better here (allow std::string proxies in lists/tuples)?
    specialize_std_vector(std::string,
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
