#ifndef CONNECT_SERVICES__SRV_CONNECTIONS_HPP
#define CONNECT_SERVICES__SRV_CONNECTIONS_HPP

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
 * Authors:  Maxim Didneko,
 *
 * File Description:  
 *
 */

#include <corelib/ncbimisc.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbistr.hpp>

#include <connect/connect_export.h>

#include <vector>

BEGIN_NCBI_SCOPE

void NCBI_XCONNECT_EXPORT DiscoverLBServices(const string& service_name, 
                                             vector<pair<string,unsigned int> >& services);

struct undefined_t {};

template<typename TDerived>
struct CServiceClientTraits {
    typedef undefined_t client_type;
};

template<typename TDerived>
struct Wrapper
{
    TDerived& derived()
    {
        return *static_cast<TDerived*>(this);
    }

    TDerived const& derived() const
    {
        return *static_cast<TDerived const*>(this);
    }
};

template<typename TDerived>
class CServiceConnections : public Wrapper<TDerived>
{
    typedef typename CServiceClientTraits<TDerived>::client_type client_t;
    typedef vector<client_t*> TConnections;
public:
    CServiceConnections(const string& service)
        : m_Service(service), m_Discovered(false)  {}

    ~CServiceConnections() 
    {
        ITERATE(typename TConnections, it, m_Connections) {
            delete *it;
        }
    }

    const string& GetService() const { return m_Service; }
    bool IsHostPortConfig() { x_DiscoverConnections(); return m_HostPort; }

    template<typename Func>
    void for_each(Func func) {
        typename TConnections::const_iterator it_b = GetCont().begin();
        typename TConnections::const_iterator it_e = GetCont().end();
        while( it_b != it_e ) {
            func( **it_b );
            ++it_b;
        }
    }

private:

    TConnections& GetCont() {
        x_DiscoverConnections();
        return m_Connections;
    }

    string m_Service;
    bool m_HostPort;
    bool m_Discovered;
    TConnections m_Connections;
    void x_DiscoverConnections()
    {
        if (m_Discovered) return;
        string sport, host;
        typedef vector<pair<string, unsigned int> > TSrvs;
        TSrvs srvs;
        if ( NStr::SplitInTwo(m_Service, ":", host, sport) ) {
            try {
                unsigned int port = NStr::StringToInt(sport);
                srvs.push_back(make_pair(host, port));
                m_HostPort = true;
            } catch (...) {
            }
        } else {
            DiscoverLBServices(m_Service, srvs);
            m_HostPort = false;
        }
        if (srvs.empty())
            NCBI_THROW(CCoreException, eInvalidArg, "\"" +m_Service+ "\" is not a valid service name");

        ITERATE(TSrvs, it, srvs) {
            m_Connections.push_back(this->derived().CreateClient(it->first, it->second));
        }
        m_Discovered = true;
    }
};

template<typename TClient>
class ICtrlCmd
{
public:
    explicit ICtrlCmd(CNcbiOstream& os) : m_Os(os) {}
    virtual ~ICtrlCmd () {}

    CNcbiOstream& GetStream() { return m_Os;};
    
    virtual void Execute(TClient& cln) = 0;

private:
    CNcbiOstream& m_Os;
};

template<typename TClient, typename TConnections>
class CCtrlCmdRunner 
{
public:
    CCtrlCmdRunner(TConnections& connections, ICtrlCmd<TClient>& cmd)
        : m_Connections(&connections), m_Cmd(&cmd) {}

    void operator()(TClient& cln) {
        if ( !m_Connections->IsHostPortConfig() ) 
            m_Cmd->GetStream() << m_Connections->GetService() 
                              << "(" << cln.GetHost() << ":" << cln.GetPort() << "): ";
        m_Cmd->Execute(cln);
    }

private:
    TConnections* m_Connections;
    ICtrlCmd<TClient>* m_Cmd;
};


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2006/12/06 15:00:00  didenko
 * Added service connections template classes
 *
 * ===========================================================================
 */


#endif // CONNECT_SERVICES__SRV_CONNECTIONS_HPP
