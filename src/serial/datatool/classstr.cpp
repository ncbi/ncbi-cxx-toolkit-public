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
* Revision 1.22  2000/07/11 20:36:28  vasilche
* Removed unnecessary generation of namespace references for enum members.
* Removed obsolete methods.
*
* Revision 1.21  2000/06/27 16:34:47  vasilche
* Fixed generated comments.
* Fixed class names conflict. Now internal classes' names begin with "C_".
*
* Revision 1.20  2000/06/27 13:22:00  vasilche
* Added const modifier to generated IsSet*() methods.
*
* Revision 1.19  2000/06/16 16:31:38  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.18  2000/05/03 14:38:18  vasilche
* SERIAL: added support for delayed reading to generated classes.
* DATATOOL: added code generation for delayed reading.
*
* Revision 1.17  2000/04/17 19:11:07  vasilche
* Fixed failed assertion.
* Removed redundant namespace specifications.
*
* Revision 1.16  2000/04/12 15:36:49  vasilche
* Added -on <namespace> argument to datatool.
* Removed unnecessary namespace specifications in generated files.
*
* Revision 1.15  2000/04/07 19:26:24  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.14  2000/04/06 16:11:25  vasilche
* Removed unneeded calls to Reset().
*
* Revision 1.13  2000/04/03 18:47:30  vasilche
* Added main include file for generated headers.
* serialimpl.hpp is included in generated sources with GetTypeInfo methods
*
* Revision 1.12  2000/03/29 15:52:26  vasilche
* Generated files names limited to 31 symbols due to limitations of Mac.
* Removed unions with only one member.
*
* Revision 1.11  2000/03/17 16:49:55  vasilche
* Added copyright message to generated files.
* All objects pointers in choices now share the only CObject pointer.
* All setters/getters made public until we'll find better solution.
*
* Revision 1.10  2000/03/15 14:18:39  vasilche
* Forgot to fix _ASSERT() expression.
*
* Revision 1.9  2000/03/14 18:32:58  vasilche
* Fixed class includes generation.
*
* Revision 1.8  2000/03/14 14:43:10  vasilche
* All OPTIONAL members implemented via CRef<> by default.
*
* Revision 1.7  2000/03/08 14:40:00  vasilche
* Renamed NewInstance() -> New().
*
* Revision 1.6  2000/03/07 20:05:00  vasilche
* Added NewInstance method to generated classes.
*
* Revision 1.5  2000/03/07 14:06:30  vasilche
* Added generation of reference counted objects.
*
* Revision 1.4  2000/02/17 20:05:06  vasilche
* Inline methods now will be generated in *_Base.inl files.
* Fixed processing of StringStore.
* Renamed in choices: Selected() -> Which(), E_choice -> E_Choice.
* Enumerated values now will preserve case as in ASN.1 definition.
*
* Revision 1.3  2000/02/02 19:08:20  vasilche
* Fixed variable conflict in generated files on MSVC.
*
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

BEGIN_NCBI_SCOPE

#define SET_PREFIX "m_set_"
#define DELAY_PREFIX "m_delay_"

CClassTypeStrings::CClassTypeStrings(const string& externalName,
                                     const string& className)
    : m_IsObject(true), m_HaveUserClass(true),
      m_ExternalName(externalName), m_ClassName(className)
{
}

CClassTypeStrings::~CClassTypeStrings(void)
{
}

CTypeStrings::EKind CClassTypeStrings::GetKind(void) const
{
    return m_IsObject? eKindObject: eKindClass;
}

void CClassTypeStrings::AddMember(const string& name,
                                  const AutoPtr<CTypeStrings>& type,
                                  const string& pointerType,
                                  bool optional,
                                  const string& defaultValue,
                                  bool delayed)
{
    m_Members.push_back(SMemberInfo(name, type,
                                    pointerType,
                                    optional, defaultValue,
                                    delayed));
}

