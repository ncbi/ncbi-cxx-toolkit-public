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
*   File generator
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.22  2000/06/16 16:31:39  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.21  2000/04/28 16:58:16  vasilche
* Added classes CByteSource and CByteSourceReader for generic reading.
* Added delayed reading of choice variants.
*
* Revision 1.20  2000/04/17 19:11:08  vasilche
* Fixed failed assertion.
* Removed redundant namespace specifications.
*
* Revision 1.19  2000/04/12 15:36:51  vasilche
* Added -on <namespace> argument to datatool.
* Removed unnecessary namespace specifications in generated files.
*
* Revision 1.18  2000/04/07 19:26:26  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.17  2000/04/03 18:47:30  vasilche
* Added main include file for generated headers.
* serialimpl.hpp is included in generated sources with GetTypeInfo methods
*
* Revision 1.16  2000/03/29 15:52:26  vasilche
* Generated files names limited to 31 symbols due to limitations of Mac.
* Removed unions with only one member.
*
* Revision 1.15  2000/03/17 17:05:59  vasilche
* String literal split to avoid influence of cvs.
*
* Revision 1.14  2000/03/17 16:49:55  vasilche
* Added copyright message to generated files.
* All objects pointers in choices now share the only CObject pointer.
* All setters/getters made public until we'll find better solution.
*
* Revision 1.13  2000/03/07 14:06:31  vasilche
* Added generation of reference counted objects.
*
* Revision 1.12  2000/02/17 21:26:18  vasilche
* Inline methods now will be at the end of *_Base.hpp files.
*
* Revision 1.11  2000/02/17 20:05:07  vasilche
* Inline methods now will be generated in *_Base.inl files.
* Fixed processing of StringStore.
* Renamed in choices: Selected() -> Which(), E_choice -> E_Choice.
* Enumerated values now will preserve case as in ASN.1 definition.
*
* Revision 1.10  2000/02/01 21:47:58  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.9  2000/01/06 16:13:17  vasilche
* Fail of file generation now fatal.
*
* Revision 1.8  1999/12/28 18:55:57  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.7  1999/12/21 17:18:34  vasilche
* Added CDelayedFostream class which rewrites file only if contents is changed.
*
* Revision 1.6  1999/12/20 21:00:17  vasilche
* Added generation of sources in different directories.
*
* Revision 1.5  1999/12/03 21:42:12  vasilche
* Fixed conflict of enums in choices.
*
* Revision 1.4  1999/12/01 17:36:25  vasilche
* Fixed CHOICE processing.
*
* Revision 1.3  1999/11/22 21:04:49  vasilche
* Cleaned main interface headers. Now generated files should include serial/serialimpl.hpp and user code should include serial/serial.hpp which became might lighter.
*
* Revision 1.2  1999/11/15 19:36:14  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <serial/tool/filecode.hpp>
#include <serial/tool/type.hpp>
#include <serial/tool/typestr.hpp>
#include <serial/tool/fileutil.hpp>
#include <serial/tool/namespace.hpp>
#include <typeinfo>

BEGIN_NCBI_SCOPE

CFileCode::CFileCode(const string& baseName)
    : m_BaseName(baseName)
{
}

CFileCode::~CFileCode(void)
{
}

string CFileCode::GetBaseFileBaseName(void) const
{
    _ASSERT(BaseName(GetFileBaseName()).size() + 5 <= MAX_FILE_NAME_LENGTH);
    return GetFileBaseName() + "_";
}

string CFileCode::GetUserFileBaseName(void) const
{
    return GetFileBaseName();
}

string CFileCode::GetBaseHPPName(void) const
{
    return GetBaseFileBaseName() + ".hpp";
}

string CFileCode::GetUserHPPName(void) const
{
    return GetUserFileBaseName() + ".hpp";
}

string CFileCode::GetBaseCPPName(void) const
{
    return GetBaseFileBaseName() + ".cpp";
}

string CFileCode::GetUserCPPName(void) const
{
    return GetUserFileBaseName() + ".cpp";
}

