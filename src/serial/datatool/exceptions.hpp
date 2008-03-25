#ifndef DATATOOL_EXCEPTIONS_HPP
#define DATATOOL_EXCEPTIONS_HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*   datatool exceptions
*
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiexpt.hpp>
#include <stdexcept>
#include <list>

BEGIN_NCBI_SCOPE

class CDataType;

/////////////////////////////////////////////////////////////////////////////
// CDatatoolException - datatool exceptions


class CDatatoolException : public CException
{
public:
    enum EErrCode {
        eNotImplemented,
        eWrongInput,
        eInvalidData,
        eIllegalCall,
        eForbidden
    };
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eNotImplemented: return "eNotImplemented";
        case eWrongInput:     return "eWrongInput";
        case eInvalidData:    return "eInvalidData";
        case eIllegalCall:    return "eIllegalCall";
        case eForbidden:      return "eForbidden";
        default:              return CException::GetErrCodeString();
        }
    }
    NCBI_EXCEPTION_DEFAULT(CDatatoolException,CException);
};

class CNotFoundException : public CDatatoolException
{
public:
    enum EErrCode {
        eType,
        eModule
    };
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eType:   return "eType";
        case eModule: return "eModule";
        default:      return CException::GetErrCodeString();
        }
    }
    NCBI_EXCEPTION_DEFAULT(CNotFoundException,CDatatoolException);
};

class CAmbiguiousTypes : public CNotFoundException
{
public:
    enum EErrCode {
        eAmbiguious
    };
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eAmbiguious:    return "eAmbiguious";
        default:       return CException::GetErrCodeString();
        }
    }

    CAmbiguiousTypes(const CDiagCompileInfo& info,
                     const CException* prev_exception,
                     EErrCode err_code, const string& message,
                     const list<CDataType*>& types, 
                     EDiagSev severity = eDiag_Error) THROWS_NONE
        : CNotFoundException(info, prev_exception,
            (CNotFoundException::EErrCode) CException::eInvalid,
            message), m_Types(types)
    NCBI_EXCEPTION_DEFAULT_IMPLEMENTATION(CAmbiguiousTypes, CNotFoundException);

public:
    const list<CDataType*>& GetTypes(void) const THROWS_NONE
    {
        return m_Types;
    }

private:
    list<CDataType*> m_Types;
};

class CResolvedTypeSet
{
public:
    CResolvedTypeSet(const string& name);
    CResolvedTypeSet(const string& module, const string& name);
    ~CResolvedTypeSet(void);

    void Add(CDataType* type);
    void Add(const CAmbiguiousTypes& types);

    CDataType* GetType(void) const THROWS((CDatatoolException));

private:
    string m_Module, m_Name;
    list<CDataType*> m_Types;
};

END_NCBI_SCOPE

#endif
