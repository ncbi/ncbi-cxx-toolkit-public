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
#include <typeinfo>

CFileCode::CFileCode(const string& baseName)
    : m_BaseName(baseName)
{
}

CFileCode::~CFileCode(void)
{
}

string CFileCode::GetHPPName(void) const
{
    return GetFileBaseName() + "_Base.hpp";
}

string CFileCode::GetUserHPPName(void) const
{
    return GetFileBaseName() + ".hpp";
}

string CFileCode::GetCPPName(void) const
{
    return GetFileBaseName() + "_Base.cpp";
}

string CFileCode::GetUserCPPName(void) const
{
    return GetFileBaseName() + ".cpp";
}

string CFileCode::GetBaseDefine(void) const
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

string CFileCode::GetHPPDefine(void) const
{
    return GetBaseDefine() + "_BASE_HPP";
}

string CFileCode::GetUserHPPDefine(void) const
{
    return GetBaseDefine() + "_HPP";
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

void CFileCode::AddForwardDeclaration(const string& cls, const string& ns)
{
    m_ForwardDeclarations[cls] = ns;
}

void CFileCode::AddHPPCode(const CNcbiOstrstream& code)
{
    Write(m_HPPCode, code);
}

void CFileCode::AddINLCode(const CNcbiOstrstream& code)
{
    Write(m_INLCode, code);
}

void CFileCode::AddCPPCode(const CNcbiOstrstream& code)
{
    Write(m_CPPCode, code);
}

void CFileCode::GenerateCode(void)
{
    if ( !m_Classes.empty() ) {
        CNamespace nsHPP(m_HPPCode);
        CNamespace nsINL(m_INLCode);
        CNamespace nsCPP(m_CPPCode);
        iterate ( TClasses, ci, m_Classes ) {
            nsHPP.Set(ci->namespaceName);
            nsINL.Set(ci->namespaceName);
            nsCPP.Set(ci->namespaceName);
            ci->code->GenerateCode(*this);
        }
    }
    m_HPPIncludes.erase(NcbiEmptyString);
    m_CPPIncludes.erase(NcbiEmptyString);
    m_HPPIncludes.erase(GetFileBaseName());
    m_CPPIncludes.erase(GetFileBaseName());
}

void CFileCode::GenerateHPP(const string& path) const
{
    string fileName = Path(path, GetHPPName());
    CDelayedOfstream header(fileName.c_str());
    if ( !header ) {
        ERR_POST(Fatal << "Cannot create file: " << fileName);
        return;
    }

    string hppDefine = GetHPPDefine();
    header <<
        "// This is generated file, don't modify\n"
        "#ifndef " << hppDefine << "\n"
        "#define " << hppDefine << "\n"
        "\n"
        "// standard includes\n"
        "#include <corelib/ncbiobj.hpp>\n";

    if ( !m_HPPIncludes.empty() ) {
        header <<
            "\n"
            "// generated includes\n";
        iterate ( TIncludes, stri, m_HPPIncludes ) {
            header <<
                "#include " << Include(*stri) << "\n";
        }
    }

    if ( !m_ForwardDeclarations.empty() ) {
        CNamespace ns(header);
        header <<
            "\n"
            "// forward declarations\n";
        iterate ( TForwards, fi, m_ForwardDeclarations ) {
            ns.Set(fi->second);
            header <<
                "class " << fi->first << ";\n";
        }
    }
    
    header <<
        "\n"
        "// generated classes\n";
    Write(header, m_HPPCode);
    
    if ( const_cast<CNcbiOstrstream&>(m_INLCode).pcount() != 0 ) {
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

        Write(header, m_INLCode);
        
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
    header <<
        "\n"
        "#endif // " << hppDefine << "\n";
    header.close();
    if ( !header )
        ERR_POST("Error writing file " << fileName);
}

void CFileCode::GenerateCPP(const string& path) const
{
    string fileName = Path(path, GetCPPName());
    CDelayedOfstream code(fileName.c_str());
    if ( !code ) {
        ERR_POST(Fatal << "Cannot create file: " << fileName);
        return;
    }

    code <<
        "// This is generated file, don't modify\n"
        "\n"
        "// standard includes\n"
        "#include <serial/serialimpl.hpp>\n"
        "\n"
        "// generated includes\n"
        "#include <" << GetUserHPPName() << ">\n";

    if ( !m_CPPIncludes.empty() ) {
        iterate ( TIncludes, stri, m_CPPIncludes ) {
            code <<
                "#include " << Include(*stri) << "\n";
        }
    }

    code <<
        "\n"
        "// generated classes\n";
    Write(code, m_CPPCode);

    code.close();
    if ( !code )
        ERR_POST("Error writing file " << fileName);
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
    header <<
        "// This is generated file, you may modify it freely\n"
        "#ifndef " << hppDefine << "\n"
        "#define " << hppDefine << "\n"
        "\n";

    header <<
        "\n"
        "// generated includes\n"
        "#include <" << GetHPPName() << ">\n";
    
    CNamespace ns(header);
    
    if ( !m_Classes.empty() ) {
        header <<
            "\n"
            "// generated classes\n";
        iterate ( TClasses, ci, m_Classes ) {
            ns.Set(ci->namespaceName);
            ci->code->GenerateUserHPPCode(header);
        }
    }
    ns.End();
    
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

    code <<
        "// This is generated file, you may modify it freely\n"
        "\n"
        "// standard includes\n"
        "\n"
        "// generated includes\n"
        "#include <" << GetUserHPPName() << ">\n";

    CNamespace ns(code);
    
    if ( !m_Classes.empty() ) {
        code <<
            "\n"
            "// generated classes\n";
        iterate ( TClasses, ci, m_Classes ) {
            ns.Set(ci->namespaceName);
            ci->code->GenerateUserCPPCode(code);
        }
    }

    code.close();
    if ( !code )
        ERR_POST("Error writing file " << fileName);
    return true;
}

CFileCode::SClassInfo::SClassInfo(const string& ns, AutoPtr<CTypeStrings> c)
    : namespaceName(ns), code(c)
{
}

bool CFileCode::AddType(const CDataType* type)
{
    string idName = type->IdName();
    if ( m_AddedClasses.find(idName) != m_AddedClasses.end() )
        return false;
    m_AddedClasses.insert(idName);
    _TRACE("AddType: " << idName << ": " << typeid(*type).name());
    m_Classes.push_front(SClassInfo(type->Namespace(), type->GenerateCode()));
    return true;
}

CNamespace::CNamespace(CNcbiOstream& out)
    : m_Out(out)
{
}

CNamespace::~CNamespace(void)
{
    End();
}

void CNamespace::Begin(void)
{
    if ( !m_Namespace.empty() ) {
        m_Out <<
            "\n"
            "namespace " << m_Namespace << " {\n"
            "\n";
    }
}

void CNamespace::End(void)
{
    if ( !m_Namespace.empty() ) {
        m_Out <<
            "\n"
            "}\n"
            "\n";
        m_Namespace.erase();
    }
}

void CNamespace::Set(const string& ns)
{
    if ( ns != m_Namespace ) {
        End();
        m_Namespace = ns;
        Begin();
    }
}

