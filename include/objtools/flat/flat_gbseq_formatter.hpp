#ifndef OBJECTS_FLAT___FLAT_GBSEQ_FORMATTER__HPP
#define OBJECTS_FLAT___FLAT_GBSEQ_FORMATTER__HPP

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
*
* File Description:
*   new (early 2003) flat-file generator -- GBSeq output
*
*/

#include <objects/flat/flat_formatter.hpp>

#include <serial/objostr.hpp>

#include <objects/gbseq/GBSeq.hpp>
#include <objects/gbseq/GBSet.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CFlatGBSeqFormatter : public IFlatFormatter
{
public:
    // populates a GBSet, but does not send it anywhere
    CFlatGBSeqFormatter(CScope& scope, EMode mode,
                        EStyle style = eStyle_Normal, TFlags flags = 0)
        : IFlatFormatter(scope, mode, style, flags), m_Set(new CGBSet),
          m_Out(0) { }

    // for automatic seq-by-seq output
    CFlatGBSeqFormatter(CObjectOStream& out, CScope& scope, EMode mode,
                        EStyle style = eStyle_Normal, TFlags flags = 0);
    ~CFlatGBSeqFormatter();

    const CGBSet& GetGBSet(void) const { return *m_Set; }

protected:
    EDatabase GetDatabase(void) const { return eDB_NCBI; }

    void BeginSequence   (SFlatContext& context);
    void FormatHead      (const SFlatHead& head);
    void FormatKeywords  (const SFlatKeywords& keys);
    void FormatSegment   (const SFlatSegment& seg);
    void FormatSource    (const SFlatSource& source);
    void FormatReference (const SFlatReference& ref);
    void FormatComment   (const SFlatComment& comment);
    void FormatPrimary   (const SFlatPrimary& prim); // TPAs
    void FormatFeatHeader(void) { }
    void FormatFeature   (const SFlatFeature& feat);
    void FormatDataHeader(const SFlatDataHeader& /* dh */) { }
    void FormatData      (const SFlatData& data);
    // alternatives to DataHeader + Data...
    void FormatContig    (const SFlatContig& contig);
    // no (current) representation for these two:
    void FormatWGSRange  (const SFlatWGSRange& /* range */) { }
    void FormatGenomeInfo(const SFlatGenomeInfo& /* g */) { } // NS_*
    void EndSequence     (void);

private:
    CRef<CGBSet> m_Set;
    CRef<CGBSeq> m_Seq;
    CObjectOStream* m_Out;
};


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.1  2003/03/10 16:39:08  ucko
* Initial check-in of new flat-file generator
*
*
* ===========================================================================
*/

#endif  /* OBJECTS_FLAT___FLAT_GBSEQ_FORMATTER__HPP */
