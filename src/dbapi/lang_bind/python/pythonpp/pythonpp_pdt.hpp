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

#ifndef PYTHONPP_PDT_H
#define PYTHONPP_PDT_H

#include "pythonpp_object.hpp"
#include <corelib/ncbistr.hpp>

BEGIN_NCBI_SCOPE

/// Flag showing whether all pythonpp::CString objects should be created as
/// Python unicode strings (TRUE value) or regular strings (FALSE value)
extern bool g_PythonStrDefToUnicode;


namespace pythonpp
{

// PyBoolObject
class CBool : public CObject
{
public:
    CBool(PyObject* obj, EOwnership ownership = eAcquireOwnership)
    : CObject(obj, ownership)
    {
        if ( !HasExactSameType(obj) ) {
            throw CTypeError("Invalid conversion");
        }
    }
    CBool(const CObject& obj)
    : CObject(obj)
    {
        if ( !HasExactSameType(obj) ) {
            throw CTypeError("Invalid conversion");
        }
    }
    CBool(const CBool& obj)
    : CObject(obj)
    {
    }
    CBool(bool value)
    : CObject(value ? Py_True : Py_False) // NCBI_FAKE_WARNING
    {
    }
    CBool(long value)
    {
        PyObject* tmp_obj = PyBool_FromLong(value);
        if ( !tmp_obj ) {
            throw CTypeError("Invalid conversion");
        }
        Set(tmp_obj, eTakeOwnership);
    }

public:
    // Assign operators ...
    CBool& operator= (const CObject& obj)
    {
        if ( this != &obj ) {
            if ( !HasExactSameType(obj) ) {
                throw CTypeError("Invalid conversion");
            }
            Set(obj);
        }
        return *this;
    }
    CBool& operator= (PyObject* obj)
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
    // Type conversion operators ...
    operator bool(void) const
    {
        return Get() == Py_True;
    }

public:
    static bool HasSameType(PyObject* obj)
    {
        return PyBool_Check (obj);
    }
    static bool HasExactSameType(PyObject* obj)
    {
        return PyBool_Check (obj);
    }
};

// PyInt_Type
class CInt : public CObject
{
//PyAPI_FUNC(PyObject *) PyInt_FromString(char*, char**, int);
//PyAPI_FUNC(unsigned long) PyInt_AsUnsignedLongMask(PyObject *);
//PyAPI_FUNC(long) PyInt_GetMax(void);
// long PyInt_AS_LONG( PyObject *io)
// unsigned long long PyInt_AsUnsignedLongLongMask(    PyObject *io)

public:
    CInt(PyObject* obj, EOwnership ownership = eAcquireOwnership)
    : CObject(obj, ownership)
    {
    }
    CInt(const CObject& obj)
    {
        PyObject* tmp_obj = PyNumber_Int(obj);
        if ( !tmp_obj ) {
            throw CTypeError("Invalid conversion");
        }
        Set(tmp_obj, eTakeOwnership);
    }
    CInt(const CInt& obj)
    : CObject(obj)
    {
    }
    CInt(int value)
    : CObject(PyInt_FromLong(long(value)), eTakeOwnership)
    {
    }
    CInt(long value = 0L)
    : CObject(PyInt_FromLong(value), eTakeOwnership)
    {
    }

public:
    // Assign operators ...
    CInt& operator= (const CObject& obj)
    {
        if ( this != &obj ) {
            PyObject* tmp_obj = PyNumber_Int(obj);
            if ( !tmp_obj ) {
                throw CTypeError("Invalid conversion");
            }
            Set(tmp_obj, eTakeOwnership);
        }
        return *this;
    }
    CInt& operator= (PyObject* obj)
    {
        if ( Get() != obj ) {
            PyObject* tmp_obj = PyNumber_Int(obj);
            if ( !tmp_obj ) {
                throw CTypeError("Invalid conversion");
            }
            Set(tmp_obj, eTakeOwnership);
        }
        return *this;
    }
    CInt& operator= (CInt value)
    {
        Set(PyInt_FromLong(long(value)), eTakeOwnership);
        return *this;
    }
    CInt& operator= (long value)
    {
        Set(PyInt_FromLong(value),  eTakeOwnership);
        return *this;
    }

public:
    // Type conversion operators ...
    operator long() const
    {
        return PyInt_AsLong(Get());
    }

public:
    static bool HasSameType(PyObject* obj)
    {
        return PyInt_Check (obj);
    }
    static bool HasExactSameType(PyObject* obj)
    {
        return PyInt_CheckExact (obj);
    }
};

// PyLong_Type
class CLong : public CObject
{
//PyAPI_FUNC(unsigned long) PyLong_AsUnsignedLongMask(PyObject *);
//PyAPI_FUNC(double) _PyLong_AsScaledDouble(PyObject *vv, int *e);

//PyAPI_FUNC(PyObject *) PyLong_FromVoidPtr(void *);
//PyAPI_FUNC(void *) PyLong_AsVoidPtr(PyObject *);
//PyAPI_FUNC(PyObject *) PyLong_FromString(char *, char **, int);

// ???
//PyAPI_FUNC(CInt) _PyLong_Sign(PyObject *v);
//PyAPI_FUNC(size_t) _PyLong_NumBits(PyObject *v);
//PyAPI_FUNC(PyObject *) _PyLong_FromByteArray(
//    const unsigned char* bytes, size_t n,
//    int little_endian, int is_signed);
//PyAPI_FUNC(CInt) _PyLong_AsByteArray(PyLongObject* v,
//    unsigned char* bytes, size_t n,
//    int little_endian, int is_signed);

// PyObject* PyLong_FromUnicode(   Py_UNICODE *u, int length, int base)

public:
    CLong(PyObject* obj, EOwnership ownership = eAcquireOwnership)
    : CObject(obj, ownership)
    {
    }
    CLong(const CObject& obj)
    {
        PyObject* tmp_obj = PyNumber_Long(obj);
        if ( !tmp_obj ) {
            throw CTypeError("Invalid conversion");
        }
        Set(tmp_obj, eTakeOwnership);
    }
    CLong(const CLong& obj)
    : CObject(obj)
    {
    }
    CLong(CInt value)
    : CObject(PyLong_FromLong(static_cast<long>(value)), eTakeOwnership)
    {
    }
    CLong(long value = 0L)
    : CObject(PyLong_FromLong(value), eTakeOwnership)
    {
    }
    CLong(unsigned long value)
    : CObject(PyLong_FromUnsignedLong(value), eTakeOwnership)
    {
    }
    CLong(long long value)
    : CObject(PyLong_FromLongLong(value), eTakeOwnership)
    {
    }
    CLong(unsigned long long value)
    : CObject(PyLong_FromUnsignedLongLong(value), eTakeOwnership)
    {
    }
    CLong(double value)
    : CObject(PyLong_FromDouble(value), eTakeOwnership)
    {
    }

public:
    // Assign operators ...
    CLong& operator= (const CObject& obj)
    {
        if ( this != &obj ) {
            PyObject* tmp_obj = PyNumber_Long(obj);
            if ( !tmp_obj ) {
                throw CTypeError("Invalid conversion");
            }
            Set(tmp_obj, eTakeOwnership);
        }
        return *this;
    }
    CLong& operator= (PyObject* obj)
    {
        if ( Get() != obj ) {
            PyObject* tmp_obj = PyNumber_Long(obj);
            if ( !tmp_obj ) {
                throw CTypeError("Invalid conversion");
            }
            Set(tmp_obj, eTakeOwnership);
        }
        return *this;
    }
    CLong& operator= (CInt value)
    {
        Set(PyLong_FromLong(long(value)), eTakeOwnership);
        return *this;
    }
    CLong& operator= (long value)
    {
        Set(PyLong_FromLong(value),  eTakeOwnership);
        return *this;
    }
    CLong& operator= (unsigned long value)
    {
        Set(PyLong_FromUnsignedLong(value),  eTakeOwnership);
        return *this;
    }
    CLong& operator= (long long value)
    {
        Set(PyLong_FromLongLong(value),  eTakeOwnership);
        return *this;
    }
    CLong& operator= (unsigned long long value)
    {
        Set(PyLong_FromUnsignedLongLong(value),  eTakeOwnership);
        return *this;
    }
    CLong& operator= (double value)
    {
        Set(PyLong_FromDouble(value),  eTakeOwnership);
        return *this;
    }

public:
    // Type conversion operators ...
    operator long() const
    {
        return PyLong_AsLong(Get());
    }
    operator unsigned long() const
    {
        return PyLong_AsUnsignedLong(Get());
    }
    operator long long() const
    {
        return PyLong_AsLongLong(Get());
    }
    operator unsigned long long() const
    {
        return PyLong_AsUnsignedLongLong(Get());
    }
    operator double() const
    {
        return PyLong_AsDouble(Get());
    }

public:
    static bool HasSameType(PyObject* obj)
    {
        return PyLong_Check (obj);
    }
    static bool HasExactSameType(PyObject* obj)
    {
        return PyLong_CheckExact (obj);
    }
};

// PyFloat_Type
class CFloat : public CObject
{
/* Return Python CFloat from string PyObject.  Second argument ignored on
input, and, if non-NULL, NULL is stored into *junk (this tried to serve a
purpose once but can't be made to work as intended). */
//PyAPI_FUNC(PyObject *) PyFloat_FromString(PyObject*, char** junk);

/* Extract C double from Python CFloat.  The macro version trades safety for
speed. */
//PyAPI_FUNC(double) PyFloat_AsDouble(PyObject *);

/* Write repr(v) into the char buffer argument, followed by null byte.  The
buffer must be "big enough"; >= 100 is very safe.
PyFloat_AsReprString(buf, x) strives to print enough digits so that
PyFloat_FromString(buf) then reproduces x exactly. */
//PyAPI_FUNC(void) PyFloat_AsReprString(char*, PyFloatObject *v);

/* Write str(v) into the char buffer argument, followed by null byte.  The
buffer must be "big enough"; >= 100 is very safe.  Note that it's
unusual to be able to get back the CFloat you started with from
PyFloat_AsString's result -- use PyFloat_AsReprString() if you want to
preserve precision across conversions. */
//PyAPI_FUNC(void) PyFloat_AsString(char*, PyFloatObject *v);

// ???
//PyAPI_FUNC(CInt) _PyFloat_Pack4(double x, unsigned char *p, int le);
//PyAPI_FUNC(CInt) _PyFloat_Pack8(double x, unsigned char *p, int le);
//PyAPI_FUNC(double) _PyFloat_Unpack4(const unsigned char *p, int le);
//PyAPI_FUNC(double) _PyFloat_Unpack8(const unsigned char *p, int le);

// PyObject* PyFloat_FromDouble(   double v)
// double PyFloat_AS_DOUBLE(   PyObject *pyfloat)

public:
    CFloat(PyObject* obj, EOwnership ownership = eAcquireOwnership)
    : CObject(obj, ownership)
    {
    }
    CFloat(const CObject& obj)
    {
        PyObject* tmp_obj = PyNumber_Float(obj);
        if ( !tmp_obj ) {
            throw CTypeError("Invalid conversion");
        }
        Set(tmp_obj, eTakeOwnership);
    }
    CFloat(const CFloat& obj)
    : CObject(obj)
    {
    }
    CFloat(double value = 0.0)
    : CObject(PyFloat_FromDouble(value), eTakeOwnership)
    {
    }

public:
    // Assign operators ...
    CFloat& operator= (const CObject& obj)
    {
        if ( this != &obj ) {
            PyObject* tmp_obj = PyNumber_Float(obj);
            if ( !tmp_obj ) {
                throw CTypeError("Invalid conversion");
            }
            Set(tmp_obj, eTakeOwnership);
        }
        return *this;
    }
    CFloat& operator= (PyObject* obj)
    {
        if ( Get() != obj ) {
            PyObject* tmp_obj = PyNumber_Float(obj);
            if ( !tmp_obj ) {
                throw CTypeError("Invalid conversion");
            }
            Set(tmp_obj, eTakeOwnership);
        }
        return *this;
    }
    CFloat& operator= (double value)
    {
        Set(PyFloat_FromDouble(value),  eTakeOwnership);
        return *this;
    }

public:
    // Type conversion operators ...
    operator double() const
    {
        return PyFloat_AsDouble(Get());
    }

public:
    static bool HasSameType(PyObject* obj)
    {
        return PyFloat_Check (obj);
    }
    static bool HasExactSameType(PyObject* obj)
    {
        return PyFloat_CheckExact (obj);
    }
};

class CComplex : public CObject
{
// Complex Numbers as C Structures ...
//PyAPI_FUNC(Py_complex) c_sum(Py_complex, Py_complex);
//PyAPI_FUNC(Py_complex) c_diff(Py_complex, Py_complex);
//PyAPI_FUNC(Py_complex) c_neg(Py_complex);
//PyAPI_FUNC(Py_complex) c_prod(Py_complex, Py_complex);
//PyAPI_FUNC(Py_complex) c_quot(Py_complex, Py_complex);
//PyAPI_FUNC(Py_complex) c_pow(Py_complex, Py_complex);

//PyAPI_FUNC(PyObject *) PyComplex_FromCComplex(Py_complex);
//PyAPI_FUNC(PyObject *) PyComplex_FromDoubles(double real, double imag);
//PyAPI_FUNC(double) PyComplex_RealAsDouble(PyObject *op);
//PyAPI_FUNC(double) PyComplex_ImagAsDouble(PyObject *op);
//PyAPI_FUNC(Py_complex) PyComplex_AsCComplex(PyObject *op);

public:
    CComplex(PyObject* obj, EOwnership ownership = eAcquireOwnership)
    : CObject(obj, ownership)
    {
    }
    CComplex(const CObject& obj)
    {
        if ( !HasExactSameType(obj) ) {
            throw CTypeError("Invalid conversion");
        }
        Set(obj);
    }
    CComplex(const CComplex& obj)
    : CObject(obj)
    {
    }
    CComplex(double v = 0.0, double w = 0.0)
    : CObject(PyComplex_FromDoubles(v, w), eTakeOwnership)
    {
    }

public:
    // Assign operators ...
    CComplex& operator= (const CObject& obj)
    {
        if ( this != &obj ) {
            if ( !HasExactSameType(obj) ) {
                throw CTypeError("Invalid conversion");
            }
            Set(obj);
        }
        return *this;
    }
    CComplex& operator= (PyObject* obj)
    {
        if ( Get() != obj ) {
            if ( !HasExactSameType(obj) ) {
                throw CTypeError("Invalid conversion");
            }
            Set(obj);
        }
        return *this;
    }
    CComplex& operator= (double value)
    {
        Set(PyComplex_FromDoubles(value, 0.0),  eTakeOwnership);
        return *this;
    }

public:
    // Type conversion operators ...
    operator double() const
    {
        return PyFloat_AsDouble(Get());
    }

public:
    double GetReal() const
    {
        return PyComplex_RealAsDouble(Get());
    }
    double GetImag() const
    {
        return PyComplex_ImagAsDouble(Get());
    }

public:
    static bool HasSameType(PyObject* obj)
    {
        return PyComplex_Check (obj);
    }
    static bool HasExactSameType(PyObject* obj)
    {
        return PyComplex_CheckExact (obj);
    }
};

// Not implemented yet ...
class CChar : public CObject
{
public:
protected:
private:
};

// PyString_Type
// PyBaseString_Type
class CString : public CObject
{
//PyAPI_FUNC(PyObject *) PyString_FromFormatV(const char*, va_list)
//                Py_GCC_ATTRIBUTE((format(printf, 1, 0)));
//PyAPI_FUNC(PyObject *) PyString_FromFormat(const char*, ...)
//                Py_GCC_ATTRIBUTE((format(printf, 1, 2)));
//PyAPI_FUNC(PyObject *) PyString_Repr(PyObject *, int);
//PyAPI_FUNC(void) PyString_Concat(PyObject **, PyObject *);
//PyAPI_FUNC(void) PyString_ConcatAndDel(PyObject **, PyObject *);
//PyAPI_FUNC(CInt) _PyString_Resize(PyObject **, int);
//PyAPI_FUNC(CInt) _PyString_Eq(PyObject *, PyObject*);
//PyAPI_FUNC(PyObject *) PyString_Format(PyObject *, PyObject *);
//PyAPI_FUNC(PyObject *) _PyString_FormatLong(PyObject*, int, int,
//                          int, char**, int*);
//PyAPI_FUNC(PyObject *) PyString_DecodeEscape(const char *, int,
//                           const char *, int,
//                           const char *);

//PyAPI_FUNC(void) PyString_InternInPlace(PyObject **);
//PyAPI_FUNC(void) PyString_InternImmortal(PyObject **);
//PyAPI_FUNC(PyObject *) PyString_InternFromString(const char *);
//PyAPI_FUNC(void) _Py_ReleaseInternedStrings(void);
//PyAPI_FUNC(PyObject *) _PyString_Join(PyObject *sep, PyObject *x);
//PyAPI_FUNC(PyObject*) PyString_Decode(
//    const char *s,              /* encoded string */
//    int size,                   /* size of buffer */
//    const char *encoding,       /* encoding */
//    const char *errors          /* error handling */
//    );
//PyAPI_FUNC(PyObject*) PyString_Encode(
//    const char *s,              /* string char buffer */
//    int size,                   /* number of chars to encode */
//    const char *encoding,       /* encoding */
//    const char *errors          /* error handling */
//    );

//PyAPI_FUNC(PyObject*) PyString_AsEncodedObject(
//    PyObject *str,      /* string object */
//    const char *encoding,   /* encoding */
//    const char *errors      /* error handling */
//    );
//PyAPI_FUNC(PyObject*) PyString_AsDecodedObject(
//    PyObject *str,      /* string object */
//    const char *encoding,   /* encoding */
//    const char *errors      /* error handling */
//    );

//PyAPI_FUNC(CInt) PyString_AsStringAndSize(
//    register PyObject *obj, /* string or Unicode object */
//    register char **s,      /* pointer to buffer variable */
//    register int *len       /* pointer to length variable or NULL
//                   (only possible for 0-terminated
//                   strings) */
//    );

// int PyString_GET_SIZE(  PyObject *string)
// char* PyString_AS_STRING(   PyObject *string)

// int PyUnicode_GET_SIZE( PyObject *o)
// int PyUnicode_GET_DATA_SIZE(    PyObject *o)
// Py_UNICODE* PyUnicode_AS_UNICODE(   PyObject *o)
// const char* PyUnicode_AS_DATA(  PyObject *o)
// int Py_UNICODE_ISSPACE( Py_UNICODE ch)
// int Py_UNICODE_ISLOWER( Py_UNICODE ch)
// int Py_UNICODE_ISUPPER( Py_UNICODE ch)
// int Py_UNICODE_ISTITLE( Py_UNICODE ch)
// int Py_UNICODE_ISLINEBREAK( Py_UNICODE ch)
// int Py_UNICODE_ISDECIMAL(   Py_UNICODE ch)
// int Py_UNICODE_ISDIGIT( Py_UNICODE ch)
// int Py_UNICODE_ISNUMERIC(   Py_UNICODE ch)
// int Py_UNICODE_ISALPHA( Py_UNICODE ch)
// int Py_UNICODE_ISALNUM( Py_UNICODE ch)
// Py_UNICODE Py_UNICODE_TOLOWER(  Py_UNICODE ch)
// Py_UNICODE Py_UNICODE_TOUPPER(  Py_UNICODE ch)
// Py_UNICODE Py_UNICODE_TOTITLE(  Py_UNICODE ch)
// int Py_UNICODE_TODECIMAL(   Py_UNICODE ch)
// int Py_UNICODE_TODIGIT( Py_UNICODE ch)
// double Py_UNICODE_TONUMERIC(    Py_UNICODE ch)
// PyObject* PyUnicode_FromUnicode(    const Py_UNICODE *u, int size)
// Py_UNICODE* PyUnicode_AsUnicode(    PyObject *unicode)
// int PyUnicode_GetSize(  PyObject *unicode)
// PyObject* PyUnicode_FromEncodedObject(  PyObject *obj, const char *encoding, const char *errors)
// PyObject* PyUnicode_FromObject( PyObject *obj)
// PyObject* PyUnicode_FromWideChar(   const wchar_t *w, int size)
// int PyUnicode_AsWideChar(   PyUnicodeObject *unicode, wchar_t *w, int size)

public:
    CString(void)
    : CObject(PyString_FromStringAndSize("", 0), eTakeOwnership)
    {
    }
    CString(PyObject* obj, EOwnership ownership = eAcquireOwnership)
    : CObject(obj, ownership)
    {
        if ( !HasExactSameType(obj) ) {
            throw CTypeError("Invalid conversion");
        }
    }
    CString(const CObject& obj)
    {
        if ( !HasExactSameType(obj) ) {
            throw CTypeError("Invalid conversion");
        }
        Set(obj);
    }
    CString(const CString& obj)
    : CObject(obj)
    {
    }
    CString(const string& str)
    {
        operator= (str);
    }
    CString(const char* str)
    {
        operator= (str);
    }
    CString(const char* str, size_t str_size)
    {
        operator= (string(str, str_size));
    }

public:
    // Assign operators ...
    CString& operator= (const CObject& obj)
    {
        if ( this != &obj ) {
            if ( !HasExactSameType(obj) ) {
                throw CTypeError("Invalid conversion");
            }
            Set(obj);
        }
        return *this;
    }
    CString& operator= (PyObject* obj)
    {
        if ( Get() != obj ) {
            if ( !HasExactSameType(obj) ) {
                throw CTypeError("Invalid conversion");
            }
            Set(obj);
        }
        return *this;
    }
    CString& operator= (const string& str)
    {
        if (g_PythonStrDefToUnicode) {
            basic_string<Py_UNICODE> str_uni(CUtf8::AsBasicString<Py_UNICODE>(str));
            Set(PyUnicode_FromUnicode(str_uni.data(), str_uni.size()), eTakeOwnership);
        }
        else {
            Set(PyString_FromStringAndSize(str.data(), str.size()), eTakeOwnership);
        }
        return *this;
    }
    CString& operator= (const char* str)
    {
        return operator= (string(str));
    }

public:
    size_t GetSize (void) const
    {
        if ( PyUnicode_Check(Get()) ) {
            return static_cast<size_t>( PyUnicode_GET_SIZE( Get() ) );
        } else {
            return static_cast<size_t>( PyString_Size ( Get() ) );
        }
    }
    operator string (void) const
    {
        return AsStdSring();
    }

