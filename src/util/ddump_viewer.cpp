
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

#include <ncbi_pch.hpp>
#include <typeinfo>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbireg.hpp>
#include <util/ddump_viewer.hpp>

#ifdef NCBI_OS_MSWIN
#  include <windows.h>
#else
#  include <signal.h>
#endif


BEGIN_NCBI_SCOPE


//---------------------------------------------------------------------------
//  CDebugDumpViewer implementation

bool CDebugDumpViewer::x_GetInput(string& input)
{
    char cBuf[512];
    cout << "command>";
    cin.getline(cBuf, sizeof(cBuf)/sizeof(cBuf[0]));
    input = cBuf;
    return (input != "go");
}

const void* CDebugDumpViewer::x_StrToPtr(const string& str)
{
    void* addr = 0;
    addr = reinterpret_cast<void*>(NStr::StringToULong(str,16));
    return addr;
}

bool CDebugDumpViewer::x_CheckAddr( const void* addr, bool report)
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

bool CDebugDumpViewer::x_CheckLocation(const char* file, int line)
{
    CNcbiRegistry& cfg = CNcbiApplication::Instance()->GetConfig();
    string section("DebugDumpBpt");
    string value = cfg.Get( section, "enabled");
    // the section is absent? - enable all
    if (value.empty()) {
        return true;
    }
    // prerequisite
    bool enabled = ((value != "false") && (value != "0"));
    // Now only listed locations will be treated accordingly

    // smth about this particular file?
    string name = CDirEntry(file).GetName();
    value = cfg.Get( section, name);
    if (value.empty() || (value=="none")) {
        return !enabled; // none are "enabled"
    } else if (value == "all") {
        return enabled;  // all are "enabled"
    }
    // otherwise - look for this particular line
    // location range must be in the form "10,20-30,150-200"
    list<string> loc;
    NStr::Split( value,",",loc);
    list<string>::iterator it_loc;
    for (it_loc = loc.begin(); it_loc != loc.end(); ++it_loc) {
        list<string> range;
        list<string>::iterator it_range;
        NStr::Split( *it_loc,"-",range);
        int from=0, to;
        try {
            it_range = range.begin();
            from = NStr::StringToInt( *it_range);
            to   = NStr::StringToInt( *(++it_range));
        } catch (...) {
            to = from;
        }
        if ((line >= from) && (line <= to)) {
            return enabled;
        }
    }
    return !enabled;
}

void CDebugDumpViewer::x_Info(
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
    if (!x_CheckLocation(file, line)) {
        return;
    }

    string location, input, cmnd0, cmnd1, cmnd2;
    list<string> cmnd;
    list<string>::iterator it_cmnd;
    size_t narg;
    unsigned int depth;
    bool need_info;

    location = string(file) + "(" + NStr::IntToString(line) + ")";
    x_Info( name, curr_object, location);
    curr_object->DebugDumpText(cout, location + ": " + name, 0);

    while (x_GetInput(input)) {
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
                    const void* addr = x_StrToPtr( cmnd1);
                    x_CheckAddr( addr, true);
                    need_info = false;
                }
                break;
            case 'd': // dump
                if (narg>1) {
                    const void* addr = x_StrToPtr( cmnd1);
                    if (x_CheckAddr( addr, false))
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
            x_Info( name, curr_object, location);
        }
    }
}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2005/02/01 21:47:15  grichenk
 * Fixed warnings
 *
 * Revision 1.4  2004/05/17 21:06:02  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.3  2002/06/04 16:34:58  gouriano
 * added possibility to enable/disable debug dump breakpoints from registry
 *
 * Revision 1.2  2002/06/03 20:37:52  gouriano
 * added include <typeinfo>
 *
 * Revision 1.1  2002/06/03 20:25:32  gouriano
 * added debug dump viewer class
 *
 *
 * ===========================================================================
 */
