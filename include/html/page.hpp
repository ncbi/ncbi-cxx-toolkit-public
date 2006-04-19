#ifndef HTML___PAGE__HPP
#define HTML___PAGE__HPP

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

/// @file page.hpp 
/// The HTML page.
///
/// Defines class to generate HTML code from template file.


#include <corelib/ncbistd.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_limits.hpp>
#include <html/html_exception.hpp>
#include <html/html.hpp>
#include <html/nodemap.hpp>
#include <html/jsmenu.hpp>


/** @addtogroup HTMLcomp
 *
 * @{
 */


BEGIN_NCBI_SCOPE


// Forward declarations.
class CCgiApplication;



/////////////////////////////////////////////////////////////////////////////
///
/// CHTMLBasicPage --
///
/// The virtual base class.
///
/// The main functionality is the turning on and off of sub HTML components
/// via style bits and a creation function that orders sub components on
/// the page. The ability to hold children and print HTML is inherited from
/// CHTMLNode.

class NCBI_XHTML_EXPORT CHTMLBasicPage: public CNCBINode
{
    /// Parent class.
    typedef CNCBINode CParent;
    typedef map<string, BaseTagMapper*> TTagMap;

public: 
    /// Default constructor.
    CHTMLBasicPage(void);

    /// Constructor.
    CHTMLBasicPage(CCgiApplication* app, int style = 0);

    /// Dectructor.
    virtual ~CHTMLBasicPage(void);

    virtual CCgiApplication* GetApplication(void) const;
    virtual void SetApplication(CCgiApplication* App);

    int  GetStyle(void) const;
    void SetStyle(int style);

    /// Resolve <@XXX@> tag.
    virtual CNCBINode* MapTag(const string& name);

    /// Add tag resolver.
    virtual void AddTagMap(const string& name, BaseTagMapper* mapper);
    virtual void AddTagMap(const string& name, CNCBINode*     node);

protected:
    CCgiApplication* m_CgiApplication;  ///< Pointer to runtime information
    int              m_Style;
    TMode            m_PrintMode;       ///< Current print mode
                                        ///< (used by RepeatHook).

    /// Tag resolvers (as registered by AddTagMap).
    TTagMap m_TagMap;
};


/////////////////////////////////////////////////////////////////////////////
///
/// CHTMLPage --
///
/// This is the basic 3 section NCBI page.

class NCBI_XHTML_EXPORT CHTMLPage : public CHTMLBasicPage
{
    /// Parent class.
    typedef CHTMLBasicPage CParent;

public:
    /// Style flags.
    enum EFlags {
        fNoTITLE      = 0x1,
        fNoVIEW       = 0x2,
        fNoTEMPLATE   = 0x4
    };
    /// Binary AND of "EFlags".
    typedef int TFlags;  

    /// Constructors.
    CHTMLPage(const string& title = kEmptyStr);
    CHTMLPage(const string& title, const string&  template_file);
    CHTMLPage(const string& title,
              const void* template_buffer, size_t size);
    CHTMLPage(const string& title, istream& template_stream);
    // HINT: use SetTemplateString to read the page from '\0'-terminated string

    CHTMLPage(CCgiApplication* app,
              TFlags           style         = 0,
              const string&    title         = kEmptyStr,
              const string&    template_file = kEmptyStr);

    static CHTMLBasicPage* New(void);

    /// Create the individual sub pages.
    virtual void CreateSubNodes(void);

    /// Create the static part of the page
    /// (here - read it from <m_TemplateFile>).
    virtual CNCBINode* CreateTemplate(CNcbiOstream* out = 0,
                                      TMode mode = eHTML);

    /// Tag substitution callbacks.
    virtual CNCBINode* CreateTitle(void);  // def for tag "@TITLE@" - <m_Title>
    virtual CNCBINode* CreateView(void);   // def for tag "@VIEW@"  - none

    /// To set title or template outside(after) the constructor.
    void SetTitle(const string&  title);

    /// Set source which contains the template.
    ///
    /// Each function assign new template source and annihilate any other.
    /// installed before.
    void SetTemplateFile  (const string&  template_file);
    void SetTemplateString(const char*    template_string);
    void SetTemplateBuffer(const void*    template_buffer, size_t size);
    void SetTemplateStream(istream& template_stream);

