
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

#include <util/ddump_viewer.hpp>

#ifdef NCBI_OS_MSWIN
#  include <windows.h>
#else
#  include <signal.h>
#endif


BEGIN_NCBI_SCOPE


//---------------------------------------------------------------------------
//  CDebugDumpViewer implementation

bool CDebugDumpViewer::GetInput(string& input)
{
    char cBuf[512];
    cout << "command>";
    cin.getline(cBuf, sizeof(cBuf)/sizeof(cBuf[0]));
    input = cBuf;
    return (input != "go");
}

const void* CDebugDumpViewer::StrToPtr(const string& str)
{
    void* addr = 0;
    addr = reinterpret_cast<void*>(NStr::StringToULong(str,16));
    return addr;
}

bool CDebugDumpViewer::CheckAddr( const void* addr, bool report)
{
    bool res = false;
    try {
        const CDebugDumpable *p = static_cast<const CDebugDumpable*>(addr);
        const type_info& t = typeid( *p);
        if (report) {
            cout << "typeid of " << addr
                << " is \"" << t.name() << "\"" << endl;
        }
        res = true;
    } catch (exception& e) {
        if (report) {
            cout << e.what() << endl;
            cout << "address " << addr
                << " does not point to a dumpable object " << endl;
        }
    }
    return res;
}

void CDebugDumpViewer::Info(
    const string& name, const CDebugDumpable* curr_object,
    const string& location)
{
    cout << endl;
    cout << "Console Debug Dump Viewer" << endl << endl;
    cout << "Stopped at " << location << endl;
    cout << "current object: " << name << " = " <<
        static_cast<const void*>(curr_object) << endl << endl;
    cout << "Available commands: "  << endl;
    cout << "    t[ypeid] <address>"  << endl;
    cout << "    d[ump]   <address> <depth>"  << endl;
#ifdef NCBI_OS_MSWIN
    cout << "    b[reak]"  << endl;
#endif
    cout << "    go"  << endl << endl;
}

void CDebugDumpViewer::Bpt(
    const string& name, const CDebugDumpable* curr_object,
    const char* file, int line)
{
    string location, input, cmnd0, cmnd1, cmnd2;
    list<string> cmnd;
    list<string>::iterator it_cmnd;
    int narg;
    unsigned int depth;
    bool need_info;

    location = string(file) + "(" + NStr::IntToString(line) + ")";
    Info( name, curr_object, location);
    curr_object->DebugDumpText(cout, location + ": " + name, 0);

    while (GetInput(input)) {
        cmnd.clear();
        NStr::Split( input, " ", cmnd);
        narg = cmnd.size();
        need_info = true;

        if (narg > 0) {
            cmnd0 = *(it_cmnd = cmnd.begin());
            cmnd1 = (narg > 1) ? *(++it_cmnd) : string("");
            cmnd2 = (narg > 2) ? *(++it_cmnd) : string("");

            switch (cmnd0[0]) {
            case 'b': // break
#ifdef NCBI_OS_MSWIN
                DebugBreak();
//#else
//                raise(SIGSTOP);
#endif
                break;
            case 't': // typeid
                if (narg > 1) {
                    const void* addr = StrToPtr( cmnd1);
                    CheckAddr( addr, true);
                    need_info = false;
                }
                break;
            case 'd': // dump
                if (narg>1) {
                    const void* addr = StrToPtr( cmnd1);
                    if (CheckAddr( addr, false))
                    {
                        depth = (narg>2) ? NStr::StringToUInt( cmnd2) : 0;
                        const CDebugDumpable *p =
                            static_cast<const CDebugDumpable*>(addr);
                        try {
                            const type_info& t = typeid( *p);
                            p->DebugDumpText(cout,
                                string(t.name()) + " " + cmnd1, depth);
                        } catch (...) {
                            cout << "Exception: Dump failed" << endl;
                        }
                    }
                    need_info = false;
                }
                break;
            default:
                break;
            }
        }
        // default = help
        if (need_info) {
            Info( name, curr_object, location);
        }
    }
}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2002/06/03 20:25:32  gouriano
 * added debug dump viewer class
 *
 *
 * ===========================================================================
 */
