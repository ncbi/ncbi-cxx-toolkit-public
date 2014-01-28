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
#include "resolver.hpp"
#include "msvc_prj_utils.hpp"
#include "proj_builder_app.hpp"
#include <corelib/ncbistr.hpp>
#include "ptb_err_codes.hpp"


BEGIN_NCBI_SCOPE

//-----------------------------------------------------------------------------
CSymResolver::CSymResolver(void)
{
    Clear();
}


CSymResolver::CSymResolver(const CSymResolver& resolver)
{
    SetFrom(resolver);
}


CSymResolver::CSymResolver(const string& file_path)
{
    LoadFrom(file_path, this);
}


CSymResolver& CSymResolver::operator= (const CSymResolver& resolver)
{
    if (this != &resolver) {
	    Clear();
	    SetFrom(resolver);
    }
    return *this;
}


CSymResolver::~CSymResolver(void)
{
    Clear();
}


string CSymResolver::StripDefine(const string& define)
{
    return string(define, 2, define.length() - 3);
}


void CSymResolver::Resolve(const string& define, list<string>* resolved_def)
{
    resolved_def->clear();
    string data(define);
    bool modified = true;
    while (HasDefine(data) && modified) {
        modified = false;
        string::size_type start, prev, end;
        for (start=0, prev=-1; ; prev=start) {
            start = data.find("$(", prev+1);
            if (start == string::npos) {
                start = prev;
                break;
            }
        }
        end = data.find(")", start);
        if (end == string::npos) {
            LOG_POST(Warning << "Possibly incorrect MACRO definition in: " + define);
	        resolved_def->push_back(define);
	        return;
        }
        string raw_define = data.substr(start,end-start+1);
        CExpansionRule exprule;
        string val_define = FilterDefine(raw_define, exprule, m_Data);
        string str_define = StripDefine( val_define );

        CSimpleMakeFileContents::TContents::const_iterator m =
            m_Cache.find(str_define);

        if (m != m_Cache.end()) {
	        *resolved_def = m->second;
        } else {
            CSimpleMakeFileContents::TContents::const_iterator p =
                m_Data.m_Contents.find(str_define);
            if (p != m_Data.m_Contents.end()) {
                ITERATE(list<string>, n, p->second) {
                    list<string> new_resolved_def;
                    Resolve(*n, &new_resolved_def);
                    copy(new_resolved_def.begin(),
                        new_resolved_def.end(),
                        back_inserter(*resolved_def));
                }
            } else {
                string tmp = GetApp().GetConfigureMacro(str_define);
                if (!tmp.empty()) {
                    resolved_def->push_back(tmp);
                }
            }
            m_Cache[str_define] = *resolved_def;
        }
        if ( !resolved_def->empty() ) {
            exprule.ApplyRule(*resolved_def);
        }
        if ( resolved_def->size() == 1) {
            modified = true;
            NStr::ReplaceInPlace(data, raw_define, 
                raw_define != resolved_def->front() ? resolved_def->front() : kEmptyStr);
            resolved_def->clear();
        }
    }
    if (resolved_def->empty() && !data.empty()) {
        resolved_def->push_back(data);
    }
}

