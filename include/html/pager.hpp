#ifndef HTML___PAGER__HPP
#define HTML___PAGER__HPP

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
 * Author:  Eugene Vasilchenko, Anton Golikov
 *
 */

/// @file page.hpp 
/// Common pager with 3 views.


#include <corelib/ncbistd.hpp>
#include <html/html.hpp>


/** @addtogroup PagerClass
 *
 * @{
 */


BEGIN_NCBI_SCOPE


class CCgiRequest;

// Make a set of pagination links
class NCBI_XHTML_EXPORT CPager : public CNCBINode
{
    // Parent class
    typedef CHTML_table CParent;

public:
    enum EPagerView {
        eImage,
	    eButtons,
        eTabs,
        eJavaLess
    };

    CPager(const CCgiRequest& request,
           int                pageBlockSize   = 10,
           int                defaultPageSize = 10,
           EPagerView         view = eImage);

    static bool IsPagerCommand(const CCgiRequest& request);

    int GetItemCount(void) const
    {
        return m_ItemCount;
    }

    bool PageChanged(void) const
    {
        return m_PageChanged;
    }

    void SetItemCount(int count);

    pair<int, int> GetRange(void) const;

    CNCBINode* GetPagerView(const string& imgDir,
                            const int imgX = 0, const int imgY = 0,
                            const string& js_suffix = kEmptyStr) const;

    CNCBINode* GetPageInfo(void) const;
    CNCBINode* GetItemInfo(void) const;

    virtual void CreateSubNodes(void);

    // Retrieve page size and current page
    static int GetPageSize(const CCgiRequest& request, int defaultPageSize = 10);
    // Get page number previously displayed
    static int GetDisplayedPage(const CCgiRequest& request);
    // Get current page number
    int GetDisplayPage(void)
        { return m_DisplayPage; }
    int GetDisplayPage(void) const
        { return m_DisplayPage; }
    // Get total pages in result
    int GetPages(void)
        { return ( (m_ItemCount - 1) / m_PageSize + 1 ); }

    // Name of hidden value holding selected page size
    static const char* KParam_PageSize;
    // Name of hidden value holding shown page size
    static const char* KParam_ShownPageSize;
    // Name of hidden value holding current page number
    static const char* KParam_DisplayPage;
    // Name of image button for previous block of pages
    static const char* KParam_PreviousPages;
    // Name of image button for next block of pages
    static const char* KParam_NextPages;
    // Beginning of names of image buttons for specific page
    static const char* KParam_Page;
    // Page number inputed by user in text field (for 2nd and 3rd view)
    static const char* KParam_InputPage;
    // Name cmd and button for next page
    static const char* KParam_NextPage;
    // Name cmd and button for previous page
    static const char* KParam_PrevPage;
    static const char* KParam_GoToPage;

private:
    // Pager parameters
    int m_PageSize;
    int m_PageBlockSize;
    int m_PageBlockStart;

    // Current output page
    int m_DisplayPage;

    // Total amount of items
    int m_ItemCount;

    // True if some of page buttons was pressed
    bool m_PageChanged;

    // View selector
    EPagerView m_view;

    friend class CPagerView;
    friend class CPagerViewButtons;
    friend class CPagerViewJavaLess;
};


class NCBI_XHTML_EXPORT CPagerView : public CHTML_table
{
public:

    CPagerView(const CPager& pager, const string& imgDir,
               const int imgX, const int imgY);

    virtual void CreateSubNodes(void);

private:
    // Source of images and it's sizes
    string m_ImagesDir;
    int    m_ImgSizeX;
    int    m_ImgSizeY;

    const CPager& m_Pager;

    void AddInactiveImageString(CNCBINode* node, int number,
                     const string& imageStart, const string& imageEnd);
    void AddImageString(CNCBINode* node, int number,
                     const string& imageStart, const string& imageEnd);
};


// Second view for CPager with buttons:
// Previous (link) Current (input) Page (button) Next (link)

class NCBI_XHTML_EXPORT CPagerViewButtons : public CHTML_table
{
public:
    CPagerViewButtons(const CPager& pager, const string& js_suffix);
    virtual void CreateSubNodes(void);

private:
    const CPager& m_Pager;
    string        m_jssuffix;
};

// Third view for CPager with buttons and without Java:
// Prev Page (button) Next Page (button) Current (input) GoTo Page (button)

class NCBI_XHTML_EXPORT CPagerViewJavaLess : public CHTML_table
{
public:
    CPagerViewJavaLess(const CPager& pager, const string& js_suffix);
    virtual void CreateSubNodes(void);

private:
    const CPager& m_Pager;
    string        m_jssuffix;
};


END_NCBI_SCOPE


/* @} */

#endif  /* HTML___PAGER__HPP */
