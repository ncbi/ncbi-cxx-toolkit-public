
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
 *   provides dump interface (in the form [name=value]) for a client.
 *   
 *
 */

#include <corelib/ddumpable.hpp>

BEGIN_NCBI_SCOPE


CDebugDumpContext::CDebugDumpContext(
    CDebugDumpFormatter& formatter, const string& bundle)
    : m_Parent(*this), m_Formatter(formatter), m_Title(bundle)
{
    m_Level = 0;
    m_Start_Bundle = true;
    m_Started = false;
}

CDebugDumpContext::CDebugDumpContext(CDebugDumpContext& ddc)
    : m_Parent(ddc), m_Formatter(ddc.m_Formatter)
{
    m_Parent.x_VerifyFrameStarted();
    m_Level = m_Parent.m_Level+1;
    m_Start_Bundle = false;
    m_Started = false;
}

CDebugDumpContext::CDebugDumpContext(CDebugDumpContext& ddc,
    const string& bundle)
    : m_Parent(ddc), m_Formatter(ddc.m_Formatter), m_Title(bundle)
{
    m_Parent.x_VerifyFrameStarted();
    m_Level = m_Parent.m_Level+1;
    m_Start_Bundle = true;
    m_Started = false;
}

CDebugDumpContext::~CDebugDumpContext(void)
{
    if (&m_Parent == this) {
        return;
    }
    x_VerifyFrameStarted();
    x_VerifyFrameEnded();
    if (m_Level == 1) {
        m_Parent.x_VerifyFrameEnded();
    }
}


void CDebugDumpContext::SetFrame(const string& frame)
{
    if (m_Started) {
        return;
    }
    if (m_Start_Bundle) {
        m_Started = m_Formatter.StartBundle( m_Level, m_Title);
    } else {
        m_Title = frame;
        m_Started = m_Formatter.StartFrame( m_Level, m_Title);
    }
}


void CDebugDumpContext::Log(const string& name, const string& value,
    bool is_string, const string& comment)
{
    x_VerifyFrameStarted();
    if (m_Started) {
        m_Formatter.PutValue(m_Level, name, value, is_string, comment);
    }
}


void CDebugDumpContext::Log(const string& name, bool value,
    const string& comment)
{
    Log(name, NStr::BoolToString(value), false, comment);
}


void CDebugDumpContext::Log(const string& name, long value,
    const string& comment)
{
    Log(name, NStr::IntToString(value), false, comment);
}


void CDebugDumpContext::Log(const string& name, unsigned long value,
    const string& comment)
{
    Log(name, NStr::UIntToString(value), false, comment);
}


void CDebugDumpContext::Log(const string& name, double value,
    const string& comment)
{
    Log(name, NStr::DoubleToString(value), false, comment);
}


void CDebugDumpContext::Log(const string& name, const void* value,
    const string& comment)
{
    Log(name, NStr::PtrToString(value), false, comment);
}


void CDebugDumpContext::Log(const string& name, const CDebugDumpable* value,
    unsigned int depth)
{
    if ((depth != 0) && value) {
        CDebugDumpContext ddc(*this,name);
        value->DebugDump(ddc, depth-1);
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
    if (!m_Started) {
        return;
    }
    if (m_Start_Bundle) {
        m_Formatter.EndBundle(m_Level, m_Title);
    } else {
        m_Formatter.EndFrame(m_Level, m_Title);
    }
    m_Started = false;
 }


END_NCBI_SCOPE
/*
 * ===========================================================================
 *  $Log$
 *  Revision 1.1  2002/05/17 14:27:10  gouriano
 *  added DebugDump base class and function to CObject
 *
 *
 * ===========================================================================
*/
