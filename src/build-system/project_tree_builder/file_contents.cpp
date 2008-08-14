/* $Id$
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
 * Author:  Viatcheslav Gorelenkov
 *
 */

#include <ncbi_pch.hpp>
#include "file_contents.hpp"
#include "proj_builder_app.hpp"
#include "msvc_prj_defines.hpp"
#include "ptb_err_codes.hpp"
#include <corelib/ncbistr.hpp>

BEGIN_NCBI_SCOPE

string MakeFileTypeAsString(EMakeFileType type)
{
    switch (type) {
    case eMakeType_Undefined:  return "";
    case eMakeType_Expendable: return "EXPENDABLE";
    case eMakeType_Potential:  return "POTENTIAL";
    case eMakeType_Excluded:   return "EXCLUDED";
    case eMakeType_ExcludedByReq:   return "EXCLUDEDBYREQ";
    default:                   return "INCORRECT!";
    }
}

//-----------------------------------------------------------------------------
CSimpleMakeFileContents::CSimpleMakeFileContents(void)
    : m_Type( eMakeType_Undefined )
{
}


CSimpleMakeFileContents::CSimpleMakeFileContents
    (const CSimpleMakeFileContents& contents)
{
    SetFrom(contents);
}


CSimpleMakeFileContents& CSimpleMakeFileContents::operator=
    (const CSimpleMakeFileContents& contents)
{
    if (this != &contents) {
        SetFrom(contents);
    }
    return *this;
}


CSimpleMakeFileContents::CSimpleMakeFileContents(
    const string& file_path, EMakeFileType type)
{
    LoadFrom(file_path, this);
    m_Type = type;
}


CSimpleMakeFileContents::~CSimpleMakeFileContents(void)
{
}


void CSimpleMakeFileContents::Clear(void)
{
    m_Contents.clear();
    m_Type = eMakeType_Undefined;
    m_Filename.erase();
}


void CSimpleMakeFileContents::SetFrom(const CSimpleMakeFileContents& contents)
{
    m_Contents = contents.m_Contents;
    m_Type = contents.m_Type;
    m_Filename = contents.m_Filename;
}


void CSimpleMakeFileContents::LoadFrom(const string&  file_path,
                                       CSimpleMakeFileContents* fc)
{
    CSimpleMakeFileContents::SParser parser(fc);
    fc->Clear();

    CNcbiIfstream ifs(file_path.c_str(), IOS_BASE::in | IOS_BASE::binary);
    if ( !ifs )
        NCBI_THROW(CProjBulderAppException, eFileOpen, file_path);

    parser.StartParse();

    string strline;
    while ( NcbiGetlineEOL(ifs, strline) )
	    parser.AcceptLine(strline);

    parser.EndParse();
    fc->m_Filename = file_path;
}

void CSimpleMakeFileContents::AddDefinition(const string& key,
                                            const string& value)
{
    SKeyValue kv;
    kv.m_Key = key;
    kv.m_Value = value;
    AddReadyKV(kv);
}

bool CSimpleMakeFileContents::HasDefinition( const string& key) const
{
    return m_Contents.find(key) != m_Contents.end();
}

bool CSimpleMakeFileContents::GetValue(const string& key, string& value) const
{
    TContents::const_iterator k = m_Contents.find(key);
    if (k == m_Contents.end()) {
        return false;
    }
    value = " ";
    const list<string>& lst = k->second;
    list<string>::const_iterator i = lst.begin();
    if (i != lst.end()) {
        value = *i;
        ++i;
    }
    for (; i != lst.end(); ++i) {
        value += ' ';
        value += *i;
    }
    if (!value.empty()) {
        string::size_type start, end, done = 0;
        while ((start = value.find("$(", done)) != string::npos) {
            end = value.find(")", start);
            if (end == string::npos) {
                PTB_WARNING_EX(m_Filename, ePTB_MacroInvalid,
                               "Invalid macro definition: " << value);
                break;
            }
            string raw_macro = value.substr(start,end-start+1);
            if (CSymResolver::IsDefine(raw_macro)) {
                string macro = CSymResolver::StripDefine(raw_macro);
                string definition;
                GetValue(macro, definition);
                value = NStr::Replace(value, raw_macro, definition);
            }
        }
        value = NStr::Replace(value,"-dll",kEmptyStr);
        value = NStr::Replace(value,"-l",kEmptyStr);
        value = NStr::Replace(value,"-static",kEmptyStr);
    }
    return true;
}


