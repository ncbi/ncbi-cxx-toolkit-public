#ifndef OBJTOOLS_READERS___GFF_READER__HPP
#define OBJTOOLS_READERS___GFF_READER__HPP

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

/// @file gff_reader.hpp
/// Reader for GFF (including GTF) files.
///
/// These formats are somewhat loosely defined, so the reader allows
/// heavy use-specific tuning, both via flags and via virtual methods.
///
/// URLs to relevant specifications:
/// http://www.sanger.ac.uk/Software/formats/GFF/GFF_Spec.shtml (GFF 2)
/// http://genes.cs.wustl.edu/GTF2.html
/// http://song.sourceforge.net/gff3-jan04.shtml (support incomplete)


#include <corelib/ncbiutil.hpp>
#include <util/range_coll.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <objtools/readers/reader_exception.hpp>

#include <set>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/** @addtogroup Miscellaneous
 *
 * @{
 */

class NCBI_XOBJREAD_EXPORT CGFFReader
{
public:
    enum EFlags {
        ///< don't honor/recognize GTF conventions
        fNoGTF              = 0x01,

        ///< attribute tags are GenBank qualifiers
        fGBQuals            = 0x02,

        ///< merge exons with the same transcript_id
        fMergeExons         = 0x04,

        ///< restrict merging to just CDS and mRNA features
        fMergeOnyCdsMrna    = 0x08,

        ///< move protein_id and transcript_id to products for mRNA and CDS features
        fSetProducts        = 0x10,

        ///< create gene features for mRNAs and CDSs if none exist already
        fCreateGeneFeats    = 0x20,

        fDefaults = 0
    };
    typedef int TFlags;

    virtual ~CGFFReader() { }

    CRef<CSeq_entry> Read(CNcbiIstream& in, TFlags flags = fDefaults);

    struct SRecord : public CObject
    {
        struct SSubLoc
        {
            string         accession;
            ENa_strand     strand;

            /// the set of ranges that make up this location
            /// this allows us to separately assign frame even if the ranges in
            /// question do not appear in the correct order
            set<TSeqRange> ranges;

            /// a subsidiary set of ranges that is merged into ranges after
            /// parsing.  this is used to account for things like start/stop
            /// codons, that are CDS intervals and should be merged into CDS
            /// intervals
            set<TSeqRange> merge_ranges;
        };

        typedef set<vector<string> > TAttrs;
        typedef vector<SSubLoc>      TLoc;

        enum EType {
            eFeat,
            eAlign
        };

        TLoc         loc;       ///< from accession, start, stop, strand
        string       source;
        string       key;
        string       score;
        TAttrs       attrs;
        int frame;
        unsigned int line_no;
        EType        type;

        TAttrs::const_iterator FindAttribute(const string& name,
                                             size_t min_values = 1) const;
    };

protected:
    virtual void            x_Warn(const string& message,
                                   unsigned int line = 0);

    /// Reset all state, since we're between streams.
    virtual void            x_Reset(void);

    TFlags                  x_GetFlags(void) const { return m_Flags; }
    bool                    x_GetNextLine(string& line);
    unsigned int            x_GetLineNumber(void) { return m_LineNumber; }

    virtual void            x_ParseStructuredComment(const string& line);
    virtual void            x_ParseDateComment(const string& date);
    virtual void            x_ParseTypeComment(const string& moltype,
                                               const string& seqname);
    virtual void            x_ReadFastaSequences(CNcbiIstream& in);

    virtual CRef<SRecord>    x_ParseFeatureInterval(const string& line);
    virtual CRef<SRecord>    x_NewRecord(void)
        { return CRef<SRecord>(new SRecord); }
    virtual CRef<CSeq_feat>  x_ParseFeatRecord(const SRecord& record);
    virtual CRef<CSeq_align> x_ParseAlignRecord(const SRecord& record);
    virtual CRef<CSeq_loc>   x_ResolveLoc(const SRecord::TLoc& loc);
    virtual void             x_ParseV2Attributes(SRecord& record,
                                                 const vector<string>& v,
                                                 SIZE_TYPE& i);
    virtual void             x_ParseV3Attributes(SRecord& record,
                                                 const vector<string>& v,
                                                 SIZE_TYPE& i);
    virtual void             x_AddAttribute(SRecord& record,
                                            vector<string>& attr);

    /// Returning the empty string indicates that record constitutes
    /// an entire feature.  Returning anything else requests merging
    /// with other records that yield the same ID.
    virtual string          x_FeatureID(const SRecord& record);

    virtual void            x_MergeRecords(SRecord& dest, const SRecord& src);
    virtual void            x_MergeAttributes(SRecord& dest,
                                              const SRecord& src);
    virtual void            x_PlaceFeature(CSeq_feat& feat,
                                           const SRecord& record);
    virtual void            x_PlaceAlignment(CSeq_align& align,
                                             const SRecord& record);
    virtual void            x_ParseAndPlace(const SRecord& record);

    /// Falls back to x_ResolveNewSeqName on cache misses.
    virtual CRef<CSeq_id>   x_ResolveSeqName(const string& name);

    virtual CRef<CSeq_id>   x_ResolveNewSeqName(const string& name);

    /// Falls back to x_ResolveNewID on cache misses.
    virtual CRef<CBioseq>   x_ResolveID(const CSeq_id& id, const string& mol);

    /// The base version just constructs a shell so as not to depend
    /// on the object manager, but derived versions may consult it.
    virtual CRef<CBioseq>   x_ResolveNewID(const CSeq_id& id,
                                           const string& mol);

    virtual void            x_PlaceSeq(CBioseq& seq);

private:
    typedef map<string, CRef<CSeq_id>, PNocase>    TSeqNameCache;
    typedef map<CConstRef<CSeq_id>, CRef<CBioseq>,
                PPtrLess<CConstRef<CSeq_id> > >    TSeqCache;
    typedef map<string, CRef<SRecord>, PNocase>    TDelayedRecords;

    CRef<CSeq_entry> m_TSE;
    TSeqNameCache    m_SeqNameCache;
    TSeqCache        m_SeqCache;
    TDelayedRecords  m_DelayedRecords;
    string           m_DefMol;
    unsigned int     m_LineNumber;
    TFlags           m_Flags;
    CNcbiIstream*    m_Stream;
    int              m_Version;
};


END_SCOPE(objects)
END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.8  2005/03/02 15:00:06  dicuccio
 * Added additional options: fCreateGeneFeats (create gene features if non exist);
 * fSetProducts (promote transcript_id and protein_id to products on mRNA and CDS
 * features)
 *
 * Revision 1.7  2005/01/18 17:56:17  dicuccio
 * Added additional flags: permit merging of intervals in non-CDS and non-mRNA
 * features.  Added enum for default options.
 *
 * Revision 1.6  2005/01/13 15:30:33  dicuccio
 * Adjusted internal struct to track out-of-order ranges
 *
 * Revision 1.5  2004/06/07 20:43:44  ucko
 * Add initial GFF 3 support.
 *
 * Revision 1.4  2004/05/08 12:14:16  dicuccio
 * Use set<TRange> instead of CRangeCollection<>
 *
 * Revision 1.3  2004/01/27 17:12:34  ucko
 * Make SRecord public to resolve visibility errors on IBM VisualAge C++.
 *
 * Revision 1.2  2003/12/03 21:03:19  ucko
 * Add Windows export specifier.
 *
 * Revision 1.1  2003/12/03 20:56:19  ucko
 * Initial commit.
 *
 * ===========================================================================
 */

#endif  /* OBJTOOLS_READERS___GFF_READER__HPP */
