#ifndef OBJTOOLS_FORMAT___GFF_FORMATTER__HPP
#define OBJTOOLS_FORMAT___GFF_FORMATTER__HPP

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
* Author:  Aaron Ucko, NCBI
*          Mati Shomrat
*
* File Description:
*
*/

/// @file flat_gff_formatter.hpp
/// Flat formatter for Generic Feature Format v2 (incl. Gene Transfer Format).
///
/// These formats are somewhat loosely defined (for the record, at
/// http://www.sanger.ac.uk/Software/formats/GFF/GFF_Spec.shtml and
/// http://genes.cs.wustl.edu/GTF2.html respectively) so we default to
/// GenBank/DDBJ/EMBL keys and qualifiers except as needed for GTF
/// compatibility.
///
/// There is a separate formatter (subclassing this one) for GFF 3.

#include <corelib/ncbistd.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objtools/format/item_formatter.hpp>

/** @addtogroup Miscellaneous
 *
 * @{
 */


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CFlatFeature;


class CGFFFormatter : public CFlatItemFormatter
{
public:
    enum EGFFFlags {
        fGTFCompat = 0x1, ///< Represent CDSs (and exons) per GTF.
        fGTFOnly   = 0x3, ///< Omit all other features.
        fShowSeq   = 0x4  ///< Show the actual sequence in a "##" comment.
    };
    typedef int TGFFFlags; ///< Binary OR of EGFFFlags

    CGFFFormatter(void);

    void Start       (IFlatTextOStream& text_os);
    void StartSection(const CStartSectionItem&, IFlatTextOStream& text_os);
    void EndSection  (const CEndSectionItem&,   IFlatTextOStream& text_os);

    void FormatLocus(const CLocusItem& locus, IFlatTextOStream& text_os);
    void FormatDate(const CDateItem& date, IFlatTextOStream& text_os);
    void FormatFeature(const CFeatureItemBase& feat, IFlatTextOStream& text_os);
    void FormatBasecount(const CBaseCountItem& bc, IFlatTextOStream& text_os);
    void FormatSequence(const CSequenceItem& seq, IFlatTextOStream& text_os);

protected:
    virtual string x_GetAttrSep(void) const { return " "; }
    virtual string x_FormatAttr(const string& name, const string& value) const;
    virtual void   x_AddGeneID(list<string>& attr_list, const string& gene_id,
                               const string& transcript_id) const;

    string x_GetSourceName(CBioseqContext& ctx) const;
    void   x_AddFeature(list<string>& l, const CSeq_loc& loc,
                        const string& source, const string& key,
                        const string& score, int frame, const string& attrs,
                        bool gtf, CBioseqContext& ctx,
                        bool tentative_stop = false) const;

private:
    string x_GetGeneID(const CFlatFeature& feat, const string& gene_name,
                       CBioseqContext& ctx) const;
    string x_GetTranscriptID(const CFlatFeature& feat, const string& gene_id,
                             CBioseqContext& ctx) const;

    mutable TGFFFlags           m_GFFFlags;
    //CRef<IFlatTextOStream> m_Stream;
    mutable string              m_SeqType;
    mutable string              m_EndSequence;
    /// Taken from head
    mutable string              m_Date;
    mutable CSeq_inst::TStrand  m_Strandedness;

    typedef vector<CConstRef<CSeq_feat> > TFeatVec;
    mutable map<string, TFeatVec>  m_Genes;
    mutable map<string, TFeatVec>  m_Transcripts;
};


END_SCOPE(objects)
END_NCBI_SCOPE


/* @} */


/*
* ===========================================================================
*
* $Log$
* Revision 1.5  2005/03/02 13:45:20  shomrat
* Fixed warning
*
* Revision 1.4  2004/06/21 18:52:13  ucko
* Refactor to ease subclassing by the GFF 3 formatter.
*
* Revision 1.3  2004/05/08 12:13:56  dicuccio
* Added x_GetTranscripID() for handling transcript IDs
*
* Revision 1.2  2004/04/22 15:45:57  shomrat
* Changes in context
*
* Revision 1.1  2004/01/14 15:59:50  shomrat
* Initial Revision
*
*
* ===========================================================================
*/

#endif  /* OBJTOOLS_FORMAT___GFF_FORMATTER__HPP */
