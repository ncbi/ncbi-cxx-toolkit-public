#ifndef NCBIEXPT__HPP
#define NCBIEXPT__HPP

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
* Author:  Denis Vakatov
*
* File Description:
*   NCBI C++ exception handling
*   Includes auxiliary ad hoc macros to "catch" and macros
*   for the C++ exception specification
*
* --------------------------------------------------------------------------
* $Log$
* Revision 1.22  2001/05/17 14:53:41  lavr
* Typos corrected
*
* Revision 1.21  2000/04/04 22:30:25  vakatov
* SetThrowTraceAbort() -- auto-set basing on the application
* environment and/or registry
*
* Revision 1.20  1999/12/29 13:58:37  vasilche
* Added THROWS_NONE.
*
* Revision 1.19  1999/12/28 21:04:14  vasilche
* Removed three more implicit virtual destructors.
*
* Revision 1.18  1999/12/28 18:55:25  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.17  1999/11/18 20:12:40  vakatov
* DoDbgPrint() -- prototyped in both _DEBUG and NDEBUG
*
* Revision 1.16  1999/10/04 16:20:56  vasilche
* Added full set of macros THROW*_TRACE
*
* Revision 1.15  1999/09/27 16:23:20  vasilche
* Changed implementation of debugging macros (_TRACE, _THROW*, _ASSERT etc),
* so that they will be much easier for compilers to eat.
*
* Revision 1.14  1999/09/23 21:15:48  vasilche
* Added namespace modifiers.
*
* Revision 1.13  1999/06/11 16:33:10  vasilche
* Fixed THROWx_TRACE
*
* Revision 1.12  1999/06/11 16:20:22  vasilche
* THROW_TRACE fixed for not very smart compiler like Sunpro
*
* Revision 1.11  1999/06/11 02:48:03  vakatov
* [_DEBUG] Refined the output from THROW*_TRACE macro
*
* Revision 1.10  1999/05/04 00:03:07  vakatov
* Removed the redundant severity arg from macro ERR_POST()
*
* Revision 1.9  1999/01/07 16:15:09  vakatov
* Explicitly specify "NCBI_NS_NCBI::" in the preprocessor macros
*
* Revision 1.8  1999/01/04 22:41:41  vakatov
* Do not use so-called "hardware-exceptions" as these are not supported
* (on the signal level) by UNIX
* Do not "set_unexpected()" as it works differently on UNIX and MSVC++
*
* Revision 1.7  1998/12/28 17:56:27  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.6  1998/12/01 01:19:47  vakatov
* Moved <string> and <stdexcept> after the <ncbidiag.hpp> to avoid weird
* conflicts under MSVC++ 6.0
*
* Revision 1.5  1998/11/24 17:51:26  vakatov
* + CParseException
* Removed "Ncbi" sub-prefix from the NCBI exception class names
*
* Revision 1.4  1998/11/10 01:17:36  vakatov
* Cleaned, adopted to the standard NCBI C++ framework and incorporated
* the "hardware exceptions" code and tests(originally written by
* V.Sandomirskiy).
* Only tested for MSVC++ compiler yet -- to be continued for SunPro...
*
* Revision 1.3  1998/11/06 22:42:38  vakatov
* Introduced BEGIN_, END_ and USING_ NCBI_SCOPE macros to put NCBI C++
* API to namespace "ncbi::" and to use it by default, respectively
* Introduced THROWS_NONE and THROWS(x) macros for the exception
* specifications
* Other fixes and rearrangements throughout the most of "corelib" code
*
* ==========================================================================
*/

#include <corelib/ncbidiag.hpp>
#include <string>
#include <stdexcept>


// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
// The use of C++ exception specification mechanism, like:
//   "f(void) throw();"       <==  "f(void) THROWS_NONE;"
//   "g(void) throw(e1,e2);"  <==  "f(void) THROWS((e1,e2));"

#if defined(NCBI_USE_THROW_SPEC)
#  define THROWS_NONE throw()
#  define THROWS(x) throw x
#else
#  define THROWS_NONE
#  define THROWS(x)
#endif


