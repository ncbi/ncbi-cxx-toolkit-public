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
* Author: Eugene Vasilchenko
*
* File Description:
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  1999/01/21 21:12:56  vasilche
* Added/used descriptions for HTML submit/select/text.
* Fixed some bugs in paging.
*
* Revision 1.1  1999/01/19 21:17:38  vasilche
* Added CPager class
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
    CPager(CCgiRequest& request, int pageBlockSize = 10);

    static bool IsPagerCommand(const CCgiRequest& request);

    bool PageChanged(void) const;

    int GetItemCount(void) const;
    void SetItemCount(int count);

    pair<int, int> GetRange(void) const;

    CNCBINode* GetPagerView(void) const;
    CNCBINode* GetPageInfo(void) const;
    CNCBINode* GetItemInfo(void) const;

    virtual void CreateSubNodes(void);
    //    virtual CNcbiOstream& PrintEnd(CNcbiOstream& out);

    // methods to retrieve page size and current page
    static int GetPageSize(CCgiRequest& request);
    static int GetDisplayPage(CCgiRequest& request);

    // name of hidden value holding page size
    static const string KParam_PageSize;
    // name of hidden value holding current page number
    static const string KParam_DisplayPage;
    // name of image button for previous block of pages
    static const string KParam_PreviousPages;
    static const string KParam_PreviousPagesX;
    // name of image button for next block of pages
    static const string KParam_NextPages;
    static const string KParam_NextPagesX;
    // beginning of names of image buttons for specific page
    static const string KParam_Page;
    // ".x" suffix for image buttons
    static const string KParam_x;

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

    friend class CPagerView;
};

class CPagerView : public CHTML_table
{
public:
    CPagerView(const CPager& pager);

    virtual void CreateSubNodes(void);

    // source of images
    string m_ImagesDir;

    // default source of images
    static string sm_DefaultImagesDir;
    
private:
    const CPager& m_Pager;

    void AddInactiveImageString(CNCBINode* node, int number,
                     const string& imageStart, const string& imageEnd);
    void AddImageString(CNCBINode* node, int number,
                     const string& imageStart, const string& imageEnd);
};

#include <html/pager.inl>

END_NCBI_SCOPE

#endif  /* PAGER__HPP */
