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

#include "filecode.hpp"
#include "code.hpp"
#include "type.hpp"
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

void CFileCode::AddHPPInclude(const string& s)
{
    m_HPPIncludes.insert(Include(s));
}

void CFileCode::AddCPPInclude(const string& s)
{
    m_CPPIncludes.insert(Include(s));
}

void CFileCode::AddForwardDeclaration(const string& cls, const string& ns)
{
    m_ForwardDeclarations[cls] = ns;
}

void CFileCode::AddHPPIncludes(const TIncludes& includes)
{
    iterate ( TIncludes, i, includes ) {
        AddHPPInclude(*i);
    }
}

void CFileCode::AddCPPIncludes(const TIncludes& includes)
{
    iterate ( TIncludes, i, includes ) {
        AddCPPInclude(*i);
    }
}

void CFileCode::AddForwardDeclarations(const TForwards& forwards)
{
	iterate ( TForwards, i, forwards ) {
		m_ForwardDeclarations.insert(*i);
    }
}

void CFileCode::GenerateHPP(const string& path) const
{
    string fileName = path + GetHPPName();
    CNcbiOfstream header(fileName.c_str());
    if ( !header ) {
        ERR_POST("Cannot create file: " << fileName);
        return;
    }

    string hppDefine = GetHPPDefine();
    header <<
        "// This is generated file, don't modify" << NcbiEndl <<
        "#ifndef " << hppDefine << NcbiEndl <<
        "#define " << hppDefine << NcbiEndl <<
        NcbiEndl <<
        "// standard includes" << NcbiEndl <<
        "#include <corelib/ncbistd.hpp>" << NcbiEndl;

    // collect parent classes
    iterate ( TClasses, ci, m_Classes ) {
        ci->second->GetParentType();
    }

    if ( !m_HPPIncludes.empty() ) {
        header << NcbiEndl <<
            "// generated includes" << NcbiEndl;
        iterate ( TIncludes, stri, m_HPPIncludes ) {
            header <<
                "#include " << *stri << NcbiEndl;
        }
    }

    CNamespace ns(header);
    
    if ( !m_ForwardDeclarations.empty() ) {
        header << NcbiEndl <<
            "// forward declarations" << NcbiEndl;
        iterate ( TForwards, fi, m_ForwardDeclarations ) {
            ns.Set(fi->second);
            header <<
                "class " << fi->first << ';'<< NcbiEndl;
        }
    }
    
    if ( !m_Classes.empty() ) {
        header << NcbiEndl <<
            "// generated classes" << NcbiEndl;
        iterate ( TClasses, ci, m_Classes ) {
            CClassCode* cls = ci->second.get();
            ns.Set(cls->GetNamespace());
            cls->GenerateHPP(header);
        }
    }
    ns.End();
    
    header << NcbiEndl <<
        "#endif // " << hppDefine << NcbiEndl;
    header.close();
    if ( !header )
        ERR_POST("Error writing file " << fileName);
}

void CFileCode::GenerateCPP(const string& path) const
{
    string fileName = path + GetCPPName();
    CNcbiOfstream code(fileName.c_str());
    if ( !code ) {
        ERR_POST("Cannot create file: " << fileName);
        return;
    }

    code <<
        "// This is generated file, don't modify" << NcbiEndl <<
        NcbiEndl <<
        "// standard includes" << NcbiEndl <<
        "#include <serial/serialimpl.hpp>" << NcbiEndl <<
        NcbiEndl <<
        "// generated includes" << NcbiEndl <<
        "#include <" << GetUserHPPName() << ">" << NcbiEndl;

    if ( !m_CPPIncludes.empty() ) {
        iterate ( TIncludes, stri, m_CPPIncludes ) {
            code <<
                "#include " << *stri << NcbiEndl;
        }
    }

    CNamespace ns(code);
    
    if ( !m_Classes.empty() ) {
        code << NcbiEndl <<
            "// generated classes" << NcbiEndl;
        iterate ( TClasses, ci, m_Classes ) {
            CClassCode* cls = ci->second.get();
            ns.Set(cls->GetNamespace());
            cls->GenerateCPP(code);
        }
    }
    code.close();
    if ( !code )
        ERR_POST("Error writing file " << fileName);
}

