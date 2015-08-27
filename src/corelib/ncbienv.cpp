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
 * Authors:  Denis Vakatov, Eugene Vasilchenko
 *
 * File Description:
 *   Unified interface to application:
 *      environment     -- CNcbiEnvironment
 *      cmd.-line args  -- CNcbiArguments
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <corelib/error_codes.hpp>
#include <algorithm>
#include <stdarg.h>
#include "ncbisys.hpp"

#ifdef NCBI_OS_LINUX
#  include <unistd.h>
#endif

#ifdef NCBI_OS_MSWIN
#  include <stdlib.h>
#elif defined (NCBI_OS_DARWIN)
#  include <crt_externs.h>
#  define environ (*_NSGetEnviron())
#else
extern char** environ;
#endif


#define NCBI_USE_ERRCODE_X   Corelib_Env


BEGIN_NCBI_SCOPE


///////////////////////////////////////////////////////
//  CNcbiEnvironment::


CNcbiEnvironment::CNcbiEnvironment(void)
{
    Reset(environ);
}


CNcbiEnvironment::CNcbiEnvironment(const char* const* envp)
{
    Reset(envp);
}


CNcbiEnvironment::~CNcbiEnvironment(void)
{
    return;
}


void CNcbiEnvironment::Reset(const char* const* envp)
{
    // load new environment values from "envp"
    if ( !envp )
        return;

    CFastMutexGuard LOCK(m_CacheMutex);
    // delete old environment values
    m_Cache.clear();

    for ( ;  *envp;  envp++) {
        const char* s = *envp;
        const char* eq = strchr(s, '=');
        if ( !eq ) {
            ERR_POST_X(3, "CNcbiEnvironment: bad string '" << s << "'");
            continue;
        }
        m_Cache[string(s, eq)] = SEnvValue(eq + 1, kEmptyXCStr);
    }
}


const string& CNcbiEnvironment::Get(const string& name, bool* found) const
{
    CFastMutexGuard LOCK(m_CacheMutex);
    TCache::const_iterator i = m_Cache.find(name);
    bool fake_found;
    if (found == NULL) {
        found = &fake_found;
    }
    if ( i != m_Cache.end() ) {
        if (i->second.ptr == NULL  &&  i->second.value.empty()) {
            *found = false;
            return kEmptyStr;
        } else {
            *found = true;
            return i->second.value;
        }
    }
    m_Cache[name] = SEnvValue(Load(name, *found), *found ? kEmptyXCStr : NULL);
    const string& s = m_Cache[name].value;
    return s.empty() ? kEmptyStr : s;
}


void CNcbiEnvironment::Enumerate(list<string>& names, const string& prefix)
    const
{
    names.clear();
    CFastMutexGuard LOCK(m_CacheMutex);
    for (TCache::const_iterator it = m_Cache.lower_bound(prefix);
         it != m_Cache.end()  &&  NStr::StartsWith(it->first, prefix);  ++it) {
        if ( !it->second.value.empty()  ||  it->second.ptr == kEmptyXCStr) {
            // ignore entries the app cleared out
            names.push_back(it->first);
        }
    }
}

void CNcbiEnvironment::Set(const string& name, const string& value)
{
    TXChar* str = NcbiSys_strdup(_T_XCSTRING(name + "=" + value));
    if ( !str ) {
        throw bad_alloc();
    }

    if (NcbiSys_putenv(str) != 0) {
        free(str);
        NCBI_THROW(CErrnoTemplException<CCoreException>, eErrno,
                   "failed to set environment variable " + name);
    }

    CFastMutexGuard LOCK(m_CacheMutex);
    TCache::const_iterator i = m_Cache.find(name);
    if ( i != m_Cache.end() ) {
        if (i->second.ptr != NULL && i->second.ptr != kEmptyXCStr) {
            free(const_cast<TXChar*>(i->second.ptr));
        }
    }
    m_Cache[name] = SEnvValue(value, str);
}