    /// Load template library.
    ///
    /// Automaticaly map all sub-templates from loaded library.
    void LoadTemplateLibFile  (const string&  template_file);
    void LoadTemplateLibString(const char*    template_string);
    void LoadTemplateLibBuffer(const void*    template_buffer, size_t size);
    void LoadTemplateLibStream(istream& template_stream);

    /// Template file caching state.
    enum ECacheTemplateFiles {
        eCTF_Enable,       ///< Enable caching
        eCTF_Disable,      ///< Disable caching
        eCTF_Default = eCTF_Disable
    };

    /// Enable/disable template caching.
    ///
    /// If caching enabled that all template and template libraries 
    /// files, loaded by any object of CHTMLPage will be read from disk
    /// only once.
    static void CacheTemplateFiles(ECacheTemplateFiles caching);

    /// Enable using popup menus. Set URL for popup menu library.
    ///
    /// @param type
    ///   Menu type to enable
    /// @param menu_script_url
    ///   An URL for popup menu library.
    ///   If "menu_lib_url" is not defined, then using default URL.
    /// @param use_dynamic_menu
    ///   Enable/disable using dynamic popup menus (eSmith menu only)
    ///   (default it is disabled).
    /// Note:
    ///   - If we not change value "menu_script_url", namely use default
    ///     value for it, then we can skip call this function.
    ///   - Dynamic menues work only in new browsers. They use one container
    ///     for all menus instead of separately container for each menu in 
    ///     nondynamic mode. This parameter have effect only with eSmith
    ///     menu type.
    void EnablePopupMenu(CHTMLPopupMenu::EType type = CHTMLPopupMenu::eSmith,
                         const string& menu_script_url= kEmptyStr,
                         bool use_dynamic_menu = false);

    /// Tag mappers. 
    virtual void AddTagMap(const string& name, BaseTagMapper* mapper);
    virtual void AddTagMap(const string& name, CNCBINode*     node);

    // Overridden to reduce latency
    CNcbiOstream& PrintChildren(CNcbiOstream& out, TMode mode);

private:
    void Init(void);

    /// Read template into string.
    ///
    /// Used by CreateTemplate() to cache templates and add
    /// on the fly modifications into it, like adding JS code for
    /// used popup menus.
    void x_LoadTemplate(CNcbiIstream& is, string& str);

    /// Create and print template.
    ///
    /// Calls by CreateTemplate() only when it can create and print
    /// template at one time, to avoid latency on large templates. 
    /// Otherwise x_ReadTemplate() will be used.
    CNCBINode* x_PrintTemplate(CNcbiIstream& is, CNcbiOstream* out,
                               CNCBINode::TMode mode);

    // Allow/disable processing of #include directives for template libraries.
    // eAllowIncludes used by default for LoadTemplateLibFile().
    enum ETemplateIncludes{
        eAllowIncludes,  // process #include's
        eSkipIncludes    // do not process #include's
    };

    /// Load template library.
    ///
    /// This is an internal version that works only with streams.
    /// @param is
    ///   Input stream to read template from
    /// @param size
    ///   Size of input, if known (0 otherwise).
    /// @param includes
    ///   Defines to process or not #include directives.
    ///   Used only for loading template libraries from files
    /// @param file_name
    ///   Name of the template library file.
    ///   Used only by LoadTemplateLibFile().
    /// @sa
    ///   LoadTemplateLibFile(), LoadTemplateLibString(),
    ///   LoadTemplateLibBuffer(), LoadTemplateLibStream()
    void x_LoadTemplateLib(CNcbiIstream& is, size_t size = 0,
                           ETemplateIncludes includes  = eSkipIncludes,
                           const string&     file_name = kEmptyStr);

private:
    /// Generate page internal name on the base of template source.
    /// Debug function used at output tag trace on exception.
    void GeneratePageInternalName(const string& template_src);

private:
    string      m_Title;          ///< Page title

    /// Template sources.
    string      m_TemplateFile;   ///< File name
    istream*    m_TemplateStream; ///< Stream
    const void* m_TemplateBuffer; ///< Some buffer
    size_t      m_TemplateSize;   ///< Size of input, if known (0 otherwise)

