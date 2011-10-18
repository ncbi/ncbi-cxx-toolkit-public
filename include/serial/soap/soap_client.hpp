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
 * Author:  Andrei Gourianov
 *
 * File Description:
 *   SOAP http client
 *
 */

#ifndef SOAP_CLIENT_HPP
#define SOAP_CLIENT_HPP

#include <corelib/ncbiobj.hpp>
#include <serial/typeinfo.hpp>
#include <serial/soap/soap_message.hpp>

BEGIN_NCBI_SCOPE

class CSoapHttpClient : public CObject
{
public:
    CSoapHttpClient(const string& server_url,
                    const string& namespace_name);
    ~CSoapHttpClient(void);

    void SetServerUrl(const string& server_url);
    const string& GetServerUrl(void) const;
    void SetDefaultNamespaceName(const string& namespace_name);
    const string& GetDefaultNamespaceName(void) const;

    void RegisterObjectType(TTypeInfoGetter type_getter);
    void Invoke(CSoapMessage& response, const CSoapMessage& request,
                CConstRef<CSoapFault>* fault=0,
                const string& soap_action = kEmptyStr) const;

protected:
    // These methods exist to provide compatibility with data object classes
    // generated from ASN.1 specification.
    // If you generate data object classes from Schema, you do not need them.
    void SetOmitScopePrefixes(bool bOmit);
    bool GetOmitScopePrefixes() const
    {
        return m_OmitScopePrefixes;
    }

private:
    string m_ServerUrl;
    string m_DefNamespace;
    vector< TTypeInfoGetter >  m_Types;
    bool m_OmitScopePrefixes;
};



END_NCBI_SCOPE

#endif // SOAP_CLIENT_HPP
