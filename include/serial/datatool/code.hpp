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
* Revision 1.13  2005/04/26 14:18:50  vasilche
* Allow allocation of objects in CObjectMemoryPool.
*
* Revision 1.12  2004/05/03 19:31:21  gouriano
* Made generation of DOXYGEN-style comments optional
*
* Revision 1.11  2004/04/29 20:09:44  gouriano
* Generate DOXYGEN-style comments in C++ headers
*
* Revision 1.10  2002/12/23 18:38:52  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.9  2002/08/14 17:14:22  grichenk
* Fixed function name conflict on Win32: renamed
* GetClassName() -> GetClassNameDT()
*
* Revision 1.8  2000/08/25 15:58:46  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.7  2000/04/17 19:11:04  vasilche
* Fixed failed assertion.
* Removed redundant namespace specifications.
*
* Revision 1.6  2000/04/12 15:36:40  vasilche
* Added -on <namespace> argument to datatool.
* Removed unnecessary namespace specifications in generated files.
*
* Revision 1.5  2000/04/07 19:26:08  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.4  2000/03/07 20:04:57  vasilche
* Added NewInstance method to generated classes.
*
* Revision 1.3  2000/02/18 14:03:30  vasilche
* Fixed ?: error.
*
* Revision 1.2  2000/02/17 20:05:02  vasilche
* Inline methods now will be generated in *_Base.inl files.
* Fixed processing of StringStore.
* Renamed in choices: Selected() -> Which(), E_choice -> E_Choice.
* Enumerated values now will preserve case as in ASN.1 definition.
*
* Revision 1.1  2000/02/01 21:46:16  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.15  1999/12/29 16:01:50  vasilche
* Added explicit virtual destructors.
* Resolved overloading of InternalResolve.
*
* Revision 1.14  1999/11/19 15:48:09  vasilche
* Modified AutoPtr template to allow its use in STL containers (map, vector etc.)
*
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
#include <serial/datatool/classctx.hpp>
#include <serial/datatool/namespace.hpp>
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