    static ECacheTemplateFiles sm_CacheTemplateFiles;

    /// Popup menu info structure.
    struct SPopupMenuInfo {
        SPopupMenuInfo() {
            m_UseDynamicMenu = false;
        };
        SPopupMenuInfo(const string& url, bool use_dynamic_menu) {
            m_Url = url;
            m_UseDynamicMenu = use_dynamic_menu;
        }
        string m_Url;             ///< Menu library URL 
        bool   m_UseDynamicMenu;  ///< Dynamic/static. Only for eSmith type.
    };

    /// Popup menus usage info.
    typedef map<CHTMLPopupMenu::EType, SPopupMenuInfo> TPopupMenus;
    TPopupMenus m_PopupMenus;
    bool        m_UsePopupMenus;
};


/* @} */


/////////////////////////////////////////////////////////////////////////////
//
//  IMPLEMENTATION of INLINE functions
//
/////////////////////////////////////////////////////////////////////////////


//
//  CHTMLBasicPage::
//

inline CCgiApplication* CHTMLBasicPage::GetApplication(void) const
{
    return m_CgiApplication;
}


inline int CHTMLBasicPage::GetStyle(void) const
{
    return m_Style;
}



//
//  CHTMLPage::
//

inline CHTMLBasicPage* CHTMLPage::New(void)
{
    return new CHTMLPage;
}


inline void CHTMLPage::SetTitle(const string& title)
{
    m_Title = title;
}


inline void CHTMLPage::SetTemplateString(const char* template_string)
{
    m_TemplateFile   = kEmptyStr;
    m_TemplateStream = 0;
    m_TemplateBuffer = template_string;
    m_TemplateSize   = strlen(template_string);
    GeneratePageInternalName("str");
}


inline void CHTMLPage::SetTemplateBuffer(const void* template_buffer,
                                         size_t size)
{
    m_TemplateFile   = kEmptyStr;
    m_TemplateStream = 0;
    m_TemplateBuffer = template_buffer;
    m_TemplateSize   = size;
    GeneratePageInternalName("buf");
}


inline void CHTMLPage::SetTemplateStream(istream& template_stream)
{
    m_TemplateFile   = kEmptyStr;
    m_TemplateStream = &template_stream;
    m_TemplateBuffer = 0;
    m_TemplateSize   = 0;
    GeneratePageInternalName("stream");
}


inline void CHTMLPage::LoadTemplateLibString(const char* template_string)
{
    size_t size = strlen(template_string);
    CNcbiIstrstream is(template_string, size);
    x_LoadTemplateLib(is, size);
}


inline void CHTMLPage::LoadTemplateLibBuffer(const void* template_buffer,
                                             size_t size)
{
    CNcbiIstrstream is((char*)template_buffer, size);
    x_LoadTemplateLib(is, size);
}


inline void CHTMLPage::LoadTemplateLibStream(istream& template_stream)
{
    x_LoadTemplateLib(template_stream);
}


