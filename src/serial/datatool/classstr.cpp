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
*   Type info for class generation: includes, used classes, C code etc.
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2000/02/02 18:54:15  vasilche
* Fixed variable coflict in MSVC.
* Added missing #include <serial/choice.hpp>
*
* Revision 1.1  2000/02/01 21:47:55  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.8  2000/01/10 19:46:47  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.7  1999/12/28 18:56:00  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.6  1999/12/17 19:05:19  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.5  1999/12/01 17:36:28  vasilche
* Fixed CHOICE processing.
*
* Revision 1.4  1999/11/18 17:13:07  vasilche
* Fixed generation of ENUMERATED CHOICE and VisibleString.
* Added generation of initializers to zero for primitive types and pointers.
*
* Revision 1.3  1999/11/16 15:41:17  vasilche
* Added plain pointer choice.
* By default we use C pointer instead of auto_ptr.
* Start adding initializers.
*
* Revision 1.2  1999/11/15 19:36:20  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <serial/tool/classstr.hpp>
#include <serial/tool/code.hpp>
#include <serial/tool/fileutil.hpp>

CClassTypeStrings::CClassTypeStrings(const string& externalName,
                                     const string& className)
    : m_ExternalName(externalName), m_ClassName(className)
{
}

CClassTypeStrings::~CClassTypeStrings(void)
{
}

void CClassTypeStrings::AddMember(const string& name,
                                  AutoPtr<CTypeStrings> type,
                                  bool optional,
                                  const string& defaultValue)
{
    m_Members.push_back(SMemberInfo(name, type, optional, defaultValue));
}

CClassTypeStrings::SMemberInfo::SMemberInfo(const string& name,
                                            AutoPtr<CTypeStrings> t,
                                            bool opt,
                                            const string& def)
    : externalName(name), cName(Identifier(name)),
      mName("m_"+cName), tName('T'+cName),
      type(t), optional(opt), defaultValue(def)
{
    if ( cName.empty() ) {
        mName = "m_data";
        tName = "Tdata";
    }
}

string CClassTypeStrings::GetCType(void) const
{
    return GetClassName();
}

string CClassTypeStrings::GetRef(void) const
{
    return '&'+GetClassName()+"::GetTypeInfo";
}

bool CClassTypeStrings::CanBeKey(void) const
{
    return false;
}

bool CClassTypeStrings::CanBeInSTL(void) const
{
    return false;
}

void CClassTypeStrings::SetParentClass(const string& className,
                                       const string& namespaceName,
                                       const string& fileName)
{
    m_ParentClassName = className;
    m_ParentClassNamespaceName = namespaceName;
    m_ParentClassFileName = fileName;
}

void CClassTypeStrings::GenerateTypeCode(CClassContext& ctx) const
{
    bool inClass = !ctx.GetMethodPrefix().empty();
    string codeClassName = GetClassName();
    if ( !inClass )
        codeClassName += "_Base";
    CClassCode code(ctx, codeClassName);
    code.SetParentClass(m_ParentClassName, m_ParentClassNamespaceName);

    code.ClassPublic() <<
        "    // type info\n"
        "    static const NCBI_NS_NCBI::CTypeInfo* GetTypeInfo(void);\n"
        "\n";

    GenerateClassCode(code,
                      inClass? code.ClassPublic(): code.ClassProtected(),
                      code.GetMethodPrefix(),
                      codeClassName);
}

