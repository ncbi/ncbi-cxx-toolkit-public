#ifndef BIOSEQ_CI__HPP
#define BIOSEQ_CI__HPP

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
* Author: Aleksey Grichenko
*
* File Description:
*   Iterates over bioseqs from a given seq-entry and scope
*
*/


#include <corelib/ncbiobj.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/impl/heap_scope.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/seq_entry_ci.hpp>
#include <objects/seq/Seq_inst.hpp>

#include <stack>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/** @addtogroup ObjectManagerIterators
 *
 * @{
 */


class CScope;
class CBioseq_Handle;
class CSeq_entry;


/////////////////////////////////////////////////////////////////////////////
///
///  CBioseq_CI --
///
///  Enumerate bioseqs in a given seq-entry
///

class NCBI_XOBJMGR_EXPORT CBioseq_CI
{
public:
    /// Class of bioseqs to iterate
    enum EBioseqLevelFlag {
        eLevel_All,    ///< Any bioseq
        eLevel_Mains,  ///< Main bioseq only
        eLevel_Parts   ///< Parts only
    };

    // 'ctors
    CBioseq_CI(void);

    /// Create an iterator that enumerates bioseqs
    /// from the entry taken from the scope. Use optional
    /// filter to iterate over selected bioseq types only.
    /// Filter value eMol_na may be used to include both
    /// dna and rna bioseqs.
    CBioseq_CI(const CSeq_entry_Handle& entry,
               CSeq_inst::EMol filter = CSeq_inst::eMol_not_set,
               EBioseqLevelFlag level = eLevel_All);

    CBioseq_CI(const CBioseq_set_Handle& bioseq_set,
               CSeq_inst::EMol filter = CSeq_inst::eMol_not_set,
               EBioseqLevelFlag level = eLevel_All);

    /// Create an iterator that enumerates bioseqs
    /// from the entry taken from the given scope. Use optional
    /// filter to iterate over selected bioseq types only.
    /// Filter value eMol_na may be used to include both
    /// dna and rna bioseqs.
    CBioseq_CI(CScope& scope, const CSeq_entry& entry,
               CSeq_inst::EMol filter = CSeq_inst::eMol_not_set,
               EBioseqLevelFlag level = eLevel_All);

    CBioseq_CI(const CBioseq_CI& bioseq_ci);
    ~CBioseq_CI(void);

    /// Get the current scope for the iterator
    CScope& GetScope(void) const;

    CBioseq_CI& operator= (const CBioseq_CI& bioseq_ci);

    /// Move to the next object in iterated sequence
    CBioseq_CI& operator++ (void);

    /// Check if iterator points to an object
    DECLARE_OPERATOR_BOOL(m_CurrentBioseq);

    const CBioseq_Handle& operator* (void) const;
    const CBioseq_Handle* operator-> (void) const;

private:
    void x_Initialize(const CSeq_entry_Handle& entry);
    void x_SetEntry(const CSeq_entry_Handle& entry);
    void x_Settle(void);
    bool x_IsValidMolType(const CBioseq_Info& seq) const;

    typedef stack<CSeq_entry_CI> TEntryStack;

    CHeapScope          m_Scope;
    CSeq_inst::EMol     m_Filter;
    EBioseqLevelFlag    m_Level;
    CSeq_entry_Handle   m_CurrentEntry;
    CBioseq_Handle      m_CurrentBioseq;
    TEntryStack         m_EntryStack;
    int                 m_InParts;
};


inline
const CBioseq_Handle& CBioseq_CI::operator* (void) const
{
    return m_CurrentBioseq;
}


inline
const CBioseq_Handle* CBioseq_CI::operator-> (void) const
{
    return &m_CurrentBioseq;
}


inline
CScope& CBioseq_CI::GetScope(void) const
{
    return m_Scope;
}


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.21  2005/03/18 16:14:26  grichenk
* Iterator with CSeq_inst::eMol_na includes both dna and rna.
*
* Revision 1.20  2005/01/24 17:09:36  vasilche
* Safe boolean operators.
*
* Revision 1.19  2005/01/18 14:58:58  grichenk
* Added constructor accepting CBioseq_set_Handle
*
* Revision 1.18  2005/01/10 19:06:27  grichenk
* Redesigned CBioseq_CI not to collect all bioseqs in constructor.
*
* Revision 1.17  2004/12/13 18:40:43  grichenk
* Removed CBioseq_CI_Base
*
* Revision 1.16  2004/10/01 14:45:19  kononenk
* Added doxygen formatting
*
* Revision 1.15  2004/03/16 15:47:25  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.14  2004/02/19 17:15:02  vasilche
* Removed unused include.
*
* Revision 1.13  2003/10/01 19:24:36  vasilche
* Added export specifier to CBioseq_CI as it's not completely inlined anymore.
*
* Revision 1.12  2003/09/30 16:21:59  vasilche
* Updated internal object manager classes to be able to load ID2 data.
* SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
* Scope caches results of requests for data to data loaders.
* Optimized CSeq_id_Handle for gis.
* Optimized bioseq lookup in scope.
* Reduced object allocations in annotation iterators.
* CScope is allowed to be destroyed before other objects using this scope are
* deleted (feature iterators, bioseq handles etc).
* Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
* Added 'adaptive' option to objmgr_demo application.
*
* Revision 1.11  2003/09/16 14:38:13  dicuccio
* Removed export specifier - the entire class is inlined, and the export
* specifier confuses MSVC in such cases.  Added #include for scope.hpp, which
* contains the base class (previously undefined without this).
*
* Revision 1.10  2003/09/03 19:59:59  grichenk
* Added sequence filtering by level (mains/parts/all)
*
* Revision 1.9  2003/06/02 16:01:36  dicuccio
* Rearranged include/objects/ subtree.  This includes the following shifts:
*     - include/objects/alnmgr --> include/objtools/alnmgr
*     - include/objects/cddalignview --> include/objtools/cddalignview
*     - include/objects/flat --> include/objtools/flat
*     - include/objects/objmgr/ --> include/objmgr/
*     - include/objects/util/ --> include/objmgr/util/
*     - include/objects/validator --> include/objtools/validator
*
* Revision 1.8  2003/03/10 17:51:36  kuznets
* iterate->ITERATE
*
* Revision 1.7  2003/02/25 14:48:06  vasilche
* Added Win32 export modifier to object manager classes.
*
* Revision 1.6  2003/02/04 16:01:48  dicuccio
* Removed export specification so that MSVC won't try to export an inlined class
*
* Revision 1.5  2002/12/26 20:42:55  dicuccio
* Added Win32 export specifier.  Removed unimplemented (private) operator++(int)
*
* Revision 1.4  2002/12/05 19:28:29  grichenk
* Prohibited postfix operator ++()
*
* Revision 1.3  2002/10/15 13:37:28  grichenk
* Fixed inline declarations
*
* Revision 1.2  2002/10/02 17:58:21  grichenk
* Added sequence type filter to CBioseq_CI
*
* Revision 1.1  2002/09/30 20:00:48  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#endif  // BIOSEQ_CI__HPP
