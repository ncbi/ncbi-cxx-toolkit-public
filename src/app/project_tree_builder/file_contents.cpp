#include <app/project_tree_builder/file_contents.hpp>
#include <app/project_tree_builder/proj_builder_app.hpp>
#include <corelib/ncbistr.hpp>

BEGIN_NCBI_SCOPE
//------------------------------------------------------------------------------

CSimpleMakeFileContents::CSimpleMakeFileContents(void)
{
}


CSimpleMakeFileContents::CSimpleMakeFileContents(
                                const CSimpleMakeFileContents& contents)
{
    SetFrom(contents);
}


CSimpleMakeFileContents& CSimpleMakeFileContents::operator = (
                                const CSimpleMakeFileContents& contents)
{
    if (this != &contents) {

        SetFrom(contents);
    }
    return *this;
}


CSimpleMakeFileContents::CSimpleMakeFileContents(const string& file_path)
{
    LoadFrom(file_path, this);
}


CSimpleMakeFileContents::~CSimpleMakeFileContents(void)
{
}


void CSimpleMakeFileContents::Clear(void)
{
    m_Contents.clear();
}


void CSimpleMakeFileContents::SetFrom(const CSimpleMakeFileContents& contents)
{
    m_Contents = contents.m_Contents;
}


void CSimpleMakeFileContents::LoadFrom(const string& path, 
                                       CSimpleMakeFileContents * pFC)
{
    CSimpleMakeFileContents::SParser parser(pFC);
    pFC->Clear();

    CNcbiIfstream ifs(path.c_str());
    if ( !ifs )
        NCBI_THROW(CProjBulderAppException, eFileOpen, path);

    parser.StartParse();

    string strline;
    while ( getline(ifs, strline) )
	    parser.AcceptLine(strline);

    parser.EndParse();
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


CSimpleMakeFileContents::SParser::SParser(CSimpleMakeFileContents * pFC)
    :m_pFC(pFC)
{
}


void CSimpleMakeFileContents::SParser::StartParse(void)
{
    m_Continue = false;
    m_CurrentKV = pair<string, string>();
}

//------------------------------------------------------------------------------
// helpers ---------------------------------------------------------------------
static bool s_WillContinue(const string& line)
{
    return NStr::EndsWith(line, "\\");
}


static void s_StripContinueStr(string * pStr)
{
    pStr->erase(pStr->length() -1, 1); // delete last '\'
}


static bool s_SplitKV(const string& line, string * pKey, string * pValue)
{
    bool ok = NStr::SplitInTwo(line, "=", *pKey, *pValue);
    if ( !ok ) 
	    return false;

    *pKey = NStr::TruncateSpaces(*pKey); // only for key - preserve sp for vals
    if ( s_WillContinue(*pValue) ) 
	    s_StripContinueStr(pValue);		

    return true;
}


static bool s_IsKVString(const string& str)
{
    return str.find("=") != NPOS;
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
	    if (strline.empty()) {
		    //fix for ill-formed makefiles:
		    m_pFC->AddReadyKV(m_CurrentKV);
		    return;
	    }
	    else if ( s_IsKVString(strline) )
	    {
		    //fix for ill-formed makefiles:
		    m_pFC->AddReadyKV(m_CurrentKV);
		    m_Continue = false; // guard 
		    AcceptLine(strline.c_str()); 
	    }
	    if (m_Continue)
		    s_StripContinueStr(&strline);
	    m_CurrentKV.second += strline;
	    return;
	    
    } else {
	    // may be only k=v string
	    if ( s_IsKVString(strline) ) {
		    m_pFC->AddReadyKV(m_CurrentKV);
		    m_Continue = s_WillContinue(strline);
		    s_SplitKV(strline, &m_CurrentKV.first, &m_CurrentKV.second);
		    return;			
	    }
    }
}


void CSimpleMakeFileContents::SParser::EndParse(void)
{
    m_pFC->AddReadyKV(m_CurrentKV);
    m_Continue = false;
    m_CurrentKV = pair<string, string>();
}


void CSimpleMakeFileContents::AddReadyKV(const pair<string, string>& kv)
{
    if ( kv.first.empty() ) 
	    return;

    list<string> values;
    NStr::Split(kv.second, " \t\n\r", values);

    m_Contents[kv.first] = values;
}

//------------------------------------------------------------------------------
END_NCBI_SCOPE
