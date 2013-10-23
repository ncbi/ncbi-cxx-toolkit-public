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
 * Author:  Lewis Geer
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <corelib/request_ctx.hpp>
#include <corelib/ncbi_strings.h>
#include <html/components.hpp>
#include <html/page.hpp>

#include <errno.h>

BEGIN_NCBI_SCOPE


// The buffer size for reading from stream.
const SIZE_TYPE kBufferSize = 4096;

extern const char* kTagStart;
extern const char* kTagEnd;
// Tag start in the end of block definition (see page templates)
const char* kTagStartEnd = "</@";  

// Template file caching (disabled by default)
CHTMLPage::ECacheTemplateFiles CHTMLPage::sm_CacheTemplateFiles = CHTMLPage::eCTF_Disable;
typedef map<string, string*> TTemplateCache;
static CSafeStatic<TTemplateCache> s_TemplateCache;


const string& CPageStat::GetValue(const string& name) const
{
    TData::const_iterator it = m_Data.find(name);
    return it == m_Data.end() ? kEmptyStr : it->second;
}


void CPageStat::SetValue(const string& name, const string& value)
{
    if ( !value.empty() ) {
        m_Data[name] = value;
    }
    else {
        TData::iterator it = m_Data.find(name);
        if (it != m_Data.end()) {
            m_Data.erase(it);
        }
    }
}


class CHTMLPageStat : public CNCBINode
{
    typedef CNCBINode CParent;
public:
    CHTMLPageStat(CHTMLBasicPage& page);
    ~CHTMLPageStat(void);
    
    virtual CNcbiOstream& PrintBegin(CNcbiOstream& out, TMode mode);

private:
    const CHTMLBasicPage& m_Page;
};


CHTMLPageStat::CHTMLPageStat(CHTMLBasicPage& page)
    : CNCBINode("ncbipagestat"),
      m_Page(page)
{
    return;
}


CHTMLPageStat::~CHTMLPageStat(void)
{
    return;
}


CNcbiOstream& CHTMLPageStat::PrintBegin(CNcbiOstream& out, TMode mode)
{
    const CPageStat::TData& stat = m_Page.GetPageStat().GetData();
    if ( stat.empty() ) {
        return out;
    }
    bool phid_present = false;
    string phid = CDiagContext::GetRequestContext().GetHitID();
    ITERATE(CPageStat::TData, it, stat) {
        if ( NStr::EqualNocase(it->first,
            g_GetNcbiString(eNcbiStrings_PHID)) ) {
            phid_present = true;
        }
        CHTML_meta meta(CHTML_meta::eName, it->first, it->second);
        meta.PrintBegin(out, mode);
        out << endl;
    }
    if ( !phid_present  &&  !phid.empty() ) {
        CHTML_meta meta(CHTML_meta::eName, g_GetNcbiString(eNcbiStrings_PHID),
            phid);
        meta.PrintBegin(out, mode);
        out << endl;
    }
    return out;
}


// CHTMLBasicPage

CHTMLBasicPage::CHTMLBasicPage(void)
    : CParent("basicpage"),
      m_CgiApplication(0),
      m_Style(0)
{
    AddTagMap("NCBI_PAGE_STAT", new CHTMLPageStat(*this));
    return;
}


CHTMLBasicPage::CHTMLBasicPage(CCgiApplication* application, int style)
    : m_CgiApplication(application),
      m_Style(style),
      m_PrintMode(eHTML)
{
    AddTagMap("NCBI_PAGE_STAT", new CHTMLPageStat(*this));
    return;
}


CHTMLBasicPage::~CHTMLBasicPage(void)
{
    for (TTagMap::iterator i = m_TagMap.begin(); i != m_TagMap.end(); ++i) {
        delete i->second;
    }
}


void CHTMLBasicPage::SetApplication(CCgiApplication* App)
{
    m_CgiApplication = App;
}


void CHTMLBasicPage::SetStyle(int style)
{
    m_Style = style;
}


CNCBINode* CHTMLBasicPage::MapTag(const string& name)
{
    map<string, BaseTagMapper*>::iterator i = m_TagMap.find(name);
    if ( i != m_TagMap.end() ) {
        return (i->second)->MapTag(this, name);
    }
    return CParent::MapTag(name);
}


void CHTMLBasicPage::AddTagMap(const string& name, CNCBINode* node)
{
    AddTagMap(name, CreateTagMapper(node));
}


