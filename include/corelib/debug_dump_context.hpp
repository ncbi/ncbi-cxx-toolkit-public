#ifndef DEBUG_DUMP_CONTEXT__HPP
#define DEBUG_DUMP_CONTEXT__HPP
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
 */


BEGIN_NCBI_SCOPE

#include <corelib/ncbistr.hpp>

class CObject;
class CDebugDumpContext
{
protected:
    // Constructor is protected
    // to prohibit creating the object directly -
    // only subclassed object may be created - 
    // the one, which provides dump formatting
    CDebugDumpContext(const string& bundle);
public:
    // This is not exactly a copy constructor -
    // this mechanism is used internally to find out
    // where are we on the Dump tree
    CDebugDumpContext(CDebugDumpContext& ddc);
    CDebugDumpContext(CDebugDumpContext& ddc, const string& bundle);
    virtual ~CDebugDumpContext(void);

public:

    // First thing in the CObject::Dump function - call this function
    // providing class type as the frame name
    void SetFrame(const string& frame);
    // Log data in the form [name, data, comment]
    void Log(const string& name, const string& value,
             const string& comment = kEmptyStr);
    void Log(const string& name, bool value,
             const string& comment = kEmptyStr);
    void Log(const string& name, long value,
             const string& comment = kEmptyStr);
    void Log(const string& name, unsigned long value,
             const string& comment = kEmptyStr);
    void Log(const string& name, double value,
             const string& comment = kEmptyStr);
    void Log(const string& name, const void* value,
             const string& comment = kEmptyStr);
    void Log(const string& name, const CObject* value,
             unsigned int depth);


protected:
    // These are to comminicate to a subclass
    virtual bool StartBundle(unsigned int level, const string& bundle);
    virtual void EndBundle(  unsigned int level, const string& bundle);

    virtual bool StartFrame( unsigned int level, const string& frame);
    virtual void EndFrame(   unsigned int level, const string& frame);

    virtual void PutValue(   unsigned int level, const string& frame,
                             const string& name, const string& value,
                             const string& comment);

protected:
    bool IsStarted() const {return m_Started;}
private:
    void x_VerifyFrameStarted(void);
    void x_VerifyFrameEnded(void);
    CDebugDumpContext& m_Formatter;
    unsigned int       m_Level;
    bool               m_Start_Bundle;
    string             m_Title; 
    bool               m_Started;
};

END_NCBI_SCOPE
/*
 * ===========================================================================
 *  $Log$
 *  Revision 1.1  2002/05/14 14:43:15  gouriano
 *  added DebugDump function to CObject
 *
 * ===========================================================================
*/
#endif // DEBUG_DUMP_CONTEXT__HPP
