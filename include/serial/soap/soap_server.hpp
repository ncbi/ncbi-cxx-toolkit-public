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

#ifndef SOAP_SERVER_HPP
#define SOAP_SERVER_HPP

#include <cgi/cgiapp.hpp>
#include <cgi/cgictx.hpp>
#include <serial/typeinfo.hpp>
#include <serial/soap/soap_message.hpp>

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
//  CSoapServerApplication
//
#if defined(NCBI_COMPILER_GCC)
#  if NCBI_COMPILER_VERSION < 300
#    define SOAPSERVER_INTERNALSTORAGE
#  endif
#endif

class CSoapServerApplication : public CCgiApplication
{
public:
    typedef bool (CSoapServerApplication::*TWebMethod)(
        CSoapMessage& response, const CSoapMessage& request);

#if defined(SOAPSERVER_INTERNALSTORAGE)
// 'vector<TWebMethod>' looks simple,
// but I could not make it compile on GCC 2.95
    class Storage
    {
    public:
        Storage(void);
        Storage(const Storage& src);
        ~Storage(void);
        typedef const TWebMethod* const_iterator;
        const_iterator begin(void) const;
        const_iterator end(void) const;
        void push_back(TWebMethod value);
    private:
        TWebMethod* m_Buffer;
        size_t m_Capacity;
        size_t m_Current;
    };
    typedef Storage TListeners;
#else
    typedef vector<TWebMethod> TListeners;
#endif

    CSoapServerApplication(const string& wsdl_filename,
                           const string& namespace_name);
    virtual int  ProcessRequest(CCgiContext& ctx);

    void SetDefaultNamespaceName(const string& namespace_name);
    const string& GetDefaultNamespaceName(void) const;
    void SetWsdlFilename(const string& wsdl_filename);

    void RegisterObjectType(TTypeInfoGetter type_getter);
    void AddMessageListener(TWebMethod listener,
                            const string& message_name,
                            const string& namespace_name = kEmptyStr);

    TDiagPostFlags GetFaultPostFlags(void) const { return m_FaultPostFlags; }
    void SetFaultPostFlags(TDiagPostFlags flags) { m_FaultPostFlags = flags; }

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
    bool x_ProcessWsdlRequest(CCgiResponse& response,
                              const CCgiRequest& request) const;
    bool x_ProcessSoapRequest(CCgiResponse& response,
                              const CCgiRequest& request);

    const TListeners* x_FindListeners(const CSoapMessage& request);
    TListeners* x_FindListenersByName(const string& message_name,
                                      const string& namespace_name);

    void x_FaultVersionMismatch(CSoapMessage& response) const;
    void x_FaultMustUnderstand(CSoapMessage& response) const;
    void x_FaultServer(CSoapMessage& response, const string& text) const;
    void x_FaultNoListeners(CSoapMessage& response) const;

    string m_DefNamespace;
    string m_Wsdl;
    vector< TTypeInfoGetter >  m_Types;
    multimap<string, pair<string, TListeners > > m_Listeners;
    bool m_OmitScopePrefixes;
    TDiagPostFlags m_FaultPostFlags;
};

END_NCBI_SCOPE

#endif // SOAP_SERVER_HPP
