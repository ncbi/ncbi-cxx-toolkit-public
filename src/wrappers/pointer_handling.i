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
 * File Description:  Facilities for dealing with pointers to built-in types
 *
 */


%include cpointer.i

#ifdef SWIGPYTHON
CATCH_OUT_OF_RANGE(Array_class::__getitem__)
CATCH_OUT_OF_RANGE(Array_class::__setitem__)
#endif

#ifdef SWIGPERL
%exception Array_class::__getitem__ {
    try {
        $action
    } catch (std::out_of_range& e) {
        SWIG_exception(SWIG_IndexError, const_cast<char*>(e.what()));
    }
}

%exception Array_class::__setitem__ {
    try {
        $action
    } catch (std::out_of_range& e) {
        SWIG_exception(SWIG_IndexError, const_cast<char*>(e.what()));
    }
}

%rename(get) Array_class::__getitem__;
%rename(set) Array_class::__setitem__;
#endif

%inline %{

template<class T> T deref(T* ptr, long idx=0) {
    return ptr[idx];
}

template<class T> void deref_assign(T* ptr, T value) {
    *ptr = value;
}

template<class T> void array_assign(T* ptr, long idx, T value) {
    ptr[idx] = value;
}

template<class T> class Array_class
{
public:
    Array_class(T* ptr) : m_Ptr(ptr), m_Size(-1) {}
    Array_class(T* ptr, long size) : m_Ptr(ptr), m_Size(size) {}

    T __getitem__(long idx) {
        if (m_Size >= 0) {
              if (idx < 0) {
                  idx = m_Size + idx;
              }
        }
        if (idx < 0 || (m_Size >= 0 && idx >= m_Size)) {
            throw std::out_of_range("index out of range");
        }
        return m_Ptr[idx];
    }

    void __setitem__(long idx, T value) {
        if (m_Size >= 0) {
              if (idx < 0) {
                  idx = m_Size + idx;
              }
        }
        if (idx < 0 || (m_Size >= 0 && idx >= m_Size)) {
            throw std::out_of_range("index out of range");
        }
        m_Ptr[idx] = value;
    }

    long size() {
        if (m_Size <= -1) {
            throw std::runtime_error("size not set");
        }
        return m_Size;
    }
    long __len__() {return size();}
    void set_size(long size) {
        m_Size = size;
    }
    T* ptr() {return m_Ptr;}

private:
    T* m_Ptr;
    long m_Size;
};


template<class T> Array_class<T> _Array(T* ptr, long size=-1) {
    if (size <= -1) {
        return Array_class<T>(ptr);
    } else {
        return Array_class<T>(ptr, size);
    }
}

%}


%define POINTER_HANDLING(type, array_name, pointer_name)

%template(deref) deref<type>;
%template(deref_assign) deref_assign<type>;
%template(array_assign) array_assign<type>;
%template(array_name) Array_class<type>;
%template(Array) _Array<type>;
%pointer_class(type, pointer_name);

%enddef


POINTER_HANDLING(bool, Array_bool, bool_p);

POINTER_HANDLING(char, Array_char, char_p);
POINTER_HANDLING(unsigned char, Array_unsigned_char, unsigned_char_p);
POINTER_HANDLING(signed char, Array_signed_char, signed_char_p);

POINTER_HANDLING(short, Array_short, short_p);
POINTER_HANDLING(int, Array_int, int_p);
POINTER_HANDLING(long, Array_long, long_p);
POINTER_HANDLING(long long, Array_long_long, long_long_p);

POINTER_HANDLING(unsigned short, Array_unsigned_short, unsigned_short_p);
POINTER_HANDLING(unsigned int, Array_unsigned_int, unsigned_int_p);
POINTER_HANDLING(unsigned long, Array_unsigned_long, unsigned_long_p);
POINTER_HANDLING(unsigned long long, Array_unsigned_long_long,
                 unsigned_long_long_p);

POINTER_HANDLING(size_t, Array_size_t, size_t_p);

POINTER_HANDLING(float, Array_float, float_p);
POINTER_HANDLING(double, Array_double, double_p);
