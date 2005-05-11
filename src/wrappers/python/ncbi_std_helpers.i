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
 * File Description:  Helper code for stl wrapping
 *
 */


%include exception.i

%define CATCH_OUT_OF_RANGE(func)
    %exception func {
        try {
            $action
        } catch (std::out_of_range& e) {
            SWIG_exception(SWIG_IndexError, e.what());
        }
    }
%enddef


%{

bool IsListOrTuple(PyObject* obj)
{
    return PyList_Check(obj) || PyTuple_Check(obj);
}

unsigned int SizeListOrTuple(PyObject* obj)
{
    return PyList_Check(obj) ? PyList_Size(obj) : PyTuple_Size(obj);
}

static int CheckPtrAllowNull(PyObject *obj, swig_type_info *ty)
{
    void *ptr;
    if (SWIG_ConvertPtr(obj, &ptr, ty, 0) == -1) {
        PyErr_Clear();
        return 0;
    } else {
        return 1;
    }
}

static int CheckPtrForbidNull(PyObject *obj, swig_type_info *ty)
{
    void *ptr;
    if (SWIG_ConvertPtr(obj, &ptr, ty, 0) == -1) {
        PyErr_Clear();
        return 0;
    } else {
        return ptr != 0;
    }
}


// Treatment of doubles and floats for container specialization

static bool DoubleCheck(PyObject *obj)
{
    return PyFloat_Check(obj) || PyInt_Check(obj) || PyLong_Check(obj);
}

static double DoubleToCpp(PyObject *obj)
{
    if (PyFloat_Check(obj)) {
        return PyFloat_AsDouble(obj);
    } else if (PyInt_Check(obj)) {
        return static_cast<double>(PyInt_AsLong(obj));
    } else {
        return static_cast<double>(PyLong_AsLong(obj));
    }
}


// Treatment of std::string's for container specialization
#include <string>
static std::string StringToCpp(PyObject *obj)
{
    return std::string(PyString_AsString(obj));
}

static PyObject* StringFromCpp(const std::string& s)
{
    return PyString_FromStringAndSize(s.data(), s.size());
}


// Treat negative indices the way Python does
inline void AdjustIndex(int& i, unsigned int size)
{
    if (i < 0) {
        i += size;
    }
}

// Python negative indices, plus limit to allowable range
inline void AdjustSlice(int& i, int& j, unsigned int size)
{
    if (i < 0) {
        i += size;
    }
    if (j < 0) {
        j += size;
    }
    if (i < 0) {
        i = 0;
    }
    if (j > size) {
        j = size;
    }
}

%}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/05/11 21:30:44  jcherry
 * Initial version
 *
 * ===========================================================================
 */
