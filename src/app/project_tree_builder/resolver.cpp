#include <app/project_tree_builder/resolver.hpp>
#include <corelib/ncbistr.hpp>

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


CSymResolver& CSymResolver::operator = (const CSymResolver& resolver)
{
    if(this != &resolver)
    {
	    Clear();
	    SetFrom(resolver);
    }
    return *this;
}


CSymResolver::~CSymResolver(void)
{
    Clear();
}


static string s_StripDefine(const string& define)
{
    return string( define, 2, define.length() - 3);
}


void CSymResolver::Resolve(const string& define, list<string> * pResolvedDef)
{
    if ( !IsDefine(define) ) {
	    pResolvedDef->push_back(define);
	    return;
    }

    string str_define = s_StripDefine(define);

    CSimpleMakeFileContents::TContents::const_iterator m = m_Cache.find(str_define);
    if (m != m_Cache.end()) {
	    *pResolvedDef = m->second;
	    return;
    }
	    
    ITERATE(CSimpleMakeFileContents::TContents, p, m_Data.m_Contents) {
	    if (p->first == str_define) {
            ITERATE(list<string>, n, p->second) {
                Resolve(*n, pResolvedDef);
            }
	    }
    }

    m_Cache[str_define] = *pResolvedDef;
}


bool CSymResolver::IsDefine(const string& param)
{
    return NStr::StartsWith(param, "$(")  &&  NStr::EndsWith(param, ")");
}


void CSymResolver::LoadFrom(const string& file_path, CSymResolver * pResolver)
{
    pResolver->Clear();
    CSimpleMakeFileContents::LoadFrom(file_path, &pResolver->m_Data);
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


