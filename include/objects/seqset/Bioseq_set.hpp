/* $Id$
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
 * Author:  .......
 *
 * File Description:
 *   .......
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using specifications from the ASN data definition file
 *   'seqset.asn'.
 *
 */

#ifndef OBJECTS_SEQSET_BIOSEQ_SET_HPP
#define OBJECTS_SEQSET_BIOSEQ_SET_HPP


// generated includes
#include <objects/seqset/Bioseq_set_.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

class CSeq_entry;
class CBioseq;

class NCBI_SEQSET_EXPORT CBioseq_set : public CBioseq_set_Base
{
    typedef CBioseq_set_Base Tparent;
public:
    // constructor
    CBioseq_set(void);
    // destructor
    ~CBioseq_set(void);

    // Manage Seq-entry tree structure
    // get parent Seq-entry.
    // NULL means that either there is no parent Seq-entry,
    // or CSeq_entry::Parentize() was never called.
    CSeq_entry* GetParentEntry(void) const;

    // Convenience function to directly get reference to parent Bioseq-set.
    // 0 means that either there is no parent Seq-entry or Bioseq-set,
    // or CSeq_entry::Parentize() was never called.
    CConstRef<CBioseq_set> GetParentSet(void) const;

    enum ELabelType {
        eType,
        eContent,
        eBoth
    };

    // Append a label to label based on type or content of CBioseq_set
    void GetLabel(string* label, ELabelType type) const;

    // Class specific methods
    // methods will throw if called on an object with the wrong TClass value.
    const CBioseq& GetNucFromNucProtSet(void) const;
    const CBioseq& GetGenomicFromGenProdSet(void) const;
    const CBioseq& GetMasterFromSegSet(void) const;

    bool NeedsDocsumTitle() const;
    static bool NeedsDocsumTitle(EClass set_class);

private:
    // Prohibit copy constructor & assignment operator
    CBioseq_set(const CBioseq_set&);
    CBioseq_set& operator= (const CBioseq_set&);

    // Seq-entry containing the Bioseq
    void SetParentEntry(CSeq_entry* entry);
    CSeq_entry* m_ParentEntry;

    friend class CSeq_entry;
};



/////////////////// CBioseq_set inline methods

// constructor
inline
CBioseq_set::CBioseq_set(void)
    : m_ParentEntry(0)
{
}

inline
void CBioseq_set::SetParentEntry(CSeq_entry* entry)
{
    m_ParentEntry = entry;
}

inline
CSeq_entry* CBioseq_set::GetParentEntry(void) const
{
    return m_ParentEntry;
}

/////////////////// end of CBioseq_set inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

#endif // OBJECTS_SEQSET_BIOSEQ_SET_HPP
