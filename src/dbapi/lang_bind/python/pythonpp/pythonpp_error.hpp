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

#ifndef PYTHONPP_ERROR_H
#define PYTHONPP_ERROR_H

#ifdef _MSC_VER
// disable warning C4005: macro redefinition.
#pragma warning(disable: 4005)
#else

#ifdef HAVE_GETHOSTBYNAME_R
#undef HAVE_GETHOSTBYNAME_R
#endif

#ifdef _POSIX_C_SOURCE
#undef _POSIX_C_SOURCE
#endif

#endif

#include <Python.h>

#ifndef _MSC_VER
#ifdef HAVE_GETHOSTBYNAME_R
#undef HAVE_GETHOSTBYNAME_R
#endif
#endif

BEGIN_NCBI_SCOPE

namespace pythonpp
{

class CError
{
///* Error testing and normalization */
//PyAPI_FUNC(int) PyErr_GivenExceptionMatches(PyObject *, PyObject *);
//PyAPI_FUNC(int) PyErr_ExceptionMatches(PyObject *);
//PyAPI_FUNC(void) PyErr_NormalizeException(PyObject**, PyObject**, PyObject**);



//PyAPI_DATA(PyObject *) PyExc_MemoryErrorInst;


/* Predefined warning categories */
//PyAPI_DATA(PyObject *) PyExc_Warning;
//PyAPI_DATA(PyObject *) PyExc_UserWarning;
//PyAPI_DATA(PyObject *) PyExc_DeprecationWarning;
//PyAPI_DATA(PyObject *) PyExc_PendingDeprecationWarning;
//PyAPI_DATA(PyObject *) PyExc_SyntaxWarning;

//PyAPI_DATA(PyObject *) PyExc_RuntimeWarning;
//PyAPI_DATA(PyObject *) PyExc_FutureWarning;


/* Convenience functions */

//PyAPI_FUNC(int) PyErr_BadArgument(void);
//PyAPI_FUNC(PyObject *) PyErr_NoMemory(void);
//PyAPI_FUNC(PyObject *) PyErr_SetFromErrno(PyObject *);
//PyAPI_FUNC(PyObject *) PyErr_SetFromErrnoWithFilenameObject(
//    PyObject *, PyObject *);
//PyAPI_FUNC(PyObject *) PyErr_SetFromErrnoWithFilename(PyObject *, char *);
//#ifdef Py_WIN_WIDE_FILENAMES
//PyAPI_FUNC(PyObject *) PyErr_SetFromErrnoWithUnicodeFilename(
//    PyObject *, Py_UNICODE *);
//#endif /* Py_WIN_WIDE_FILENAMES */

//PyAPI_FUNC(PyObject *) PyErr_Format(PyObject *, const char *, ...)
//            Py_GCC_ATTRIBUTE((format(printf, 2, 3)));

//#ifdef MS_WINDOWS
//PyAPI_FUNC(PyObject *) PyErr_SetFromWindowsErrWithFilenameObject(
//    int, const char *);
//PyAPI_FUNC(PyObject *) PyErr_SetFromWindowsErrWithFilename(
//    int, const char *);
//#ifdef Py_WIN_WIDE_FILENAMES
//PyAPI_FUNC(PyObject *) PyErr_SetFromWindowsErrWithUnicodeFilename(
//    int, const Py_UNICODE *);
//#endif /* Py_WIN_WIDE_FILENAMES */
//PyAPI_FUNC(PyObject *) PyErr_SetFromWindowsErr(int);
//PyAPI_FUNC(PyObject *) PyErr_SetExcFromWindowsErrWithFilenameObject(
//    PyObject *,int, PyObject *);
//PyAPI_FUNC(PyObject *) PyErr_SetExcFromWindowsErrWithFilename(
//    PyObject *,int, const char *);
//#ifdef Py_WIN_WIDE_FILENAMES
//PyAPI_FUNC(PyObject *) PyErr_SetExcFromWindowsErrWithUnicodeFilename(
//    PyObject *,int, const Py_UNICODE *);
//#endif /* Py_WIN_WIDE_FILENAMES */
//PyAPI_FUNC(PyObject *) PyErr_SetExcFromWindowsErr(PyObject *, int);
//#endif /* MS_WINDOWS */

/* Function to create a new exception */
//PyAPI_FUNC(PyObject *) PyErr_NewException(char *name, PyObject *base,
//                                         PyObject *dict);
//PyAPI_FUNC(void) PyErr_WriteUnraisable(PyObject *);

/* Issue a warning or exception */
//PyAPI_FUNC(int) PyErr_Warn(PyObject *, char *);
//PyAPI_FUNC(int) PyErr_WarnExplicit(PyObject *, const char *,
//                   const char *, int,
//                   const char *, PyObject *);

/* In sigcheck.c or signalmodule.c */
//PyAPI_FUNC(int) PyErr_CheckSignals(void);
//PyAPI_FUNC(void) PyErr_SetInterrupt(void);

/* Support for adding program text to SyntaxErrors */
//PyAPI_FUNC(void) PyErr_SyntaxLocation(const char *, int);
//PyAPI_FUNC(PyObject *) PyErr_ProgramText(const char *, int);


public:
    CError(const string& msg)
    {
        SetString(msg);
    }

protected:
    CError(const string& msg, PyObject* err_type)
    {
        SetString(err_type, msg);
    }

public:
    static void Check(void)
    {
        PyObject* err_type = PyErr_Occurred();
        if ( err_type ) {
            throw CError();
        }
    }
    static void Check(const char* value)
    {
        if ( !value ) {
            Check();
        }
    }
    static void Check(PyObject* obj)
    {
        if ( !obj ) {
            Check();
        }
    }
    static void Clear(void)
    {
        PyErr_Clear();
    }

public:
    static void SetString(const string& msg)
    {
        PyErr_SetString(PyExc_Exception, msg.c_str());
    }

protected:
    // Error without parameters should be thrown only in case
    // of system/python error. User should always provide
    // a message.
    CError(void)
    {
    }

protected:
    static void SetString(PyObject* excp_obj, const string& msg)
    {
        PyErr_SetString(excp_obj, msg.c_str());
    }
    static void SetObject(PyObject* excp_obj, PyObject* value)
    {
        PyErr_SetObject(excp_obj, value);
    }
    static void SetNone(PyObject* excp_obj)
    {
        PyErr_SetNone(excp_obj);
    }
};


///////////////////////////////////
// Predefined exceptions
///////////////////////////////////

class CZeroDivisionError : public CError
{
public:
    CZeroDivisionError(const string& msg)
    : CError(msg, PyExc_ZeroDivisionError)
    {
    }
};

class CValueError : public CError
{
public:
    CValueError(const string& msg)
    : CError(msg, PyExc_ValueError)
    {
    }
};

class CUnicodeError : public CError
{
public:
    CUnicodeError(const string& msg)
    : CError(msg, PyExc_UnicodeError)
    {
    }

protected:
    CUnicodeError(const string& msg, PyObject* err_type)
    : CError(msg, err_type)
    {
    }
};

class CUnicodeEncodeError : public CUnicodeError
{
public:
    CUnicodeEncodeError(const string& msg)
    : CUnicodeError(msg, PyExc_UnicodeEncodeError)
    {
    }
};

class CUnicodeDecodeError : public CUnicodeError
{
public:
    CUnicodeDecodeError(const string& msg)
    : CUnicodeError(msg, PyExc_UnicodeDecodeError)
    {
    }
};

class CUnicodeTranslateError : public CUnicodeError
{
public:
    CUnicodeTranslateError(const string& msg)
    : CUnicodeError(msg, PyExc_UnicodeTranslateError)
    {
    }
};

class CTypeError : public CError
{
public:
    CTypeError(const string& msg)
    : CError(msg, PyExc_TypeError)
    {
    }
};

class CSystemError : public CError
{
public:
    CSystemError(const string& msg)
    : CError(msg, PyExc_SystemError)
    {
    }
};

class CUnboundLocalError : public CError
{
public:
    CUnboundLocalError(const string& msg)
    : CError(msg, PyExc_UnboundLocalError)
    {
    }
};

class CSystemExit : public CError
{
public:
    CSystemExit(const string& msg)
    : CError(msg, PyExc_SystemExit)
    {
    }
};

class CReferenceError : public CError
{
public:
    CReferenceError(const string& msg)
    : CError(msg, PyExc_ReferenceError)
    {
    }
};

class CTabError : public CError
{
public:
    CTabError(const string& msg)
    : CError(msg, PyExc_TabError)
    {
    }
};

class CIndentationError : public CError
{
public:
    CIndentationError(const string& msg)
    : CError(msg, PyExc_IndentationError)
    {
    }
};

class CSyntaxError : public CError
{
public:
    CSyntaxError(const string& msg)
    : CError(msg, PyExc_SyntaxError)
    {
    }
};

class CNotImplementedError : public CError
{
public:
    CNotImplementedError(const string& msg)
    : CError(msg, PyExc_NotImplementedError)
    {
    }

protected:
    CNotImplementedError(const string& msg, PyObject* err_type)
    : CError(msg, err_type)
    {
    }
};

class CRuntimeError : public CError
{
public:
    CRuntimeError(const string& msg)
    : CError(msg, PyExc_RuntimeError)
    {
    }
};

class COverflowError : public CError
{
public:
    COverflowError(const string& msg)
    : CError(msg, PyExc_OverflowError)
    {
    }
};

class CNameError : public CError
{
public:
    CNameError(const string& msg)
    : CError(msg, PyExc_NameError)
    {
    }
};

class CMemoryError : public CError
{
public:
    CMemoryError(const string& msg)
    : CError(msg, PyExc_MemoryError)
    {
    }
};

class CKeyboardInterrupt : public CError
{
public:
    CKeyboardInterrupt(const string& msg)
    : CError(msg, PyExc_KeyboardInterrupt)
    {
    }
};

class CKeyError : public CError
{
public:
    CKeyError(const string& msg)
    : CError(msg, PyExc_KeyError)
    {
    }
};

class CIndexError : public CError
{
public:
    CIndexError(const string& msg)
    : CError(msg, PyExc_IndexError)
    {
    }
};

class CImportError : public CError
{
public:
    CImportError(const string& msg)
    : CError(msg, PyExc_ImportError)
    {
    }
};

class COSError : public CError
{
public:
    COSError(const string& msg)
    : CError(msg, PyExc_OSError)
    {
    }

protected:
    COSError(const string& msg, PyObject* err_type)
    : CError(msg, err_type)
    {
    }
};

class CIOError : public CError
{
public:
    CIOError(const string& msg)
    : CError(msg, PyExc_IOError)
    {
    }
};

class CEnvironmentError : public CError
{
public:
    CEnvironmentError(const string& msg)
    : CError(msg, PyExc_EnvironmentError)
    {
    }
};

class CFloatingPointError : public CError
{
public:
    CFloatingPointError(const string& msg)
    : CError(msg, PyExc_FloatingPointError)
    {
    }
};

class CEOFError : public CError
{
public:
    CEOFError(const string& msg)
    : CError(msg, PyExc_EOFError)
    {
    }
};

class CAttributeError : public CError
{
public:
    CAttributeError(const string& msg)
    : CError(msg, PyExc_AttributeError)
    {
    }
};

class CAssertionError : public CError
{
public:
    CAssertionError(const string& msg)
    : CError(msg, PyExc_AssertionError)
    {
    }
};

class CLookupError : public CError
{
public:
    CLookupError(const string& msg)
    : CError(msg, PyExc_LookupError)
    {
    }
};

class CArithmeticError : public CError
{
public:
    CArithmeticError(const string& msg)
    : CError(msg, PyExc_ArithmeticError)
    {
    }
};

class CStandardError : public CError
{
public:
    CStandardError(const string& msg)
    : CError(msg, PyExc_StandardError)
    {
    }
};

class CStopIteration : public CError
{
public:
    CStopIteration(const string& msg)
    : CError(msg, PyExc_StopIteration)
    {
    }
};

#ifdef MS_WINDOWS
class CWindowsError : public COSError
{
public:
    CWindowsError(const string& msg)
    : COSError(msg, PyExc_WindowsError)
    {
    }
};
#endif
#ifdef __VMS
class CVMSError : public COSError
{
public:
    CVMSError(const string& msg)
    : COSError(msg, PyExc_VMSError)
    {
    }
};
#endif




// This class is supposed to store and retrieve a python error code
class CErrorStackElem
{
public:
    CErrorStackElem(void)
    : m_ExceptionType(NULL),
        m_Value(NULL),
        m_Traceback(NULL)
    {
    }

public:
    void Fetch(void)
    {
        _ASSERT(!m_ExceptionType && !m_Value && !m_Traceback);

        PyErr_Fetch(&m_ExceptionType, &m_Value, &m_Traceback);

        _ASSERT(
            (m_ExceptionType) ||
            (!m_ExceptionType && !m_Value && !m_Traceback)
        );
    }
    void Restore(void)
    {
        // Do not pass a NULL type and non-NULL value or traceback.
        _ASSERT(
            (m_ExceptionType) ||
            (!m_ExceptionType && !m_Value && !m_Traceback)
        );

        //  This call takes away a reference to each object: you must own
        // a reference to each object before the call and after the call
        // you no longer own these references.
        PyErr_Restore(m_ExceptionType, m_Value, m_Traceback);

        m_ExceptionType = NULL;
        m_Value = NULL;
        m_Traceback = NULL;
    }

private:
    PyObject* m_ExceptionType;
    PyObject* m_Value;
    PyObject* m_Traceback;
};

}                                       // namespace pythonpp

END_NCBI_SCOPE

#endif                                  // PYTHONPP_ERROR_H

/* ===========================================================================
*
* $Log$
* Revision 1.1  2005/01/18 19:26:07  ssikorsk
* Initial version of a Python DBAPI module
*
*
* ===========================================================================
*/
