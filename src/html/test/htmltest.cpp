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
* Author:  !!! PUT YOUR NAME(s) HERE !!!
*
* File Description:
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  1998/12/11 18:11:17  lewisg
* frontpage added
*

*
* ===========================================================================
*/

#include <ncbistd.hpp>
#include <ncbicgi.hpp>
#include <html.hpp>
#include <page.hpp>
#include <factory.hpp>
#include <ncbiapp.hpp>
#include <components.hpp>
#include <querypages.hpp>
BEGIN_NCBI_SCOPE
 

extern "C" int main(int argc, char *argv[])
{

    CPmFrontPage FrontPage;
    FrontPage.Create();
    FrontPage.Print(NcbiCout);

#if 0
	CHTML_form Form;

	CQueryBox QueryBox;
	QueryBox.Create(1);
	Form.AppendChild(&QueryBox);


	CPagerBox Box;
	Box.InitMembers(0);
	Box.m_TopButton->m_Name = "Display";
	Box.m_TopButton->m_Select = "display";
	Box.m_TopButton->m_List["dopt"] = "Top";
	Box.m_RightButton->m_Name = "save";
	Box.m_RightButton->m_Select = "m_s";
	Box.m_RightButton->m_List["m_s"] = "Right";
	Box.m_LeftButton->m_Name = "Order";
	Box.m_LeftButton->m_Select = "order";
	Box.m_LeftButton->m_List["m_o"] = "Left";
	Box.m_PageList->m_Pages[1] = "http://one";
	Box.m_PageList->m_Forward = "http://forward";
	Box.m_PageList->m_Backward = "http://backward";
	Box.Finish(0);
	Form.AppendChild(&Box);

	CSmallPagerBox Box2;
	Box2.InitMembers(0);
	Box2.m_PageList->m_Pages[1] = "http://one";
	Box2.m_PageList->m_Forward = "http://forward";
	Box2.m_PageList->m_Backward = "http://backward";
	Box2.Finish(0);
	Form.AppendChild(&Box2);
	Form.Print(NcbiCout);
#endif

    return 0;  
}

END_NCBI_SCOPE
