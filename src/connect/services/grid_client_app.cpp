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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Maxim Didenko
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <connect/services/grid_client.hpp>
#include <connect/services/grid_client_app.hpp>
#include <connect/services/grid_default_factories.hpp>


BEGIN_NCBI_SCOPE


CGridClientApp::CGridClientApp(CNetScheduleClient* ns_client, 
                               INetScheduleStorage* storage)
: m_NSClient(ns_client), m_NSStorage(storage)
{
}

CGridClientApp::~CGridClientApp()
{
}

void CGridClientApp::Init(void)
{
    CNcbiApplication::Init();
    if (!m_NSClient.get()) {
        CNetScheduleClientFactory cf(GetConfig());
        m_NSClient.reset(cf.CreateInstance());
        m_NSClient->SetProgramVersion(GetProgramVersion());
    }
    if( !m_NSStorage.get()) {
        CNetScheduleStorageFactory_NetCache cf(GetConfig());
        m_NSStorage.reset(cf.CreateInstance());
    }
    m_GridClient.reset(new CGridClient(*m_NSClient, *m_NSStorage));
}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2005/04/07 16:47:03  didenko
 * + Program Version checking
 *
 * Revision 1.1  2005/03/25 16:25:41  didenko
 * Initial version
 *
 * ===========================================================================
 */
 
