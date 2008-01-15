/* $Id$
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
 *  ===========================================================================
 *
 * Author:  Vladimir Soussov
 *
 * File Description:  Data Server public interfaces
 *
 */

#include <ncbi_pch.hpp>

#include <dbapi/driver/interfaces.hpp>


BEGIN_NCBI_SCOPE


////////////////////////////////////////////////////////////////////////////
// C_ITDescriptorGuard
C_ITDescriptorGuard::C_ITDescriptorGuard(I_ITDescriptor* d)
{
    m_D = d;
}

C_ITDescriptorGuard::~C_ITDescriptorGuard(void)
{
    if ( m_D )
        delete m_D;
}

////////////////////////////////////////////////////////////////////////////
//  I_ITDescriptor::
//

I_ITDescriptor::~I_ITDescriptor(void)
{
    return;
}


////////////////////////////////////////////////////////////////////////////
//  I_BaseCmd::
//  I_LangCmd::
//  I_RPCCmd::
//  I_BCPInCmd::
//  I_CursorCmd::
//  I_SendDataCmd::
//

I_BaseCmd::I_BaseCmd(void)
{
}

I_BaseCmd::~I_BaseCmd(void)
{
    return;
}

I_LangCmd::I_LangCmd(void)
{
}

I_LangCmd::~I_LangCmd(void)
{
    return;
}

I_RPCCmd::I_RPCCmd(void)
{
}

I_RPCCmd::~I_RPCCmd(void)
{
    return;
}

I_BCPInCmd::I_BCPInCmd(void)
{
    return;
}

I_BCPInCmd::~I_BCPInCmd(void)
{
    return;
}

I_CursorCmd::I_CursorCmd(void)
{
}

I_CursorCmd::~I_CursorCmd(void)
{
    return;
}

I_SendDataCmd::I_SendDataCmd(void)
{
}

I_SendDataCmd::~I_SendDataCmd(void)
{
    return;
}


////////////////////////////////////////////////////////////////////////////
//  I_Result::
//

I_Result::I_Result(void)
{
}

I_Result::~I_Result(void)
{
    return;
}


////////////////////////////////////////////////////////////////////////////
//  I_DriverContext::SConnAttr::
//

I_DriverContext::SConnAttr::SConnAttr(void)
: mode(fDoNotConnect)
{
}

////////////////////////////////////////////////////////////////////////////
//  I_DriverContext::
//

I_DriverContext::I_DriverContext(void) :
    m_LoginTimeout(0),
    m_Timeout(0)
{
    return;
}

I_DriverContext::~I_DriverContext(void)
{
    return;
}

void
I_DriverContext::SetApplicationName(const string& app_name)
{
    m_AppName = app_name;
}

const string&
I_DriverContext::GetApplicationName(void) const
{
    return m_AppName;
}

void
I_DriverContext::SetHostName(const string& host_name)
{
    m_HostName = host_name;
}

const string&
I_DriverContext::GetHostName(void) const
{
    return m_HostName;
}

bool I_DriverContext::SetLoginTimeout (unsigned int nof_secs)
{
    m_LoginTimeout = nof_secs;

    return true;
}

bool I_DriverContext::SetTimeout      (unsigned int nof_secs)
{
    m_Timeout = nof_secs;

    return true;
}

////////////////////////////////////////////////////////////////////////////
I_Connection::I_Connection(void)
{
}

I_Connection::~I_Connection(void)
{
    return;
}

END_NCBI_SCOPE