string CFileCode::GetDefineBase(void) const
{
    string s;
    iterate ( string, i, GetFileBaseName() ) {
        char c = *i;
        if ( c >= 'a' && c <= 'z' )
            c = c + ('A' - 'a');
        else if ( (c < 'A' || c > 'Z') &&
                  (c < '0' || c > '9') )
            c = '_';
        s += c;
    }
    return s;
}

string CFileCode::GetBaseHPPDefine(void) const
{
    return GetDefineBase() + "_BASE_HPP";
}

string CFileCode::GetUserHPPDefine(void) const
{
    return GetDefineBase() + "_HPP";
}

string CFileCode::Include(const string& s) const
{
    if ( s.empty() )
        THROW1_TRACE(runtime_error, "Empty file name");
    switch ( s[0] ) {
    case '<':
    case '"':
        return s;
    default:
        return '<' + s + ".hpp>";
    }
}

string CFileCode::GetMethodPrefix(void) const
{
    return string();
}

CFileCode::TIncludes& CFileCode::HPPIncludes(void)
{
    return m_HPPIncludes;
}

CFileCode::TIncludes& CFileCode::CPPIncludes(void)
{
    return m_CPPIncludes;
}

void CFileCode::AddForwardDeclaration(const string& cls, const CNamespace& ns)
{
    m_ForwardDeclarations[cls] = ns;
}

const CNamespace& CFileCode::GetNamespace(void) const
{
    _ASSERT(m_CurrentClass != 0);
    return m_CurrentClass->ns;
}

void CFileCode::AddHPPCode(const CNcbiOstrstream& code)
{
    m_CurrentClass->hppCode =
        CNcbiOstrstreamToString(const_cast<CNcbiOstrstream&>(code));
}

void CFileCode::AddINLCode(const CNcbiOstrstream& code)
{
    m_CurrentClass->inlCode = 
        CNcbiOstrstreamToString(const_cast<CNcbiOstrstream&>(code));
}

void CFileCode::AddCPPCode(const CNcbiOstrstream& code)
{
    m_CurrentClass->cppCode = 
        CNcbiOstrstreamToString(const_cast<CNcbiOstrstream&>(code));
}

void CFileCode::GenerateCode(void)
{
    if ( !m_Classes.empty() ) {
        non_const_iterate ( TClasses, i, m_Classes ) {
            m_CurrentClass = &*i;
            m_CurrentClass->code->GenerateCode(*this);
        }
        m_CurrentClass = 0;
    }
    m_HPPIncludes.erase(NcbiEmptyString);
    m_CPPIncludes.erase(NcbiEmptyString);
}

CNcbiOstream& CFileCode::WriteCopyrightHeader(CNcbiOstream& out) const
{
    return out <<
        "/* $""Id$\n"
        " * ===========================================================================\n"
        " *\n"
        " *                            PUBLIC DOMAIN NOTICE\n"
        " *               National Center for Biotechnology Information\n"
        " *\n"
        " *  This software/database is a \"United States Government Work\" under the\n"
        " *  terms of the United States Copyright Act.  It was written as part of\n"
        " *  the author's official duties as a United States Government employee and\n"
        " *  thus cannot be copyrighted.  This software/database is freely available\n"
        " *  to the public for use. The National Library of Medicine and the U.S.\n"
        " *  Government have not placed any restriction on its use or reproduction.\n"
        " *\n"
        " *  Although all reasonable efforts have been taken to ensure the accuracy\n"
        " *  and reliability of the software and data, the NLM and the U.S.\n"
        " *  Government do not and cannot warrant the performance or results that\n"
        " *  may be obtained by using this software or data. The NLM and the U.S.\n"
        " *  Government disclaim all warranties, express or implied, including\n"
        " *  warranties of performance, merchantability or fitness for any particular\n"
        " *  purpose.\n"
        " *\n"
        " *  Please cite the author in any work or product based on this material.\n"
        " *\n"
        " * ===========================================================================\n"
        " *\n";
}

CNcbiOstream& CFileCode::WriteSourceFile(CNcbiOstream& out) const
{
    iterate ( set<string>, i, m_SourceFiles ) {
        if ( i != m_SourceFiles.begin() )
            out << ", ";
        out << '\'' << *i << '\'';
    }
    return out;
}

