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
*/

#include <corelib/ncbistd.hpp>
#include "classctx.hpp"
#include "namespace.hpp"
#include <list>

BEGIN_NCBI_SCOPE

class CDataType;
class CFileCode;

class CClassCode : public CClassContext
{
public:
    CClassCode(CClassContext& ownerClass, const string& className);
    virtual ~CClassCode(void);

    const CNamespace& GetNamespace(void) const;

    // DT added to avoid conflict with the standard Windows interfaces
    const string& GetClassNameDT(void) const
        {
            return m_ClassName;
        }
    const string& GetParentClassName(void) const
        {
            return m_ParentClassName;
        }
    const CNamespace& GetParentClassNamespace(void) const
        {
            return m_ParentClassNamespace;
        }

    void SetParentClass(const string& className, const CNamespace& ns);
    bool HaveVirtualDestructor(void) const
        {
            return m_VirtualDestructor;
        }
    void SetVirtualDestructor(bool v = true)
        {
            m_VirtualDestructor = v;
        }

    string GetMethodPrefix(void) const;
    bool InternalClass(void) const;
    TIncludes& HPPIncludes(void);
    TIncludes& CPPIncludes(void);
    void AddForwardDeclaration(const string& s, const CNamespace& ns);
    void AddInitializer(const string& member, const string& init);
    void AddConstructionCode(const string& code);
    void AddDestructionCode(const string& code);

    bool HaveInitializers(void) const;
    CNcbiOstream& WriteInitializers(CNcbiOstream& out) const;
    CNcbiOstream& WriteConstructionCode(CNcbiOstream& out) const;
    CNcbiOstream& WriteDestructionCode(CNcbiOstream& out) const;

    CNcbiOstream& ClassPublic(void)
        {
            return m_ClassPublic;
        }
    CNcbiOstream& ClassProtected(void)
        {
            return m_ClassProtected;
        }
    CNcbiOstream& ClassPrivate(void)
        {
            return m_ClassPrivate;
        }
    CNcbiOstream& InlineMethods(void)
        {
            return m_InlineMethods;
        }
    CNcbiOstream& Methods(bool inl = false)
        {
            return inl? m_InlineMethods: m_Methods;
        }
    CNcbiOstream& MethodStart(bool inl = false)
        {
			if ( inl ) {
				m_InlineMethods << "inline\n";
				return m_InlineMethods;
			}
			else
				return m_Methods;
        }

    CNcbiOstream& GenerateHPP(CNcbiOstream& header) const;
    CNcbiOstream& GenerateINL(CNcbiOstream& code) const;
    CNcbiOstream& GenerateCPP(CNcbiOstream& code) const;
    CNcbiOstream& GenerateUserHPP(CNcbiOstream& header) const;
    CNcbiOstream& GenerateUserCPP(CNcbiOstream& code) const;

    void AddHPPCode(const CNcbiOstrstream& code);
    void AddINLCode(const CNcbiOstrstream& code);
    void AddCPPCode(const CNcbiOstrstream& code);

    static void SetExportSpecifier(const string& str);
    static const string& GetExportSpecifier(void);

    static void SetDoxygenComments(bool set);
    static bool GetDoxygenComments(void);

    static void SetDoxygenGroup(const string& str);
    static const string& GetDoxygenGroup(void);

    static void SetDocRootURL(const string& str);
    static const string& GetDocRootURL(void);

private:
    CClassContext& m_Code;
    string m_ClassName;
    string m_ParentClassName;
    CNamespace m_ParentClassNamespace;
    static string sm_ExportSpecifier;
    static bool   sm_DoxygenComments;
    static string sm_DoxygenGroup;
    static string sm_DocRootURL;

    bool m_VirtualDestructor;
    CNcbiOstrstream m_ClassPublic;
    CNcbiOstrstream m_ClassProtected;
    CNcbiOstrstream m_ClassPrivate;
    CNcbiOstrstream m_Initializers;
    list<string> m_ConstructionCode;
    list<string> m_DestructionCode;
    CNcbiOstrstream m_InlineMethods;
    CNcbiOstrstream m_Methods;

    CClassCode(const CClassCode&);
    CClassCode& operator=(const CClassCode&);
};

END_NCBI_SCOPE

#endif
