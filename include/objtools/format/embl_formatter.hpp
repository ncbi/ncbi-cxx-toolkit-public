#ifndef OBJTOOLS_FORMAT___EMBL_FORMATTER__HPP
#define OBJTOOLS_FORMAT___EMBL_FORMATTER__HPP

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
*   new (early 2003) flat-file generator -- base class for items
*   (which roughly correspond to blocks/paragraphs in the C version)
*
*/
#include <corelib/ncbistd.hpp>

#include <objtools/format/item_formatter.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CEmblFormatter : public CFlatItemFormatter
{
public:
    CEmblFormatter(void);

    virtual SIZE_TYPE GetWidth(void) const { return 78; }

    virtual void EndSection(const CEndSectionItem&, IFlatTextOStream& text_os);

    virtual void FormatLocus(const CLocusItem& locus, IFlatTextOStream& text_os);
    virtual void FormatDefline(const CDeflineItem& defline, IFlatTextOStream& text_os);
    virtual void FormatAccession(const CAccessionItem& acc, IFlatTextOStream& text_os);
    virtual void FormatVersion(const CVersionItem& version, IFlatTextOStream& text_os);
    virtual void FormatKeywords(const CKeywordsItem& keys, IFlatTextOStream& text_os);
    virtual void FormatSource(const CSourceItem& source, IFlatTextOStream& text_os);
    virtual void FormatReference(const CReferenceItem& keys, IFlatTextOStream& text_os);
    virtual void FormatComment(const CCommentItem& keys, IFlatTextOStream& text_os);
    virtual void FormatBasecount(const CBaseCountItem& bc, IFlatTextOStream& text_os);
    virtual void FormatSequence(const CSequenceItem& seq, IFlatTextOStream& text_os);
    virtual void FormatFeatHeader(const CFeatHeaderItem& fh, IFlatTextOStream& text_os);
    virtual void FormatFeature(const CFeatureItemBase& feat, IFlatTextOStream& text_os);
    virtual void FormatDate(const CDateItem& date, IFlatTextOStream& text_os);

private:
    virtual string& Pad(const string& s, string& out, EPadContext where) const;

    void x_AddXX(IFlatTextOStream& text_os) const;

    // OS, OC, OG
    void x_OrganismSource(list<string>& l, const CSourceItem& source) const;
    void x_OrganisClassification(list<string>& l, const CSourceItem& source) const;
    void x_Organelle(list<string>& l, const CSourceItem& source) const;

    // data
    list<string>    m_XX;
};


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.4  2004/04/22 15:42:24  shomrat
* EndSection() signature changed
*
* Revision 1.3  2004/02/19 17:57:40  shomrat
* removed commented code
*
* Revision 1.2  2004/01/14 15:51:12  shomrat
* const removed
*
* Revision 1.1  2003/12/17 19:51:12  shomrat
* Initial revision (adapted from flat lib)
*
*
* ===========================================================================
*/

#endif  /* OBJTOOLS_FORMAT___EMBL_FORMATTER__HPP */
