#ifndef SELECTION__HPP
#define SELECTION__HPP

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
* Revision 1.1  1999/01/20 17:39:43  vasilche
* Selection as separate class CSelection.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <html/node.hpp>
#include <html/htmlhelper.hpp>

BEGIN_NCBI_SCOPE

class CCgiRequest;

class CSelection : public CNCBINode, public CIDs
{
public:
    CSelection(CCgiRequest& request,
               const string& checkboxName = sm_DefaultCheckboxName,
               const string& saveName = sm_DefaultSaveName);

    virtual void CreateSubNodes(void);

    static string sm_DefaultCheckboxName;
    static string sm_DefaultSaveName;

private:
    const string m_SaveName;
};

#include <html/selection.inl>

END_NCBI_SCOPE

#endif  /* SELECTION__HPP */
