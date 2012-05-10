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
 *      Debug Dump functionality
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ddumpable.hpp>

BEGIN_NCBI_SCOPE


//---------------------------------------------------------------------------
//  CDebugDumpable defines DebugDump() functionality (abstract base class)

bool CDebugDumpable::sm_DumpEnabled = true;

CDebugDumpable::~CDebugDumpable(void)
{
    return;
}


void CDebugDumpable::EnableDebugDump(bool on)
{
    sm_DumpEnabled = on;
}


void CDebugDumpable::DebugDumpText(ostream&      out,
                                   const string& bundle,
                                   unsigned int  depth)
    const
{
    if ( sm_DumpEnabled ) {
        CDebugDumpFormatterText ddf(out);
        DebugDumpFormat(ddf, bundle, depth);
    }
}


void CDebugDumpable::DebugDumpFormat(CDebugDumpFormatter& ddf,
                                     const string&        bundle,
                                     unsigned int         depth)
    const
{
    if ( sm_DumpEnabled ) {
        CDebugDumpContext ddc(ddf, bundle);
        DebugDump(ddc, depth);
    }
}


//---------------------------------------------------------------------------
//  CDebugDumpContext provides client interface in the form [name=value]

CDebugDumpContext::CDebugDumpContext(CDebugDumpFormatter& formatter,
                                     const string&        bundle)
    : m_Parent(*this),
      m_Formatter(formatter),
      m_Title(bundle)
{
    m_Level        = 0;
    m_Start_Bundle = true;
    m_Started      = false;
}


CDebugDumpContext::CDebugDumpContext(CDebugDumpContext& ddc)
    : m_Parent(ddc),
      m_Formatter(ddc.m_Formatter)
{
    m_Parent.x_VerifyFrameStarted();
    m_Level        = m_Parent.m_Level + 1;
    m_Start_Bundle = false;
    m_Started      = false;
}


CDebugDumpContext::CDebugDumpContext(CDebugDumpContext& ddc,
                                     const string&      bundle)
    : m_Parent(ddc),
      m_Formatter(ddc.m_Formatter),
      m_Title(bundle)
{
    m_Parent.x_VerifyFrameStarted();
    m_Level        = m_Parent.m_Level + 1;
    m_Start_Bundle = true;
    m_Started      = false;
}


CDebugDumpContext::~CDebugDumpContext(void)
{
    if (&m_Parent == this)
        return;

    x_VerifyFrameStarted();
    x_VerifyFrameEnded();
    if (m_Level == 1) {
        m_Parent.x_VerifyFrameEnded();
    }
}


void CDebugDumpContext::SetFrame(const string& frame)
{
    if ( m_Started )
        return;

    if (m_Start_Bundle) {
        m_Started = m_Formatter.StartBundle(m_Level, m_Title);
    } else {
        m_Title   = frame;
        m_Started = m_Formatter.StartFrame(m_Level, m_Title);
    }
}


void CDebugDumpContext::Log(const string& name,
                            const char* value, 
                            CDebugDumpFormatter::EValueType type,
                            const string& comment)
{
    Log(name,value ? string(value) : kEmptyStr,type,comment);
}


void CDebugDumpContext::Log(const string&                   name,
                            const string&                   value,
                            CDebugDumpFormatter::EValueType type,
                            const string&                   comment)
{
    x_VerifyFrameStarted();
    if ( m_Started ) {
        m_Formatter.PutValue(m_Level, name, value, type, comment);
    }
}


void CDebugDumpContext::Log(const string& name, bool value,
                            const string& comment)
{
    Log(name, NStr::BoolToString(value), CDebugDumpFormatter::eValue, comment);
}


void CDebugDumpContext::Log(const string& name, short value,
                            const string& comment)
{
    Log(name, NStr::IntToString(value), CDebugDumpFormatter::eValue, comment);
}


void CDebugDumpContext::Log(const string& name, unsigned short value,
                            const string& comment)
{
    Log(name, NStr::UIntToString((unsigned int)value), CDebugDumpFormatter::eValue, comment);
}


void CDebugDumpContext::Log(const string& name, int value,
                            const string& comment)
{
    Log(name, NStr::IntToString(value), CDebugDumpFormatter::eValue, comment);
}


void CDebugDumpContext::Log(const string& name, unsigned int value,
                            const string& comment)
{
    Log(name, NStr::UIntToString(value), CDebugDumpFormatter::eValue, comment);
}


