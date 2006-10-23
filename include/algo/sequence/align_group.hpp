#ifndef GUI_PLUGINS_ALGO_ALIGN___ALIGN_GROUP__HPP
#define GUI_PLUGINS_ALGO_ALIGN___ALIGN_GROUP__HPP

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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <corelib/ncbiobj.hpp>
#include <set>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CSeq_align;
    class CSeq_annot;
    class CScope;
    class COrg_ref;
    class CSeq_id_Handle;
    class CTaxon1;
END_SCOPE(objects)


class NCBI_XALGOSEQ_EXPORT CAlignGroup
{
public:

    typedef list< CRef<objects::CSeq_align> > TAlignList;
    typedef list< CRef<objects::CSeq_annot> > TAnnotList;

    CAlignGroup();
    ~CAlignGroup();

    /// @name Taxonomic Separation Interface
    /// @{

    /// typedefs for dealing with separations by tax-id
    typedef set<int> TTaxIds;
    typedef map<TTaxIds, list< CRef<objects::CSeq_align> > > TTaxAlignMap;

    /// typedefs for caching of taxonomic information
    typedef map<objects::CSeq_id_Handle, size_t> TTaxIdMap;
    typedef map<int, CConstRef<objects::COrg_ref> > TTaxInfoMap;

    /// Separate a set of alignments into groups that describe
    /// how the alignments relate taxonomically
    void GroupByTaxIds(const TAlignList& aligns,
                       TAnnotList&       align_groups,
                       const string&     annot_base_name,
                       objects::CScope&  scope);

    /// Separate a set of alignments into groups that describe
    /// how the alignments relate taxonomically.  This version
    /// will group together all alignments that have more than one
    /// tax-id represented.
    void GroupByLikeTaxIds(const TAlignList& aligns,
                           TAnnotList&       align_groups,
                           const string&     annot_base_name,
                           objects::CScope&  scope);

    /// @}

    /// @name Sequence Type Separation Interface
    /// @{

    enum ESequenceFlags {
        fRefSeq          = 0x01,
        fRefSeqPredicted = 0x02,
        fEST             = 0x04,
        fWGS             = 0x08,
        fHTGS            = 0x10,
        fPatent          = 0x20,
        fGB_EMBL_DDBJ    = 0x40,

        fSequenceDefaults = fRefSeq | fEST | fGB_EMBL_DDBJ
    };
    typedef int TSequenceFlags;

    /// Group alignments into sequence-related categories.
    void GroupBySequenceType(const TAlignList& aligns,
                             TAnnotList&       align_groups,
                             const string&     annot_base_name,
                             objects::CScope&  scope,
                             TSequenceFlags    flags = fSequenceDefaults);


    enum ESeqIdFlags {
        fResolveToGi     = 0x01
    };
    typedef int TSeqIdFlags;

    /// Group alignments into bins for each set of seq-ids.
    void GroupBySeqIds(const TAlignList& aligns,
                       TAnnotList&       align_groups,
                       const string&     annot_base_name,
                       objects::CScope&  scope,
                       TSeqIdFlags       flags = 0);

    /// Group alignments into bins for each set of strands.
    void GroupByStrand(const TAlignList& aligns,
                       TAnnotList&       align_groups,
                       const string&     annot_base_name,
                       objects::CScope&  scope);

    /// Subdivide based on an Entrez query
    void GroupByEntrezQuery(const TAlignList& aligns,
                            TAnnotList&       align_groups,
                            const string&     annot_base_name,
                            objects::CScope&  scope,
                            const string&     query,
                            const string&     db);

    /// @}


private:

    auto_ptr<objects::CTaxon1> m_Taxon1;
    TTaxIdMap   m_TaxIds;
    TTaxInfoMap m_TaxInfo;

    void x_SeparateByTaxId(const TAlignList& alignments,
                           TTaxAlignMap&     tax_aligns,
                           objects::CScope&  scope);

    CConstRef<objects::COrg_ref> x_GetOrgRef(int tax_id);
    CConstRef<objects::COrg_ref> x_GetOrgRef(const objects::CSeq_id_Handle& id,
                                             objects::CScope& scope);
    int x_GetTaxId(const objects::CSeq_id_Handle& id,
                   objects::CScope& scope);

private:
    /// forbidden!
    CAlignGroup(const CAlignGroup&);
    CAlignGroup& operator=(const CAlignGroup&);
};



END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2006/10/23 14:54:10  dicuccio
 * Moved over from gui plugin tree
 *
 * Revision 1.1  2006/03/08 12:55:16  dicuccio
 * Added two new tools: general alignment clean-up function; alignment
 * subcategorization and grouping function
 *
 * ===========================================================================
 */

#endif  // GUI_PLUGINS_ALGO_ALIGN___ALIGN_GROUP__HPP