void CClassTypeStrings::GenerateClassCode(CClassCode& code,
                                          CNcbiOstream& getters,
                                          const string& methodPrefix,
                                          const string& codeClassName) const
{
    // generate member methods
    {
        iterate ( TMembers, i, m_Members ) {
            i->type->GenerateTypeCode(code);
        }
    }

    // generate member types
    {
        code.ClassPublic() <<
            "    // members types\n";
        iterate ( TMembers, i, m_Members ) {
            code.ClassPublic() <<
                "    typedef "<<i->type->GetCType()<<" "<<i->tName<<";\n";
        }
        code.ClassPublic() << 
            "\n";
    }

    // generate member getters
    {
        getters <<
            "    // members\n";
        iterate ( TMembers, i, m_Members ) {
            getters <<
                "    const "<<i->tName<<"& Get"<<i->cName<<"(void) const\n"
                "        {\n"
                "            return "<<i->mName<<";\n"
                "        }\n";
            if ( i->type->CanBeInSTL() ) {
                getters <<
                    "    void Set"<<i->cName<<"(const "<<i->tName<<"& value)\n"
                    "        {\n"
                    "            "<<i->mName<<" = value;\n";
                if ( i->optional && i->type->NeedSetFlag() ) {
                    getters <<
                        "            m_set_"<<i->cName<<" = true;\n";
                }
                getters <<
                    "        }\n";
            }
            else {
                getters <<
                    "    "<<i->tName<<"& Set"<<i->cName<<"(void)\n"
                    "        {\n";
                if ( i->optional && i->type->NeedSetFlag() ) {
                    getters <<
                        "            m_set_"<<i->cName<<" = true;\n";
                }
                getters <<
                    "            return "<<i->mName<<";\n"
                    "        }\n";
            }
            if ( i->optional ) {
                if ( i->type->NeedSetFlag() ) {
                    getters <<
                        "    bool IsSet"<<i->cName<<"(void)\n"
                        "        {\n"
                        "            return m_set_"<<i->cName<<";\n"
                        "        }\n";
                    string assignValue;
                    string resetCode;
                    if ( !i->defaultValue.empty() ) {
                        assignValue = i->defaultValue;
                    }
                    else {
                        resetCode =
                            i->type->GetDestructionCode(i->mName) +
                            i->type->GetResetCode(i->mName);
                        if ( resetCode.empty() )
                            assignValue = i->type->GetInitializer();
                    }
                    if ( !assignValue.empty() ) {
                        // assign default value
                        getters <<
                            "    void Reset"<<i->cName<<"(void)\n"
                            "        {\n"
                            "            "<<i->mName<<" = "<<assignValue<<";\n"
                            "            m_set_"<<i->cName<<" = false;\n"
                            "        }\n";
                    }
                    else {
                        // no default value
                        getters <<
                            "    void Reset"<<i->cName<<"(void);\n";
                        code.Methods() <<
                            "void "<<methodPrefix<<"Reset"<<i->cName<<"(void)\n"
                            "{\n";
                        WriteTabbed(code.Methods(), resetCode);
                        code.Methods() <<
                            "    m_set_"<<i->cName<<" = false;\n"
                            "}\n"
                            "\n";
                    }
                }
                else {
                    // doesn't need set flag -> use special code
                    getters <<
                        "    bool IsSet"<<i->cName<<"(void)\n"
                        "        {\n"
                        "            return "<<i->type->GetIsSetCode(i->mName)<<";\n"
                        "        }\n"
                        "    void Reset"<<i->cName<<"(void);\n";
                    code.Methods() <<
                        "void "<<methodPrefix<<"Reset"<<i->cName<<"(void)\n"
                        "{\n";
                    WriteTabbed(code.Methods(),
                                i->type->GetDestructionCode(i->mName));
                    WriteTabbed(code.Methods(),
                                i->type->GetResetCode(i->mName));
                    code.Methods() <<
                        "}\n"
                        "\n";
                }
            }
            if ( i->cName.empty() ) {
                _ASSERT(!i->optional);
                getters <<
                    "    operator const "<<i->tName<<"& (void) const\n"
                    "        {\n"
                    "            return "<<i->mName<<";\n"
                    "        }\n"
                    "    operator "<<i->tName<<"& (void)\n"
                    "        {\n"
                    "            return "<<i->mName<<";\n"
                    "        }\n";
            }
            getters <<
                "\n";
        }
    }

    // generate member data
    {
        code.ClassPrivate() <<
            "    // members data\n";
		{
	        iterate ( TMembers, i, m_Members ) {
		        if ( i->optional && i->type->NeedSetFlag() ) {
			        code.ClassPrivate() <<
				        "    bool m_set_"<<i->cName<<";\n";
				}
			}
        }
		{
			iterate ( TMembers, i, m_Members ) {
				code.ClassPrivate() <<
	                "    "<<i->tName<<" "<<i->mName<<";\n";
		    }
		}
    }

    // generate member initializers/destructors
    {
		{
	        iterate ( TMembers, i, m_Members ) {
		        if ( i->optional && i->type->NeedSetFlag() ) {
			        code.AddInitializer("m_set_"+i->cName, "false");
				}
			}
        }
		{
			iterate ( TMembers, i, m_Members ) {
		        string init = i->defaultValue;
			    if ( init.empty() )
				    init = i->type->GetInitializer();
	            code.AddInitializer(i->mName, init);
		        code.AddDestructionCode(i->type->GetDestructionCode(i->mName));
			}
        }
    }

    // generate type info
    code.Methods() <<
        "const NCBI_NS_NCBI::CTypeInfo* "<<methodPrefix<<"GetTypeInfo(void)\n"
        "{\n"
        "    static NCBI_NS_NCBI::CClassInfoTmpl* info = 0;\n"
        "    if ( !info ) {\n"
        "        typedef "<<codeClassName<<" CClass_Base;\n"
        "        typedef "<<GetClassName()<<" CClass;\n"
        "        info = new NCBI_NS_NCBI::CClassInfo<CClass>(\""<<GetExternalName()<<"\");\n";
    if ( m_Members.size() == 1 && m_Members.front().cName.empty() ) {
        code.Methods() <<
            "        info->SetImplicit();\n";
    }
    {
        iterate ( TMembers, i, m_Members ) {
            code.Methods() <<
                "        "<<i->type->GetTypeInfoCode(i->externalName,
                                                     i->mName);
            if ( i->optional ) {
                if ( !i->defaultValue.empty() ) {
                    code.Methods() <<
                        "->SetDefault(new "<<i->tName<<"("<<i->defaultValue<<"))";
                }
                else {
                    code.Methods() <<
                        "->SetOptional()";
                }
                if ( i->type->NeedSetFlag() ) {
                    code.Methods() <<
                        "->SetSetFlag(MEMBER_PTR(m_set_"<<i->cName<<"))";
                }
            }
            code.Methods() <<
                ";\n";
        }
    }
    code.Methods() <<
        "    }\n"
        "    return info;\n"
        "}\n"
        "\n";
}