void CNcbiEnvironment::Unset(const string& name)
{
#ifdef NCBI_OS_MSWIN
    Set(name, kEmptyStr);
#elif defined(NCBI_OS_IRIX)
    {{
        char* p = getenv(name.c_str());
        if (p) {
            _ASSERT(p[-1] == '=');
            _ASSERT( !memcmp(p - name.size() - 1, name.data(), name.size()) );
            p[-1] = '\0';
        }
    }}
#else
    unsetenv(name.c_str());
#endif

    CFastMutexGuard LOCK(m_CacheMutex);
    TCache::iterator i = m_Cache.find(name);
    if ( i != m_Cache.end() ) {
        if (i->second.ptr != NULL && i->second.ptr != kEmptyXCStr) {
            free(const_cast<TXChar*>(i->second.ptr));
        }
        m_Cache.erase(i);
    }
}

string CNcbiEnvironment::Load(const string& name, bool& found) const
{
    const TXChar* s = NcbiSys_getenv(_T_XCSTRING(name));
    if (s == NULL) {
        found = false;
        return NcbiEmptyString;
    } else {
        found = true;
        return _T_STDSTRING(s);
    }
}




///////////////////////////////////////////////////////
//  CAutoEnvironmentVariable::


CAutoEnvironmentVariable::CAutoEnvironmentVariable(const CTempString& var_name,
                                                   const CTempString& value,
                                                   CNcbiEnvironment*  env)
    : m_Env(env, eNoOwnership), m_VariableName(var_name)
{
    if ( !env ) {
        CMutexGuard guard(CNcbiApplication::GetInstanceMutex());
        CNcbiApplication* app = CNcbiApplication::Instance();
        if (app) {
            m_Env.reset(&app->SetEnvironment(), eNoOwnership);
        } else {
            m_Env.reset(new CNcbiEnvironment(NULL), eTakeOwnership);
        }
    }
    m_PrevValue = m_Env->Get(m_VariableName, &m_WasSet);
    if ( value.empty() ) {
        m_Env->Unset(m_VariableName);
    } else {
        m_Env->Set(m_VariableName, value);
    }
}

CAutoEnvironmentVariable::~CAutoEnvironmentVariable()
{
    if (m_WasSet) {
        m_Env->Set(m_VariableName, m_PrevValue);
    } else {
        m_Env->Unset(m_VariableName);
    }
}




///////////////////////////////////////////////////////
//  CEnvironmentCleaner::


CEnvironmentCleaner::CEnvironmentCleaner(const char* s, ...)
{
    if (s != NULL) {
        Clean(s);
        va_list ap;
        va_start(ap, s);
        for (;;) {
            const char* p = va_arg(ap, const char*);
            if (p == NULL) {
                break;
            }
            Clean(p);
        }
        va_end(ap);
    }
}

void CEnvironmentCleaner::Clean(const string& name)
{
    CMutexGuard guard(CNcbiApplication::GetInstanceMutex());
    CNcbiApplication* app = CNcbiApplication::Instance();
    if (app) {
        app->SetEnvironment().Unset(name);
    } else {
#ifdef NCBI_OS_MSWIN
        ::SetEnvironmentVariable(_T_XCSTRING(name), NULL);
#elif defined(NCBI_OS_IRIX)
        char* p = getenv(name.c_str());
        if (p) {
            _ASSERT(p[-1] == '=');
            _ASSERT( !memcmp(p - name.size() - 1, name.data(), name.size()) );
            p[-1] = '\0';
        }
#else
        unsetenv(name.c_str());
#endif
    }
}




///////////////////////////////////////////////////////
//  CNcbiArguments::


CNcbiArguments::CNcbiArguments(int argc, const char* const* argv,
                               const string& program_name,
                               const string& real_name)
{
    Reset(argc, argv, program_name, real_name);
}


CNcbiArguments::~CNcbiArguments(void)
{
    return;
}