void CSymResolver::Resolve(const string& define, list<string>* resolved_def,
                           const CSimpleMakeFileContents& mdata)
{
    resolved_def->clear();
    string data(define);
    bool modified = true;
    while (HasDefine(data) && modified) {
        modified = false;
        string::size_type start, prev, end;
        for (start=0, prev=-1; ; prev=start) {
            start = data.find("$(", prev+1);
            if (start == string::npos) {
                start = prev;
                break;
            }
        }
        end = data.find(")", start);
        if (end == string::npos) {
            LOG_POST(Warning << "Possibly incorrect MACRO definition in: " + define);
	        resolved_def->push_back(define);
	        return;
        }
        string raw_define = data.substr(start,end-start+1);
        CExpansionRule exprule;
        string val_define = FilterDefine(raw_define, exprule, mdata);
        string str_define = StripDefine( val_define );

        CSimpleMakeFileContents::TContents::const_iterator p =
            mdata.m_Contents.find(str_define);
	    if (p != mdata.m_Contents.end()) {
            ITERATE(list<string>, n, p->second) {
                list<string> new_resolved_def;
                Resolve(*n, &new_resolved_def, mdata);
                copy(new_resolved_def.begin(),
                    new_resolved_def.end(),
                    back_inserter(*resolved_def));
            }
	    }
        if ( resolved_def->empty() ) {
            Resolve(raw_define, resolved_def);
        } else {
            exprule.ApplyRule(*resolved_def);
        }
        if ( resolved_def->size() == 1) {
            modified = true;
            NStr::ReplaceInPlace(data, raw_define, 
                raw_define != resolved_def->front() ? resolved_def->front() : kEmptyStr);
            resolved_def->clear();
        }
    }
    if (resolved_def->empty() && !data.empty()) {
        resolved_def->push_back(data);
    }
}

CSymResolver& CSymResolver::Append(const CSymResolver& src, bool warn_redef)
{
    // Clear cache for resolved defines
    m_Cache.clear();

    list<string> redefs;
    ITERATE( CSimpleMakeFileContents::TContents, i, src.m_Data.m_Contents) {
        if (m_Data.m_Contents.empty()) {
            m_Trusted.insert(i->first);
        } else {
            if (m_Data.m_Contents.find(i->first) != m_Data.m_Contents.end() &&
                m_Trusted.find(i->first) == m_Trusted.end() && warn_redef) {
                redefs.push_back(i->first);
                PTB_WARNING_EX(src.m_Data.GetFileName(),ePTB_ConfigurationError,
                    "Attempt to redefine already defined macro: " << i->first);
            }
        }
    }
    // Add contents of src
    copy(src.m_Data.m_Contents.begin(), 
         src.m_Data.m_Contents.end(), 
         inserter(m_Data.m_Contents, m_Data.m_Contents.end()));

    ITERATE( list<string>, r, redefs) {
        PTB_WARNING_EX(m_Data.GetFileName(),ePTB_ConfigurationError,
            *r << "= " << NStr::Join(m_Data.m_Contents[*r]," "));
    }
    return *this;
}

bool CSymResolver::IsDefine(const string& param)
{
    return (
        NStr::StartsWith(param, "$(") &&
        NStr::EndsWith(param, ")") &&
        NStr::FindNoCase(param, "$(", 2) == NPOS);
}

bool CSymResolver::HasDefine(const string& param)
{
    return (param.find("$(") != string::npos && param.find(")") != string::npos );
}

string CSymResolver::TrimDefine(const string& define)
{
    string::size_type start, end;
    string trimmed( define);
    while ((start = trimmed.find("$(")) != string::npos) {
        end = trimmed.rfind(")");
        if (end == string::npos) {
            break;;
        }
        trimmed.erase(start,end);
    }
    return trimmed;
}


void CSymResolver::LoadFrom(const string& file_path, 
                            CSymResolver * resolver)
{
    resolver->Clear();
    CSimpleMakeFileContents::LoadFrom(file_path, &resolver->m_Data);
}

void CSymResolver::AddDefinition(const string& key, const string& value)
{
    m_Data.AddDefinition(key, value);
}

bool CSymResolver::HasDefinition( const string& key) const
{
    return m_Data.HasDefinition(key);
}

bool CSymResolver::IsEmpty(void) const
{
    return m_Data.m_Contents.empty();
}


void CSymResolver::Clear(void)
{
    m_Data.m_Contents.clear();
    m_Cache.clear();
}


void CSymResolver::SetFrom(const CSymResolver& resolver)
{
    m_Data  = resolver.m_Data;
    m_Cache = resolver.m_Cache;
}