CClassTypeStrings::SMemberInfo::SMemberInfo(const string& name,
                                            const AutoPtr<CTypeStrings>& t,
                                            const string& pType,
                                            bool opt,
                                            const string& defValue,
                                            bool del)
    : externalName(name), cName(Identifier(name)),
      mName("m_"+cName), tName('T'+cName),
      type(t), ptrType(pType),
      optional(opt), delayed(del), defaultValue(defValue)
{
    if ( cName.empty() ) {
        mName = "m_data";
        tName = "Tdata";
    }

    bool haveDefault = !defaultValue.empty();
    if ( optional && !haveDefault ) {
        // true optional CObject type should be implemented as CRef
        if ( type->GetKind() == eKindObject && ptrType.empty() ) {
            ptrType = "Ref";
        }
    }
    
    if ( ptrType == "Ref" ) {
        ref = true;
    }
    else if ( ptrType.empty() ) {
        ref = false;
    }
    else {
        _ASSERT("Unknown reference type: "+ref);
    }
    
    if ( ref ) {
        valueName = "(*"+mName+")";
        haveFlag = false;
    }
    else {
        valueName = mName;
        haveFlag = optional && type->NeedSetFlag();
    }
    if ( haveDefault ) // cannot detect DEFAULT value
        haveFlag = true;
    
    canBeNull = ref && optional && !haveFlag;
}

string CClassTypeStrings::GetCType(const CNamespace& /*ns*/) const
{
    return GetClassName();
}

string CClassTypeStrings::GetRef(const CNamespace& /*ns*/) const
{
    return "CLASS, ("+GetClassName()+')';
}

string CClassTypeStrings::NewInstance(const string& init) const
{
    return GetCType(CNamespace::KEmptyNamespace)+"::New("+init+')';
}

string CClassTypeStrings::GetResetCode(const string& var) const
{
    return var+".Reset();\n";
}

void CClassTypeStrings::SetParentClass(const string& className,
                                       const CNamespace& ns,
                                       const string& fileName)
{
    m_ParentClassName = className;
    m_ParentClassNamespace = ns;
    m_ParentClassFileName = fileName;
}

static
CNcbiOstream& DeclareStaticConstructor(CNcbiOstream& out,
                                       const string className)
{
    return out <<
        "    // constructor for static/automatic objects\n"
        "    "<<className<<"(void);\n";
}

static
CNcbiOstream& DeclareHeapConstructor(CNcbiOstream& out, const string className)
{
    return out <<
        "    // hidden constructor for dynamic objects\n"
        "    "<<className<<"(ECanDelete);\n"
        "\n";
}

static
CNcbiOstream& DeclareDestructor(CNcbiOstream& out, const string className,
                                bool virt)
{
    out <<
        "    // destructor\n"
        "    ";
    if ( virt )
        out << "virtual ";
    return out << '~'<<className<<"(void);\n"
        "\n";
}

static
CNcbiOstream& GenerateNewMethod(CNcbiOstream& out, const string& className)
{
    return out <<
        "    // create dynamically allocated object\n"
        "    static "<<className<<"* New(void)\n"
        "        {\n"
        "            return new "<<className<<"(eCanDelete);\n"
        "        }\n"
        "\n";
}

