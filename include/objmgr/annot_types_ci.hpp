#ifndef ANNOT_TYPES_CI__HPP
#define ANNOT_TYPES_CI__HPP

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
* Author: Aleksey Grichenko, Michael Kimelman, Eugene Vasilchenko
*
* File Description:
*   Object manager iterators
*
*/

#include <objmgr/impl/annot_collector.hpp>
#include <set>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)

class CAnnot_CI;
class CTableFieldHandle_Base;

// Base class for specific annotation iterators
class NCBI_XOBJMGR_EXPORT CAnnotTypes_CI
{
public:
    typedef SAnnotSelector::TAnnotType TAnnotType;

    CAnnotTypes_CI(void);

    CAnnotTypes_CI(CScope& scope);

    // Search on the part of bioseq
    CAnnotTypes_CI(TAnnotType type,
                   const CBioseq_Handle& bioseq,
                   const CRange<TSeqPos>& range,
                   ENa_strand strand,
                   const SAnnotSelector* params = 0);

    // Search on location
    CAnnotTypes_CI(TAnnotType type,
                   CScope& scope,
                   const CSeq_loc& loc,
                   const SAnnotSelector* params = 0);

    // Iterate everything from the seq-annot
    CAnnotTypes_CI(TAnnotType type,
                   const CSeq_annot_Handle& annot,
                   const SAnnotSelector* params = 0);

    // Iterate everything from the seq-entry
    CAnnotTypes_CI(TAnnotType type,
                   const CSeq_entry_Handle& entry,
                   const SAnnotSelector* params = 0);

    virtual ~CAnnotTypes_CI(void);

    // Rewind annot iterator to point to the very first annot object,
    // the same as immediately after construction.
    void Rewind(void);

    // Get parent seq-annot
    CSeq_annot_Handle GetAnnot(void) const;

    // Get number of annotations
    size_t GetSize(void) const;

    typedef vector<SAnnotTypeSelector> TAnnotTypes;
    // Get annot types
    const TAnnotTypes& GetAnnotTypes(void) const;

    typedef set<CAnnotName> TAnnotNames;
    const TAnnotNames& GetAnnotNames(void) const;

protected:
    friend class CAnnot_CI;
    friend class CTableFieldHandle_Base;

    typedef CAnnot_Collector::TAnnotSet TAnnotSet;
    typedef TAnnotSet::const_iterator   TIterator;

    // Check if a datasource and an annotation are selected.
    bool IsValid(void) const;
    // Move to the next valid position
    void Next(void);
    void Prev(void);
    // Return current annotation
    const CAnnotObject_Ref& Get(void) const;
    CScope& GetScope(void) const;

    CAnnot_Collector& GetCollector(void);
    const TIterator& GetIterator(void) const;

private:
    const TAnnotSet& x_GetAnnotSet(void) const;
    void x_Init(CScope& scope,
                const CSeq_loc& loc,
                const SAnnotSelector& params);

    CRef<CAnnot_Collector> m_DataCollector;
    // Current annotation
    TIterator              m_CurrAnnot;
    mutable TAnnotTypes    m_AnnotTypes;
};


/////////////////////////////////////////////////////////////////////////////
// CAnnotTypes_CI
/////////////////////////////////////////////////////////////////////////////


inline
const CAnnotTypes_CI::TAnnotSet& CAnnotTypes_CI::x_GetAnnotSet(void) const
{
    _ASSERT(m_DataCollector);
    return m_DataCollector->GetAnnotSet();
}


inline
bool CAnnotTypes_CI::IsValid(void) const
{
    return m_DataCollector &&  m_CurrAnnot != x_GetAnnotSet().end();
}


inline
void CAnnotTypes_CI::Rewind(void)
{
    m_CurrAnnot = x_GetAnnotSet().begin();
}


inline
void CAnnotTypes_CI::Next(void)
{
    ++m_CurrAnnot;
}


inline
void CAnnotTypes_CI::Prev(void)
{
    --m_CurrAnnot;
}


inline
const CAnnotObject_Ref& CAnnotTypes_CI::Get(void) const
{
    _ASSERT( IsValid() );
    return *m_CurrAnnot;
}


inline
CAnnot_Collector& CAnnotTypes_CI::GetCollector(void)
{
    _ASSERT(m_DataCollector);
    return *m_DataCollector;
}


inline
const CAnnotTypes_CI::TIterator& CAnnotTypes_CI::GetIterator(void) const
{
    _ASSERT( IsValid() );
    return m_CurrAnnot;
}


inline
size_t CAnnotTypes_CI::GetSize(void) const
{
    return x_GetAnnotSet().size();
}

inline
CScope& CAnnotTypes_CI::GetScope(void) const
{
    _ASSERT(m_DataCollector);
    return m_DataCollector->GetScope();
}


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // ANNOT_TYPES_CI__HPP
