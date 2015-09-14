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

class LBOS
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
        e_ServerError,          /**< lbos failed on server side
                                 and returned error code
                                 and error message (possibly)          */
        e_DeannounceFail,       /**< Error while deannounce/           */
        e_NotFound,             /**< For deannouncement only. Did not 
                                     find such server to deannounce    */
        e_Off,  
    };

    /// Translate from the error code value to its string representation.   
    virtual const char* GetErrCodeString(void) const;

    // Standard exception boilerplate code.    
    NCBI_EXCEPTION_DEFAULT(CLBOSException, CException);
};

END_NCBI_SCOPE

#endif /* CONNECT___NCBI_LBOS__HPP */