void CClassTypeStrings::GenerateUserHPPCode(CNcbiOstream& out) const
{
    out <<
        "class "<<GetClassName()<<" : public "<<GetClassName()<<"_Base\n"
        "{\n"
        "public:\n"
        "    // constructors/destructors\n"
        "    "<<GetClassName()<<"(void);\n"
        "    ~"<<GetClassName()<<"(void);\n"
        "\n"
        "};\n"
        "\n";
}

void CClassTypeStrings::GenerateUserCPPCode(CNcbiOstream& out) const
{
    out <<
        GetClassName()<<"::"<<GetClassName()<<"(void)\n"
        "{\n"
        "}\n"
        "\n"
        <<GetClassName()<<"::~"<<GetClassName()<<"(void)\n"
        "{\n"
        "}\n"
        "\n";
}

CClassRefTypeStrings::CClassRefTypeStrings(const string& className,
                                           const string& namespaceName,
                                           const string& fileName)
    : m_ClassName(className),
      m_NamespaceName(namespaceName),
      m_FileName(fileName)
{
}

void CClassRefTypeStrings::GenerateTypeCode(CClassContext& ctx) const
{
    ctx.HPPIncludes().insert(m_FileName);
}

void CClassRefTypeStrings::GeneratePointerTypeCode(CClassContext& ctx) const
{
    ctx.AddForwardDeclaration(m_ClassName, m_NamespaceName);
    ctx.CPPIncludes().insert(m_FileName);
}

string CClassRefTypeStrings::GetCType(void) const
{
    return m_NamespaceName + "::" + m_ClassName;
}

string CClassRefTypeStrings::GetRef(void) const
{
    return '&'+GetCType()+"::GetTypeInfo";
}

bool CClassRefTypeStrings::CanBeKey(void) const
{
    return false;
}

bool CClassRefTypeStrings::CanBeInSTL(void) const
{
    return false;
}
