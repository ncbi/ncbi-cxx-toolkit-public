#ifndef LINKBAR__HPP
#define LINKBAR__HPP

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
*   Image links bar
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  1999/02/03 15:03:55  vasilche
* Added check for wrong link names.
*
* Revision 1.2  1999/02/02 17:57:47  vasilche
* Added CHTML_table::Row(int row).
* Linkbar now have equal image spacing.
*
* Revision 1.1  1999/01/29 20:48:01  vasilche
* Added class CLinkBar.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <html/html.hpp>
#include <list>
#include <map>

BEGIN_NCBI_SCOPE

class CLinkDefinition
{
public:
    CLinkDefinition(const string& name,
                    const string& linkImage = NcbiEmptyString,
                    const string& noLinkImage = NcbiEmptyString);
    CLinkDefinition(const string& name,
                    int width, int height,
                    const string& linkImage = NcbiEmptyString,
                    const string& noLinkImage = NcbiEmptyString);

    string m_LinkName;
    string m_LinkImage;
    string m_NoLinkImage;
    int m_Width;
    int m_Height;
};

typedef list<CLinkDefinition> TLinkBarTemplate;

class CLinkBar : public CHTML_table
{
public:

    CLinkBar(const TLinkBarTemplate* templ);
    ~CLinkBar(void);

    //    const TLinkBarTemplate* GetTemplate(void) const;

    void AddLink(const string& name, const string& linkUrl);

    //    string GetLink(const string& name) const;

    virtual void CreateSubNodes(void);

    CHTMLNode* CreateLink(const CLinkDefinition& def);

private:
    const TLinkBarTemplate* m_Template;

    map<string, string> m_Links;
};

#include <html/linkbar.inl>

END_NCBI_SCOPE

#endif  /* LINKBAR__HPP */