void CHTMLBasicPage::AddTagMap(const string& name, BaseTagMapper* mapper)
{
    delete m_TagMap[name];
    m_TagMap[name] = mapper;
}


// CHTMLPage

CHTMLPage::CHTMLPage(const string& title)
    : m_Title(title)
{
    Init();
}


CHTMLPage::CHTMLPage(const string& title, const string& template_file)
    : m_Title(title)
{
    Init();
    SetTemplateFile(template_file);
}


CHTMLPage::CHTMLPage(const string& title, istream& template_stream)
    : m_Title(title)
{
    Init();
    SetTemplateStream(template_stream);
}


CHTMLPage::CHTMLPage(const string& /*title*/,
                     const void* template_buffer, SIZE_TYPE size)
{
    Init();
    SetTemplateBuffer(template_buffer, size);
}


CHTMLPage::CHTMLPage(CCgiApplication* application, int style,
                     const string& title, const string& template_file)
    : CParent(application, style),
      m_Title(title)
{
    Init();
    SetTemplateFile(template_file);
}


void CHTMLPage::Init(void)
{
    // Generate internal page name
    GeneratePageInternalName();

    // Template sources
    m_TemplateFile   = kEmptyStr;
    m_TemplateStream = 0;
    m_TemplateBuffer = 0;
    m_TemplateSize   = 0;
    
    AddTagMap("TITLE", CreateTagMapper(this, &CHTMLPage::CreateTitle));
    AddTagMap("VIEW",  CreateTagMapper(this, &CHTMLPage::CreateView));
}


void CHTMLPage::CreateSubNodes(void)
{
    bool create_on_print = (m_TemplateFile.empty()  || sm_CacheTemplateFiles == eCTF_Disable);
    if ( !create_on_print ) {
        AppendChild(CreateTemplate());
    }
    // Otherwise, create template while printing to avoid
    // latency on large files
}


CNCBINode* CHTMLPage::CreateTitle(void) 
{
    if ( GetStyle() & fNoTITLE )
        return 0;

    return new CHTMLText(m_Title);
}


CNCBINode* CHTMLPage::CreateView(void) 
{
    return 0;
}


void CHTMLPage::AddTagMap(const string& name, CNCBINode* node)
{
    CParent::AddTagMap(name, node);
}


void CHTMLPage::AddTagMap(const string& name, BaseTagMapper* mapper)
{
    CParent::AddTagMap(name,mapper);
}


CNcbiOstream& CHTMLPage::PrintChildren(CNcbiOstream& out, TMode mode)
{
    if (HaveChildren()) {
        return CParent::PrintChildren(out, mode);
    } else {
        m_PrintMode = mode;
        AppendChild(CreateTemplate(&out, mode));
        return out;
    }
}


CNCBINode* CHTMLPage::CreateTemplate(CNcbiOstream* out, CNCBINode::TMode mode)
{
    string  str;
    string* pstr = &str;

    TTemplateCache& cache = s_TemplateCache.Get();

    // File
    if ( !m_TemplateFile.empty() ) {
        if ( sm_CacheTemplateFiles == eCTF_Enable ) {
            TTemplateCache::const_iterator i 
                = cache.find(m_TemplateFile);
            if ( i != cache.end() ) {
                pstr = i->second;
            } else {
                pstr = new string();
                CNcbiIfstream is(m_TemplateFile.c_str());
                x_LoadTemplate(is, *pstr);
                cache[m_TemplateFile] = pstr;
            }
        } else {
            CNcbiIfstream is(m_TemplateFile.c_str());
            if ( out ) {
                return x_PrintTemplate(is, out, mode);
            }
            x_LoadTemplate(is, str);
        }

    // Stream
    } else if ( m_TemplateStream ) {
        if ( out ) {
            return x_PrintTemplate(*m_TemplateStream, out, mode);
        }
        x_LoadTemplate(*m_TemplateStream, str);

    // Buffer
    } else if ( m_TemplateBuffer ) {
        str.assign((char*)m_TemplateBuffer, m_TemplateSize);

    // Otherwise
    } else {
        return new CHTMLText(kEmptyStr);
    }

    // Print and return node
    {{
        auto_ptr<CHTMLText> node(new CHTMLText(*pstr));
        if ( out ) {
            node->Print(*out, mode);
        }
        return node.release();
    }}
}