void CClassTypeStrings::GenerateTypeCode(CClassContext& ctx) const
{
    bool haveUserClass = HaveUserClass();
    string codeClassName = GetClassName();
    if ( haveUserClass )
        codeClassName += "_Base";
    CClassCode code(ctx, codeClassName);
    if ( m_ParentClassName.empty() ) {
        code.SetParentClass("CObject", CNamespace::KNCBINamespace);
    }
    else {
        code.SetParentClass(m_ParentClassName, m_ParentClassNamespace);
    }
    string methodPrefix = code.GetMethodPrefix();

    DeclareStaticConstructor(code.ClassPublic(), codeClassName);
    DeclareHeapConstructor(code.ClassProtected(), codeClassName);
    DeclareDestructor(code.ClassPublic(), codeClassName, haveUserClass);

    if ( !haveUserClass ) {
        GenerateNewMethod(code.ClassPublic(), codeClassName);
    }

    string ncbiNamespace =
        code.GetNamespace().GetNamespaceRef(CNamespace::KNCBINamespace);

    code.ClassPublic() <<
        "    // type info\n"
        "    DECLARE_INTERNAL_TYPE_INFO();\n"
        "\n";

    GenerateClassCode(code,
                      code.ClassPublic(),
                      methodPrefix, haveUserClass, ctx.GetMethodPrefix());

    // constructors/destructor code
    code.Methods() <<
        "// constructor for static/automatic objects\n"<<
        methodPrefix<<codeClassName<<"(void)\n";
    if ( code.HaveInitializers() ) {
        code.Methods() <<
            "    : ";
        code.WriteInitializers(code.Methods());
        code.Methods() << '\n';
    }
    code.Methods() <<
        "{\n"
        "}\n"
        "\n";

    code.Methods() <<
        "// constructor for dynamic objects\n"<<
        methodPrefix<<codeClassName<<"(NCBI_NS_NCBI::CObject::ECanDelete)\n";
    code.Methods() <<
        "    : Tparent(NCBI_NS_NCBI::CObject::eCanDelete)";
    if ( code.HaveInitializers() ) {
        code.Methods() << ", ";
        code.WriteInitializers(code.Methods());
    }
    code.Methods() <<
        "\n"
        "{\n"
        "}\n"
        "\n";

    code.Methods() <<
        "// destructor\n"<<
        methodPrefix<<"~"<<codeClassName<<"(void)\n"
        "{\n";
    code.WriteDestructionCode(code.Methods());
    code.Methods() <<
        "}\n"
        "\n";
}

