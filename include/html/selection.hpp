#ifndef HTML___SELECTION__HPP
#define HTML___SELECTION__HPP

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
 * Author:  Eugene Vasilchenko
 *
 */

/// @file selection.hpp
/// 

#include <corelib/ncbistd.hpp>
#include <html/node.hpp>
#include <html/htmlhelper.hpp>


/** @addtogroup HTMLcomp
 *
 * @{
 */


BEGIN_NCBI_SCOPE


// Forward declaration.
class CCgiRequest;


class NCBI_XHTML_EXPORT CSelection : public CNCBINode, public CIDs
{
public:
    static string sm_DefaultCheckboxName;
    static string sm_DefaultSaveName;

    CSelection(const CCgiRequest& request,
               const string& checkboxName = sm_DefaultCheckboxName,
               const string& saveName     = sm_DefaultSaveName);

    virtual void CreateSubNodes(void);

private:
    const string m_SaveName;
};


END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.8  2004/01/16 15:12:32  ivanov
 * Minor cosmetic changes
 *
 * Revision 1.7  2003/11/05 18:41:06  dicuccio
 * Added export specifiers
 *
 * Revision 1.6  2003/11/03 17:02:53  ivanov
 * Some formal code rearrangement. Move log to end.
 *
 * Revision 1.5  2003/04/25 13:45:40  siyan
 * Added doxygen groupings
 *
 * Revision 1.4  2001/05/17 14:55:48  lavr
 * Typos corrected
 *
 * Revision 1.3  1999/05/12 21:11:44  vakatov
 * Minor fixes to compile by the Mac CodeWarrior C++ compiler
 *
 * Revision 1.2  1999/03/26 22:00:00  sandomir
 * checked option in Radio button fixed; minor fixes in Selection
 *
 * Revision 1.1  1999/01/20 17:39:43  vasilche
 * Selection as separate class CSelection.
 *
 * ===========================================================================
 */

#endif  /* HTML___SELECTION__HPP */
