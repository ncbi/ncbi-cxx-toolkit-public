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
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>
#include "exceptions.hpp"
#include "generate.hpp"
#include "filecode.hpp"
#include "type.hpp"
#include "typestr.hpp"
#include "fileutil.hpp"
#include "namespace.hpp"
#include "module.hpp"
#include "code.hpp"
#include <serial/error_codes.hpp>
#include <util/checksum.hpp>
#include <typeinfo>


#define NCBI_USE_ERRCODE_X   Serial_FileCode


BEGIN_NCBI_SCOPE

string CFileCode::m_PchHeader;

CFileCode::CFileCode(const CCodeGenerator* codeGenerator,
                     const string& baseName)
    : m_CodeGenerator(codeGenerator),m_BaseName(baseName)
{
    m_UseQuotedForm = false;
    return;
}


CFileCode::~CFileCode(void)
{
    return;
}


const string& CFileCode::ChangeFileBaseName(void)
{
    m_BaseName += "x";
    return GetFileBaseName();
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
    if (!m_Classes.empty()) {
        const CNamespace& ns = m_Classes.begin()->ns;
        if (!ns.InNCBI()) {
            string t(ns);
            ITERATE ( string, i, t ) {
                s += (isalnum((unsigned char)*i) ? (*i) : '_');
            }
        }
    }
    ITERATE ( string, i, GetFileBaseName() ) {
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


string CFileCode::Include(const string& s, bool addExt) const
{
    if ( s.empty() ) {
        NCBI_THROW(CDatatoolException,eInvalidData,"Empty file name");
    }

    switch ( s[0] ) {
    case '<':
    case '"':
        return s[0] + GetStdPath(s.substr(1, s.length()-2)) + s[s.length()-1];
    default:
    {
        string result(1, m_UseQuotedForm ? '\"' : '<');
        result += GetStdPath(addExt ? (s + ".hpp") : s);
        result += m_UseQuotedForm ? '\"' : '>';
        return result;
    }
    }
}


string CFileCode::GetMethodPrefix(void) const
{
    return kEmptyStr;
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
    m_ForwardDeclarations[ ns.ToString() ].insert(cls);
}


const CNamespace& CFileCode::GetNamespace(void) const
{
    _ASSERT(m_CurrentClass != 0);
    return m_CurrentClass->ns;
}


void CFileCode::AddHPPCode(const CNcbiOstrstream& code)
{
    m_CurrentClass->hppCode +=
        CNcbiOstrstreamToString(const_cast<CNcbiOstrstream&>(code));
}


void CFileCode::AddINLCode(const CNcbiOstrstream& code)
{
    m_CurrentClass->inlCode += 
        CNcbiOstrstreamToString(const_cast<CNcbiOstrstream&>(code));
}


void CFileCode::AddCPPCode(const CNcbiOstrstream& code)
{
    m_CurrentClass->cppCode += 
        CNcbiOstrstreamToString(const_cast<CNcbiOstrstream&>(code));
}

void CFileCode::UseQuotedForm(bool use)
{
    m_UseQuotedForm = use;
}

void CFileCode::CreateFileFolder(const string& fileName) const
{
    CDirEntry entry(fileName);
    CDir  dir(entry.GetDir());
    dir.CreatePath();
}

void CFileCode::GenerateCode(void)
{
    if ( !m_Classes.empty() ) {
        NON_CONST_ITERATE ( TClasses, i, m_Classes ) {
            m_CurrentClass = &*i;
            m_CurrentClass->code->GenerateCode(*this);
        }
        m_CurrentClass = 0;
    }
    m_HPPIncludes.erase(kEmptyStr);
    m_CPPIncludes.erase(kEmptyStr);
}


CNcbiOstream& CFileCode::WriteCopyrightHeader(CNcbiOstream& out)
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
    ITERATE ( set<string>, i, m_SourceFiles ) {
        if ( i != m_SourceFiles.begin() )
            out << ", ";
        {
            CDirEntry entry(*i);
            out << '\'' << entry.GetName() << '\'';
        }
    }
    return out;
}


CNcbiOstream& CFileCode::WriteSpecRefs(CNcbiOstream& out) const
{
    string docroot = CClassCode::GetDocRootURL();
    string rootdir = MakeAbsolutePath(m_CodeGenerator->GetRootDir());
    if (docroot.empty()) {
        out << "/// ";
        WriteSourceFile(out) << ".\n";
    } else {
        out << "/// <a href=\"";
        ITERATE ( set<string>, i, m_SourceFiles ) {
            CDirEntry entry(MakeAbsolutePath(*i));
            string link;
            if (!rootdir.empty()) {
                link = NStr::Replace(entry.GetPath(),rootdir,docroot);
            } else {
                link = Path( docroot, entry.GetPath());
            }
            out << GetStdPath(link) << "\">" << entry.GetName() << "</a>\n";
        }
        string deffile = m_CodeGenerator->GetDefFile();
        if (!deffile.empty()) {
            CDirEntry entry(MakeAbsolutePath(deffile));
            out
                << "/// and additional tune-up parameters:\n";
            string link;
            if (!rootdir.empty()) {
                link = NStr::Replace(entry.GetPath(),rootdir,docroot);
            } else {
                link = Path( docroot, entry.GetPath());
            }
            out << "/// <a href=\"" << GetStdPath(link) << "\">" << entry.GetName() << "</a>\n";
        }
    }
    return out;
}


CNcbiOstream& CFileCode::WriteCopyright(CNcbiOstream& out, bool header) const
{
    if (header) {
        WriteCopyrightHeader(out)
            << " */\n\n"
            << "/// @file " << CDirEntry(GetBaseHPPName()).GetName() << "\n"
            << "/// Data storage class.\n"
            << "///\n"
            << "/// This file was generated by application DATATOOL\n"
            << "/// using the following specifications:\n";
        WriteSpecRefs(out) <<
            "///\n"
            "/// ATTENTION:\n"
            "///   Don't edit or commit this file into CVS as this file will\n"
            "///   be overridden (by DATATOOL) without warning!\n";
    } else {
        WriteCopyrightHeader(out) <<
            " * File Description:\n"
            " *   This code was generated by application DATATOOL\n"
            " *   using the following specifications:\n"
            " *   ";
        WriteSourceFile(out) << ".\n"
            " *\n"
            " * ATTENTION:\n"
            " *   Don't edit or commit this file into CVS as this file will\n"
            " *   be overridden (by DATATOOL) without warning!\n"
            " * ===========================================================================\n"
            " */\n";
    }
    return out;
}


CNcbiOstream& CFileCode::WriteUserCopyright(CNcbiOstream& out, bool header) const
{
    if (header) {
        WriteCopyrightHeader(out)
            << " */\n\n"
            << "/// @file " << CDirEntry(GetUserHPPName()).GetName() << "\n"
            << "/// User-defined methods of the data storage class.\n"
            << "///\n"
            << "/// This file was originally generated by application DATATOOL\n"
            << "/// using the following specifications:\n";
        WriteSpecRefs(out) <<
            "///\n"
            "/// New methods or data members can be added to it if needed.\n";
        string name = CDirEntry(GetBaseHPPName()).GetName();
        if (CClassCode::GetDocRootURL().empty()) {
            out << "/// See also: " << name << "\n\n";
        } else {
            string base = CDirEntry(GetBaseHPPName()).GetBase();
// Doxygen magic
            base = NStr::Replace(base, "_", "__");
            base += "_8hpp.html";

            out << "/// See also: <a href=\"" << base << "\">" << name << "</a>\n\n";
        }
    } else {
        WriteCopyrightHeader(out) <<
            " * Author:  .......\n"
            " *\n"
            " * File Description:\n"
            " *   .......\n"
            " *\n"
            " * Remark:\n"
            " *   This code was originally generated by application DATATOOL\n"
            " *   using the following specifications:\n"
            " *   ";
        WriteSourceFile(out) << ".\n"
            " */\n";
    }
    return out;
}


CNcbiOstream& CFileCode::WriteLogKeyword(CNcbiOstream& out)
{
#if 0
    out << "\n"
        "/*\n"
        "* ===========================================================================\n"
        "*\n"
        "* $""Log$\n"
        "*\n"
        "* ===========================================================================\n"
        "*/\n";
#endif
    return out;
}

void CFileCode::GenerateHPP(const string& path, string& fileName) const
{
    fileName = Path(path, GetBaseHPPName());
    CreateFileFolder(fileName);
    CDelayedOfstream header(fileName);
    if ( !header ) {
        ERR_POST_X(1, Fatal << "Cannot create file: " << fileName);
        return;
    }

    string hppDefine = GetBaseHPPDefine();
    WriteCopyright(header, true) <<
        "\n"
        "#ifndef " << hppDefine << "\n"
        "#define " << hppDefine << "\n"
        "\n";
    string extra = m_CodeGenerator->GetConfig().Get("-","_extra_headers");
    if (!extra.empty()) {
        list<string> extra_values;
        NStr::Split(extra, " \t\n\r,;", extra_values, NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
        header << "// extra headers\n";
        list<string>::const_iterator i;
        for (i = extra_values.begin(); i != extra_values.end(); ++i) {
            if (i->at(0) == '\"') {
                header << "#include "<< *i << "\n";
            } else {
                header << "#include <" << *i << ">\n";
            }
        }
        header << "\n";
    }
    header <<
        "// standard includes\n"
        "#include <serial/serialbase.hpp>\n";

    if ( !m_HPPIncludes.empty() ) {
        header <<
            "\n"
            "// generated includes\n";
        ITERATE ( TIncludes, i, m_HPPIncludes ) {
            header <<
                "#include " << Include(*i, true) << "\n";
        }
        header <<
            '\n';
    }

    CNamespace ns;
    if ( !m_ForwardDeclarations.empty() ) {
        bool begin = false;
        ITERATE ( TForwards, i, m_ForwardDeclarations ) {
            ns.Set(CNamespace(i->first), header);
            ITERATE( set<string>, s, i->second) {
                if ( !begin ) {
                    header <<
                        "\n"
                        "// forward declarations\n";
                    begin = true;
                }
                header <<
                    "class " << *s << ";\n";
            }
        }
        if ( begin )
            header << '\n';
    }
    
    if ( !m_Classes.empty() ) {
        bool begin = false;
        ITERATE ( TClasses, i, m_Classes ) {
            if ( !i->hppCode.empty() ) {
                ns.Set(i->ns, header);
                if ( !begin ) {
                    header <<
                        "\n"
                        "// generated classes\n"
                        "\n";
                    if (CClassCode::GetDoxygenComments()) {
                        header
                            << "\n"
                            << "/** @addtogroup ";
                        if (!CClassCode::GetDoxygenGroup().empty()) {
                            header << CClassCode::GetDoxygenGroup();
                        } else {
                            header << "dataspec_" << i->code->GetDoxygenModuleName();
                        }
                        header
                            << "\n *\n"
                            << " * @{\n"
                            << " */\n\n";
                    }
                    begin = true;
                }
                header << i->hppCode;
            }
        }
        if ( begin ) {
            if (CClassCode::GetDoxygenComments()) {
                header << "\n/* @} */";
            }
            header << "\n";
        }
    }

    if ( !m_Classes.empty() ) {
        bool begin = false;
        ITERATE ( TClasses, i, m_Classes ) {
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
        ERR_POST_X(2, Fatal << "Error writing file " << fileName);
}


void CFileCode::GenerateCPP(const string& path, string& fileName) const
{
    fileName = Path(path, GetBaseCPPName());
    CreateFileFolder(fileName);
    CDelayedOfstream code(fileName);
    if ( !code ) {
        ERR_POST_X(3, Fatal << "Cannot create file: " << fileName);
        return;
    }

    WriteCopyright(code, false) <<
        "\n"
        "// standard includes\n";
    if (!m_PchHeader.empty()) {
        code <<
            "#include <" << m_PchHeader << ">\n";
    }
    string userinc(Include(GetUserHPPName()));
    code <<
        "#include <serial/serialimpl.hpp>\n"
        "\n"
        "// generated includes\n"
        "#include " << userinc << "\n";

    if ( !m_CPPIncludes.empty() ) {
        ITERATE ( TIncludes, i, m_CPPIncludes ) {
            string cppinc(Include(m_CodeGenerator->ResolveFileName(*i), true));
            if (cppinc != userinc) {
                code << "#include " << cppinc << "\n";
            }
        }
    }

    CNamespace ns;
    if ( !m_Classes.empty() ) {
        bool begin = false;
        ITERATE ( TClasses, i, m_Classes ) {
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
        ERR_POST_X(4, Fatal << "Error writing file " << fileName);
}


bool CFileCode::GenerateUserHPP(const string& path, string& fileName) const
{
    return WriteUserFile(path, GetUserHPPName(), fileName,
                         &CFileCode::GenerateUserHPPCode);
}


bool CFileCode::GenerateUserCPP(const string& path, string& fileName) const
{
    return WriteUserFile(path, GetUserCPPName(), fileName,
                         &CFileCode::GenerateUserCPPCode);
}

bool CFileCode::ModifiedByUser(const string& fileName,
                               const list<string>& newLines) const
{
    // first check if file exists
    CNcbiIfstream in(fileName.c_str());
    if ( !in ) {
        // file doesn't exist -> was not modified by user
        return false;
    }

    CChecksum checksum;
    bool haveChecksum = false;
    bool equal = true;
    
    list<string>::const_iterator newLinesI = newLines.begin();
    SIZE_TYPE lineOffset = 0;
    while ( in ) {
        char buffer[1024]; // buffer must be as big as checksum line
        in.getline(buffer, sizeof(buffer), '\n');
        SIZE_TYPE count = (size_t)in.gcount();
        if ( count == 0 ) {
            // end of file
            break;
        }
        if ( haveChecksum || in.eof() ) {
            // text after checksum -> modified by user
            //    OR
            // partial last line -> modified by user
            ERR_POST_X(5, Info <<
                       "Will not overwrite modified user file: "<<fileName);
            return true;
        }

        bool eol;
        // check where EOL was read
        if ( in.fail() ) {
            // very long line
            // reset fail flag
            in.clear(in.rdstate() & ~in.failbit);
            eol = false;
        }
        else {
            // full line was read
            --count; // do not include EOL symbol
            eol = true;
        }

        // check for checksum line
        if ( lineOffset == 0 && eol ) {
            haveChecksum = checksum.ValidChecksumLine(buffer, count);
            if ( haveChecksum )
                continue;
        }

        // update checksum
        checksum.AddChars(buffer, count);
        // update equal flag
        if ( equal ) {
            if ( newLinesI == newLines.end() )
                equal = false;
            else if ( newLinesI->size() < lineOffset + count )
                equal = false;
            else {
                const char* ptr = newLinesI->data() + lineOffset;
                equal = memcmp(ptr, buffer, count) == 0;
            }
        }
        lineOffset += count;
        if ( eol ) {
            checksum.NextLine();
            if ( equal ) {
                // check for end of line in newLines
                equal = newLinesI->size() == lineOffset;
                ++newLinesI;
            }
            lineOffset = 0;
        }
    }

    if ( haveChecksum ) {
        // file contains valid checksum -> it was not modified by user
        return false;
    }

    // file doesn't have checksum
    // we assume it modified if its content different from newLines
    return !equal  ||  newLinesI != newLines.end();
}


void CFileCode::LoadLines(TGenerateMethod method, list<string>& lines) const
{
    CNcbiOstrstream code;

    // generate code
    (this->*method)(code);

    // get code length
    size_t count = (size_t)GetOssSize(code);
    if ( count == 0 ) {
        NCBI_THROW(CDatatoolException,eInvalidData,"empty generated code");
    }

    // get code string pointer
    string codeStr = CNcbiOstrstreamToString(code);
    const char* codePtr = codeStr.data();

    // split code by lines
    while ( count > 0 ) {
        // find end of next line
        const char* eolPtr = (const char*)memchr(codePtr, '\n', count);
        if ( !eolPtr ) {
            NCBI_THROW(CDatatoolException,eInvalidData,
                       "unended line in generated code");
        }

        // add next line to list
        lines.push_back(kEmptyStr);
        lines.back().assign(codePtr, eolPtr);

        // skip EOL symbol ('\n')
        ++eolPtr;

        // update code length
        count -= (eolPtr - codePtr);
        // update code pointer
        codePtr = eolPtr;
    }
}


bool CFileCode::WriteUserFile(const string& path, const string& name,
                              string& fileName, TGenerateMethod method) const
{
    // parse new code lines
    list<string> newLines;
    LoadLines(method, newLines);

    fileName = Path(path, name);
    CreateFileFolder(fileName);
    if ( ModifiedByUser(fileName, newLines) ) {
        // do nothing on user modified files
        return false;
    }

    // write new contents of nonmodified file
    CDelayedOfstream out(fileName);
    if ( !out ) {
        ERR_POST_X(6, Fatal << "Cannot create file: " << fileName);
        return false;
    }

    CChecksum checksum;
    ITERATE ( list<string>, i, newLines ) {
        checksum.AddLine(*i);
        out << *i << '\n';
    }
    out << checksum;

    out.close();
    if ( !out ) {
        ERR_POST_X(7, "Error writing file " << fileName);
        return false;
    }
    return true;
}


void CFileCode::GenerateUserHPPCode(CNcbiOstream& header) const
{
    string hppDefine = GetUserHPPDefine();
    WriteUserCopyright(header, true) <<
        "\n"
        "#ifndef " << hppDefine << "\n"
        "#define " << hppDefine << "\n"
        "\n";

    header <<
        "\n"
        "// generated includes\n"
        "#include " << Include(GetBaseHPPName()) << "\n";
    
    CNamespace ns;
    if ( !m_Classes.empty() ) {
        header <<
            "\n"
            "// generated classes\n"
            "\n";
        ITERATE ( TClasses, i, m_Classes ) {
            ns.Set(i->ns, header, false);
            i->code->GenerateUserHPPCode(header);
        }
    }
    ns.Reset(header);
    
    WriteLogKeyword(header);
    header <<
        "\n"
        "#endif // " << hppDefine << "\n";
}


void CFileCode::GenerateUserCPPCode(CNcbiOstream& code) const
{
    WriteUserCopyright(code, false) <<
        "\n"
        "// standard includes\n";
    if (!m_PchHeader.empty()) {
        code <<
            "#include <" << m_PchHeader << ">\n";
    }
    code <<
        "\n"
        "// generated includes\n"
        "#include " << Include(GetUserHPPName()) << "\n";

    CNamespace ns;
    if ( !m_Classes.empty() ) {
        code <<
            "\n"
            "// generated classes\n"
            "\n";
        ITERATE ( TClasses, i, m_Classes ) {
            ns.Set(i->ns, code, false);
            i->code->GenerateUserCPPCode(code);
        }
    }
    ns.Reset(code);
    WriteLogKeyword(code);
}

CTypeStrings* CFileCode::GetPrimaryClass(void)
{
    m_CurrentClass = &*(m_Classes.begin());
    return m_CurrentClass->code.get();
}

bool CFileCode::GetClasses(list<CTypeStrings*>& types)
{
    m_CurrentClass = &*(m_Classes.begin());
    types.clear();
    ITERATE ( TClasses, i, m_Classes ) {
        types.push_back(i->code.get());
    }
    return !types.empty();
}

CNamespace CFileCode::GetClassNamespace(CTypeStrings* type)
{
    ITERATE ( TClasses, i, m_Classes ) {
        if (type == i->code.get()) {
            return i->ns;
        }
    }
    return m_CurrentClass->ns;
}

bool CFileCode::AddType(const CDataType* type)
{
    string idName = type->IdName() + type->GetNamespaceName();
    if ( m_AddedClasses.find(idName) != m_AddedClasses.end() )
        return false;
    m_AddedClasses.insert(idName);
    _TRACE("AddType: " << idName << ": " << typeid(*type).name());
    m_SourceFiles.insert(type->GetSourceFileName());
    AutoPtr<CTypeStrings> code = type->GenerateCode();
    code->SetModuleName(type->GetModule()->GetName());
    m_Classes.push_front(SClassInfo(type->Namespace(), code));
    return true;
}

void CFileCode::GetModuleNames( map<string, pair<string,string> >& names) const
{
    CNcbiOstrstream ostr;
    WriteSourceFile(ostr);
//    ostr.put('\0');
    string src_file = string(CNcbiOstrstreamToString(ostr));
    string doxmodule_name, module_name;

    ITERATE ( TClasses, i, m_Classes ) {
        doxmodule_name = i->code->GetDoxygenModuleName();
        module_name = i->code->GetModuleName();
        if (names.find(module_name) == names.end()) {
            names[module_name] = make_pair(src_file,doxmodule_name);
        }
    }
}


END_NCBI_SCOPE