void CHTMLPage::x_LoadTemplate(CNcbiIstream& is, string& str)
{
    if ( !is.good() ) {
        NCBI_THROW(CHTMLException, eTemplateAccess,
                   "CHTMLPage::x_LoadTemplate(): failed to open template");
    }

    char buf[kBufferSize];

    // If loading template from the file, get its size first
    if ( m_TemplateFile.size() ) {
        Int8 size = CFile(m_TemplateFile).GetLength();
        if (size < 0) {
            NCBI_THROW(CHTMLException, eTemplateAccess,
                       "CHTMLPage::x_LoadTemplate(): failed to "  \
                       "open template file '" + m_TemplateFile + "'");
        }
        if ((Uint8)size >= numeric_limits<size_t>::max()) {
            NCBI_THROW(CHTMLException, eTemplateTooBig,
                       "CHTMLPage: input template " + m_TemplateFile
                       + " too big to handle");
        }
        m_TemplateSize = (SIZE_TYPE)size;
    }
    // Reserve space
    if ( m_TemplateSize ) {
        str.reserve(m_TemplateSize);
    }
    while ( is ) {
        is.read(buf, sizeof(buf));
        if (m_TemplateSize == 0  &&  is.gcount() > 0
            &&  str.size() == str.capacity()) {
            // We don't know how big string will need to be,
            // so we grow it exponentially.
            str.reserve(str.size() + max((SIZE_TYPE)is.gcount(),
                        str.size() / 2));
        }
        str.append(buf, (SIZE_TYPE)is.gcount());
    }

    if ( !is.eof() ) {
        NCBI_THROW(CHTMLException, eTemplateAccess,
                   "CHTMLPage::x_LoadTemplate(): error reading template");
    }
}


CNCBINode* CHTMLPage::x_PrintTemplate(CNcbiIstream& is, CNcbiOstream* out,
                                      CNCBINode::TMode mode)
{
    if ( !is.good() ) {
        NCBI_THROW(CHTMLException, eTemplateAccess,
                   "CHTMLPage::x_PrintTemplate(): failed to open template");
    }
    if ( !out ) {
        NCBI_THROW(CHTMLException, eNullPtr,
                   "CHTMLPage::x_PrintTemplate(): " \
                   "output stream must be specified");
    }

    string str;
    char   buf[kBufferSize];
    auto_ptr<CNCBINode> node(new CNCBINode);

    while (is) {
        is.read(buf, sizeof(buf));
        str.append(buf, (SIZE_TYPE)is.gcount());
        SIZE_TYPE pos = str.rfind('\n');
        if (pos != NPOS) {
            ++pos;
            CHTMLText* child = new CHTMLText(str.substr(0, pos));
            child->Print(*out, mode);
            node->AppendChild(child);
            str.erase(0, pos);
        }
    }
    if ( !str.empty() ) {
        CHTMLText* child = new CHTMLText(str);
        child->Print(*out, mode);
        node->AppendChild(child);
    }

    if ( !is.eof() ) {
        NCBI_THROW(CHTMLException, eTemplateAccess,
                    "CHTMLPage::x_PrintTemplate(): error reading template");
    }
    
    return node.release();
}


void CHTMLPage::CacheTemplateFiles(ECacheTemplateFiles caching)
{
    sm_CacheTemplateFiles = caching;
}


void CHTMLPage::SetTemplateFile(const string& template_file)
{
    m_TemplateFile   = template_file;
    m_TemplateStream = 0;
    m_TemplateBuffer = 0;
    m_TemplateSize   = 0;
    GeneratePageInternalName(template_file);
}


static SIZE_TYPE s_Find(const string& s, const char* target,
                        SIZE_TYPE start = 0)
{
    // Return s.find(target);
    // Some implementations of string::find call memcmp at every
    // possible position, which is way too slow.
    if ( start >= s.size() ) {
        return NPOS;
    }
    const char* cstr = s.c_str();
    const char* p    = strstr(cstr + start, target);
    return p ? p - cstr : NPOS;
}

