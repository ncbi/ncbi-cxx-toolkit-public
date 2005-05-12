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


bool CMetaRegistry::SEntry::Reload(CMetaRegistry::TFlags flags)
{
    CFile file(actual_name);
    if ( !file.Exists() ) {
        _TRACE("No such registry file " << actual_name);
        return false;
    }
    CMutexGuard GUARD(s_Instance->m_Mutex);
    Int8  new_length = file.GetLength();
    CTime new_timestamp;
    file.GetTime(&new_timestamp);
    if ( ((flags & fAlwaysReload) != fAlwaysReload)
         &&  new_length == length  &&  new_timestamp == timestamp ) {
        _TRACE("Registry file " << actual_name
               << " appears not to have changed since last loaded");
        return false;
    }
    CNcbiIfstream ifs(actual_name.c_str(), IOS_BASE::in | IOS_BASE::binary);
    if ( !ifs.good() ) {
        _TRACE("Unable to (re)open registry file " << actual_name);
        return false;
    }
    if (registry) {
        CRegistryWriteGuard REG_GUARD(*registry);
        if ( !(flags & fKeepContents) ) {
            TRegFlags rflags = IRWRegistry::AssessImpact(reg_flags,
                                                         IRWRegistry::eRead);
            bool was_modified = registry->Modified(rflags);
            registry->Clear(rflags);
            if ( !was_modified ) {
                registry->SetModifiedFlag(false, rflags);
            }
        }
        registry->Read(ifs, reg_flags);
    } else {
        registry.Reset(new CNcbiRegistry(ifs, reg_flags));
    }

    timestamp = new_timestamp;
    length    = new_length;
    return true;
}


CMetaRegistry& CMetaRegistry::Instance(void)
{
    return *s_Instance;
}


CMetaRegistry::~CMetaRegistry()
{
    // XX - optionally save modified registries?
}


CMetaRegistry::SEntry CMetaRegistry::Load(const string& name,
                                          CMetaRegistry::ENameStyle style,
                                          CMetaRegistry::TFlags flags,
                                          IRegistry::TFlags reg_flags,
                                          IRWRegistry* reg)
{
    SEntry scratch_entry;
    if ( reg  &&  !reg->Empty() ) { // shouldn't share
        flags |= fPrivate;
    }
    const SEntry& entry = Instance().x_Load(name, style, flags, reg_flags,
                                            reg, name, style, scratch_entry);
    if (reg  &&  entry.registry  &&  reg != entry.registry) {
        _ASSERT( !(flags & fPrivate) );
        // Copy the relevant data in
        if (&entry != &scratch_entry) {
            scratch_entry = entry;
        }
        TRegFlags rflags = IRWRegistry::AssessImpact(reg_flags,
                                                     IRWRegistry::eRead);
        CNcbiStrstream str;
        entry.registry->Write(str, rflags);
        str.seekg(0);
        CRegistryWriteGuard REG_GUARD(*reg);
        if ( !(flags & fKeepContents) ) {
            bool was_modified = reg->Modified(rflags);
            reg->Clear(rflags);
            if ( !was_modified ) {
                reg->SetModifiedFlag(false, rflags);
            }
        }
        reg->Read(str, reg_flags);
        scratch_entry.registry.Reset(reg);
        return scratch_entry;
    }
    return entry;
}


const CMetaRegistry::SEntry&
CMetaRegistry::x_Load(const string& name, CMetaRegistry::ENameStyle style,
                      CMetaRegistry::TFlags flags,
                      IRegistry::TFlags reg_flags, IRWRegistry* reg,
                      const string& name0, CMetaRegistry::ENameStyle style0,
                      CMetaRegistry::SEntry& scratch_entry)
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
            SEntry& result = m_Contents[iit->second];
            result.Reload(flags);
            return result;
        }

        NON_CONST_ITERATE (vector<SEntry>, it, m_Contents) {
            if (it->flags != flags  ||  it->reg_flags != reg_flags)
                continue;

            if (style == eName_AsIs  &&  it->actual_name == name) {
                _TRACE("found in cache");
                it->Reload(flags);
                return *it;
            }
        }
    }

    string dir;
    CDirEntry::SplitPath(name, &dir, 0, 0);
    if ( dir.empty() ) {
        ITERATE (TSearchPath, it, m_SearchPath) {
            const SEntry& result = x_Load(CDirEntry::MakePath(*it, name),
                                          style, flags, reg_flags, reg,
                                          name0, style0, scratch_entry);
            if ( result.registry ) {
                return result;
            }
        }
    } else {
        switch (style) {
        case eName_AsIs: {
            string abs_name;
            if ( CDirEntry::IsAbsolutePath(name) ) {
                abs_name = name;
            } else {
                abs_name = CDirEntry::ConcatPath(CDir::GetCwd(), name);
            }
            scratch_entry.actual_name = CDirEntry::NormalizePath(abs_name);
            scratch_entry.flags       = flags;
            scratch_entry.reg_flags   = reg_flags;
            scratch_entry.registry.Reset(reg);
            if ( !scratch_entry.Reload(flags | fAlwaysReload
                                       | fKeepContents) ) {
                scratch_entry.registry.Reset();
                return scratch_entry;
            } else if (flags & fPrivate) {
                return scratch_entry;
            } else {
                m_Contents.push_back(scratch_entry);
                m_Index[SKey(name0, style0, flags, reg_flags)]
                    = m_Contents.size() - 1;
                return m_Contents.back();
            }
        }
        case eName_Ini: {
            string name2(name);
            for (;;) {
                const SEntry& result = x_Load(name2 + ".ini", eName_AsIs,
                                              flags, reg_flags, reg, name0,
                                              style0, scratch_entry);
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
                          eName_AsIs, flags, reg_flags, reg, name0, style0,
                          scratch_entry);
        }
        }  // switch (style)
    }

    // not found
    scratch_entry.flags     = flags;
    scratch_entry.reg_flags = reg_flags;
    scratch_entry.registry.Reset();
    return scratch_entry;
}


bool CMetaRegistry::x_Reload(const string& path, IRWRegistry& reg,
                             TFlags flags, TRegFlags reg_flags)
{
    SEntry* entryp = 0;
    NON_CONST_ITERATE (vector<SEntry>, it, m_Contents) {
        if (it->registry == &reg  ||  it->actual_name == path) {
            entryp = &*it;
            break;
        }
    }
    if (entryp) {
        return entryp->Reload(flags);
    } else {
        SEntry entry = Load(path, eName_AsIs, flags, reg_flags, &reg);
        _ASSERT(entry.registry.IsNull()  ||  entry.registry == &reg);
        return entry.registry;
    }
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
 * Revision 1.17  2005/05/12 15:15:32  ucko
 * Fix some (meta)registry buglets and add support for reloading.
 *
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
