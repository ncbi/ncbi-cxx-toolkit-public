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
class CCommentItem;
class CFeatureItemBase;
class CSequenceItem;
class CSegmentItem;
class CContigItem;


class CGBSeqFormatter : public CFlatItemFormatter
{
public:
    CGBSeqFormatter(void);
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
    virtual void FormatComment(const CCommentItem& keys, IFlatTextOStream& text_os);
    virtual void FormatFeature(const CFeatureItemBase& feat, IFlatTextOStream& text_os);
    virtual void FormatSequence(const CSequenceItem& seq, IFlatTextOStream& text_os);
    virtual void FormatSegment(const CSegmentItem& seg, IFlatTextOStream& text_os);
    virtual void FormatContig(const CContigItem& contig, IFlatTextOStream& text_os);

private:
    void x_WriteFileHeader(IFlatTextOStream& text_os);
    void x_StartWriteGBSet(IFlatTextOStream& text_os);
    void x_WriteGBSeq(IFlatTextOStream& text_os);
    void x_EndWriteGBSet(IFlatTextOStream& text_os);
    void x_StrOStreamToTextOStream(IFlatTextOStream& text_os);

    struct SOStreamContainer
    {
        SOStreamContainer(CObjectOStream& out, 
            const CObjectTypeInfo& containerType) :
            m_Cont(out, containerType)
        {}
        void WriteElement(const CConstObjectInfo& element) {
            m_Cont.WriteElement(element);
        }
    private:
        COStreamContainer  m_Cont;
    };
    CRef<CGBSeq> m_GBSeq;
    auto_ptr<CObjectOStream> m_Out;
    CNcbiOstrstream m_StrStream;
    auto_ptr<SOStreamContainer>  m_Cont;
};


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2004/04/22 15:45:05  shomrat
* Start/End section changed
*
* Revision 1.1  2004/04/13 16:43:59  shomrat
* Inital Revision
*
*
* ===========================================================================
*/

#endif  /* OBJTOOLS_FORMAT___GBSEQ_FORMATTER__HPP */
