
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
 *   SOAP server application class
 *
 */

#include <cgi/cgiapp.hpp>
#include <cgi/cgictx.hpp>
#include <serial/soap/soap_message.hpp>

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
//  CSoapServerApplication
//

class CSoapServerApplication;
typedef bool (CSoapServerApplication::*TSoapServerCallback)(
    CSoapMessage& response, const CSoapMessage& request);

class CSoapServerApplication : public CCgiApplication
{
public:
    typedef vector<TSoapServerCallback*> TListeners;

    CSoapServerApplication(const string& wsdl_filename,
                           const string& namespace_name);
    virtual int  ProcessRequest(CCgiContext& ctx);

    void SetDefaultNamespaceName(const string& namespace_name);
    const string& GetDefaultNamespaceName(void) const;
    void SetWsdlFilename(const string& wsdl_filename);

    void RegisterObjectType(TTypeInfoGetter type_getter);
    void AddMessageListener(TSoapServerCallback* listener,
                            const string& message_name,
                            const string& namespace_name = kEmptyStr);

private:
    bool x_ProcessWsdlRequest(CCgiResponse& response,
                              const CCgiRequest& request);
    bool x_ProcessSoapRequest(CCgiResponse& response,
                              const CCgiRequest& request);

    const TListeners* x_FindListeners(const CSoapMessage& request);
    TListeners* x_FindListenersByName(const string& message_name,
                                      const string& namespace_name);

    string m_DefNamespace;
    string m_Wsdl;
    vector< TTypeInfoGetter >  m_Types;
    multimap<string, pair<string, TListeners > > m_Listeners;
};

END_NCBI_SCOPE
