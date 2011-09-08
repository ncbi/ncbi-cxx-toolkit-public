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
#include "ncbisys.hpp"

// strstream (aka CNcbiStrstream) remains the default for historical
// reasons; however, MIPSpro's implementation is buggy, yielding
// truncated results in some cases. :-/
#ifdef NCBI_COMPILER_MIPSPRO
#  include <sstream>
typedef std::stringstream TStrStream;
#else
typedef ncbi::CNcbiStrstream TStrStream;
#endif

BEGIN_NCBI_SCOPE


static CSafeStaticPtr<CMetaRegistry> s_Instance;


bool CMetaRegistry::SEntry::Reload(CMetaRegistry::TFlags reload_flags)
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
    if ( ((reload_flags & fAlwaysReload) != fAlwaysReload)
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
    IRWRegistry* dest = NULL;
    if (registry) {
        CRegistryWriteGuard REG_GUARD(*registry);
        TRegFlags rflags = IRWRegistry::AssessImpact(reg_flags,
                                                     IRWRegistry::eRead);
        if ((reload_flags & fKeepContents)  ||  registry->Empty(rflags)) {
            dest = registry->Read(ifs, reg_flags | IRegistry::fJustCore);
        } else {
            // Go through a temporary so errors (exceptions) won't
            // cause *registry to be incomplete.
            CMemoryRegistry tmp_reg(reg_flags & IRegistry::fCaseFlags);
            TStrStream      str;
            tmp_reg.Read(ifs, reg_flags);
            tmp_reg.Write(str, reg_flags);
            str.seekg(0);
            bool was_modified = registry->Modified(rflags);
            registry->Clear(rflags);
            dest = registry->Read(str, reg_flags | IRegistry::fJustCore);
            if ( !was_modified ) {
                registry->SetModifiedFlag(false, rflags);
            }
        }

        if (dest) {
            dest->WriteLock();
        } else {
            dest = registry.GetPointer();
        }
    } else {
        registry.Reset(new CNcbiRegistry(ifs, reg_flags));
    }

    CCompoundRWRegistry* crwreg = dynamic_cast<CCompoundRWRegistry*>(dest);
    if (crwreg) {
        crwreg->LoadBaseRegistries(reg_flags, reload_flags);
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
    if (reg  &&  entry.registry  &&  reg != entry.registry.GetPointer()) {
        _ASSERT( !(flags & fPrivate) );
        // Copy the relevant data in
        if (&entry != &scratch_entry) {
            scratch_entry = entry;
        }
        TRegFlags rflags = IRWRegistry::AssessImpact(reg_flags,
                                                     IRWRegistry::eRead);
        TStrStream str;
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
        reg->Read(str, reg_flags | IRegistry::fJustCore);
        scratch_entry.registry.Reset(reg);
        CCompoundRWRegistry* crwreg = dynamic_cast<CCompoundRWRegistry*>(reg);
        if (crwreg) {
            REG_GUARD.Release();
            crwreg->LoadBaseRegistries(reg_flags, flags);
        }
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

    scratch_entry.actual_name = x_FindRegistry(name, style);
    scratch_entry.flags       = flags;
    scratch_entry.reg_flags   = reg_flags;
    scratch_entry.registry.Reset(reg);
    if (scratch_entry.actual_name.empty()
        ||  !scratch_entry.Reload(flags | fAlwaysReload | fKeepContents) ) {
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


string CMetaRegistry::x_FindRegistry(const string& name, ENameStyle style)
{
    _TRACE("CMetaRegistry::FindRegistry: looking for " << name);

    string dir;
    CDirEntry::SplitPath(name, &dir, 0, 0);
    if ( dir.empty() ) {
        ITERATE (TSearchPath, it, m_SearchPath) {
            const string& result
                = x_FindRegistry(CDirEntry::MakePath(*it, name), style);
            if ( !result.empty() ) {
                return result;
            }
        }
    } else {
        switch (style) {
        case eName_AsIs:
            if (CFile(name).Exists()) {
                string abs_name;
                if ( CDirEntry::IsAbsolutePath(name) ) {
                    abs_name = name;
                } else {
                    abs_name = CDirEntry::ConcatPath(CDir::GetCwd(), name);
                }
                return CDirEntry::NormalizePath(abs_name);
            }
            break;
        case eName_Ini:
            for (string name2(name); ; ) {
                string result = x_FindRegistry(name2 + ".ini", eName_AsIs);
                if ( !result.empty() ) {
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
        case eName_DotRc: {
            string base, ext;
            CDirEntry::SplitPath(name, 0, &base, &ext);
            return x_FindRegistry(CDirEntry::MakePath(dir, '.' + base, ext)
                                  + "rc", eName_AsIs);
        }
        }  // switch (style)
    }
    return kEmptyStr;
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
        return !entry.registry.IsNull();
    }
}


void CMetaRegistry::GetDefaultSearchPath(CMetaRegistry::TSearchPath& path)
{
    path.clear();

    const TXChar* cfg_path = NcbiSys_getenv(_TX("NCBI_CONFIG_PATH"));
    if (cfg_path) {
        path.push_back(_T_STDSTRING(cfg_path));
        return;
    }

    if (NcbiSys_getenv(_TX("NCBI_DONT_USE_LOCAL_CONFIG")) == NULL) {
        path.push_back(".");
        string home = CDir::GetHome();
        if ( !home.empty() ) {
            path.push_back(home);
        }
    }

    {{
        const TXChar* ncbi = NcbiSys_getenv(_TX("NCBI"));
        if (ncbi  &&  *ncbi) {
            path.push_back(_T_STDSTRING(ncbi));
        }
    }}

#ifdef NCBI_OS_MSWIN
    {{
        const TXChar* sysroot = NcbiSys_getenv(_TX("SYSTEMROOT"));
        if (sysroot  &&  *sysroot) {
            path.push_back(_T_STDSTRING(sysroot));
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