void CClassTypeStrings::GenerateClassCode(CClassCode& code,
                                          CNcbiOstream& setters,
                                          const string& methodPrefix,
                                          bool haveUserClass,
                                          const string& classPrefix) const
{
    bool delayed = false;
    // generate member methods
    {
        iterate ( TMembers, i, m_Members ) {
            if ( i->ref ) {
                i->type->GeneratePointerTypeCode(code);
            }
            else {
                i->type->GenerateTypeCode(code);
            }
            if ( i->delayed )
                delayed = true;
        }
    }
    if ( delayed )
        code.HPPIncludes().insert("serial/delaybuf");

    // generate member types
    {
        code.ClassPublic() <<
            "    // members' types\n";
        iterate ( TMembers, i, m_Members ) {
            string cType = i->type->GetCType(code.GetNamespace());
            code.ClassPublic() <<
                "    typedef "<<cType<<" "<<i->tName<<";\n";
        }
        code.ClassPublic() << 
            "\n";
    }

    string ncbiNamespace =
        code.GetNamespace().GetNamespaceRef(CNamespace::KNCBINamespace);

    CNcbiOstream& methods = code.Methods();
    CNcbiOstream& inlineMethods = code.InlineMethods();

    bool generateMainReset = true;
    // generate member getters & setters
    {
        code.ClassPublic() <<
            "    // members' getters\n";
        setters <<
            "    // members' setters\n";
        iterate ( TMembers, i, m_Members ) {
            // generate IsSet... method
            if ( i->optional ) {
                code.ClassPublic() <<
                    "    bool IsSet"<<i->cName<<"(void) const;\n";
                inlineMethods <<
                    "inline\n"
                    "bool "<<methodPrefix<<"IsSet"<<i->cName<<"(void) const\n"
                    "{\n";
                if ( i->haveFlag ) {
                    // use special boolean flag
                    inlineMethods <<
                        "    return "SET_PREFIX<<i->cName<<";\n";
                }
                else {
                    if ( i->delayed ) {
                        inlineMethods <<
                            "    if ( "DELAY_PREFIX<<i->cName<<" )\n"
                            "        return true;\n";
                    }
                    if ( i->ref ) {
                        // CRef
                        inlineMethods <<
                            "    return "<<i->mName<<";\n";
                    }
                    else {
                        // doesn't need set flag -> use special code
                        inlineMethods <<
                            "    return "<<i->type->GetIsSetCode(i->mName)<<";\n";
                    }
                }
                inlineMethods <<
                    "}\n"
                    "\n";
            }
            
            // generate Reset... method
            string destructionCode = i->type->GetDestructionCode(i->valueName);
            string assignValue = i->defaultValue;
            string resetCode;
            if ( assignValue.empty() && !i->ref ) {
                resetCode = i->type->GetResetCode(i->valueName);
                if ( resetCode.empty() )
                    assignValue = i->type->GetInitializer();
            }
            if ( i->cName.empty() ) {
                if ( m_Members.size() != 1 ) {
                    THROW1_TRACE(runtime_error,
                                 "unnamed member is not the only member");
                }
                generateMainReset = false;
            }
            else {
                setters <<
                    "    void Reset"<<i->cName<<"(void);\n";
            }
            // inline only when non reference and doesn't have reset code
            bool inl = !i->ref && resetCode.empty();
            code.MethodStart(inl) <<
                "void "<<methodPrefix<<"Reset"<<i->cName<<"(void)\n"
                "{\n";
            if ( i->delayed ) {
                code.Methods(inl) <<
                    "    "DELAY_PREFIX<<i->cName<<".Forget();\n";
            }
            WriteTabbed(code.Methods(inl), destructionCode);
            if ( i->ref ) {
                if ( !i->optional ) {
                    // just reset value
                    resetCode = i->type->GetResetCode(i->valueName);
                    if ( !resetCode.empty() ) {
                        WriteTabbed(code.Methods(inl), resetCode);
                    }
                    else {
                        code.Methods(inl) <<
                            "    "<<i->valueName<<" = "<<i->type->GetInitializer()<<";\n";
                    }
                }
                else if ( assignValue.empty() ) {
                    // plain OPTIONAL
                    code.Methods(inl) <<
                        "    "<<i->mName<<".Reset();\n";
                }
                else {
                    if ( assignValue.empty() )
                        assignValue = i->type->GetInitializer();
                    // assign default value
                    code.Methods(inl) <<
                        "    "<<i->mName<<".Reset(new "<<i->tName<<"("<<assignValue<<"));\n";
                }
            }
            else {
                if ( !assignValue.empty() ) {
                    // assign default value
                    code.Methods(inl) <<
                        "    "<<i->mName<<" = "<<assignValue<<";\n";
                }
                else {
                    // no default value
                    WriteTabbed(code.Methods(inl), resetCode);
                }
            }
            if ( i->haveFlag ) {
                code.Methods(inl) <<
                    "    "SET_PREFIX<<i->cName<<" = false;\n";
            }
            code.Methods(inl) <<
                "}\n"
                "\n";

            string cType = i->type->GetCType(code.GetNamespace());
            // generate getter
            code.ClassPublic() <<
                "    const "<<cType<<"& Get"<<i->cName<<"(void) const;\n";
            inl = !i->ref;
            code.MethodStart(inl) <<
                "const "<<methodPrefix<<i->tName<<"& "<<methodPrefix<<"Get"<<i->cName<<"(void) const\n"
                "{\n";
            if ( i->delayed ) {
                code.Methods(inl) <<
                    "    "DELAY_PREFIX<<i->cName<<".Update();\n";
            }
            code.Methods(inl) <<
                "    return "<<i->valueName<<";\n"
                "}\n"
                "\n";

            // generate setter
            if ( i->ref ) {
                // generate reference setter
                setters <<
                    "    void Set"<<i->cName<<"(const "<<ncbiNamespace<<"CRef< "<<cType<<" >& value);\n";
                methods <<
                    "void "<<methodPrefix<<"Set"<<i->cName<<"(const NCBI_NS_NCBI::CRef< "<<i->tName<<" >& value)\n"
                    "{\n";
                if ( i->delayed ) {
                    methods <<
                        "    "DELAY_PREFIX<<i->cName<<".Forget();\n";
                }
                if ( !i->canBeNull ) {
                    methods <<
                        "    "<<i->mName<<".Reset(&*value); // assure non null value\n";
                }
                else {
                    methods <<
                        "    "<<i->mName<<" = value;\n";
                }
                if ( i->haveFlag ) {
                    methods <<
                        "    "SET_PREFIX<<i->cName<<" = true;\n";
                }
                methods <<
                    "}\n"
                    "\n";
                setters <<
                    "    "<<cType<<"& Set"<<i->cName<<"(void);\n";
                if ( i->canBeNull ) {
                    // we have to init ref before returning
                    _ASSERT(!i->haveFlag);
                    methods <<
                        methodPrefix<<i->tName<<"& "<<methodPrefix<<"Set"<<i->cName<<"(void)\n"
                        "{\n";
                    if ( i->delayed ) {
                        methods <<
                            "    "DELAY_PREFIX<<i->cName<<".Update();\n";
                    }
                    methods <<
                        "    if ( !"<<i->mName<<" )\n"
                        "        "<<i->mName<<".Reset("<<i->type->NewInstance(NcbiEmptyString)<<");\n"
                        "    return "<<i->valueName<<";\n"
                        "}\n"
                        "\n";
                }
                else {
                    // value already not null -> simple inline method
                    inlineMethods <<
                        "inline\n"<<
                        methodPrefix<<i->tName<<"& "<<methodPrefix<<"Set"<<i->cName<<"(void)\n"
                        "{\n";
                    if ( i->delayed ) {
                        inlineMethods <<
                            "    "DELAY_PREFIX<<i->cName<<".Update();\n";
                    }
                    if ( i->haveFlag ) {
                        inlineMethods <<
                            "    "SET_PREFIX<<i->cName<<" = true;\n";
                    }
                    inlineMethods <<
                        "    return "<<i->valueName<<";\n"
                        "}\n"
                        "\n";
                }
            }
            else {
                if ( i->type->CanBeCopied() ) {
                    setters <<
                        "    void Set"<<i->cName<<"(const "<<cType<<"& value);\n";
                    inlineMethods <<
                        "inline\n"
                        "void "<<methodPrefix<<"Set"<<i->cName<<"(const "<<i->tName<<"& value)\n"
                        "{\n";
                    if ( i->delayed ) {
                        inlineMethods <<
                            "    "DELAY_PREFIX<<i->cName<<".Forget();\n";
                    }
                    inlineMethods <<                        
                        "    "<<i->valueName<<" = value;\n";
                    if ( i->haveFlag ) {
                        inlineMethods <<
                            "    "SET_PREFIX<<i->cName<<" = true;\n";
                    }
                    inlineMethods <<
                        "}\n"
                        "\n";
                }
                setters <<
                    "    "<<cType<<"& Set"<<i->cName<<"(void);\n";
                inlineMethods <<
                    "inline\n"<<
                    methodPrefix<<i->tName<<"& "<<methodPrefix<<"Set"<<i->cName<<"(void)\n"
                    "{\n";
                if ( i->delayed ) {
                    inlineMethods <<
                        "    "DELAY_PREFIX<<i->cName<<".Update();\n";
                }
                if ( i->haveFlag ) {
                    inlineMethods <<
                        "    "SET_PREFIX<<i->cName<<" = true;\n";
                }
                inlineMethods <<
                    "    return "<<i->valueName<<";\n"
                    "}\n"
                    "\n";
            }


            // generate conversion operators
            if ( i->cName.empty() ) {
                if ( i->optional ) {
                    THROW1_TRACE(runtime_error, "the only member of adaptor class is optional");
                }
                code.ClassPublic() <<
                    "    operator const "<<cType<<"& (void) const;\n"
                    "    operator "<<cType<<"& (void);\n";
                inlineMethods <<
                    "inline\n"<<
                    methodPrefix<<"operator const "<<methodPrefix<<i->tName<<"& (void) const\n"
                    "{\n";
                if ( i->delayed ) {
                    inlineMethods <<
                        "    "DELAY_PREFIX<<i->cName<<".Update();\n";
                }
                inlineMethods <<
                    "    return "<<i->mName<<";\n"
                    "}\n"
                    "\n"
                    "inline\n"<<
                    methodPrefix<<"operator "<<methodPrefix<<i->tName<<"& (void)\n"
                    "{\n";
                if ( i->delayed ) {
                    inlineMethods <<
                        "    "DELAY_PREFIX<<i->cName<<".Update();\n";
                }
                inlineMethods <<
                    "    return "<<i->mName<<";\n"
                    "}\n"
                    "\n";
            }

            setters <<
                "\n";
        }
    }

    // generate member data
    {
        code.ClassPrivate() <<
            "    // members' data\n";
		{
	        iterate ( TMembers, i, m_Members ) {
		        if ( i->haveFlag ) {
			        code.ClassPrivate() <<
				        "    bool "SET_PREFIX<<i->cName<<";\n";
				}
			}
        }
		{
	        iterate ( TMembers, i, m_Members ) {
		        if ( i->delayed ) {
			        code.ClassPrivate() <<
				        "    mutable NCBI_NS_NCBI::CDelayBuffer "DELAY_PREFIX<<i->cName<<";\n";
				}
			}
        }
		{
			iterate ( TMembers, i, m_Members ) {
                if ( i->ref ) {
                    code.ClassPrivate() <<
                        "    "<<ncbiNamespace<<"CRef< "<<i->tName<<" > "<<i->mName<<";\n";
                }
                else {
                    code.ClassPrivate() <<
                        "    "<<i->tName<<" "<<i->mName<<";\n";
                }
		    }
		}
    }

    // generate member initializers
    {
		{
	        iterate ( TMembers, i, m_Members ) {
                if ( i->haveFlag ) {
                    code.AddInitializer(SET_PREFIX+i->cName, "false");
                }
			}
        }
        {
            iterate ( TMembers, i, m_Members ) {
                if ( i->ref ) {
                    if ( !i->canBeNull ) {
                        string init = i->defaultValue;
                        if ( init.empty() ) {
                            init = i->type->GetInitializer();
                        }
                        code.AddInitializer(i->mName,
                                            i->type->NewInstance(init));
                    }
                }
                else {
                    string init = i->defaultValue;
                    if ( init.empty() )
                        init = i->type->GetInitializer();
                    if ( !init.empty() )
                        code.AddInitializer(i->mName, init);
                }
            }
        }
    }

    // generate Reset method
    code.ClassPublic() <<
        "    // reset whole object\n"
        "    ";
    if ( HaveUserClass() )
        code.ClassPublic() << "virtual ";
    code.ClassPublic() <<
        "void Reset(void);\n"
        "\n";
    if ( generateMainReset ) {
        methods <<
            "void "<<methodPrefix<<"Reset(void)\n"
            "{\n";
        iterate ( TMembers, i, m_Members ) {
            methods <<
                "    Reset"<<i->cName<<"();\n";
        }
        methods <<
            "}\n"
            "\n";
    }

    // generate destruction code
    {
        for ( TMembers::const_reverse_iterator i = m_Members.rbegin();
              i != m_Members.rend(); ++i ) {
            code.AddDestructionCode(i->type->GetDestructionCode(i->valueName));
        }
    }

    // generate type info
    if ( haveUserClass ) {
        methods <<
            "BEGIN_NAMED_BASE_CLASS_INFO";
    }
    else {
        methods <<
            "BEGIN_NAMED_CLASS_INFO";
    }
    methods<<"(\""<<GetExternalName()<<"\", "<<classPrefix<<GetClassName()<<")\n"
        "{\n";
    if ( m_Members.size() == 1 && m_Members.front().cName.empty() ) {
        methods <<
            "    info->SetImplicit();\n";
    }
    {
        iterate ( TMembers, i, m_Members ) {
            methods << "    ADD_NAMED_";
            
            bool addNamespace = false;
            bool addCType = false;
            bool addEnum = false;
            bool addRef = false;
            
            bool ref = i->ref;
            if ( ref ) {
                methods << "REF_";
                addCType = true;
            }
            else {
                switch ( i->type->GetKind() ) {
                case eKindStd:
                case eKindString:
                    if ( i->type->HaveSpecialRef() ) {
                        addRef = true;
                    }
                    else {
                        methods << "STD_";
                    }
                    break;
                case eKindEnum:
                    methods << "ENUM_";
                    addEnum = true;
                    if ( !i->type->GetNamespace().IsEmpty() &&
                         code.GetNamespace() != i->type->GetNamespace()) {
                        _TRACE("EnumNamespace: "<<i->type->GetNamespace()<<" from "<<code.GetNamespace());
                        methods << "IN_";
                        addNamespace = true;
                    }
                    break;
                default:
                    addRef = true;
                    break;
                }
            }

            methods <<
                "MEMBER(\""<<i->externalName<<"\", "<<i->mName;
            if ( addNamespace )
                methods << ", "<<i->type->GetNamespace();
            if ( addCType )
                methods << ", "<<i->type->GetCType(code.GetNamespace());
            if ( addEnum )
                methods << ", "<<i->type->GetEnumName();
            if ( addRef )
                methods << ", "<<i->type->GetRef(code.GetNamespace());
            methods << ')';

            if ( !i->defaultValue.empty() ) {
                methods << "->SetDefault(";
                if ( ref )
                    methods << "new NCBI_NS_NCBI::CRef< "+i->tName+" >(";
                methods << "new "<<i->tName<<"("<<i->defaultValue<<')';
                if ( ref )
                    methods << ')';
                methods << ')';
                if ( i->haveFlag )
                    methods <<
                        "->SetSetFlag(MEMBER_PTR("SET_PREFIX<<i->cName<<"))";
            }
            else if ( i->optional ) {
                methods << "->SetOptional(";
                if ( i->haveFlag )
                    methods << "MEMBER_PTR("SET_PREFIX<<i->cName<<')';
                methods << ')';
            }
            if ( i->delayed ) {
                methods <<
                    "->SetDelayBuffer(MEMBER_PTR("DELAY_PREFIX<<i->cName<<"))";
            }
            methods << ";\n";
        }
    }
    methods <<
        "}\n"
        "END_CLASS_INFO\n"
        "\n";
}

