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
* Author:  Aaron Ucko
*
* File Description:
*   CMetaRegistry
*
* ===========================================================================
*/

#include <corelib/metareg.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>

BEGIN_NCBI_SCOPE

auto_ptr<CMetaRegistry> CMetaRegistry::sm_Instance;
CMutex                  CMetaRegistry::sm_Mutex;

CMetaRegistry::~CMetaRegistry()
{
    ITERATE (vector<SEntry>, it, m_Contents) {
        // optionally save if modified?
        if ( !(it->flags & fDontOwn) ) {
            delete it->registry;
        }
    }
}

CMetaRegistry::SEntry
CMetaRegistry::x_Load(const string& name, CMetaRegistry::ENameStyle style,
                      CMetaRegistry::TFlags flags,
                      CNcbiRegistry::TFlags reg_flags, CNcbiRegistry* reg,
                      const string& name0, CMetaRegistry::ENameStyle style0)
{
    _TRACE("CMetaRegistry::Load: looking for " << name);
    CMutexGuard GUARD(sm_Mutex);
    if (flags & fPrivate) {
        GUARD.Release();
    } else { // see if we already have it
        ITERATE (vector<SEntry>, it, m_Contents) {
            if (it->flags != flags  ||  it->reg_flags != reg_flags) {
                continue;
            } else if (it->requested_name == name  &&  it->style == style) {
                _TRACE("found in cache");
                return *it;
            } else if (style == eName_AsIs  &&  it->actual_name == name) {
                _TRACE("found in cache");
                return *it;
            }
        }
    }
    string dir;
    CDirEntry::SplitPath(name, &dir, 0, 0);
    if (dir.empty()) {
        iterate (TSearchPath, it, m_SearchPath) {
            SEntry result = x_Load(CDirEntry::MakePath(*it, name), style,
                                   flags, reg_flags, reg, name0, style0);
            if (result.registry) {
                return result;
            }
        }
    } else {
        switch (style) {
        case eName_AsIs:
        {
            CNcbiIfstream in(name.c_str(), IOS_BASE::in | IOS_BASE::binary);
            if (in.good()) {
                _TRACE("CMetaRegistry::Load() -- reading registry file "
                       << name);
                SEntry result;
                result.requested_name = name0;
                result.style          = style0;
                result.flags          = flags;
                result.reg_flags      = reg_flags;
                if (reg) {
                    if ( !reg->Empty() ) { // shouldn't share
                        result.flags  = fPrivate;
                    }
                    result.registry   = reg;
                    reg->Read(in, reg_flags);
                } else {
                    result.registry   = new CNcbiRegistry(in, reg_flags);
                }
                if (CDirEntry::IsAbsolutePath(name)) {
                    result.actual_name = name;
                } else {
                    result.actual_name
                        = CDirEntry::ConcatPath(CDir::GetCwd(), name);
                }
                if ( !(flags & fPrivate) ) {
                    m_Contents.push_back(result);
                }
                return result;
            } else {
                _TRACE("CMetaRegistry::Load() -- cannot open registry file "
                       << name);
            }
            break;
        }
        case eName_Ini:
        {
            string name2(name);
            for (;;) {
                SEntry result = x_Load(name2 + ".ini", eName_AsIs, flags,
                                       reg_flags, reg, name0, style0);
                if (result.registry) {
                    return result;
                }
                string base, ext; // dir already known
                CDirEntry::SplitPath(name2, 0, &base, &ext);
                if (ext.empty()) {
                    break;
                } else {
                    name2 = CDirEntry::MakePath(dir, base);
                }
            }
            break;
        }
        case eName_DotRc:
        {
            string base, ext;
            CDirEntry::SplitPath(name, 0, &base, &ext);
            return x_Load(CDirEntry::MakePath(dir, '.' + base, ext) + "rc",
                          eName_AsIs, flags, reg_flags, reg, name0, style0);
        }
        }
    }

    // not found :-/
    SEntry result;
    result.requested_name = name0;
    result.style          = style0;
    result.flags          = flags;
    result.reg_flags      = reg_flags;
    result.registry       = 0;
    if (reg  &&  !(flags & fDontOwn) ) {
        delete reg;
    }
    return result;
}

void CMetaRegistry::GetDefaultSearchPath(CMetaRegistry::TSearchPath& path)
{
    // set up the default search path:
    //    - The current working directory.
    //    - The directory, if any, given by the environment variable "NCBI".
    //    - The user's home directory.
    //    - The directory containing the application, if known
    path.push_back(".");
    {{
        const char* ncbi = getenv("NCBI");
        if (ncbi && *ncbi) {
            path.push_back(ncbi);
        }
    }}
    path.push_back(CDir::GetHome());
    {{
        CNcbiApplication* the_app = CNcbiApplication::Instance();
        if (the_app) {
            string dir = the_app->GetArguments().GetProgramDirname();
            if (dir.size()) {
                path.push_back(dir);
            }
        }
    }}
}

END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.6  2003/08/21 20:50:54  ucko
* eName_DotRc: avoid accidentally adding a dot before the rc...
*
* Revision 1.5  2003/08/18 19:49:09  ucko
* Remove an unreachable statement.
*
* Revision 1.4  2003/08/12 15:15:50  ucko
* Open registry files in binary mode per CNcbiRegistry's expectations.
*
* Revision 1.3  2003/08/06 20:26:17  ucko
* Allow Load to take an existing registry to reuse; properly handle flags.
*
* Revision 1.2  2003/08/05 20:28:51  ucko
* CMetaRegistry::GetDefaultSearchPath: don't add the program's directory
* if it's empty.
*
* Revision 1.1  2003/08/05 19:57:59  ucko
* CMetaRegistry: Singleton class for loading CRegistry data from files;
* keeps track of what it loaded from where, for potential reuse.
*
*
* ===========================================================================
*/
