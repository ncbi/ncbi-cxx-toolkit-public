#ifndef CODE_HPP
#define CODE_HPP

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
*   Class code generator
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.13  1999/11/18 17:13:06  vasilche
* Fixed generation of ENUMERATED CHOICE and VisibleString.
* Added generation of initializers to zero for primitive types and pointers.
*
* Revision 1.12  1999/11/15 19:36:14  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <set>
#include <map>
#include <list>
#include "autoptr.hpp"

USING_NCBI_SCOPE;

class CDataType;
class CChoiceDataType;
class CFileCode;

class CClassCode
{
public:
    typedef set<string> TIncludes;
    typedef map<string, string> TForwards;
    typedef list< pair<string, string> > TMethods;

    CClassCode(CFileCode& file, const string& typeName, const CDataType* type);
    ~CClassCode(void);
    
    const string& GetTypeName(void) const
        {
            return m_TypeName;
        }
    const CDataType* GetType(void) const
        {
            return m_Type;
        }

    const string& GetNamespace(void) const
        {
            return m_Namespace;
        }
    const string& GetClassName(void) const
        {
            return m_ClassName;
        }

    const CDataType* GetParentType(void) const;
    string GetParentClass(void) const;

    enum EClassType {
        eNormal,
        eAbstract,
        eEnum,
        eAlias
    };
    void SetClassType(EClassType type);
    EClassType GetClassType(void) const
        {
            return m_ClassType;
        }

    void AddHPPInclude(const string& s);
    void AddCPPInclude(const string& s);
    void AddForwardDeclaration(const string& s, const string& ns);
    void AddHPPIncludes(const TIncludes& includes);
    void AddCPPIncludes(const TIncludes& includes);
    void AddForwardDeclarations(const TForwards& forwards);
    void AddInitializer(const string& member, const string& init);

    CNcbiOstream& ClassPublic(void)
        {
            return m_ClassPublic;
        }
    CNcbiOstream& ClassPrivate(void)
        {
            return m_ClassPrivate;
        }
    CNcbiOstream& Methods(void)
        {
            return m_Methods;
        }
    CNcbiOstream& TypeInfoBody(void)
        {
            return m_TypeInfoBody;
        }

    CNcbiOstream& GenerateHPP(CNcbiOstream& header) const;
    CNcbiOstream& GenerateCPP(CNcbiOstream& code) const;
    CNcbiOstream& GenerateUserHPP(CNcbiOstream& header) const;
    CNcbiOstream& GenerateUserCPP(CNcbiOstream& code) const;

private:
    CFileCode& m_Code;
    string m_TypeName;
    const CDataType* m_Type;
    string m_Namespace;
    string m_ClassName;
    string m_ParentClass;

    CNcbiOstrstream m_ClassPublic;
    CNcbiOstrstream m_ClassPrivate;
    CNcbiOstrstream m_Methods;
    CNcbiOstrstream m_Initializers;
    CNcbiOstrstream m_TypeInfoBody;

    EClassType m_ClassType;
};

#endif