bool CFileCode::GenerateUserHPP(const string& path) const
{
    string fileName = path + GetUserHPPName();
    CNcbiOfstream header(fileName.c_str(), IOS_BASE::app);
    if ( !header ) {
        ERR_POST("Cannot create file: " << fileName);
        return false;
    }
	header.seekp(0, IOS_BASE::end);
    if ( streampos(header.tellp()) != streampos(0) ) {
        ERR_POST(Warning <<
                 "Will not overwrite existing user file: " << fileName);
        return false;
    }

    string hppDefine = GetUserHPPDefine();
    header <<
        "// This is generated file, you may modify it freely" << NcbiEndl <<
        "#ifndef " << hppDefine << NcbiEndl <<
        "#define " << hppDefine << NcbiEndl <<
        NcbiEndl <<
        "// standard includes" << NcbiEndl <<
        "#include <corelib/ncbistd.hpp>" << NcbiEndl;

    header << NcbiEndl <<
        "// generated includes" << NcbiEndl <<
        "#include <" << GetHPPName() << '>' << NcbiEndl;
    
    CNamespace ns(header);
    
    if ( !m_Classes.empty() ) {
        header << NcbiEndl <<
            "// generated classes" << NcbiEndl;
        iterate ( TClasses, ci, m_Classes ) {
            CClassCode* cls = ci->second.get();
            ns.Set(cls->GetNamespace());
            cls->GenerateUserHPP(header);
        }
    }
    ns.End();
    
    header << NcbiEndl <<
        "#endif // " << hppDefine << NcbiEndl;
    header.close();
    if ( !header )
        ERR_POST("Error writing file " << fileName);
    return true;
}

bool CFileCode::GenerateUserCPP(const string& path) const
{
    string fileName = path + GetUserCPPName();
    CNcbiOfstream code(fileName.c_str(), IOS_BASE::app);
    if ( !code ) {
        ERR_POST("Cannot create file: " << fileName);
        return false;
    }
	code.seekp(0, IOS_BASE::end);
    if ( streampos(code.tellp()) != streampos(0) ) {
        ERR_POST(Warning <<
                 "Will not overwrite existing user file: " << fileName);
        return false;
    }

    code <<
        "// This is generated file, you may modify it freely" << NcbiEndl <<
        NcbiEndl <<
        "// standard includes" << NcbiEndl <<
        NcbiEndl <<
        "// generated includes" << NcbiEndl <<
        "#include <" << GetUserHPPName() << ">" << NcbiEndl;

    CNamespace ns(code);
    
    if ( !m_Classes.empty() ) {
        code << NcbiEndl <<
            "// generated classes" << NcbiEndl;
        iterate ( TClasses, ci, m_Classes ) {
            CClassCode* cls = ci->second.get();
            ns.Set(cls->GetNamespace());
            cls->GenerateUserCPP(code);
        }
    }

    code.close();
    if ( !code )
        ERR_POST("Error writing file " << fileName);
    return true;
}

bool CFileCode::AddType(const CDataType* type)
{
    AutoPtr<CClassCode>& cls = m_Classes[type->IdName()];
    if ( cls ) {
        _ASSERT(cls->GetType() == type);
        return false;
    }

    _TRACE("AddType: " << type->IdName() << ": " << typeid(*type).name());
    cls = new CClassCode(*this, type->IdName(), type);
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
        m_Out << NcbiEndl <<
            "namespace " << m_Namespace << " {" << NcbiEndl <<
            NcbiEndl;
    }
}

void CNamespace::End(void)
{
    if ( !m_Namespace.empty() ) {
        m_Out << NcbiEndl <<
            "}" << NcbiEndl << NcbiEndl;
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