/////////////////////////////////////////////////////////////////////////////
// Macro to trace the exceptions being thrown(for the -D_DEBUG mode only):
//   RETHROW_TRACE
// The following macros accept objects of types:
//      std::string, char*, const char* and std::exception
//   THROW0_TRACE
//   THROW1_TRACE
//   THROW_TRACE
// The following macros accept objects of any printable type (any type which
//      allow output to std::ostream via operator <<):
//      int, double, complex etc.
//   THROW0p_TRACE
//   THROW1p_TRACE
//   THROWp_TRACE
// The following macros accept objects of any type:
//   THROW0np_TRACE
//   THROW1np_TRACE
//   THROWnp_TRACE

// Specify if to call "abort()" inside the DoThrowTraceAbort().
// By default, this feature is not activated unless
//   1)  environment variable $ABORT_ON_THROW is set (to any value), or
//   2)  registry value of ABORT_ON_THROW, section DEBUG is set (to any value)
#define ABORT_ON_THROW "ABORT_ON_THROW"
extern void SetThrowTraceAbort(bool abort_on_throw_trace);

// "abort()" the program if set by SetThrowTraceAbort() or $ABORT_ON_THROW
extern void DoThrowTraceAbort(void);

extern void DoDbgPrint(const char* file, int line, const char* message);
extern void DoDbgPrint(const char* file, int line, const string& message);
extern void DoDbgPrint(const char* file, int line,
                       const char* msg1, const char* msg2);

#if defined(_DEBUG)

template<typename T>
inline
const T& DbgPrint(const char* file, int line,
                  const T& e, const char* e_str)
{
    DoDbgPrint(file, line, e_str, e.what());
    return e;
}

inline
const char* DbgPrint(const char* file, int line,
                     const char* e, const char* )
{
    DoDbgPrint(file, line, e);
    return e;
}

inline
char* DbgPrint(const char* file, int line,
               char* e, const char* )
{
    DoDbgPrint(file, line, e);
    return e;
}

inline
const string& DbgPrint(const char* file, int line,
                       const string& e, const char* )
{
    DoDbgPrint(file, line, e);
    return e;
}

template<typename T>
inline
const T& DbgPrintP(const char* file, int line, const T& e, const char* e_str)
{
    CNcbiDiag(file, line, eDiag_Trace, eDPF_Trace) << e_str << ": " << e;
    DoThrowTraceAbort();
    return e;
}

template<typename T>
inline
const T& DbgPrintNP(const char* file, int line, const T& e, const char* e_str)
{
    DoDbgPrint(file, line, e_str);
    return e;
}

// Example:  RETHROW_TRACE;
#  define RETHROW_TRACE do { \
    _TRACE("EXCEPTION: re-throw"); \
    NCBI_NS_NCBI::DoThrowTraceAbort(); \
    throw; \
} while(0)