void CClassTypeStrings::GenerateUserHPPCode(CNcbiOstream& out) const
{
    out <<
        "class "<<GetClassName()<<" : public "<<GetClassName()<<"_Base\n"
        "{\n"
        "    typedef "<<GetClassName()<<"_Base Tparent;\n"
        "public:\n";
    DeclareStaticConstructor(out, GetClassName());
    DeclareDestructor(out, GetClassName(), false);
    GenerateNewMethod(out, GetClassName());
    out <<
        "protected:\n";
    DeclareHeapConstructor(out, GetClassName());
    out <<
        "};\n"
        "\n";
}

void CClassTypeStrings::GenerateUserCPPCode(CNcbiOstream& out) const
{
    out <<
        "// constructor for static/automatic objects\n"<<
        GetClassName()<<"::"<<GetClassName()<<"(void)\n"
        "{\n"
        "}\n"
        "\n"
        "// constructor for dynamic objects\n"<<
        GetClassName()<<"::"<<GetClassName()<<"(ECanDelete)\n"
        "    : Tparent(eCanDelete)\n"
        "{\n"
        "}\n"
        "\n"
        "// destructor\n"<<
        GetClassName()<<"::~"<<GetClassName()<<"(void)\n"
        "{\n"
        "}\n"
        "\n";
}

