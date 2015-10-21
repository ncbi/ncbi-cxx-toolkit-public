#ifndef CONNECT___NCBI_LBOS__HPP
#define CONNECT___NCBI_LBOS__HPP
/*
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
* Authors:  Dmitriy Elisov
* @file
* File Description:
*   A C++ wrapper for service discovery API based on lbos. 
*   lbos is an adapter for ZooKeeper cloud-based DB. 
*   lbos allows to announce, deannounce and resolve services.
*/

#include <corelib/ncbiexpt.hpp>

BEGIN_NCBI_SCOPE

class NCBI_XNCBI_EXPORT LBOS
{
public:
    static void Announce(const string&    service,
                         const string&    version,
                         unsigned short   port,
                         const string&    healthcheck_url);
                                                          
    static void AnnounceFromRegistry(const string&  registry_section);

    static void DeannounceAll(void);

    static void Deannounce(const string&  service,
                           const string&  version,
                           const string&  host,
                           unsigned short port);
};


class NCBI_XNCBI_EXPORT CLBOSException : public CException
{
public:
    enum EErrCode {
        e_NoLBOS,               /**< lbos was not found                */
        e_DNSResolveError,      /**< Local address not resolved        */
        e_InvalidArgs,          /**< Arguments not valid               */
        e_DeannounceFail,       /**< Error while deannounce/           */
        e_NotFound,             /**< For deannouncement only. Did not
                                     find such server to deannounce    */
        e_Off,
        e_MemAllocError,
        e_LBOSCorruptOutput,
        e_BadRequest,
        e_Unknown               /**< No information about this error 
                                     code meaning                      */
    };

    CLBOSException(const CDiagCompileInfo& info,
                   const CException* prev_exception, EErrCode err_code,
                   const string& message, unsigned short status_code,
                   EDiagSev severity = eDiag_Error)
               : CException(info, prev_exception, (message), severity, 0)
    {
        
        x_Init(info, message, prev_exception, severity);                
        x_InitErrCode((CException::EErrCode) err_code);
        m_StatusCode = status_code;
    }

    CLBOSException(const CDiagCompileInfo& info, 
                   const CException* prev_exception, 
                   const CExceptionArgs<EErrCode>& args, 
                   const string& message,
                   unsigned short status_code)
        : CException(info, prev_exception, (message), 
        args.GetSeverity(), 0)                                        
    {                                                                   
        x_Init(info, message, prev_exception, args.GetSeverity());      
        x_InitArgs(args);
        x_InitErrCode((CException::EErrCode) args.GetErrCode());
        m_StatusCode = status_code;
    }


    /// Translate from the error code value to its string representation.   
    virtual const char* GetErrCodeString(void) const;

    /**Translate from numerical HTTP status code to lbos-specific
     * error code */
    static EErrCode s_HTTPCodeToEnum(unsigned short http_code);

    unsigned short GetStatusCode(void) const;
    virtual ~CLBOSException(void) throw() {}
    virtual const char* GetType(void) const { return "CLBOSException"; }
    typedef int TErrCode;
    TErrCode GetErrCode(void) const
    {
        return typeid(*this) == typeid(CLBOSException) ?
            (TErrCode) this->x_GetErrCode() :
            (TErrCode)CException::eInvalid;
    }
    NCBI_EXCEPTION_DEFAULT_THROW(CLBOSException)
protected:
    CLBOSException(void) {}
    virtual const CException* x_Clone(void) const
    {
        return new CLBOSException(*this);
    }
private:
    unsigned short m_StatusCode;
};

END_NCBI_SCOPE

#endif /* CONNECT___NCBI_LBOS__HPP */