CNcbiOstream& CFileCode::WriteCopyright(CNcbiOstream& out) const
{
    WriteCopyrightHeader(out) <<
        " * File Description:\n"
        " *   This code is generated by application DATATOOL\n"
        " *   using specifications from the ASN data definition file\n"
        " *   ";
    WriteSourceFile(out) << ".\n"
        " *\n"
        " * ATTENTION:\n"
        " *   Don't edit or check-in this file to the CVS as this file will\n"
        " *   be overridden (by DATATOOL) without warning!\n"
        " * ===========================================================================\n"
        " */\n";
    return out;
}

CNcbiOstream& CFileCode::WriteUserCopyright(CNcbiOstream& out) const
{
    WriteCopyrightHeader(out) <<
        " * Author:  .......\n"
        " *\n"
        " * File Description:\n"
        " *   .......\n"
        " *\n"
        " * Remark:\n"
        " *   This code was originally generated by application DATATOOL\n"
        " *   using specifications from the ASN data definition file\n"
        " *   ";
    WriteSourceFile(out) << ".\n"
        " *\n"
        " * ---------------------------------------------------------------------------\n"
        " * $""Log$\n"
        " *\n"
        " * ===========================================================================\n"
        " */\n";
    return out;
}

void CFileCode::GenerateHPP(const string& path) const
{
    string fileName = Path(path, GetBaseHPPName());
    CDelayedOfstream header(fileName.c_str());
    if ( !header ) {
        ERR_POST(Fatal << "Cannot create file: " << fileName);
        return;
    }

    string hppDefine = GetBaseHPPDefine();
    WriteCopyright(header) <<
        "\n"
        "#ifndef " << hppDefine << "\n"
        "#define " << hppDefine << "\n"
        "\n"
        "// standard includes\n"
        "#include <serial/serialbase.hpp>\n";

    if ( !m_HPPIncludes.empty() ) {
        header <<
            "\n"
            "// generated includes\n";
        iterate ( TIncludes, i, m_HPPIncludes ) {
            header <<
                "#include " << Include(*i) << "\n";
        }
        header <<
            '\n';
    }

    CNamespace ns;
    if ( !m_ForwardDeclarations.empty() ) {
        bool begin = false;
        iterate ( TForwards, i, m_ForwardDeclarations ) {
            ns.Set(i->second, header);
            if ( !begin ) {
                header <<
                    "\n"
                    "// forward declarations\n";
                begin = true;
            }
            header <<
                "class " << i->first << ";\n";
        }
        if ( begin )
            header << '\n';
    }
    
    if ( !m_Classes.empty() ) {
        bool begin = false;
        iterate ( TClasses, i, m_Classes ) {
            if ( !i->hppCode.empty() ) {
                ns.Set(i->ns, header);
                if ( !begin ) {
                    header <<
                        "\n"
                        "// generated classes\n"
                        "\n";
                    begin = true;
                }
                header << i->hppCode;
            }
        }
        if ( begin )
            header << '\n';
    }

    if ( !m_Classes.empty() ) {
        bool begin = false;
        iterate ( TClasses, i, m_Classes ) {
            if ( !i->inlCode.empty() ) {
                ns.Set(i->ns, header, false);
                if ( !begin ) {
                    // have inline methods
                    header <<
                        "\n"
                        "\n"
                        "\n"
                        "\n"
                        "\n"
                        "///////////////////////////////////////////////////////////\n"
                        "///////////////////// inline methods //////////////////////\n"
                        "///////////////////////////////////////////////////////////\n";
                    begin = true;
                }
                header << i->inlCode;
            }
        }
        if ( begin ) {
            header <<
                "///////////////////////////////////////////////////////////\n"
                "////////////////// end of inline methods //////////////////\n"
                "///////////////////////////////////////////////////////////\n"
                "\n"
                "\n"
                "\n"
                "\n"
                "\n";
        }
    }
    ns.Reset(header);
    header <<
        "\n"
        "#endif // " << hppDefine << "\n";
    header.close();
    if ( !header )
        ERR_POST(Fatal << "Error writing file " << fileName);
}