//-----------------------------------------------------------------------------
// Filter opt defines like $(SRC_C:.core_%)           to $(SRC_C).
// or $(OBJMGR_LIBS:dbapi_driver=dbapi_driver-static) to $(OBJMGR_LIBS)
string FilterDefine(const string& define)
{
//    if ( !CSymResolver::IsDefine(define) )
//        return define;

    string res;
    for(string::const_iterator p = define.begin(); p != define.end(); ++p) {
        char ch = *p;
        if ( !(ch == '$'   || 
               ch == '('   || 
               ch == '_'   || 
               isalpha((unsigned char) ch) || 
               isdigit((unsigned char) ch) ) )
            break;
        res += ch;
    }
    res += ')';
    return res;
}

string CSymResolver::FilterDefine(const string& define, CExpansionRule& rule,
    const CSimpleMakeFileContents& data)
{
    rule.Reset();
//    if (CMsvc7RegSettings::GetMsvcPlatform() == CMsvc7RegSettings::eUnix) {
{
        if ( !CSymResolver::IsDefine(define) )
            return define;
        string::size_type start = define.find(':');
        string::size_type end = define.rfind(')');
        if (start != string::npos && end != string::npos && end > start) {
            string textrule = define.substr(start+1, end-start-1);
            rule.Init(textrule, this, &data);
            string toreplace(":");
            toreplace += textrule;
            return NStr::Replace(define, toreplace, "");
        }
    }
    return ncbi::FilterDefine(define);
}

CExpansionRule::CExpansionRule(void)
{
    Reset();
}
CExpansionRule::CExpansionRule(const string& textrule)
{
    Init(textrule);
}
CExpansionRule::CExpansionRule(const CExpansionRule& other)
{
    m_Rule = other.m_Rule;
    m_Lvalue = other.m_Lvalue;
    m_Rvalue = other.m_Rvalue;
}
CExpansionRule& CExpansionRule::operator= (const CExpansionRule& other)
{
    if (this != &other) {
        m_Rule = other.m_Rule;
        m_Lvalue = other.m_Lvalue;
        m_Rvalue = other.m_Rvalue;
    }
    return *this;
}
void CExpansionRule::Reset()
{
   m_Rule = eNoop;
}
void CExpansionRule::Init(const string& textrule,
    CSymResolver* resolver,  const CSimpleMakeFileContents* data)
{
    Reset();
    if (NStr::SplitInTwo(textrule, "=", m_Lvalue, m_Rvalue)) {
        m_Rule = eReplace;
        m_Lvalue = NStr::TruncateSpaces(m_Lvalue);
        m_Rvalue = NStr::TruncateSpaces(m_Rvalue);
        if (m_Lvalue.empty() || m_Lvalue == "%") {
            m_Rule = ePattern;
            m_Lvalue.clear();
        } else if (NStr::FindCase(m_Lvalue, "%") != NPOS) {
            m_Rule = ePattern;
        }
    }
}

string CExpansionRule::ApplyRule( const string& value) const
{
    if (m_Rule == eNoop) {
        return value;
    } else if (value[0] == '$') {
        return value;
    } else if (m_Rule == eReplace && !m_Lvalue.empty()) {
        return NStr::Replace(value, m_Lvalue, m_Rvalue);
    }
//    else if (m_Rule == ePattern) {
    string tmp(value);
    if (!m_Lvalue.empty()) {
        string begins, ends;
        if (NStr::SplitInTwo(m_Lvalue, "%", begins, ends)) {
            if (!begins.empty() && !NStr::StartsWith( value, begins)) {
                return value;
            }
            if (!ends.empty() && !NStr::EndsWith( value, ends)) {
                return value;
            }
            if (!begins.empty()) {
                NStr::ReplaceInPlace(tmp, begins, "");
            }
            if (!ends.empty()) {
                NStr::ReplaceInPlace(tmp, ends, "");
            }
        }
    }
    return NStr::Replace( m_Rvalue, "%", tmp);
}

void  CExpansionRule::ApplyRule( list<string>& value) const
{
    if (m_Rule != eNoop) {
        for (list<string>::iterator i = value.begin(); i != value.end(); ++i) {
            *i = ApplyRule(*i);;
        }
    }
}


END_NCBI_SCOPE
