#ifndef NCBIENV__HPP
#define NCBIENV__HPP

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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  2000/08/31 23:50:04  vakatov
* CNcbiArguments:: Inlined Size() and operator[];   use <deque>
*
* Revision 1.2  2000/01/20 16:36:02  vakatov
* Added class CNcbiArguments::   application command-line arguments & name
* Added CNcbiEnvironment::Reset(), and comments to CNcbiEnvironment::
* Dont #include <ncbienv.inl>
*
* Revision 1.1  1999/05/04 16:14:06  vasilche
* Fixed problems with program environment.
* Added class CNcbiEnvironment for cached access to C environment.
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <map>
#include <deque>

BEGIN_NCBI_SCOPE


///////////////////////////////////////////////////////
//  CNcbiEnvironment::   application environment

class CNcbiEnvironment
{
public:
    // 'ctors
    CNcbiEnvironment(void);
    CNcbiEnvironment(const char* const* envp);
    virtual ~CNcbiEnvironment(void);

    // Delete all cached entries, load new ones from "envp" (if not NULL)
    void Reset(const char* const* envp = 0);

    // Get env.value by name
    // If it is not cached then call "Load(name)" to load the env.value
    // (the loaded name/value pair will then be cached, too)
    const string& Get(const string& name) const;

protected:
    // Call "::getenv()"
    virtual string Load(const string& name) const;

private:
    // Cached environment <name,value>
    mutable map<string, string> m_Cache;
};



///////////////////////////////////////////////////////
//  CNcbiArguments::   application command-line arguments & application name

class CNcbiArguments
{
public:
    // 'tors & assignment operator
    CNcbiArguments(int argc, const char* const* argv,
                   const string& program_name = NcbiEmptyString);
    virtual ~CNcbiArguments(void);

    CNcbiArguments(const CNcbiArguments& args);
    CNcbiArguments& operator= (const CNcbiArguments& args);

    // Delete all cached args & prog.name
    // Load new ones from "argc", "argv", "program_name"
    void Reset(int argc, const char* const* argv,
               const string& program_name = NcbiEmptyString);

    // Accessing and adding args
    SIZE_TYPE     Size       (void)          const { return m_Args.size(); }
    const string& operator[] (SIZE_TYPE pos) const { return m_Args[pos]; }
    void          Add        (const string& arg);    // add new arg

    // Program name
    const string& GetProgramName(void) const;
    string GetProgramBasename(void) const;
    string GetProgramDirname (void) const;  // (including last '/')
    void SetProgramName(const string& program_name);

private:
    string        m_ProgramName;  // if different from the default m_Args[0]
    deque<string> m_Args;
};


END_NCBI_SCOPE

#endif  /* NCBIENV__HPP */
