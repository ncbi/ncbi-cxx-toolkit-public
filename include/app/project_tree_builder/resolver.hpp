#ifndef PROJECT_TREE_BUILDER__RESOLVER__HPP
#define PROJECT_TREE_BUILDER__RESOLVER__HPP

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

#include <app/project_tree_builder/stl_msvc_usage.hpp>
#include <app/project_tree_builder/file_contents.hpp>

#include <map>
#include <string>

#include <corelib/ncbistre.hpp>
#include <corelib/ncbienv.hpp>


BEGIN_NCBI_SCOPE


class CSymResolver
{
public:
    CSymResolver(void);
    CSymResolver(const CSymResolver& resolver);
    CSymResolver& operator= (const CSymResolver& resolver);
    CSymResolver(const string& file_path);
    ~CSymResolver(void);

    void Resolve(const string& define, list<string>* resolved_def);

    CSymResolver& operator+= (const CSymResolver& src);

    static void LoadFrom(const string& file_path, CSymResolver* resolver);

    bool IsEmpty(void) const;

    static bool   IsDefine   (const string& param);
    static string StripDefine(const string& define);

private:
    void Clear(void);
    void SetFrom(const CSymResolver& resolver);

    CSimpleMakeFileContents m_Data;

    CSimpleMakeFileContents::TContents m_Cache;
};

// Filter opt defines like $(SRC_C:.core_%)           to $(SRC_C).
// or $(OBJMGR_LIBS:dbapi_driver=dbapi_driver-static) to $(OBJMGR_LIBS)
string FilterDefine(const string& define);


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2004/06/10 15:12:55  gorelenk
 * Added newline at the file end to avoid GCC warning.
 *
 * Revision 1.4  2004/02/04 23:11:43  gorelenk
 * StripDefine helper promoted to class CSymResolver member. FilterDefine was
 * moved here from proj_src_resolver.cpp module.
 *
 * Revision 1.3  2004/01/22 17:57:09  gorelenk
 * first version
 *
 * ===========================================================================
 */

#endif //PROJECT_TREE_BUILDER__RESOLVER__HPP