void CSimpleMakeFileContents::Dump(CNcbiOfstream& ostr) const
{
    ITERATE(TContents, p, m_Contents) {
	    ostr << p->first << " = ";
	    ITERATE(list<string>, m, p->second) {
		    ostr << *m << " ";
	    }
	    ostr << endl;
    }
}


CSimpleMakeFileContents::SParser::SParser(CSimpleMakeFileContents* fc)
    :m_FileContents(fc)
{
}


void CSimpleMakeFileContents::SParser::StartParse(void)
{
    m_Continue  = false;
    m_CurrentKV = SKeyValue();
}



//------------------------------------------------------------------------------
// helpers ---------------------------------------------------------------------

static bool s_WillContinue(const string& line)
{
    return NStr::EndsWith(line, "\\");
}


static void s_StripContinueStr(string* str)
{
    str->erase(str->length() -1, 1); // delete last '\'
    *str += " ";
}


static bool s_SplitKV(const string& line, string* key, string* value)
{
    if ( !NStr::SplitInTwo(line, "=", *key, *value) )
	    return false;

    *key = NStr::TruncateSpaces(*key); // only for key - preserve sp for vals
    if ( s_WillContinue(*value) ) 
	    s_StripContinueStr(value);		

    return true;
}


static bool s_IsKVString(const string& str)
{
    size_t eq_pos = str.find("=");
    if (eq_pos == NPOS)
        return false;
    string mb_key = str.substr(0, eq_pos - 1);
    return mb_key.find_first_of("$()") == NPOS;
}


static bool s_IsCommented(const string& str)
{
    return NStr::StartsWith(str, "#");
}



void CSimpleMakeFileContents::SParser::AcceptLine(const string& line)
{
    string strline = NStr::TruncateSpaces(line);
    if ( s_IsCommented(strline) )
	    return;

    if (m_Continue) {
	    m_Continue = s_WillContinue(strline);
	    if ( strline.empty() ) {
		    //fix for ill-formed makefiles:
		    m_FileContents->AddReadyKV(m_CurrentKV);
		    return;
	    } else if ( s_IsKVString(strline) ) {
		    //fix for ill-formed makefiles:
		    m_FileContents->AddReadyKV(m_CurrentKV);
		    m_Continue = false; // guard 
		    AcceptLine(strline.c_str()); 
	    }
	    if (m_Continue)
		    s_StripContinueStr(&strline);
	    m_CurrentKV.m_Value += strline;
	    return;
	    
    } else {
	    // may be only k=v string
	    if ( s_IsKVString(strline) ) {
		    m_FileContents->AddReadyKV(m_CurrentKV);
		    m_Continue = s_WillContinue(strline);
		    s_SplitKV(strline, &m_CurrentKV.m_Key, &m_CurrentKV.m_Value);
		    return;			
	    }
    }
}


void CSimpleMakeFileContents::SParser::EndParse(void)
{
    m_FileContents->AddReadyKV(m_CurrentKV);
    m_Continue = false;
    m_CurrentKV = SKeyValue();
}


void CSimpleMakeFileContents::AddReadyKV(const SKeyValue& kv)
{
    if ( kv.m_Key.empty() ) 
	    return;

    list<string> values;
    NStr::Split(kv.m_Value, LIST_SEPARATOR, values);

    if (kv.m_Key == "CHECK_CMD") {
        m_Contents[kv.m_Key].push_back( kv.m_Value);
    } else {
        m_Contents[kv.m_Key] = values;
    }
}


END_NCBI_SCOPE