    string AsStdSring(void) const
    {
        if( PyUnicode_Check(Get()) ) {
            return CUtf8::AsUTF8(
                               PyUnicode_AS_UNICODE( Get() ),
                               static_cast<size_t>( PyUnicode_GET_SIZE( Get() ) ) );
        } else {
            return string( PyString_AsString( Get() ), static_cast<size_t>( PyString_Size( Get() ) ) );
        }
    }

public:
    static bool HasExactSameType(PyObject* obj)
    {
        return PyString_CheckExact(obj)  ||  PyUnicode_CheckExact(obj);
    }
    static bool HasSameType(PyObject* obj)
    {
        return PyString_Check(obj)  ||  PyUnicode_Check(obj);
    }
};

// PyFile_Type
class CFile : public CObject
{
// ????
//PyAPI_FUNC(CInt) PyFile_SetEncoding(PyObject *, const char *);
//PyAPI_FUNC(CInt) PyObject_AsFileDescriptor(PyObject *);


// Documented ...
//PyAPI_FUNC(PyObject *) PyFile_FromString(char *, char *);
//PyAPI_FUNC(PyObject *) PyFile_FromFile(FILE *, char *, char *, (*)(FILE *));
//PyAPI_FUNC(FILE *) PyFile_AsFile(PyObject *);
//PyAPI_FUNC(PyObject *) PyFile_GetLine(PyObject *, int);
//PyAPI_FUNC(PyObject *) PyFile_Name(PyObject *);
//PyAPI_FUNC(void) PyFile_SetBufSize(PyObject *, int);
// int PyFile_Encoding(    PyFileObject *p, char *enc)
//PyAPI_FUNC(CInt) PyFile_SoftSpace(PyObject *, int);
//PyAPI_FUNC(CInt) PyFile_WriteObject(PyObject *, PyObject *, int);
//PyAPI_FUNC(CInt) PyFile_WriteString(const char *, PyObject *);

public:
public:
    static bool HasExactSameType(PyObject* obj)
    {
        return PyFile_CheckExact(obj);
    }
    static bool HasSameType(PyObject* obj)
    {
        return PyFile_Check(obj);
    }
};

///////////////////////////////////////////////////////////////////////////
inline CObject operator+ (const CObject& a, int j)
{
    PyObject* tmp_obj = PyNumber_Add(a.Get(), CInt(j).Get());
    if ( !tmp_obj ) {
        throw CArithmeticError("PyNumber_Add");
    }
    return CObject(tmp_obj, eTakeOwnership);
}
inline CObject operator+ (const CObject& a, double v)
{
    PyObject* tmp_obj = PyNumber_Add(a.Get(), CFloat(v).Get());
    if ( !tmp_obj ) {
        throw CArithmeticError("PyNumber_Add");
    }
    return CObject(tmp_obj, eTakeOwnership);
}
inline CObject operator+ (CInt j, const CObject& b)
{
    PyObject* tmp_obj = PyNumber_Add(CInt(j).Get(), b.Get());
    if ( !tmp_obj ) {
        throw CArithmeticError("PyNumber_Add");
    }
    return CObject(tmp_obj, eTakeOwnership);
}
inline CObject operator+ (double v, const CObject& b)
{
    PyObject* tmp_obj = PyNumber_Add(CFloat(v).Get(), b.Get());
    if ( !tmp_obj ) {
        throw CArithmeticError("PyNumber_Add");
    }
    return CObject(tmp_obj, eTakeOwnership);
}
inline CObject operator- (const CObject& a, int j)
{
    PyObject* tmp_obj = PyNumber_Subtract(a.Get(), CInt(j).Get());
    if ( !tmp_obj ) {
        throw CArithmeticError("PyNumber_Subtract");
    }
    return CObject(tmp_obj, eTakeOwnership);
}
inline CObject operator- (const CObject& a, double v)
{
    PyObject* tmp_obj = PyNumber_Subtract(a.Get(), CFloat(v).Get());
    if ( !tmp_obj ) {
        throw CArithmeticError("PyNumber_Subtract");
    }
    return CObject(tmp_obj, eTakeOwnership);
}
inline CObject operator- (CInt j, const CObject& b)
{
    PyObject* tmp_obj = PyNumber_Subtract(CInt(j).Get(), b.Get());
    if ( !tmp_obj ) {
        throw CArithmeticError("PyNumber_Subtract");
    }
    return CObject(tmp_obj, eTakeOwnership);
}
inline CObject operator- (double v, const CObject& b)
{
    PyObject* tmp_obj = PyNumber_Subtract(CFloat(v).Get(), b.Get());
    if ( !tmp_obj ) {
        throw CArithmeticError("PyNumber_Subtract");
    }
    return CObject(tmp_obj, eTakeOwnership);
}
inline CObject operator* (const CObject& a, int j)
{
    PyObject* tmp_obj = PyNumber_Multiply(a.Get(), CInt(j).Get());
    if ( !tmp_obj ) {
        throw CArithmeticError("PyNumber_Multiply");
    }
    return CObject(tmp_obj, eTakeOwnership);
}
inline CObject operator* (const CObject& a, double v)
{
    PyObject* tmp_obj = PyNumber_Multiply(a.Get(), CFloat(v).Get());
    if ( !tmp_obj ) {
        throw CArithmeticError("PyNumber_Multiply");
    }
    return CObject(tmp_obj, eTakeOwnership);
}
inline CObject operator* (CInt j, const CObject& b)
{
    PyObject* tmp_obj = PyNumber_Multiply(CInt(j).Get(), b.Get());
    if ( !tmp_obj ) {
        throw CArithmeticError("PyNumber_Multiply");
    }
    return CObject(tmp_obj, eTakeOwnership);
}
inline CObject operator* (double v, const CObject& b)
{
    PyObject* tmp_obj = PyNumber_Multiply(CFloat(v).Get(), b.Get());
    if ( !tmp_obj ) {
        throw CArithmeticError("PyNumber_Multiply");
    }
    return CObject(tmp_obj, eTakeOwnership);
}
inline CObject operator/ (const CObject& a, int j)
{
    PyObject* tmp_obj = PyNumber_Divide(a.Get(), CInt(j).Get());
    if ( !tmp_obj ) {
        throw CArithmeticError("PyNumber_Divide");
    }
    return CObject(tmp_obj, eTakeOwnership);
}
inline CObject operator/ (const CObject& a, double v)
{
    PyObject* tmp_obj = PyNumber_Divide(a.Get(), CFloat(v).Get());
    if ( !tmp_obj ) {
        throw CArithmeticError("PyNumber_Divide");
    }
    return CObject(tmp_obj, eTakeOwnership);
}
inline CObject operator/ (CInt j, const CObject& b)
{
    PyObject* tmp_obj = PyNumber_Divide(CInt(j).Get(), b.Get());
    if ( !tmp_obj ) {
        throw CArithmeticError("PyNumber_Divide");
    }
    return CObject(tmp_obj, eTakeOwnership);
}
inline CObject operator/ (double v, const CObject& b)
{
    PyObject* tmp_obj = PyNumber_Divide(CFloat(v).Get(), b.Get());
    if ( !tmp_obj ) {
        throw CArithmeticError("PyNumber_Divide");
    }
    return CObject(tmp_obj, eTakeOwnership);
}
inline CObject operator% (const CObject& a, int j)
{
    PyObject* tmp_obj = PyNumber_Remainder(a.Get(), CInt(j).Get());
    if ( !tmp_obj ) {
        throw CArithmeticError("PyNumber_Remainder");
    }
    return CObject(tmp_obj, eTakeOwnership);
}
inline CObject operator% (const CObject& a, double v)
{
    PyObject* tmp_obj = PyNumber_Remainder(a.Get(), CFloat(v).Get());
    if ( !tmp_obj ) {
        throw CArithmeticError("PyNumber_Remainder");
    }
    return CObject(tmp_obj, eTakeOwnership);
}
inline CObject operator% (CInt j, const CObject& b)
{
    PyObject* tmp_obj = PyNumber_Remainder(CInt(j).Get(), b.Get());
    if ( !tmp_obj ) {
        throw CArithmeticError("PyNumber_Remainder");
    }
    return CObject(tmp_obj, eTakeOwnership);
}
inline CObject operator% (double v, const CObject& b)
{
    PyObject* tmp_obj = PyNumber_Remainder(CFloat(v).Get(), b.Get());
    if ( !tmp_obj ) {
        throw CArithmeticError("PyNumber_Remainder");
    }
    return CObject(tmp_obj, eTakeOwnership);
}

}                                       // namespace pythonpp

END_NCBI_SCOPE

#endif                                  // PYTHONPP_PDT_H