inline void CHTMLPage::GeneratePageInternalName(const string& template_src = kEmptyStr)
{
    m_Name = "htmlpage";
    if ( !template_src.empty() ) {
        m_Name += "(" + template_src + ")";
    }
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.39  2006/04/19 15:21:23  ivanov
 * Minor comment changes
 *
 * Revision 1.38  2006/01/25 17:55:14  ivanov
 * Split up x_CreateTemplate to x_LoadTemplate and x_PrintTemplate.
 * Restored feature of creating templates while printing to avoid latency
 * on large templates.
 *
 * Revision 1.37  2006/01/24 18:04:23  ivanov
 * CHTMLPage:: added possibility to cache template files and template libs
 *
 * Revision 1.36  2005/08/01 16:00:42  ivanov
 * Added support #include command for template libraries
 *
 * Revision 1.35  2004/02/04 17:15:10  ivanov
 * Added debug function GeneratePageInternalName()
 *
 * Revision 1.34  2004/02/02 14:27:05  ivanov
 * Added HTML template support
 *
 * Revision 1.33  2003/11/05 18:41:06  dicuccio
 * Added export specifiers
 *
 * Revision 1.32  2003/10/02 18:16:46  ivanov
 * Get rid of compilation warnings; some formal code rearrangement
 *
 * Revision 1.31  2003/10/01 15:53:11  ivanov
 * Formal code rearrangement
 *
 * Revision 1.30  2003/07/08 17:12:40  gouriano
 * changed thrown exceptions to CException-derived ones
 *
 * Revision 1.29  2003/05/23 17:34:10  ucko
 * CHTMLPage::SetTemplateFile: fix logic for setting m_TemplateSize.
 *
 * Revision 1.28  2003/05/14 21:53:02  ucko
 * Adjust interface to allow automatic streaming of large templates when
 * not using JavaScript menus.
 *
 * Revision 1.27  2003/05/13 15:44:19  ucko
 * Make reading large templates more efficient.
 *
 * Revision 1.26  2003/04/25 13:45:37  siyan
 * Added doxygen groupings
 *
 * Revision 1.25  2002/12/09 22:12:45  ivanov
 * Added support for Sergey Kurdin's popup menu.
 *
 * Revision 1.24  2002/09/11 16:07:32  dicuccio
 * added x_CreateTemplate()
 * moved cvs log to the bottom of the page
 *
 * Revision 1.23  2002/08/09 21:12:15  ivanov
 * Added stuff to read template from a stream and string
 *
 * Revision 1.22  2002/02/13 20:17:06  ivanov
 * Added support of dynamic popup menus. Changed EnablePopupMenu().
 *
 * Revision 1.21  2001/08/14 16:57:14  ivanov
 * Added support for work HTML templates with JavaScript popup menu.
 * Renamed type Flags -> ETypes. Moved all code from "page.inl" to header file.
 *
 * Revision 1.20  1999/10/28 13:40:31  vasilche
 * Added reference counters to CNCBINode.
 *
 * Revision 1.19  1999/05/28 16:32:11  vasilche
 * Fixed memory leak in page tag mappers.
 *
 * Revision 1.18  1999/04/27 14:49:59  vasilche
 * Added FastCGI interface.
 * CNcbiContext renamed to CCgiContext.
 *
 * Revision 1.17  1999/04/26 21:59:28  vakatov
 * Cleaned and ported to build with MSVC++ 6.0 compiler
 *
 * Revision 1.16  1999/04/20 13:51:59  vasilche
 * Removed unused parameter name to avoid warning.
 *
 * Revision 1.15  1999/04/19 20:11:47  vakatov
 * CreateTagMapper() template definitions moved from "page.inl" to
 * "page.hpp" because MSVC++ gets confused(cannot understand what
 * is declaration and what is definition).
 *
 * Revision 1.14  1999/04/15 22:06:46  vakatov
 * CQueryBox:: use "enum { kNo..., };" rather than "static const int kNo...;"
 * Fixed "class BaseTagMapper" to "struct ..."
 *
 * Revision 1.13  1998/12/28 23:29:03  vakatov
 * New CVS and development tree structure for the NCBI C++ projects
 *
 * Revision 1.12  1998/12/28 21:48:13  vasilche
 * Made Lewis's 'tool' compilable
 *
 * Revision 1.11  1998/12/24 16:15:38  vasilche
 * Added CHTMLComment class.
 * Added TagMappers from static functions.
 *
 * Revision 1.10  1998/12/23 21:21:00  vasilche
 * Added more HTML tags (almost all).
 * Importent ones: all lists (OL, UL, DIR, MENU), fonts (FONT, BASEFONT).
 *
 * Revision 1.9  1998/12/22 16:39:12  vasilche
 * Added ReadyTagMapper to map tags to precreated nodes.
 *
 * Revision 1.8  1998/12/21 22:24:59  vasilche
 * A lot of cleaning.
 *
 * Revision 1.7  1998/12/11 22:53:41  lewisg
 * added docsum page
 *
 * Revision 1.6  1998/12/11 18:13:51  lewisg
 * frontpage added
 *
 * Revision 1.5  1998/12/09 23:02:56  lewisg
 * update to new cgiapp class
 *
 * Revision 1.4  1998/12/09 17:27:44  sandomir
 * tool should be changed to work with the new CCgiApplication
 *
 * Revision 1.2  1998/12/01 19:09:06  lewisg
 * uses CCgiApplication and new page factory
 *
 * ===========================================================================
 */

#endif  /* HTML___PAGE__HPP */