CClassRefTypeStrings::CClassRefTypeStrings(const string& className,
                                           const CNamespace& ns,
                                           const string& fileName)
    : m_ClassName(className),
      m_Namespace(ns),
      m_FileName(fileName)
{
}

CTypeStrings::EKind CClassRefTypeStrings::GetKind(void) const
{
    return eKindObject;
}

const CNamespace& CClassRefTypeStrings::GetNamespace(void) const
{
    return m_Namespace;
}

void CClassRefTypeStrings::GenerateTypeCode(CClassContext& ctx) const
{
    ctx.HPPIncludes().insert(m_FileName);
}

void CClassRefTypeStrings::GeneratePointerTypeCode(CClassContext& ctx) const
{
    ctx.AddForwardDeclaration(m_ClassName, m_Namespace);
    ctx.CPPIncludes().insert(m_FileName);
}

string CClassRefTypeStrings::GetCType(const CNamespace& ns) const
{
    return ns.GetNamespaceRef(m_Namespace)+m_ClassName;
}

string CClassRefTypeStrings::GetRef(const CNamespace& ns) const
{
    return "CLASS, ("+GetCType(ns)+')';
}

string CClassRefTypeStrings::NewInstance(const string& init) const
{
    return GetCType(CNamespace::KEmptyNamespace)+"::New("+init+')';
}

string CClassRefTypeStrings::GetResetCode(const string& var) const
{
    return var+".Reset();\n";
}

END_NCBI_SCOPE
