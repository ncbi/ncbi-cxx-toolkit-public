#ifndef FILECODE_HPP
#define FILECODE_HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*   C++ file generator
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.5  1999/12/29 16:01:50  vasilche
* Added explicit virtual destructors.
* Resolved overloading of InternalResolve.
*
* Revision 1.4  1999/12/20 21:00:17  vasilche
* Added generation of sources in different directories.
*
* Revision 1.3  1999/11/19 15:48:10  vasilche
* Modified AutoPtr template to allow its use in STL containers (map, vector etc.)
*
* Revision 1.2  1999/11/15 19:36:15  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiutil.hpp>
#include <map>
#include <set>

USING_NCBI_SCOPE;

class CDataType;
class CClassCode;

class CFileCode
{
public:
    typedef set<string> TIncludes;
    typedef map<string, string> TForwards;
    typedef map<string, AutoPtr<CClassCode> > TClasses;

    CFileCode(const string& baseName);
    ~CFileCode(void);

    bool AddType(const CDataType* type);

    string Include(const string& s) const;
    const string& GetFileBaseName(void) const
        {
            return m_BaseName;
        }
    const string& GetHeaderPrefix(void) const
        {
            return m_HeaderPrefix;
        }
    string GetHPPName(void) const;
    string GetCPPName(void) const;
    string GetUserHPPName(void) const;
    string GetUserCPPName(void) const;
    string GetBaseDefine(void) const;
    string GetHPPDefine(void) const;
    string GetUserHPPDefine(void) const;

    void AddHPPInclude(const string& s);
    void AddCPPInclude(const string& s);
    void AddForwardDeclaration(const string& cls,
                               const string& ns = NcbiEmptyString);
    void AddHPPIncludes(const TIncludes& includes);
    void AddCPPIncludes(const TIncludes& includes);
    void AddForwardDeclarations(const TForwards& forwards);

    void GenerateHPP(const string& path) const;
    void GenerateCPP(const string& path) const;
    bool GenerateUserHPP(const string& path) const;
    bool GenerateUserCPP(const string& path) const;

private:
    // file names
    string m_BaseName;
    string m_HeaderPrefix;

    TIncludes m_HPPIncludes;
    TIncludes m_CPPIncludes;
    TForwards m_ForwardDeclarations;

    // classes code
    TClasses m_Classes;
    
    CFileCode(const CFileCode&);
    CFileCode& operator=(const CFileCode&);
};

class CNamespace
{
public:
    CNamespace(CNcbiOstream& out);
    ~CNamespace(void);
    
    void Set(const string& ns);

    void End(void);

private:
    void Begin(void);

    CNcbiOstream& m_Out;
    string m_Namespace;
};

#endif
