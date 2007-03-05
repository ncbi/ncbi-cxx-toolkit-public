#ifndef DEBUGDUMP_VIEWER__H
#define DEBUGDUMP_VIEWER__H

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
 *      Console Debug Dump Viewer
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ddumpable.hpp>


/** @addtogroup ConsoleDebug
 *
 * @{
 */


BEGIN_NCBI_SCOPE

//---------------------------------------------------------------------------
//

#define DEBUGDUMP_BPT(name,obj)  \
    do {CDebugDumpViewer().Bpt(name,&(obj),__FILE__,__LINE__);} while (0)


//---------------------------------------------------------------------------
//  CDebugDumpViewer interface

class NCBI_XUTIL_EXPORT CDebugDumpViewer
{
public:
    CDebugDumpViewer() {}
    virtual ~CDebugDumpViewer() {}

    void Bpt(const string& name, const CDebugDumpable* curr_object,
             const char* file, int line);
private:
    void        x_Info(const string& name, const CDebugDumpable* curr_object,
                     const string& location);
    bool        x_GetInput(string& input);
    const void* x_StrToPtr(const string& str);
    bool        x_CheckAddr( const void* addr, bool report);
    bool        x_CheckLocation(const char* file, int line);
};


END_NCBI_SCOPE


/* @} */

#endif // DEBUGDUMP_VIEWER__H
