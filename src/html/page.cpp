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
#include <html/components.hpp>
#include <html/page.hpp>
#include <html/jsmenu.hpp>
#include <corelib/ncbiutil.hpp>

#include <errno.h>


BEGIN_NCBI_SCOPE



// The buffer size for reading from stream.
const SIZE_TYPE kBufferSize = 4096;

extern const char* kTagStart;
extern const char* kTagEnd;
extern const char* kTagStartEnd;
 

// CHTMLBasicPage

CHTMLBasicPage::CHTMLBasicPage(void)
    : CParent("basicpage"),
      m_CgiApplication(0),
      m_Style(0)
{
    return;
}


CHTMLBasicPage::CHTMLBasicPage(CCgiApplication* application, int style)
    : m_CgiApplication(application),
      m_Style(style),
      m_PrintMode(eHTML)
{
    return;
}


CHTMLBasicPage::~CHTMLBasicPage(void)
{
    // BW_02:  the following does not compile on MSVC++ 6.0 SP3:
    // DeleteElements(m_TagMap);

    for (TTagMap::iterator i = m_TagMap.begin();  i != m_TagMap.end();  ++i) {
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
    
    m_UsePopupMenus  = false;

    AddTagMap("TITLE", CreateTagMapper(this, &CHTMLPage::CreateTitle));
    AddTagMap("VIEW",  CreateTagMapper(this, &CHTMLPage::CreateView));
}


void CHTMLPage::CreateSubNodes(void)
{
    if (m_UsePopupMenus) {
        AppendChild(CreateTemplate());
    }
    // Otherwise, done while printing to avoid latency on large files
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


void CHTMLPage::EnablePopupMenu(CHTMLPopupMenu::EType type,
                                 const string& menu_script_url,
                                 bool use_dynamic_menu)
{
    SPopupMenuInfo info(menu_script_url, use_dynamic_menu);
    m_PopupMenus[type] = info;
}


static bool s_CheckUsePopupMenus(const CNCBINode* node,
                                 CHTMLPopupMenu::EType type)
{
    if ( !node  ||  !node->HaveChildren() ) {
        return false;
    }
    ITERATE ( CNCBINode::TChildren, i, node->Children() ) {
        const CNCBINode* cnode = node->Node(i);
        if ( dynamic_cast<const CHTMLPopupMenu*>(cnode) ) {
            const CHTMLPopupMenu* menu
                = dynamic_cast<const CHTMLPopupMenu*>(cnode);
            if ( menu->GetType() == type )
                return true;
        }
        if ( cnode->HaveChildren()  &&  s_CheckUsePopupMenus(cnode, type)) {
            return true;
        }
    }
    return false;
}


void CHTMLPage::AddTagMap(const string& name, CNCBINode* node)
{
    CParent::AddTagMap(name, node);

    for (int t = CHTMLPopupMenu::ePMFirst; t <= CHTMLPopupMenu::ePMLast; t++ )
    {
        CHTMLPopupMenu::EType type = (CHTMLPopupMenu::EType)t;
        if ( m_PopupMenus.find(type) == m_PopupMenus.end() ) {
            if ( s_CheckUsePopupMenus(node, type) ) {
                EnablePopupMenu(type);
                m_UsePopupMenus = true;
            }
        } else {
            m_UsePopupMenus = true;
        }
    }
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
    // Get template stream
    if ( !m_TemplateFile.empty() ) {
        CNcbiIfstream is(m_TemplateFile.c_str());
        return x_CreateTemplate(is, out, mode);
    } else if ( m_TemplateStream ) {
        return x_CreateTemplate(*m_TemplateStream, out, mode);
    } else if ( m_TemplateBuffer ) {
        CNcbiIstrstream is((char*)m_TemplateBuffer, m_TemplateSize);
        return x_CreateTemplate(is, out, mode);
    } else {
        return new CHTMLText(kEmptyStr);
    }
}


CNCBINode* CHTMLPage::x_CreateTemplate(CNcbiIstream& is, CNcbiOstream* out,
                                       CNCBINode::TMode mode)
{
    string str;
    char   buf[kBufferSize];

    if ( !is.good() ) {
        NCBI_THROW(CHTMLException, eTemplateAccess,
                   "CHTMLPage::CreateTemplate(): failed to open template");
    }

    // Special case: stream large templates on the first pass
    // to reduce latency.
    if (out  &&  !m_UsePopupMenus) {
        auto_ptr<CNCBINode> node(new CNCBINode);

        while (is) {
            is.read(buf, sizeof(buf));
            str.append(buf, is.gcount());
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
                       "CHTMLPage::CreateTemplate(): error reading template");
        }
        
        return node.release();
    }

    if ( m_TemplateSize ) {
        str.reserve(m_TemplateSize);
    }
    while ( is ) {
        is.read(buf, sizeof(buf));
        if (m_TemplateSize == 0  &&  is.gcount() > 0
            &&  str.size() == str.capacity()) {
            // We don't know how big str will need to be, so we grow it
            // exponentially.
            str.reserve(str.size() +
                        max((SIZE_TYPE)is.gcount(), str.size() / 2));
        }
        str.append(buf, is.gcount());
    }

    if ( !is.eof() ) {
        NCBI_THROW(CHTMLException, eTemplateAccess,
                   "CHTMLPage::CreateTemplate(): error reading template");
    }

    // Insert code in end of <HEAD> and <BODY> blocks for support popup menus
    if ( m_UsePopupMenus ) {
        // a "do ... while (false)" lets us avoid a goto
        do {
            // Search </HEAD> tag
            SIZE_TYPE pos = NStr::FindNoCase(str, "/head");
            if ( pos == NPOS) {
                break;
            }
            pos = str.rfind("<", pos);
            if ( pos == NPOS) {
                break;
            }

            // Insert code for load popup menu library
            for (int t = CHTMLPopupMenu::ePMFirst;
                 t <= CHTMLPopupMenu::ePMLast; t++ ) 
            {
                CHTMLPopupMenu::EType type = (CHTMLPopupMenu::EType)t;
                TPopupMenus::const_iterator info = m_PopupMenus.find(type);
                if ( info != m_PopupMenus.end() ) {
                    string script
                        = CHTMLPopupMenu::GetCodeHead(type,info->second.m_Url);
                    str.insert(pos, script);
                    pos += script.length();
                }
            }

            // Search </BODY> tag
            pos = NStr::FindNoCase(str, "/body", 0, NPOS, NStr::eLast);
            if ( pos == NPOS) {
                break;
            }
            pos = str.rfind("<", pos);
            if ( pos == NPOS) {
                break;
            }

            // Insert code for init popup menus
            for (int t = CHTMLPopupMenu::ePMFirst;
                 t <= CHTMLPopupMenu::ePMLast; t++ ) {
                CHTMLPopupMenu::EType type = (CHTMLPopupMenu::EType)t;
                TPopupMenus::const_iterator info = m_PopupMenus.find(type);
                if ( info != m_PopupMenus.end() ) {
                    string script = CHTMLPopupMenu::GetCodeBody(type,
                        info->second.m_UseDynamicMenu);
                    str.insert(pos, script);
                }
            }
        }
        while (false);
    }
    {{
        auto_ptr<CHTMLText> node(new CHTMLText(str));
        if (out) {
            node->Print(*out, mode);
        }
        return node.release();
    }}
}


void CHTMLPage::SetTemplateFile(const string& template_file)
{
    m_TemplateFile   = template_file;
    m_TemplateStream = 0;
    m_TemplateBuffer = 0;
    GeneratePageInternalName(template_file);
    {{
        Int8 size = CFile(template_file).GetLength();
        if (size <= 0) {
            m_TemplateSize = 0;
        } else if (size >= numeric_limits<size_t>::max()) {
            NCBI_THROW(CHTMLException, eTemplateTooBig,
                       "CHTMLPage: input template " + template_file
                       + " too big to handle");
        } else {
            m_TemplateSize = (SIZE_TYPE)size;
        }
    }}
}


void CHTMLPage::LoadTemplateLibFile(const string& template_file)
{
    Int8 size = CFile(template_file).GetLength();

    if (size <= 0) {
        return;
    } else if (size >= numeric_limits<size_t>::max()) {
        NCBI_THROW(CHTMLException, eTemplateTooBig,
                   "CHTMLPage: input template " + template_file
                   + " too big to handle");
    }
    CNcbiIfstream is(template_file.c_str());
    x_LoadTemplateLib(is, (SIZE_TYPE)size);
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


void CHTMLPage::x_LoadTemplateLib(CNcbiIstream& is, SIZE_TYPE size)
{
    string str("\n");
    char   buf[kBufferSize];

    // Load template in memory all-in-all

    if ( size ) {
        str.reserve(size);
    }
    while (is) {
        is.read(buf, sizeof(buf));
        if (size == 0  &&  is.gcount() > 0
            &&  str.size() == str.capacity()) {
            // We don't know how big str will need to be, so we grow it
            // exponentially.
            str.reserve(str.size() + max((SIZE_TYPE)is.gcount(),
                        str.size() / 2));
        }
        str.append(buf, is.gcount());
    }
    if ( !is.eof() ) {
        NCBI_THROW(CHTMLException, eTemplateAccess,
                   "CHTMLPage::x_LoadTemplate(): error reading template");
    }

    // Parse template

    const string kTagStartBOL(string("\n") + kTagStart); 
    SIZE_TYPE ts_size  = kTagStartBOL.length();
    SIZE_TYPE te_size  = strlen(kTagEnd);
    SIZE_TYPE tse_size = strlen(kTagStartEnd);

    SIZE_TYPE tag_start = s_Find(str, kTagStartBOL.c_str());

    while ( tag_start != NPOS ) {

        // Get name
        string name;
        SIZE_TYPE name_start = tag_start + ts_size;
        SIZE_TYPE name_end   = s_Find(str, kTagEnd, name_start);
        if ( name_end == NPOS ) {
            // Tag not closed
            NCBI_THROW(CHTMLException, eTextUnclosedTag,
                "opening tag \"" + name + "\" not closed, " \
                "stream pos = " + NStr::IntToString(tag_start));
        }
        if (name_end != name_start) {
            // Tag found
            name = str.substr(name_start, name_end - name_start);
        }
        SIZE_TYPE tag_end = name_end + te_size;

        // Find close tags for "name"
        string close_str = kTagStartEnd;
        if ( !name.empty() ) {
            close_str += name + kTagEnd;
        }
        SIZE_TYPE last = s_Find(str, close_str.c_str(), tag_end);
        if ( last == NPOS ) {
            // Tag not closed
            NCBI_THROW(CHTMLException, eTextUnclosedTag,
                "closing tag \"" + name + "\" not closed, " \
                "stream pos = " + NStr::IntToString(tag_end));
        }
        if ( name.empty() ) {
            tag_start = s_Find(str, kTagStartBOL.c_str(), last + tse_size);
            continue;
        }

        // Is it a multi-line template? Remove redundand line breaks.
        SIZE_TYPE pos = str.find_first_not_of(" ", tag_end);
        if (pos != NPOS  &&  str[pos] == '\n') {
            tag_end = pos + 1;
        }
        pos = str.find_first_not_of(" ", last - 1);
        if (pos != NPOS  &&  str[pos] == '\n') {
            last = pos;
        }

        // Get sub-template
        string subtemplate = str.substr(tag_end, last - tag_end);


        // Add sub-template resolver
        AddTagMap(name, CreateTagMapper(new CHTMLText(subtemplate)));

        // Find next
        tag_start = s_Find(str, kTagStartBOL.c_str(),
                           last + te_size + name_end - name_start + tse_size);

    }
}

    
END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.44  2004/05/17 20:59:50  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.43  2004/02/04 17:15:22  ivanov
 * Added debug function GeneratePageInternalName()
 *
 * Revision 1.42  2004/02/03 19:45:14  ivanov
 * Binded dummy names for the unnamed nodes
 *
 * Revision 1.41  2004/02/02 14:27:49  ivanov
 * Added HTML template support
 *
 * Revision 1.40  2003/12/02 14:26:35  ivanov
 * Removed obsolete functions GetCodeBodyTag[Handler|Action]().
 *
 * Revision 1.39  2003/11/03 17:03:08  ivanov
 * Some formal code rearrangement. Move log to end.
 *
 * Revision 1.38  2003/07/08 17:13:53  gouriano
 * changed thrown exceptions to CException-derived ones
 *
 * Revision 1.37  2003/05/15 13:07:32  ucko
 * Fix a more serious (and correctly diagnosed ;-)) logic error: include
 * the last line of streamed templates that don't end in newlines.
 *
 * Revision 1.36  2003/05/15 13:00:24  ucko
 * When breaking large templates into chunks, be sure to include the
 * relevant newline in each chunk to avoid accidentally repeating it if a
 * really long line follows.
 *
 * Revision 1.35  2003/05/15 00:07:05  ucko
 * x_CreateTemplate: don't assume out implies !m_UsePopupMenus
 *
 * Revision 1.34  2003/05/14 21:54:27  ucko
 * Adjust interface to allow automatic streaming of large templates when
 * not using JavaScript menus.
 * Other performance improvements -- in particular, use NStr::FindNoCase
 * instead of making a lowercase copy of the template.
 *
 * Revision 1.33  2003/05/13 15:44:41  ucko
 * Make reading large templates more efficient.
 *
 * Revision 1.32  2003/03/11 15:28:57  kuznets
 * iterate -> ITERATE
 *
 * Revision 1.31  2002/12/09 22:11:59  ivanov
 * Added support for Sergey Kurdin's popup menu
 *
 * Revision 1.30  2002/09/16 22:24:52  vakatov
 * Formal fix to get rid of an "unused func arg" warning
 *
 * Revision 1.29  2002/09/11 16:09:27  dicuccio
 * fixed memory leak in CreateTemplate(): added x_CreateTemplate() to get
 * around heap allocation of stream.
 * moved cvs log to the bottom of the page.
 *
 * Revision 1.28  2002/08/09 21:12:02  ivanov
 * Added stuff to read template from a stream and string
 *
 * Revision 1.27  2002/02/23 04:08:25  vakatov
 * Commented out "// template struct TagMapper<CHTMLPage>;" to see if it's
 * still needed for any compiler
 *
 * Revision 1.26  2002/02/13 20:16:45  ivanov
 * Added support of dynamic popup menus. Changed EnablePopupMenu().
 *
 * Revision 1.25  2001/08/14 16:56:42  ivanov
 * Added support for work HTML templates with JavaScript popup menu.
 * Renamed type Flags -> ETypes. Moved all code from "page.inl" to header file.
 *
 * Revision 1.24  2000/03/31 17:08:43  kans
 * cast ifstr.rdstate() to int
 *
 * Revision 1.23  1999/10/28 13:40:36  vasilche
 * Added reference counters to CNCBINode.
 *
 * Revision 1.22  1999/09/27 16:17:18  vasilche
 * Fixed several incompatibilities with Windows
 *
 * Revision 1.21  1999/09/23 15:51:42  vakatov
 * Added <unistd.h> for the getcwd() proto
 *
 * Revision 1.20  1999/09/17 14:16:09  sandomir
 * tmp diagnostics to find error
 *
 * Revision 1.19  1999/09/15 15:04:47  sandomir
 * minor memory leak in tag mapping
 *
 * Revision 1.18  1999/07/19 21:05:02  pubmed
 * minor change in CHTMLPage::CreateTemplate() - show file name
 *
 * Revision 1.17  1999/05/28 20:43:10  vakatov
 * ::~CHTMLBasicPage(): MSVC++ 6.0 SP3 cant compile:  DeleteElements(m_TagMap);
 *
 * Revision 1.16  1999/05/28 16:32:16  vasilche
 * Fixed memory leak in page tag mappers.
 *
 * Revision 1.15  1999/05/27 21:46:25  vakatov
 * CHTMLPage::CreateTemplate():  throw exception if cannot open or read
 * the page template file specified by user
 *
 * Revision 1.14  1999/04/28 16:52:45  vasilche
 * Restored optimized code for reading from file.
 *
 * Revision 1.13  1999/04/27 16:48:44  vakatov
 * Rollback of the buggy "optimization" in CHTMLPage::CreateTemplate()
 *
 * Revision 1.12  1999/04/26 21:59:31  vakatov
 * Cleaned and ported to build with MSVC++ 6.0 compiler
 *
 * Revision 1.11  1999/04/19 16:51:36  vasilche
 * Fixed error with member pointers detected by GCC.
 *
 * Revision 1.10  1999/04/15 22:10:43  vakatov
 * Fixed "class TagMapper<>" to "struct ..."
 *
 * Revision 1.9  1998/12/28 23:29:10  vakatov
 * New CVS and development tree structure for the NCBI C++ projects
 *
 * Revision 1.8  1998/12/28 21:48:17  vasilche
 * Made Lewis's 'tool' compilable
 *
 * Revision 1.7  1998/12/28 16:48:09  vasilche
 * Removed creation of QueryBox in CHTMLPage::CreateView()
 * CQueryBox extends from CHTML_form
 * CButtonList, CPageList, CPagerBox, CSmallPagerBox extend from CNCBINode.
 *
 * Revision 1.6  1998/12/22 16:39:15  vasilche
 * Added ReadyTagMapper to map tags to precreated nodes.
 *
 * Revision 1.3  1998/12/01 19:10:39  lewisg
 * uses CCgiApplication and new page factory
 *
 * ===========================================================================
 */