// Example:  THROW0_TRACE("Throw just a string");
// Example:  THROW0_TRACE(runtime_error("message"));
#  define THROW0_TRACE(exception_object) \
    throw NCBI_NS_NCBI::DbgPrint(__FILE__, __LINE__, \
        exception_object, #exception_object)

// Example:  THROW0p_TRACE(123);
// Example:  THROW0p_TRACE(complex(1,2));
#  define THROW0p_TRACE(exception_object) \
    throw NCBI_NS_NCBI::DbgPrintP(__FILE__, __LINE__, \
        exception_object, #exception_object)

// Example:  THROW0np_TRACE(vector<char>());
#  define THROW0np_TRACE(exception_object) \
    throw NCBI_NS_NCBI::DbgPrintNP(__FILE__, __LINE__, \
        exception_object, #exception_object)

// Example:  THROW1_TRACE(runtime_error, "Something is weird...");
#  define THROW1_TRACE(exception_class, exception_arg) \
    throw NCBI_NS_NCBI::DbgPrint(__FILE__, __LINE__, \
        exception_class(exception_arg), #exception_class)

// Example:  THROW1p_TRACE(int, 32);
#  define THROW1p_TRACE(exception_class, exception_arg) \
    throw NCBI_NS_NCBI::DbgPrintP(__FILE__, __LINE__, \
        exception_class(exception_arg), #exception_class)

// Example:  THROW1np_TRACE(CUserClass, "argument");
#  define THROW1np_TRACE(exception_class, exception_arg) \
    throw NCBI_NS_NCBI::DbgPrintNP(__FILE__, __LINE__, \
        exception_class(exception_arg), #exception_class)

// Example:  THROW_TRACE(bad_alloc, ());
// Example:  THROW_TRACE(runtime_error, ("Something is weird..."));
// Example:  THROW_TRACE(CParseException, ("Some parse error", 123));
#  define THROW_TRACE(exception_class, exception_args) \
    throw NCBI_NS_NCBI::DbgPrint(__FILE__, __LINE__, \
        exception_class exception_args, #exception_class)

// Example:  THROWp_TRACE(complex, (2, 3));
#  define THROWp_TRACE(exception_class, exception_args) \
    throw NCBI_NS_NCBI::DbgPrintP(__FILE__, __LINE__, \
        exception_class exception_args, #exception_class)

// Example:  THROWnp_TRACE(CUserClass, (arg1, arg2));
#  define THROWnp_TRACE(exception_class, exception_args) \
    throw NCBI_NS_NCBI::DbgPrintNP(__FILE__, __LINE__, \
        exception_class exception_args, #exception_class)

#else  /* _DEBUG */

#  define RETHROW_TRACE \
    throw
#  define THROW0_TRACE(exception_object) \
    throw exception_object
#  define THROW0p_TRACE(exception_object) \
    throw exception_object
#  define THROW0np_TRACE(exception_object) \
    throw exception_object
#  define THROW1_TRACE(exception_class, exception_arg) \
    throw exception_class(exception_arg)
#  define THROW1p_TRACE(exception_class, exception_arg) \
    throw exception_class(exception_arg)
#  define THROW1np_TRACE(exception_class, exception_arg) \
    throw exception_class(exception_arg)
#  define THROW_TRACE(exception_class, exception_args) \
    throw exception_class exception_args
#  define THROWp_TRACE(exception_class, exception_args) \
    throw exception_class exception_args
#  define THROWnp_TRACE(exception_class, exception_args) \
    throw exception_class exception_args

#endif  /* else!_DEBUG */


/////////////////////////////////////////////////////////////////////////////
// Standard handling of "exception"-derived exceptions

#define STD_CATCH(message) \
catch (NCBI_NS_STD::exception& e) { \
      NCBI_NS_NCBI::CNcbiDiag() << NCBI_NS_NCBI::Error << "[" << message << "]" \
           << "Exception: " << e.what(); \
}


/////////////////////////////////////////////////////////////////////////////
// Standard handling of "exception"-derived and all other exceptions
#define STD_CATCH_ALL(message) \
STD_CATCH(message) \
    catch (...) { \
      NCBI_NS_NCBI::CNcbiDiag() << NCBI_NS_NCBI::Error << "[" << message << "]" \
           << "Unknown exception"; \
}


/////////////////////////////////////////////////////////////////////////////
// Auxiliary exception classes:
//   CErrnoException
//   CParseException
//
class CErrnoException : public runtime_error {
    int m_Errno;
public:
    // Report description of "errno" along with "what"
    CErrnoException(const string& what) THROWS_NONE;
    ~CErrnoException(void) THROWS_NONE;
    int GetErrno(void) const THROWS_NONE { return m_Errno; }
};


class CParseException : public runtime_error {
    SIZE_TYPE m_Pos;
public:
    // Report "pos" along with "what"
    CParseException(const string& what, SIZE_TYPE pos) THROWS_NONE;
    ~CParseException(void) THROWS_NONE;
    SIZE_TYPE GetPos(void) const THROWS_NONE { return m_Pos; }
};

// (END_NCBI_SCOPE must be preceded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE

#endif  /* NCBIEXPT__HPP */
