#ifndef PAGER__HPP
#define PAGER__HPP

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
* Author: Eugene Vasilchenko, Anton Golikov
*
* File Description: Common pager with 2 views
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <html/html.hpp>

BEGIN_NCBI_SCOPE

class CCgiRequest;

// makes a set of pagination links

class CPager : public CNCBINode
{
    // parent class
    typedef CHTML_table CParent;

public:

    enum EPagerView {
	eImage,
	eButtons
    };

    CPager(const CCgiRequest& request,
           int pageBlockSize = 10,
           int defaultPageSize = 10,
           EPagerView view = eImage);

    static bool IsPagerCommand(const CCgiRequest& request);

    int CPager::GetItemCount(void) const
    {
        return m_ItemCount;
    }

    bool CPager::PageChanged(void) const
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
    //    virtual CNcbiOstream& PrintEnd(CNcbiOstream& out);

    // methods to retrieve page size and current page
    static int GetPageSize(const CCgiRequest& request, int defaultPageSize = 10);
    // gets page number previously displayed
    static int GetDisplayedPage(const CCgiRequest& request);
    // get current page number
    int GetDisplayPage(void)
        { return m_DisplayPage; }
    int GetDisplayPage(void) const
        { return m_DisplayPage; }

    // name of hidden value holding selected page size
    static const string KParam_PageSize;
    // name of hidden value holding shown page size
    static const string KParam_ShownPageSize;
    // name of hidden value holding current page number
    static const string KParam_DisplayPage;
    // name of image button for previous block of pages
    static const string KParam_PreviousPages;
    // name of image button for next block of pages
    static const string KParam_NextPages;
    // beginning of names of image buttons for specific page
    static const string KParam_Page;
    // page number inputed by user in text field (for 2nd view)
    static const string KParam_InputPage;

private:
    // pager parameters
    int m_PageSize;
    int m_PageBlockSize;
    int m_PageBlockStart;

    // current output page
    int m_DisplayPage;

    // total amount of items
    int m_ItemCount;

    // true if some of page buttons was pressed
    bool m_PageChanged;

    // view selector
    EPagerView m_view;

    friend class CPagerView;
    friend class CPagerViewButtons;
};

class CPagerView : public CHTML_table
{
public:

    CPagerView(const CPager& pager, const string& imgDir,
               const int imgX, const int imgY);

    virtual void CreateSubNodes(void);

private:

    // source of images and it's sizes
    string m_ImagesDir;
    int m_ImgSizeX;
    int m_ImgSizeY;

    const CPager& m_Pager;

    void AddInactiveImageString(CNCBINode* node, int number,
                     const string& imageStart, const string& imageEnd);
    void AddImageString(CNCBINode* node, int number,
                     const string& imageStart, const string& imageEnd);
};

/**
 ** 2nd view for CPager with buttons:
   Previous (link) Current (input) Page (button) Next (link)
*/

class CPagerViewButtons : public CHTML_table
{
public:

    CPagerViewButtons(const CPager& pager, const string& js_suffix);

    virtual void CreateSubNodes(void);

private:

    const CPager& m_Pager;
    string m_jssuffix;
};

END_NCBI_SCOPE

#endif  /* PAGER__HPP */
