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


/** @addtogroup HTMLcomp
 *
 * @{
 */


BEGIN_NCBI_SCOPE


// Forward declarations.
class CCgiApplication;


/////////////////////////////////////////////////////////////////////////////
///
/// CPageStat --
///
/// Page information used for tracking and logging: hit ID, design ID etc.
/// Contains pairs of name+value strings.
/// Elements of page stat are included in the page as meta tags in place
/// of <@NCBI_PAGE_STAT@> tag.

class NCBI_XHTML_EXPORT CPageStat
{
public:
    CPageStat(void) {}
    ~CPageStat(void) {}

    /// Get value by name. Return empty string if the name is unknown.
    const string& GetValue(const string& name) const;
    /// Set new value for the name. If the value is empty, delete the
    /// element completely.
    void SetValue(const string& name, const string& value);
    /// Remove all entries
    void Clear(void) { m_Data.clear(); }

    typedef map<string, string> TData;

    /// Return the whole internal string map (read-only).
    const TData& GetData(void) const { return m_Data; }

private:
    // Prohibit copying
    CPageStat(const CPageStat&);
    CPageStat& operator=(const CPageStat&);

    TData m_Data;
};


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

    /// Get CPageStat used to create meta-tags (design ID, hit ID etc.)
    const CPageStat& GetPageStat(void) const { return m_PageStat; }
    /// Get editable CPageStat object
    CPageStat& SetPageStat(void) { return m_PageStat; }

protected:
    CCgiApplication* m_CgiApplication;  ///< Pointer to runtime information
    int              m_Style;
    TMode            m_PrintMode;       ///< Current print mode
                                        ///< (used by RepeatHook).

    /// Tag resolvers (as registered by AddTagMap).
    TTagMap m_TagMap;
    CPageStat m_PageStat;
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

    /// Interface for a filter, which must be passed to one the
    /// LoadTemplateLib methods to select relevant parts of the loaded
    /// template library.
    ///
    /// The TestAttribute() method is called for each attribute test
    /// defined in the template library.
    typedef class CTemplateLibFilter
    {
    public:
        /// This method is called by LoadTemplateLib methods to check
        /// whether a template within library should be loaded.
        /// If the method returns true, the template is loaded,
        /// otherwise it's skipped.
        virtual bool TestAttribute(
            const string& attr_name,
            const string& test_pattern) = 0;

        virtual ~CTemplateLibFilter() {}
    } TTemplateLibFilter;

    /// Load template library.
    ///
    /// Automatically map all sub-templates from the loaded library.
    void LoadTemplateLibFile  (const string&  template_file,
        TTemplateLibFilter* filter = NULL);
    void LoadTemplateLibString(const char*    template_string,
        TTemplateLibFilter* filter = NULL);
    void LoadTemplateLibBuffer(const void*    template_buffer, size_t size,
        TTemplateLibFilter* filter = NULL);
    void LoadTemplateLibStream(istream& template_stream,
        TTemplateLibFilter* filter = NULL);

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
    /// on the fly modifications into it.
    void x_LoadTemplate(CNcbiIstream& is, string& str);

    /// Create and print template.
    ///
    /// Calls by CreateTemplate() only when it can create and print
    /// template at one time, to avoid latency on large templates. 
    /// Otherwise x_ReadTemplate() will be used.
    CNCBINode* x_PrintTemplate(CNcbiIstream& is, CNcbiOstream* out,
                               CNCBINode::TMode mode);

    bool x_ApplyFilters(TTemplateLibFilter* filter, const char* buffer);

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
    void x_LoadTemplateLib(CNcbiIstream& is, size_t size /*= 0*/,
                           ETemplateIncludes includes    /*= eSkipIncludes*/,
                           const string&     file_name   /*= kEmptyStr*/,
                           TTemplateLibFilter* filter);

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


inline void CHTMLPage::LoadTemplateLibString(const char* template_string,
                                             TTemplateLibFilter* filter)
{
    size_t size = strlen(template_string);
    CNcbiIstrstream is(template_string, size);
    x_LoadTemplateLib(is, size, eSkipIncludes, kEmptyStr, filter);
}


inline void CHTMLPage::LoadTemplateLibBuffer(const void* template_buffer,
                                             size_t size,
                                             TTemplateLibFilter* filter)
{
    CNcbiIstrstream is((char*)template_buffer, (int) size);
    x_LoadTemplateLib(is, size, eSkipIncludes, kEmptyStr, filter);
}


inline void CHTMLPage::LoadTemplateLibStream(istream& template_stream,
                                             TTemplateLibFilter* filter)
{
    x_LoadTemplateLib(template_stream, 0, eSkipIncludes, kEmptyStr, filter);
}


inline void CHTMLPage::GeneratePageInternalName(const string& template_src = kEmptyStr)
{
    m_Name = "htmlpage";
    if ( !template_src.empty() ) {
        m_Name += "(" + template_src + ")";
    }
}


END_NCBI_SCOPE

#endif  /* HTML___PAGE__HPP */
