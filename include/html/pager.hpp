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
* Revision 1.7  1999/09/15 20:33:33  golikov
* GetPagerView defaults added
*
* Revision 1.6  1999/09/13 15:37:16  golikov
* Image sizes in page numbers added
*
* Revision 1.5  1999/06/25 20:02:58  golikov
* "Show:" trasfered to pager
*
* Revision 1.4  1999/04/14 17:28:54  vasilche
* Added parsing of CGI parameters from IMAGE input tag like "cmd.x=1&cmd.y=2"
* As a result special parameter is added with empty name: "=cmd"
*
* Revision 1.3  1999/04/06 19:33:46  vasilche
* Added defalut page size.
*
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
    CPager(CCgiRequest& request, int pageBlockSize = 10, int defaultPageSize = 10);

    static bool IsPagerCommand(const CCgiRequest& request);

    bool PageChanged(void) const;

    int GetItemCount(void) const;
    void SetItemCount(int count);

    pair<int, int> GetRange(void) const;

    CNCBINode* GetPagerView(const string& imgDir,
                            const int imgX = 0, const int imgY = 0) const;

    CNCBINode* GetPageInfo(void) const;
    CNCBINode* GetItemInfo(void) const;

    virtual void CreateSubNodes(void);
    //    virtual CNcbiOstream& PrintEnd(CNcbiOstream& out);

    // methods to retrieve page size and current page
    static int GetPageSize(CCgiRequest& request, int defaultPageSize = 10);
    static int GetDisplayPage(CCgiRequest& request);

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

#include <html/pager.inl>

END_NCBI_SCOPE

#endif  /* PAGER__HPP */