bool CHTMLPage::x_ApplyFilters(TTemplateLibFilter* filter, const char* buffer)
{
    bool template_applicable = true;

    while (*buffer != '\0') {
        while (isspace(*buffer))
            ++buffer;

        const char* id_begin = buffer;

        for (; *buffer != '\0'; ++buffer)
            if (*buffer == '(' || *buffer == '<' || *buffer == '{')
                break;

        if (id_begin == buffer || *buffer == '\0')
            break;

        string id(id_begin, buffer - id_begin);

        char bracket_stack[sizeof(long)];
        char* bracket_stack_pos = bracket_stack + sizeof(bracket_stack) - 1;

        *bracket_stack_pos = '\0';

        for (;;) {
            char closing_bracket;

            if (*buffer == '(')
                closing_bracket = ')';
            else if (*buffer == '<')
                closing_bracket = '>';
            else if (*buffer == '{')
                closing_bracket = '}';
            else
                break;

            if (bracket_stack_pos == bracket_stack) {
                NCBI_THROW(CHTMLException, eUnknown,
                    "Bracket nesting is too deep");
            }

            *--bracket_stack_pos = closing_bracket;
            ++buffer;
        }

        const char* pattern_end;

        if ((pattern_end = strstr(buffer, bracket_stack_pos)) == NULL) {
            NCBI_THROW(CHTMLException, eUnknown,
                    "Unterminated filter expression");
        }

        if (template_applicable && (filter == NULL ||
                !filter->TestAttribute(id, string(buffer, pattern_end))))
            template_applicable = false;

        buffer = pattern_end + (bracket_stack +
            sizeof(bracket_stack) - 1 - bracket_stack_pos);
    }

    return template_applicable;
}

