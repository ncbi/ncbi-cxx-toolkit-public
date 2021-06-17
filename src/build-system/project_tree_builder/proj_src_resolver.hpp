#ifndef PROJECT_TREE_BUILDER__PROJ_SRC_RESOLVER__HPP
#define PROJECT_TREE_BUILDER__PROJ_SRC_RESOLVER__HPP

/* $Id$
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
 * Author:  Viatcheslav Gorelenkov
 *
 */


/// Resolver for SRC keys in Makefiles:

#include "resolver.hpp"


#include <corelib/ncbienv.hpp>
BEGIN_NCBI_SCOPE


/// Collect sources in resolve macrodefines
class CProjSRCResolver
{
public:
    CProjSRCResolver(const string&       applib_mfilepath,
                     const string&       source_base_dir,
                     const list<string>& sources_src);
    
    ~CProjSRCResolver(void);

    void ResolveTo(list<string>* sources_dst);

private:
    const string        m_MakefilePath;
    const string        m_SourcesBaseDir;
    const list<string>& m_SourcesSrc;
    list<string>        m_MakefileDirs;

    CSymResolver m_Resolver;
    void PrepareResolver(void);

    //no value type semantics
    CProjSRCResolver(void);
    CProjSRCResolver(const CProjSRCResolver&);
    CProjSRCResolver& operator=(const CProjSRCResolver&);
};


END_NCBI_SCOPE

#endif //PROJECT_TREE_BUILDER__PROJ_SRC_RESOLVER__HPP
