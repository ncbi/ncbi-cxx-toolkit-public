#ifndef RESOLVER_01072004_HEADER
#define RESOLVER_01072004_HEADER

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

    static void LoadFrom(const string& file_path, CSymResolver* resolver);

    bool IsEmpty(void) const;

    static bool IsDefine(const string& param);
private:
    void Clear(void);
    void SetFrom(const CSymResolver& resolver);

    CSimpleMakeFileContents m_Data;

    CSimpleMakeFileContents::TContents m_Cache;
};


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2004/01/22 17:57:09  gorelenk
 * first version
 *
 * ===========================================================================
 */

#endif //RESOLVER_01072004_HEADER