void CHTMLPage::x_LoadTemplateLib(CNcbiIstream& istrm, SIZE_TYPE size,
                                  ETemplateIncludes includes, 
                                  const string& file_name /* = kEmptyStr */,
                                  TTemplateLibFilter* filter)
{
    string  template_buf("\n");
    string* pstr      = &template_buf;
    bool    caching   = false;
    bool    need_read = true;

    AutoPtr<CNcbiIstream> is(&istrm, eNoOwnership);
    TTemplateCache& cache = s_TemplateCache.Get();

    if ( !file_name.empty()  &&  sm_CacheTemplateFiles == eCTF_Enable ) {
        TTemplateCache::const_iterator i = cache.find(file_name);
        if ( i != cache.end() ) {
            pstr = i->second;
            need_read = false;
        } else { 
            pstr = new string();
            caching = true;
        }
    }

    // Load template in memory all-in-all
    if ( need_read ) {
        // Open and check file, if this is a file template
        if ( !file_name.empty() ) {
            Int8 x_size = CFile(file_name).GetLength();
            if (x_size == 0) {
                return;
            } else if (x_size < 0) {
                NCBI_THROW(CHTMLException, eTemplateAccess,
                           "CHTMLPage::x_LoadTemplateLib(): failed to "  \
                           "open template file '" + file_name  + "'");
            } else if ((Uint8)x_size >= numeric_limits<size_t>::max()) {
                NCBI_THROW(CHTMLException, eTemplateTooBig,
                           "CHTMLPage::x_LoadTemplateLib(): template " \
                           "file '" + file_name + 
                           "' is too big to handle");
            }
            is.reset(new CNcbiIfstream(file_name.c_str()), eTakeOwnership);
            size = (SIZE_TYPE)x_size;
        }

        // Reserve space
        if ( size ) {
            pstr->reserve(size);
        }
        if (includes == eAllowIncludes) {
            // Read line by line and parse it for #includes
            string s;
            static const char*     kInclude = "#include ";
            static const SIZE_TYPE kIncludeLen = strlen(kInclude);

            for (int i = 1;  NcbiGetline(*is, s, "\r\n");  ++i) {

                if ( NStr::StartsWith(s, kInclude) ) {
                    SIZE_TYPE pos = kIncludeLen;
                    SIZE_TYPE len = s.length();
                    while (pos < len  && isspace((unsigned char)s[pos])) {
                        pos++;
                    }
                    bool error = false;
                    if (pos < len  &&  s[pos] == '\"') {
                        pos++;
                        SIZE_TYPE pos_end = s.find("\"", pos);
                        if (pos_end == NPOS) {
                            error = true;
                        } else {
                            string fname = s.substr(pos, pos_end-pos);
                            LoadTemplateLibFile(fname);
                        }
                    } else {
                        error = true;
                    }
                    if ( error ) {
                        NCBI_THROW(CHTMLException, eTemplateAccess,
                                   "CHTMLPage::x_LoadTemplateLib(): " \
                                   "incorrect #include syntax, file '" +
                                   file_name + "', line " + 
                                   NStr::IntToString(i));
                    }

                } else {  // General line

                    if (pstr->size() == pstr->capacity() &&
                        s.length() > 0) {
                        // We don't know how big str will need to be,
                        // so we grow it exponentially.
                        pstr->reserve(pstr->size() + 
                                      max((SIZE_TYPE)is->gcount(),
                                      pstr->size() / 2));
                    }
                    pstr->append(s + "\n");
                }
            }
        } else {
            // Use faster block read
            char buf[kBufferSize];
            while (is) {
                is->read(buf, sizeof(buf));
                if (pstr->size() == pstr->capacity()  &&
                    is->gcount() > 0) {
                    // We don't know how big str will need to be,
                    // so we grow it exponentially.
                    pstr->reserve(pstr->size() + 
                                  max((SIZE_TYPE)is->gcount(),
                                  pstr->size() / 2));
                }
                pstr->append(buf, (SIZE_TYPE)is->gcount());
            }
        }
        if ( !is->eof() ) {
            NCBI_THROW(CHTMLException, eTemplateAccess,
                       "CHTMLPage::x_LoadTemplateLib(): " \
                       "error reading template");
        }
    }

    // Cache template lib
    if ( caching ) {
        cache[file_name] = pstr;
    }

    // Parse template
    // Note: never change pstr here!

    const string kTagStartBOL(string("\n") + kTagStart); 
    SIZE_TYPE ts_size   = kTagStartBOL.length();
    SIZE_TYPE te_size   = strlen(kTagEnd);
    SIZE_TYPE tse_size  = strlen(kTagStartEnd);
    SIZE_TYPE tag_start = s_Find(*pstr, kTagStartBOL.c_str());

    while ( tag_start != NPOS ) {

        // Get name
        string name;
        SIZE_TYPE name_start = tag_start + ts_size;
        SIZE_TYPE name_end   = s_Find(*pstr, kTagEnd, name_start);
        if ( name_end == NPOS ) {
            // Tag not closed
            NCBI_THROW(CHTMLException, eTextUnclosedTag,
                "opening tag \"" + name + "\" not closed, " \
                "stream pos = " + NStr::NumericToString(tag_start));
        }
        if (name_end != name_start) {
            // Tag found
            name = pstr->substr(name_start, name_end - name_start);
        }
        bool template_applicable = true;
        string::size_type space_pos;
        if ((space_pos = name.find_first_of(" \t")) != string::npos) {
            template_applicable =
                x_ApplyFilters(filter, name.c_str() + space_pos + 1);
            name.erase(space_pos);
        }
        SIZE_TYPE tag_end = name_end + te_size;

        // Find close tags for "name"
        string close_str = kTagStartEnd;
        if ( !name.empty() ) {
            close_str += name + kTagEnd;
        }
        SIZE_TYPE last = s_Find(*pstr, close_str.c_str(), tag_end);
        if ( last == NPOS ) {
            // Tag not closed
            NCBI_THROW(CHTMLException, eTextUnclosedTag,
                "closing tag \"" + name + "\" not closed, " \
                "stream pos = " + NStr::NumericToString(tag_end));
        }
        if ( name.empty() ) {
            tag_start = s_Find(*pstr, kTagStartBOL.c_str(),
                               last + tse_size);
            continue;
        }

        // Is it a multi-line template? Remove redundant line breaks.
        SIZE_TYPE pos = pstr->find_first_not_of(" ", tag_end);
        if (pos != NPOS  &&  (*pstr)[pos] == '\n') {
            tag_end = pos + 1;
        }
        pos = pstr->find_first_not_of(" ", last - 1);
        if (pos != NPOS  &&  (*pstr)[pos] == '\n') {
            last = pos;
        }

        // Get sub-template
        string subtemplate = pstr->substr(tag_end, last - tag_end);

        // Add sub-template resolver
        if (template_applicable)
            AddTagMap(name, CreateTagMapper(new CHTMLText(subtemplate)));

        // Find next
        tag_start = s_Find(*pstr, kTagStartBOL.c_str(),
                           last + te_size + name_end - name_start + tse_size);

    }
}


void CHTMLPage::LoadTemplateLibFile(const string& template_file,
                                    TTemplateLibFilter* filter)
{
    // We will open file in x_LoadTemplateLib just before reading from it.
    // This allow to minimize stat() calls when template caching is enabled.
    CNcbiIfstream is;
    x_LoadTemplateLib(is, 0 /* size - determine later */,
                      eAllowIncludes, template_file, filter);
}

    
END_NCBI_SCOPE
