#ifndef OBJTOOLS_FLAT___FLAT_GFF_FORMATTER__HPP
#define OBJTOOLS_FLAT___FLAT_GFF_FORMATTER__HPP

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
 * Authors:  Aaron Ucko, Wratko Hlavina
 *
 */

/// @file flat_gff_formatter.hpp
/// Flat formatter for Generic Feature Format (incl. Gene Transfer Format)
///
/// These formats are somewhat loosely defined (for the record, at
/// http://www.sanger.ac.uk/Software/formats/GFF/GFF_Spec.shtml and
/// http://genes.cs.wustl.edu/GTF2.html respectively) so we default to
/// GenBank/DDBJ/EMBL keys and qualifiers except as needed for GTF
/// compatibility.

#include <objtools/flat/flat_text_formatter.hpp>

#include <objects/seqfeat/Seq_feat.hpp>

/** @addtogroup Miscellaneous
 *
 * @{
 */


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CFlatFeature;

class NCBI_FLAT_EXPORT CFlatGFFFormatter : public IFlatFormatter
{
public:
    enum EGFFFlags {
        fGTFCompat = 0x1, ///< Represent CDSs (and exons) per GTF.
        fGTFOnly   = 0x3, ///< Omit all other features.
        fShowSeq   = 0x4  ///< Show the actual sequence in a "##" comment.
    };
    typedef int TGFFFlags; ///< Binary OR of EGFFFlags

    CFlatGFFFormatter(IFlatTextOStream& stream, CScope& scope,
                      EMode mode = eMode_Dump, TGFFFlags gff_flags = 0,
                      EStyle style = eStyle_Master, TFlags flags = 0);

protected: // mostly no-ops
    EDatabase GetDatabase(void) const { return eDB_NCBI; }

    void FormatHead      (const CFlatHead& head);
    void FormatKeywords  (const CFlatKeywords& /* keys */)   { }
    void FormatSegment   (const CFlatSegment& /* seg */)     { }
    void FormatSource    (const CFlatSource& /* source */)   { }
    void FormatReference (const CFlatReference& /* ref */)   { }
    void FormatComment   (const CFlatComment& /* comment */) { }
    void FormatPrimary   (const CFlatPrimary& /* prim */)    { }
    void FormatFeatHeader(const CFlatFeatHeader& /* fh */)   { }
    void FormatFeature   (const IFlattishFeature& f);
    void FormatDataHeader(const CFlatDataHeader& dh);
    void FormatData      (const CFlatData& data);
    void FormatContig    (const CFlatContig& /* contig */)   { }
    void FormatWGSRange  (const CFlatWGSRange& /* range */)  { }
    void FormatGenomeInfo(const CFlatGenomeInfo& /* g */)    { }
    void EndSequence     (void);

private:
    string x_GetGeneID(const CFlatFeature& feat, const string& gene_name);
    string x_GetSourceName(const IFlattishFeature&);
    void   x_AddFeature(list<string>& l, const CSeq_loc& loc,
                        const string& source, const string& key,
                        const string& score, int frame, const string& attrs,
                        bool gtf);

    TGFFFlags              m_GFFFlags;
    CRef<IFlatTextOStream> m_Stream;
    string                 m_SeqType;
    string                 m_EndSequence;
    /// Taken from head
    string                 m_Date;
    CSeq_inst::TStrand     m_Strandedness;

    typedef vector<CConstRef<CSeq_feat> > TFeatVec;
    map<string, TFeatVec>  m_Genes;
};


END_SCOPE(objects)
END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2005/02/02 19:49:54  grichenk
 * Fixed more warnings
 *
 * Revision 1.4  2003/11/04 19:39:45  ucko
 * Default style changed from normal to master now that the OM no longer
 * splits remapped features.
 *
 * Revision 1.3  2003/10/17 21:01:54  ucko
 * +x_AddFeature (helper factored out of FormatFeature)
 *
 * Revision 1.2  2003/10/09 17:01:48  dicuccio
 * Added export specifiers
 *
 * Revision 1.1  2003/10/08 21:11:29  ucko
 * New GFF/GTF formatter
 *
 * ===========================================================================
 */

#endif  /* OBJTOOLS_FLAT___FLAT_GFF_FORMATTER__HPP */