void CDebugDumpContext::Log(const string& name, long value,
                            const string& comment)
{
    Log(name, NStr::LongToString(value), CDebugDumpFormatter::eValue, comment);
}


void CDebugDumpContext::Log(const string& name, unsigned long value,
                            const string& comment)
{
    Log(name, NStr::ULongToString(value), CDebugDumpFormatter::eValue, comment);
}

#ifndef NCBI_INT8_IS_LONG
void CDebugDumpContext::Log(const string& name, Int8 value,
                            const string& comment)
{
    Log(name, NStr::Int8ToString(value), CDebugDumpFormatter::eValue, comment);
}


void CDebugDumpContext::Log(const string& name, Uint8 value,
                            const string& comment)
{
    Log(name, NStr::UInt8ToString(value), CDebugDumpFormatter::eValue, comment);
}
#endif


void CDebugDumpContext::Log(const string& name, double value,
                            const string& comment)
{
    Log(name, NStr::DoubleToString(value), CDebugDumpFormatter::eValue,
        comment);
}


void CDebugDumpContext::Log(const string& name, const void* value,
                            const string& comment)
{
    Log(name, NStr::PtrToString(value), CDebugDumpFormatter::eValue, comment);
}


void CDebugDumpContext::Log(const string& name, const CDebugDumpable* value,
                            unsigned int depth)
{
    if (depth != 0  &&  value) {
        CDebugDumpContext ddc(*this, name);
        value->DebugDump(ddc, depth - 1);
    } else {
        Log(name, NStr::PtrToString(static_cast<const void*> (value)),
            CDebugDumpFormatter::ePointer);
    }
}


void CDebugDumpContext::x_VerifyFrameStarted(void)
{
    SetFrame(m_Title);
}


void CDebugDumpContext::x_VerifyFrameEnded(void)
{
    if ( !m_Started )
        return;

    if (m_Start_Bundle) {
        m_Formatter.EndBundle(m_Level, m_Title);
    } else {
        m_Formatter.EndFrame(m_Level, m_Title);
    }

    m_Started = false;
}


//---------------------------------------------------------------------------
//  CDebugDumpFormatterText defines text debug dump formatter class

CDebugDumpFormatterText::CDebugDumpFormatterText(ostream& out)
    : m_Out(out)
{
}

CDebugDumpFormatterText::~CDebugDumpFormatterText(void)
{
}

bool CDebugDumpFormatterText::StartBundle(unsigned int  level,
                                          const string& bundle)
{
    if (level == 0) {
        x_InsertPageBreak(bundle);
    } else {
        m_Out << endl;
        x_IndentLine(level);
        m_Out << (bundle.empty() ? "?" : bundle.c_str()) << " = {";
    }
    return true;
}


void CDebugDumpFormatterText::EndBundle(unsigned int  level,
                                        const string& /*bundle*/)
{
    if (level == 0) {
        x_InsertPageBreak();
        m_Out << endl;
    } else {
        m_Out << endl;
        x_IndentLine(level);
        m_Out << "}";
    }
}


bool CDebugDumpFormatterText::StartFrame(unsigned int  level,
                                         const string& frame)
{
    m_Out << endl;
    x_IndentLine(level);
    m_Out << (frame.empty() ? "?" : frame.c_str()) << " {";
    return true;
}


void CDebugDumpFormatterText::EndFrame(unsigned int  /*level*/,
                                       const string& /*frame*/)
{
    m_Out << " }";
}


void CDebugDumpFormatterText::PutValue(unsigned int level,
                                       const string& name, const string& value,
                                       EValueType type, const string& comment)
{
    m_Out << endl;
    x_IndentLine(level + 1);

    m_Out << name << " = " ;
    if (type == eString) {
        m_Out << '"' << value << '"';
    } else {
        m_Out << value;
    }

    if ( !comment.empty() ) {
        m_Out << " (" << comment << ")";
    }
}


void CDebugDumpFormatterText::x_IndentLine(unsigned int level, char c,
                                           unsigned int len)
{
    m_Out << string(level * len, c);
}


void CDebugDumpFormatterText::x_InsertPageBreak(const string& title, char c,
                                                unsigned int len)
{
    m_Out << endl;

    string tmp;
    if ( !title.empty() ) {
        if (len < title.length() + 2) {
            tmp = title;
        } else {
            size_t i1 = (len - title.length() - 2) / 2;
            tmp.append(i1, c);
            tmp += " " + title + " ";
            tmp.append(i1, c);
        }
    } else {
        tmp.append(len, c);
    }

    m_Out << tmp;
}


END_NCBI_SCOPE
