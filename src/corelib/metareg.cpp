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

#include <ncbi_pch.hpp>
#include <corelib/metareg.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_safe_static.hpp>

BEGIN_NCBI_SCOPE


CSafeStaticPtr<CMetaRegistry> s_Instance;


CMetaRegistry& CMetaRegistry::Instance(void)
{
    return *s_Instance;
}


CMetaRegistry::~CMetaRegistry()
{
    // XX - optionally save modified registries?
}


CMetaRegistry::SEntry
CMetaRegistry::x_Load(const string& name, CMetaRegistry::ENameStyle style,
                      CMetaRegistry::TFlags flags,
                      IRegistry::TFlags reg_flags, IRWRegistry* reg,
                      const string& name0, CMetaRegistry::ENameStyle style0)
{
    _TRACE("CMetaRegistry::Load: looking for " << name);

    CMutexGuard GUARD(m_Mutex);

    if (flags & fPrivate) {
        GUARD.Release();
    }
    else { // see if we already have it
        TIndex::const_iterator iit
            = m_Index.find(SKey(name, style, flags, reg_flags));
        if (iit != m_Index.end()) {
            _TRACE("found in cache");
            _ASSERT(iit->second < m_Contents.size());
            return m_Contents[iit->second];
        }

        ITERATE (vector<SEntry>, it, m_Contents) {
            if (it->flags != flags  ||  it->reg_flags != reg_flags)
                continue;

            if (style == eName_AsIs  &&  it->actual_name == name) {
                _TRACE("found in cache");
                return *it;
            }
        }
    }

    string dir;
    CDirEntry::SplitPath(name, &dir, 0, 0);
    if ( dir.empty() ) {
        ITERATE (TSearchPath, it, m_SearchPath) {
            SEntry result = x_Load(CDirEntry::MakePath(*it, name), style,
                                   flags, reg_flags, reg, name0, style0);
            if ( result.registry ) {
                return result;
            }
        }
    } else {
        switch (style) {
        case eName_AsIs: {
            CNcbiIfstream in(name.c_str(), IOS_BASE::in | IOS_BASE::binary);
            if ( !in.good() ) {
                _TRACE("CMetaRegistry::Load() -- cannot open registry file: "
                       << name);
                break;
            }

            _TRACE("CMetaRegistry::Load() -- reading registry file: " << name);
            SEntry result;
            result.flags     = flags;
            result.reg_flags = reg_flags;
            if ( reg ) {
                if ( !reg->Empty() ) { // shouldn't share
                    result.flags |= fPrivate;
                }
                result.registry = reg;
                reg->Read(in, reg_flags);
            } else {
                result.registry.Reset(new CNcbiRegistry(in, reg_flags));
            }

            if ( CDirEntry::IsAbsolutePath(name) ) {
                result.actual_name = name;
            } else {
                result.actual_name = CDirEntry::ConcatPath(CDir::GetCwd(),
                                                           name);
            }
            if ( !(flags & fPrivate) ) {
                m_Contents.push_back(result);
                m_Index[SKey(name0, style0, flags, reg_flags)]
                    = (unsigned int)m_Contents.size() - 1;
            }
            return result;
        }
        case eName_Ini: {
            string name2(name);
            for (;;) {
                SEntry result = x_Load(name2 + ".ini", eName_AsIs, flags,
                                       reg_flags, reg, name0, style0);
                if (result.registry) {
                    return result;
                }

                string base, ext; // dir already known
                CDirEntry::SplitPath(name2, 0, &base, &ext);
                if ( ext.empty() ) {
                    break;
                }
                name2 = CDirEntry::MakePath(dir, base);
            }
            break;
        }
        case eName_DotRc: {
            string base, ext;
            CDirEntry::SplitPath(name, 0, &base, &ext);
            return x_Load(CDirEntry::MakePath(dir, '.' + base, ext) + "rc",
                          eName_AsIs, flags, reg_flags, reg, name0, style0);
        }
        }  // switch (style)
    }

    // not found
    SEntry result;
    result.flags     = flags;
    result.reg_flags = reg_flags;
    result.registry.Reset();
    return result;
}


void CMetaRegistry::GetDefaultSearchPath(CMetaRegistry::TSearchPath& path)
{
    const char* cfg_path = getenv("NCBI_CONFIG_PATH");
    if (cfg_path) {
        path.push_back(cfg_path);
        return;
    }

    if (getenv("NCBI_DONT_USE_LOCAL_CONFIG") == NULL) {
        path.push_back(".");
        path.push_back(CDir::GetHome());
    }

    {{
        const char* ncbi = getenv("NCBI");
        if (ncbi  &&  *ncbi) {
            path.push_back(ncbi);
        }
    }}

#ifdef NCBI_OS_MSWIN
    {{
        const char* sysroot = getenv("SYSTEMROOT");
        if (sysroot  &&  *sysroot) {
            path.push_back(sysroot);
        }
    }}
#else
    path.push_back("/etc");
#endif

    {{
        CNcbiApplication* the_app = CNcbiApplication::Instance();
        if ( the_app ) {
            const CNcbiArguments& args = the_app->GetArguments();
            string                dir  = args.GetProgramDirname(eIgnoreLinks);
            string                dir2 = args.GetProgramDirname(eFollowLinks);
            if (dir.size()) {
                path.push_back(dir);
            }
            if (dir2.size() && dir2 != dir) {
                path.push_back(dir2);
            }
        }
    }}
}


bool CMetaRegistry::SKey::operator <(const SKey& k) const
{
    if (requested_name < k.requested_name) {
        return true;
    } else if (requested_name > k.requested_name) {
        return false;
    }

    if (style < k.style) {
        return true;
    } else if (style > k.style) {
        return false;
    }

    if (flags < k.flags) {
        return true;
    } else if (flags > k.flags) {
        return false;
    }

    if (reg_flags < k.reg_flags) {
        return true;
    } else if (reg_flags > k.reg_flags) {
        return false;
    }

    return false;
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.16  2005/04/25 20:21:55  ivanov
 * Get rid of Workshop compilation warnings
 *
 * Revision 1.15  2005/01/25 19:56:41  ucko
 * Hold the single instance with a CSafeStaticPtr rather than a static auto_ptr.
 *
 * Revision 1.14  2005/01/10 16:56:49  ucko
 * Support working with arbitrary IRWRegistry objects, and take
 * advantage of the fact that they're CObjects these days.
 *
 * Revision 1.13  2005/01/10 16:23:05  ucko
 * Adjust default search path (now ordered like the C Toolkit's), and add
 * support for NCBI_CONFIG_PATH and NCBI_DONT_USE_LOCAL_CONFIG.
 *
 * Revision 1.12  2004/05/14 13:59:26  gorelenk
 * Added include of ncbi_pch.hpp
 *
 * Revision 1.11  2003/12/08 18:40:22  ucko
 * Use DEFINE_CLASS_STATIC_MUTEX to avoid possible premature locking.
 *
 * Revision 1.10  2003/10/01 14:32:09  ucko
 * +EFollowLinks
 *
 * Revision 1.9  2003/10/01 01:02:49  ucko
 * Fix (syntactically) broken assertion.
 *
 * Revision 1.8  2003/09/30 21:06:24  ucko
 * Refactored cache to allow flushing of path searches when the search
 * path changes.
 * If the application ran through a symlink to another directory, search
 * the real directory as a fallback.
 *
 * Revision 1.7  2003/09/29 20:15:20  vakatov
 * CMetaRegistry::x_Load() -- thinko fix:  "result.flags |= fPrivate;" (added '|')
 *
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
 * ===========================================================================
 */
