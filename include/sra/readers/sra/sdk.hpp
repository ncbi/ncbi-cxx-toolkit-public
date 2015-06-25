#ifndef SRA__READER__SRA__SDK__HPP
#define SRA__READER__SRA__SDK__HPP
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
 * Authors:  Eugene Vasilchenko
 *
 * File Description:
 *   Access to SRA SDK
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbiobj.hpp>

#include <klib/rc.h>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;

/////////////////////////////////////////////////////////////////////////////
//  CSraException
/////////////////////////////////////////////////////////////////////////////

#ifndef NCBI_EXCEPTION3_VAR
/// Create an instance of the exception with one additional parameter.
# define NCBI_EXCEPTION3_VAR(name, exc_cls, err_code, msg, extra1, extra2) \
    exc_cls name(DIAG_COMPILE_INFO, 0, exc_cls::err_code, msg,          \
                 extra1, extra2)
#endif

#ifndef NCBI_EXCEPTION3
# define NCBI_EXCEPTION3(exc_cls, err_code, msg, extra1, extra2)        \
    NCBI_EXCEPTION3_VAR(NCBI_EXCEPTION_EMPTY_NAME,                      \
                        exc_cls, err_code, msg, extra1, extra2)
#endif

#ifndef NCBI_THROW3
# define NCBI_THROW3(exc_cls, err_code, msg, extra1, extra2)            \
    throw NCBI_EXCEPTION3(exc_cls, err_code, msg, extra1, extra2)
#endif

#ifndef NCBI_RETHROW3
# define NCBI_RETHROW3(prev_exc, exc_cls, err_code, msg, extra1, extra2) \
    throw exc_cls(DIAG_COMPILE_INFO, &(prev_exc), exc_cls::err_code, msg, \
                  extra1, extra2)
#endif


class CSraRcFormatter
{
public:
    explicit CSraRcFormatter(rc_t rc) : m_RC(rc) {}

    rc_t GetRC(void) const
        {
            return m_RC;
        }
private:
    rc_t m_RC;
};
NCBI_SRAREAD_EXPORT
ostream& operator<<(ostream& out, const CSraRcFormatter& f);

class NCBI_SRAREAD_EXPORT CSraException
    : EXCEPTION_VIRTUAL_BASE public CException
{
public:
    /// Error types that  corelib can generate.
    ///
    /// These generic error conditions can occur for corelib applications.
    enum EErrCode {
        eOtherError,
        eNullPtr,       ///< Null pointer error
        eAddRefFailed,  ///< AddRef failed
        eInvalidArg,    ///< Invalid argument error
        eInitFailed,    ///< Initialization failed
        eNotFound,      ///< Data not found
        eInvalidState,  ///< State of object is invalid for the operation
        eInvalidIndex,  ///< Invalid index for array-like retrieval
        eNotFoundDb,    ///< DB main file not found
        eNotFoundTable, ///< DB table not found
        eNotFoundColumn,///< DB column not found
        eNotFoundValue, ///< DB value not found
        eDataError,     ///< VDB data is incorrect
        eNotFoundIndex  ///< VDB index not found
    };
    /// Constructors.
    CSraException(const CDiagCompileInfo& info,
                  const CException* prev_exception,
                  EErrCode err_code,
                  const string& message,
                  EDiagSev severity = eDiag_Error);
    CSraException(const CDiagCompileInfo& info,
                  const CException* prev_exception,
                  EErrCode err_code,
                  const string& message,
                  rc_t rc,
                  EDiagSev severity = eDiag_Error);
    CSraException(const CDiagCompileInfo& info,
                  const CException* prev_exception,
                  EErrCode err_code,
                  const string& message,
                  rc_t rc,
                  const string& param,
                  EDiagSev severity = eDiag_Error);
    CSraException(const CDiagCompileInfo& info,
                  const CException* prev_exception,
                  EErrCode err_code,
                  const string& message,
                  rc_t rc,
                  uint64_t param,
                  EDiagSev severity = eDiag_Error);
    CSraException(const CSraException& other);

    ~CSraException(void) throw();

    /// Report "non-standard" attributes.
    virtual void ReportExtra(ostream& out) const;

    virtual const char* GetType(void) const;

    /// Translate from the error code value to its string representation.
    typedef int TErrCode;
    virtual TErrCode GetErrCode(void) const;

    /// Translate from the error code value to its string representation.
    virtual const char* GetErrCodeString(void) const;

    rc_t GetRC(void) const
        {
            return m_RC;
        }

    const string& GetParam(void) const
        {
            return m_Param;
        }
    void SetParam(const string& param)
        {
            m_Param = param;
        }

    static void ReportError(const char* msg, rc_t rc);

protected:
    /// Constructor.
    CSraException(void);

    /// Helper clone method.
    virtual const CException* x_Clone(void) const;

private:
    rc_t   m_RC;
    string m_Param;
};


template<class Object>
struct CSraRefTraits
{
};

#define DECLARE_SRA_REF_TRAITS(T, Const)                                \
    template<>                                                          \
    struct CSraRefTraits<Const T>                                       \
    {                                                                   \
        static rc_t x_Release(const T* t);                              \
        static rc_t x_AddRef (const T* t);                              \
    }
#define DEFINE_SRA_REF_TRAITS(T, Const)                                 \
    rc_t CSraRefTraits<Const T>::x_Release(const T* t)                  \
    { return T##Release(t); }                                           \
    rc_t CSraRefTraits<Const T>::x_AddRef (const T* t)                  \
    { return T##AddRef(t); }


template<class Object>
class CSraRef
{
protected:
    typedef CSraRef<Object> TSelf;
    typedef CSraRefTraits<Object> TTraits;
public:
    typedef Object TObject;

    CSraRef(void)
        : m_Object(0)
        {
        }
    CSraRef(const TSelf& ref)
        : m_Object(s_AddRef(ref))
        {
        }
    TSelf& operator=(const TSelf& ref)
        {
            if ( this != &ref ) {
                Release();
                m_Object = s_AddRef(ref);
            }
            return *this;
        }
    ~CSraRef(void)
        {
            Release();
        }

    void Release(void)
        {
            if ( m_Object ) {
                if ( rc_t rc = TTraits::x_Release(m_Object) ) {
                    CSraException::ReportError("Cannot release ref", rc);
                }
                m_Object = 0;
            }
        }
    
    TObject* GetPointer(void) const
        {
            if ( !m_Object ) {
                NCBI_THROW(CSraException, eNullPtr,
                           "Null SRA pointer");
            }
            return m_Object;
        }

    operator TObject*(void) const
        {
            return m_Object;
        }
    TObject* operator->(void) const
        {
            return GetPointer();
        }
    TObject& operator*(void) const
        {
            return *GetPointer();
        }

protected:
    TObject** x_InitPtr(void)
        {
            Release();
            return &m_Object;
        }

protected:
    static TObject* s_AddRef(const TSelf& ref)
        {
            TObject* obj = ref.m_Object;
            if ( obj ) {
                if ( rc_t rc = TTraits::x_AddRef(obj) ) {
                    NCBI_THROW2(CSraException, eAddRefFailed,
                                "Cannot add ref", rc);
                }
            }
            return obj;
        }

private:
    TObject* m_Object;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // SRA__READER__SRA__SDK__HPP
