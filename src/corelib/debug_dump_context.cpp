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
 *  Please say you saw it at NCBI
 *
 * ===========================================================================
 *
 * Author:  Andrei Gourianov
 *
 * File Description:
 *   Base class for debug dumping of objects -
 *   provides only the interface (in the form [name=value]) for a client.
 *   A subclass should provide dump formatting.
 *   
 *
 */

#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE


CDebugDumpContext::CDebugDumpContext(const string& bundle)
    : m_Formatter(*this), m_Title(bundle)
{
    m_Level = 0;
    m_Start_Bundle = true;
    m_Started = false;
}

CDebugDumpContext::CDebugDumpContext(CDebugDumpContext& ddc)
    : m_Formatter(ddc.m_Formatter)
{
    ddc.x_VerifyFrameStarted();
    m_Level = ddc.m_Level+1;
    m_Start_Bundle = false;
    m_Started = false;
}

CDebugDumpContext::CDebugDumpContext(CDebugDumpContext& ddc,
    const string& bundle)
    : m_Formatter(ddc.m_Formatter), m_Title(bundle)
{
    ddc.x_VerifyFrameStarted();
    m_Level = ddc.m_Level+1;
    m_Start_Bundle = true;
    m_Started = false;
}

CDebugDumpContext::~CDebugDumpContext(void)
{
    // never call its own virtual functions from dtor
    if (&m_Formatter != this) {
        x_VerifyFrameStarted();
        x_VerifyFrameEnded();
        if (m_Level == 1) {
            m_Formatter.x_VerifyFrameEnded();
        }
    }
}


void CDebugDumpContext::SetFrame(const string& frame)
{
    if (!m_Started) {
        if (m_Start_Bundle) {
            m_Started = m_Formatter.StartBundle( m_Level, m_Title);
        } else {
            m_Title = frame;
            m_Started = m_Formatter.StartFrame( m_Level, m_Title);
        }
    }
}


void CDebugDumpContext::Log(const string& name, const string& value,
    const string& comment)
{
    x_VerifyFrameStarted();
    if (m_Started) {
        m_Formatter.PutValue(m_Level, m_Title, name, value, comment);
    }
}


void CDebugDumpContext::Log(const string& name, bool value,
    const string& comment)
{
    Log(name, NStr::BoolToString(value), comment);
}


void CDebugDumpContext::Log(const string& name, long value,
    const string& comment)
{
    Log(name, NStr::IntToString(value,true), comment);
}


void CDebugDumpContext::Log(const string& name, unsigned long value,
    const string& comment)
{
    Log(name, NStr::UIntToString(value), comment);
}


void CDebugDumpContext::Log(const string& name, double value,
    const string& comment)
{
    Log(name, NStr::DoubleToString(value), comment);
}


void CDebugDumpContext::Log(const string& name, const void* value,
    const string& comment)
{
    Log(name, NStr::PtrToString(value), comment);
}


void CDebugDumpContext::Log(const string& name, const CObject* value,
    unsigned int depth)
{
    if ((depth != 0) && value) {
        CObject::DebugDump(*value, CDebugDumpContext(*this,name), depth-1);
    } else {
        Log(name, dynamic_cast<const void*>(value));
    }
}

void CDebugDumpContext::x_VerifyFrameStarted(void)
{
    SetFrame(m_Title);
}

void CDebugDumpContext::x_VerifyFrameEnded(void)
{
    if (m_Started) {
        if (m_Start_Bundle) {
            m_Formatter.EndBundle(m_Level, m_Title);
        } else {
            m_Formatter.EndFrame(m_Level, m_Title);
        }
    }
    m_Started = false;
 }


bool CDebugDumpContext::StartBundle(
    unsigned int /*level*/, const string& /*title*/)
{
    return true;
}

void CDebugDumpContext::EndBundle(
    unsigned int /*level*/, const string& /*title*/)
{
}

bool CDebugDumpContext::StartFrame(
    unsigned int /*level*/, const string& /*frame*/)
{
    return true;
}

void CDebugDumpContext::EndFrame(
    unsigned int /*level*/, const string& /*frame*/)
{
}

void CDebugDumpContext::PutValue(
    unsigned int /*level*/, const string& /*frame*/,
    const string& /*name*/, const string& /*value*/,
    const string& /*comment*/)
{
}

END_NCBI_SCOPE
/*
 * ===========================================================================
 *  $Log$
 *  Revision 1.1  2002/05/14 14:44:24  gouriano
 *  added DebugDump function to CObject
 *
 * ===========================================================================
*/