CNcbiArguments::CNcbiArguments(const CNcbiArguments& args)
    : m_ProgramName(args.m_ProgramName),
      m_Args(args.m_Args),
      m_ResolvedName(args.m_ResolvedName)
{
    return;
}


CNcbiArguments& CNcbiArguments::operator= (const CNcbiArguments& args)
{
    if (&args == this)
        return *this;

    m_ProgramName = args.m_ProgramName;
    m_Args.clear();
    copy(args.m_Args.begin(), args.m_Args.end(), back_inserter(m_Args));
    return *this;
}


void CNcbiArguments::Reset(int argc, const char* const* argv,
                           const string& program_name, const string& real_name)
{
    // check args
    if (argc < 0) {
        NCBI_THROW(CArgumentsException,eNegativeArgc,
            "Negative number of command-line arguments");
    }

    if ((argc == 0) != (argv == 0)) {
        if (argv == 0) {
            NCBI_THROW(CArgumentsException,eNoArgs,
                "Command-line arguments are absent");
        }
        ERR_POST_X(4, Info <<
                      "CNcbiArguments(): zero \"argc\", non-zero \"argv\"");
    }

    // clear old args, store new ones
    m_Args.clear();
    for (int i = 0;  i < argc;  i++) {
        if ( !argv[i] ) {
            ERR_POST_X(5, Warning <<
                          "CNcbiArguments() -- NULL cmd.-line arg #" << i);
            continue;
        }
        m_Args.push_back(argv[i]);
    }

    // set application name
    SetProgramName(program_name, real_name);
}


const string& CNcbiArguments::GetProgramName(EFollowLinks follow_links) const
{
    if (follow_links) {
        CFastMutexGuard LOCK(m_ResolvedNameMutex);
        if ( !m_ResolvedName.size() ) {
#ifdef NCBI_OS_LINUX
            string proc_link = "/proc/" + NStr::IntToString(getpid()) + "/exe";
            m_ResolvedName = CDirEntry::NormalizePath(proc_link, follow_links);
#else
            m_ResolvedName = CDirEntry::NormalizePath
                (GetProgramName(eIgnoreLinks), follow_links);
#endif
        }
        return m_ResolvedName;
    } else if ( !m_ProgramName.empty() ) {
        return m_ProgramName;
    } else if ( m_Args.size() ) {
        return m_Args[0];
    } else {
        static CSafeStatic<string> kDefProgramName;
        kDefProgramName->assign("ncbi");
        return kDefProgramName.Get();
    }
}


string CNcbiArguments::GetProgramBasename(EFollowLinks follow_links) const
{
    const string& name = GetProgramName(follow_links);
    SIZE_TYPE base_pos = name.find_last_of("/\\:");
    if (base_pos == NPOS)
        return name;
    return name.substr(base_pos + 1);
}


string CNcbiArguments::GetProgramDirname(EFollowLinks follow_links) const
{
    const string& name = GetProgramName(follow_links);
    SIZE_TYPE base_pos = name.find_last_of("/\\:");
    if (base_pos == NPOS)
        return NcbiEmptyString;
    return name.substr(0, base_pos + 1);
}


void CNcbiArguments::SetProgramName(const string& program_name,
                                    const string& real_name)
{
    m_ProgramName = program_name;
    CFastMutexGuard LOCK(m_ResolvedNameMutex);
    m_ResolvedName = real_name;
}


void CNcbiArguments::Add(const string& arg)
{
    m_Args.push_back(arg);
}

void CNcbiArguments::Shift(int n)
{
    while (n-- > 0) {
        if (m_Args.size() > 1) {
            m_Args.erase( ++m_Args.begin());
        }
    }
}

const char* CArgumentsException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eNegativeArgc:  return "eNegativeArgc";
    case eNoArgs:        return "eNoArgs";
    default:    return CException::GetErrCodeString();
    }
}


END_NCBI_SCOPE
