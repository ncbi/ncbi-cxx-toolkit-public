#ifndef TEXT_DUMP_CONTEXT__HPP
#define TEXT_DUMP_CONTEXT__HPP
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
 *   Formats object's dump as a text and sends it into any output stream
 *
 */

#include <corelib/debug_dump_context.hpp>

BEGIN_NCBI_SCOPE

class CTextDumpContext : public CDebugDumpContext
{
public:
    // Only constructor is public (usually)
    // CObject::Dump function communicates then with
    // CDebugDumpContext public methods only
    CTextDumpContext(ostream& os, const string& bundle);
    virtual ~CTextDumpContext(void);

protected:
    virtual bool StartBundle(unsigned int level, const string& bundle);
    virtual void EndBundle(  unsigned int level, const string& bundle);

    virtual bool StartFrame( unsigned int level, const string& frame);
    virtual void EndFrame(   unsigned int level, const string& frame);

    virtual void PutValue(  unsigned int level, const string& frame,
                            const string& name, const string& value,
                            const string& comment);

private:
    void x_IndentLine(unsigned int level, char c = ' ', int len = 2);
    void x_InsertPageBreak(const string& title = "", char c = '=', int len = 78);

    ostream&     m_Out;
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
#endif // TEXT_DUMP_CONTEXT__HPP
