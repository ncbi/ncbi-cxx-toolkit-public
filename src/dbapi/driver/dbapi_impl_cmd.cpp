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
* Author:  Sergey Sikorskiy
*
*/

#include <ncbi_pch.hpp>

#include <dbapi/driver/impl/dbapi_impl_cmd.hpp>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(impl)

CDB_Result* CCommand::Create_Result(CResult& result)
{
    return new CDB_Result(&result);
}

///////////////////////////////////////////////////////////////////////////
CBaseCmd::CBaseCmd(void)
{
}

CBaseCmd::~CBaseCmd(void)
{
    return;
}

///////////////////////////////////////////////////////////////////////////
CLangCmd::CLangCmd(void)
{
}

CLangCmd::~CLangCmd(void)
{
    return;
}

void CLangCmd::DetachInterface(void)
{
    m_Interface.DetachInterface();
}

void CLangCmd::AttachTo(CDB_LangCmd* interface)
{
    m_Interface = interface;
}

///////////////////////////////////////////////////////////////////////////
CRPCCmd::CRPCCmd(void)
{
}

CRPCCmd::~CRPCCmd(void)
{
    return;
}

void CRPCCmd::DetachInterface(void)
{
    m_Interface.DetachInterface();
}

void CRPCCmd::AttachTo(CDB_RPCCmd* interface)
{
    m_Interface = interface;
}

///////////////////////////////////////////////////////////////////////////
CBCPInCmd::CBCPInCmd(void)
{
    return;
}

CBCPInCmd::~CBCPInCmd(void)
{
    return;
}

void CBCPInCmd::DetachInterface(void)
{
    m_Interface.DetachInterface();
}

void CBCPInCmd::AttachTo(CDB_BCPInCmd* interface)
{
    m_Interface = interface;
}

///////////////////////////////////////////////////////////////////////////
CCursorCmd::CCursorCmd(void)
{
}

CCursorCmd::~CCursorCmd(void)
{
    return;
}

void CCursorCmd::DetachInterface(void)
{
    m_Interface.DetachInterface();
}

void CCursorCmd::AttachTo(CDB_CursorCmd* interface)
{
    m_Interface = interface;
}

///////////////////////////////////////////////////////////////////////////
CSendDataCmd::CSendDataCmd(void)
{
}

CSendDataCmd::~CSendDataCmd(void)
{
    return;
}

void CSendDataCmd::DetachInterface(void)
{
    m_Interface.DetachInterface();
}

void CSendDataCmd::AttachTo(CDB_SendDataCmd* interface)
{
    m_Interface = interface;
}

END_SCOPE(impl)


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2006/07/12 18:55:53  ssikorsk
 * Moved implementations of DetachInterface and AttachTo into cpp for MIPS sake.
 *
 * Revision 1.1  2006/07/12 16:29:30  ssikorsk
 * Separated interface and implementation of CDB classes.
 *
 * ===========================================================================
 */

