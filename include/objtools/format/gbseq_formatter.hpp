#ifndef OBJTOOLS_FORMAT___GBSEQ_FORMATTER__HPP
#define OBJTOOLS_FORMAT___GBSEQ_FORMATTER__HPP

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
*   GBSeq formatting
*/
#include <corelib/ncbistd.hpp>
#include <serial/objectio.hpp>
#include <objects/gbseq/GBSeq.hpp>
#include <objtools/format/item_formatter.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CLocusItem;
class CDeflineItem;
class CAccessionItem;
class CVersionItem;
class CKeywordsItem;
class CSourceItem;
class CReferenceItem;
class CCacheItem;
class CCommentItem;
class CPrimaryItem;
class CDBSourceItem;
class CFeatureItemBase;
class CSequenceItem;
class CSegmentItem;
class CContigItem;
class CGenomeProjectItem;


class NCBI_FORMAT_EXPORT CGBSeqFormatter : public CFlatItemFormatter
{
public:
    CGBSeqFormatter(bool isInsd=false);
    ~CGBSeqFormatter(void);

    virtual void Start       (IFlatTextOStream&);
    virtual void StartSection(const CStartSectionItem&, IFlatTextOStream&);
    virtual void EndSection  (const CEndSectionItem&, IFlatTextOStream&);
    virtual void End         (IFlatTextOStream&);

    virtual void FormatLocus(const CLocusItem& locus, IFlatTextOStream& text_os);
    virtual void FormatDefline(const CDeflineItem& defline, IFlatTextOStream& text_os);
    virtual void FormatAccession(const CAccessionItem& acc, IFlatTextOStream& text_os);
    virtual void FormatVersion(const CVersionItem& version, IFlatTextOStream& text_os);
    virtual void FormatKeywords(const CKeywordsItem& keys, IFlatTextOStream& text_os);
    virtual void FormatSource(const CSourceItem& source, IFlatTextOStream& text_os);
    virtual void FormatReference(const CReferenceItem& keys, IFlatTextOStream& text_os);
    virtual void FormatCache(const CCacheItem& csh, IFlatTextOStream& text_os);
    virtual void FormatComment(const CCommentItem& keys, IFlatTextOStream& text_os);
    virtual void FormatPrimary(const CPrimaryItem& primary, IFlatTextOStream& text_os);
    virtual void FormatDBSource(const CDBSourceItem& dbs, IFlatTextOStream& text_os);
    virtual void FormatFeature(const CFeatureItemBase& feat, IFlatTextOStream& text_os);
    virtual void FormatSequence(const CSequenceItem& seq, IFlatTextOStream& text_os);
    virtual void FormatSegment(const CSegmentItem& seg, IFlatTextOStream& text_os);
    virtual void FormatContig(const CContigItem& contig, IFlatTextOStream& text_os);
    virtual void FormatGenomeProject(const CGenomeProjectItem&, IFlatTextOStream&);
    virtual void FormatGap(const CGapItem& gap, IFlatTextOStream& text_os);
    virtual void FormatWGS(const CWGSItem& wgs, IFlatTextOStream& text_os);
    virtual void FormatTSA(const CTSAItem& tsa, IFlatTextOStream& text_os);
    virtual void Reset(void);

private:
    void x_WriteFileHeader(IFlatTextOStream& text_os);
    void x_WriteGBSeq(IFlatTextOStream& text_os);
    void x_StrOStreamToTextOStream(IFlatTextOStream& text_os);

    // ID-4631 : common code for formatting accession ranges for WGS/TSA/TLS masters
    template <typename T> void
    x_FormatAltSeq(const T& item, const string& name, IFlatTextOStream& text_os);

    CRef<CGBSeq> m_GBSeq;
    unique_ptr<CObjectOStream> m_Out;
    CNcbiOstrstream m_StrStream;
    bool m_IsInsd;
    bool m_DidFeatStart;
    bool m_DidJourStart;
    bool m_DidKeysStart;
    bool m_DidRefsStart;
    bool m_DidWgsStart;
    bool m_DidSequenceStart;
    bool m_NeedFeatEnd;
    bool m_NeedJourEnd;
    bool m_NeedRefsEnd;
    bool m_NeedWgsEnd;
    bool m_NeedComment;
    bool m_NeedPrimary;
    bool m_NeedDbsource;
    bool m_NeedXrefs;
    string m_OtherSeqIDs;
    string m_SecondaryAccns;
    list<string> m_Comments;
    string m_Primary;
    list<string> m_Dbsource;
    list<string> m_Xrefs;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJTOOLS_FORMAT___GBSEQ_FORMATTER__HPP */
