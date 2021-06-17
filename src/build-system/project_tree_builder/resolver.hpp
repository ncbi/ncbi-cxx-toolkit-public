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

#include "stl_msvc_usage.hpp"
#include "file_contents.hpp"

#include <map>
#include <set>
#include <string>

#include <corelib/ncbistre.hpp>
#include <corelib/ncbienv.hpp>


BEGIN_NCBI_SCOPE


class CSymResolver;
class CExpansionRule
{
public:
    enum EExpRule {
        eNoop,
        eReplace,
        ePattern
    };

    CExpansionRule(void);
    CExpansionRule(const string& textrule);
    CExpansionRule(const CExpansionRule& other);
    CExpansionRule& operator= (const CExpansionRule& other);

    void Reset();
    void Init(const string& textrule,
        CSymResolver* resolver=NULL,  const CSimpleMakeFileContents* data=NULL);

    string ApplyRule( const string& value) const;
    void  ApplyRule( list<string>& value) const;

private:
    EExpRule m_Rule;
    string m_Lvalue;
    string m_Rvalue;
};

class CSymResolver
{
public:
    CSymResolver(void);
    CSymResolver(const CSymResolver& resolver);
    CSymResolver& operator= (const CSymResolver& resolver);
    CSymResolver(const string& file_path);
    ~CSymResolver(void);

    void Resolve(const string& define, list<string>* resolved_def);
    void Resolve(const string& define, list<string>* resolved_def,
                        const CSimpleMakeFileContents& data);

    CSymResolver& Append(const CSymResolver& src, bool warn_redef=false);

    static void LoadFrom(const string& file_path, CSymResolver* resolver);
    void AddDefinition( const string& key, const string& value);
    bool HasDefinition( const string& key) const;
    bool GetValue(const string& key, string& value) const
    {
        return m_Data.GetValue(key,value);
    }

    bool IsEmpty(void) const;

    static bool   StripSuffix(string& libname, string* suffix=nullptr);
    static bool   IsDefine   (const string& param);
    static bool   HasDefine   (const string& param);
    static string StripDefine(const string& define);
    static string TrimDefine(const string& define);
    string FilterDefine(const string& define, CExpansionRule& rule,
                        const CSimpleMakeFileContents& data);

private:
    void Clear(void);
    void SetFrom(const CSymResolver& resolver);

    CSimpleMakeFileContents m_Data;

    CSimpleMakeFileContents::TContents m_Cache;
    set<string> m_Trusted;
};

// Filter opt defines like $(SRC_C:.core_%)           to $(SRC_C).
// or $(OBJMGR_LIBS:dbapi_driver=dbapi_driver-static) to $(OBJMGR_LIBS)
string FilterDefine(const string& define);


END_NCBI_SCOPE

#endif //PROJECT_TREE_BUILDER__RESOLVER__HPP