void CFileCode::GenerateCPP(const string& path) const
{
    string fileName = Path(path, GetBaseCPPName());
    CDelayedOfstream code(fileName.c_str());
    if ( !code ) {
        ERR_POST(Fatal << "Cannot create file: " << fileName);
        return;
    }

    WriteCopyright(code) <<
        "\n"
        "// standard includes\n"
        "#include <serial/serialimpl.hpp>\n"
        "\n"
        "// generated includes\n"
        "#include <" << GetUserHPPName() << ">\n";

    if ( !m_CPPIncludes.empty() ) {
        iterate ( TIncludes, i, m_CPPIncludes ) {
            code <<
                "#include " << Include(*i) << "\n";
        }
    }

    CNamespace ns;
    if ( !m_Classes.empty() ) {
        bool begin = false;
        iterate ( TClasses, i, m_Classes ) {
            if ( !i->cppCode.empty() ) {
                ns.Set(i->ns, code, false);
                if ( !begin ) {
                    code <<
                        "\n"
                        "// generated classes\n"
                        "\n";
                    begin = true;
                }
                code << i->cppCode;
            }
        }
        if ( begin )
            code << '\n';
    }
    ns.Reset(code);

    code.close();
    if ( !code )
        ERR_POST(Fatal << "Error writing file " << fileName);
}

bool CFileCode::GenerateUserHPP(const string& path) const
{
    string fileName = Path(path, GetUserHPPName());
    CNcbiOfstream header(fileName.c_str(), IOS_BASE::app);
    if ( !header ) {
        ERR_POST(Fatal << "Cannot create file: " << fileName);
        return false;
    }
	header.seekp(0, IOS_BASE::end);
    if ( streampos(header.tellp()) != streampos(0) ) {
        ERR_POST(Info <<
                 "Will not overwrite existing user file: " << fileName);
        return false;
    }

    string hppDefine = GetUserHPPDefine();
    WriteUserCopyright(header) <<
        "\n"
        "#ifndef " << hppDefine << "\n"
        "#define " << hppDefine << "\n"
        "\n";

    header <<
        "\n"
        "// generated includes\n"
        "#include <" << GetBaseHPPName() << ">\n";
    
    CNamespace ns;
    if ( !m_Classes.empty() ) {
        header <<
            "\n"
            "// generated classes\n"
            "\n";
        iterate ( TClasses, i, m_Classes ) {
            ns.Set(i->ns, header, false);
            i->code->GenerateUserHPPCode(header);
        }
    }
    ns.Reset(header);
    
    header <<
        "\n"
        "#endif // " << hppDefine << "\n";
    header.close();
    if ( !header )
        ERR_POST("Error writing file " << fileName);
    return true;
}

bool CFileCode::GenerateUserCPP(const string& path) const
{
    string fileName = Path(path, GetUserCPPName());
    CNcbiOfstream code(fileName.c_str(), IOS_BASE::app);
    if ( !code ) {
        ERR_POST(Fatal << "Cannot create file: " << fileName);
        return false;
    }
	code.seekp(0, IOS_BASE::end);
    if ( streampos(code.tellp()) != streampos(0) ) {
        ERR_POST(Info <<
                 "Will not overwrite existing user file: " << fileName);
        return false;
    }

    WriteUserCopyright(code) <<
        "\n"
        "// standard includes\n"
        "\n"
        "// generated includes\n"
        "#include <" << GetUserHPPName() << ">\n";

    CNamespace ns;
    if ( !m_Classes.empty() ) {
        code <<
            "\n"
            "// generated classes\n"
            "\n";
        iterate ( TClasses, i, m_Classes ) {
            ns.Set(i->ns, code, false);
            i->code->GenerateUserCPPCode(code);
        }
    }
    ns.Reset(code);

    code.close();
    if ( !code )
        ERR_POST(Fatal << "Error writing file " << fileName);
    return true;
}

bool CFileCode::AddType(const CDataType* type)
{
    string idName = type->IdName();
    if ( m_AddedClasses.find(idName) != m_AddedClasses.end() )
        return false;
    m_AddedClasses.insert(idName);
    _TRACE("AddType: " << idName << ": " << typeid(*type).name());
    m_SourceFiles.insert(type->GetSourceFileName());
    m_Classes.push_front(SClassInfo(type->Namespace(), type->GenerateCode()));
    return true;
}

END_NCBI_SCOPE
