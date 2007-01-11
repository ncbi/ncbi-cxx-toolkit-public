#ifndef CLEANUP___CLEANUP__HPP
#define CLEANUP___CLEANUP__HPP

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
 * Author:  Robert Smith
 *
 * File Description:
 *   Basic Cleanup of CSeq_entries.
 *   .......
 *
 */
#include <corelib/ncbistd.hpp>
#include <corelib/mswin_export.h>
#include <corelib/ncbiobj.hpp>
#include <serial/serialbase.hpp>
#include <objmgr/scope.hpp>
#include <objtools/cleanup/cleanup_change.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;
class CBioseq;
class CBioseq_set;
class CSeq_annot;
class CSeq_feat;
class CSeq_submit;

class CSeq_entry_Handle;
class CBioseq_Handle;
class CBioseq_set_Handle;
class CSeq_annot_Handle;
class CSeq_feat_Handle;

class CCleanupChange;

class NCBI_CLEANUP_EXPORT CCleanup : public CObject 
{
public:

    enum EValidOptions {
        eClean_NoReporting = 0x1
    };

    // Construtor / Destructor
    CCleanup();
    ~CCleanup();

    void SetScope(CRef<CScope> scope);
    
    // BASIC CLEANUP
    
    /// Cleanup a Seq-entry. 
    CConstRef<CCleanupChange> BasicCleanup(CSeq_entry& se,  Uint4 options = 0);
    /// Cleanup a Seq-submit. 
    CConstRef<CCleanupChange> BasicCleanup(CSeq_submit& ss,  Uint4 options = 0);
    /// Cleanup a Bioseq. 
    CConstRef<CCleanupChange> BasicCleanup(CBioseq& bs,     Uint4 options = 0);
    /// Cleanup a Bioseq_set.
    CConstRef<CCleanupChange> BasicCleanup(CBioseq_set& bss, Uint4 options = 0);
    /// Cleanup a Seq-Annot.
    CConstRef<CCleanupChange> BasicCleanup(CSeq_annot& sa,  Uint4 options = 0);
    /// Cleanup a Seq-feat. 
    CConstRef<CCleanupChange> BasicCleanup(CSeq_feat& sf,   Uint4 options = 0);

    // Handle versions.
    CConstRef<CCleanupChange> BasicCleanup(CSeq_entry_Handle& seh, Uint4 options = 0);
    CConstRef<CCleanupChange> BasicCleanup(CBioseq_Handle& bsh,    Uint4 options = 0);
    CConstRef<CCleanupChange> BasicCleanup(CBioseq_set_Handle& bssh, Uint4 options = 0);
    CConstRef<CCleanupChange> BasicCleanup(CSeq_annot_Handle& sak, Uint4 options = 0);
    CConstRef<CCleanupChange> BasicCleanup(CSeq_feat_Handle& sfh,  Uint4 options = 0);
    
    // Extended Cleanup
        /// Cleanup a Seq-entry. 
    CConstRef<CCleanupChange> ExtendedCleanup(const CSeq_entry& se);
    /// Cleanup a Seq-submit. 
    CConstRef<CCleanupChange> ExtendedCleanup(const CSeq_submit& ss);
    /// Cleanup a Seq-Annot.
    CConstRef<CCleanupChange> ExtendedCleanup(const CSeq_annot& sa);

private:
    // Prohibit copy constructor & assignment operator
    CCleanup(const CCleanup&);
    CCleanup& operator= (const CCleanup&);

    CRef<CScope>            m_Scope;
};



END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.15  2007/01/11 19:09:24  bollin
* Bug fixes for ExtendedCleanup
*
* Revision 1.14  2006/07/26 19:35:13  rsmith
* add BasicCleanup w/Handles
*
* Revision 1.13  2006/06/23 17:10:20  rsmith
* totally rewrite CCleanupChange class.
*
* Revision 1.12  2006/06/22 13:49:48  bollin
* added method for extended cleanup of CSeq_annot
*
* Revision 1.11  2006/06/20 19:43:10  bollin
* added extended cleanup for SeqSubmit
*
* Revision 1.10  2006/06/19 12:59:02  bollin
* added ExtendedCleanup method
*
* Revision 1.9  2006/05/17 17:39:05  bollin
* added CScope member
*
* Revision 1.8  2006/03/23 18:30:18  rsmith
* cosmetic changes.
*
* Revision 1.7  2006/03/20 14:20:37  rsmith
* add BasicCleanup for CSeq_submit
*
* Revision 1.6  2006/03/15 14:09:54  dicuccio
* Fix compilation errors: hide assignment operator, drop import specifier for
* private functions
*
* Revision 1.5  2006/03/14 20:21:50  rsmith
* Move BasicCleanup functionality from objects to objtools/cleanup
*
* Revision 1.4  2005/10/18 22:47:12  vakatov
* Static class members sm_Verbose[] and sm_Terse[] to become static
* in-function s_Verbose[] and s_Terse[], respectively (didn't work on
* MSVC otherwise; didn't have time to try EXPORT'ing them)
*
* Revision 1.3  2005/10/18 14:22:18  rsmith
* BasicCleanup entry points for more classes.
*
* Revision 1.2  2005/10/18 13:34:09  rsmith
* add change reporting classes
*
* Revision 1.1  2005/10/17 12:21:07  rsmith
* initial checkin
*
*
* ===========================================================================
*/

#endif  /* CLEANUP___CLEANUP__HPP */
