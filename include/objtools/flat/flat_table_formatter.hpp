#ifndef OBJECTS_FLAT___FLAT_TABLE_FORMATTER__HPP
#define OBJECTS_FLAT___FLAT_TABLE_FORMATTER__HPP

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
*   new (early 2003) flat-file generator -- 5-column tabular output
*
*/

#include <objtools/flat/flat_text_formatter.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CFlatTableFormatter : public IFlatFormatter
{
public:
    CFlatTableFormatter(IFlatTextOStream& stream, CScope& scope,
                        EStyle style = eStyle_Normal, TFlags flags = 0)
        : IFlatFormatter(scope, eMode_Dump, style, flags), m_Stream(&stream)
        { }

protected: // mostly no-ops
    EDatabase GetDatabase(void) const { return eDB_NCBI; }

    void BeginSequence   (CFlatContext& ctx);
    void FormatHead      (const CFlatHead& head);
    void FormatKeywords  (const CFlatKeywords& keys)   { }
    void FormatSegment   (const CFlatSegment& seg)     { }
    void FormatSource    (const CFlatSource& source)   { }
    void FormatReference (const CFlatReference& ref)   { }
    void FormatComment   (const CFlatComment& comment) { }
    void FormatPrimary   (const CFlatPrimary& prim)    { }
    void FormatFeatHeader(const CFlatFeatHeader& fh)   { }
    void FormatFeature   (const IFlattishFeature& f);
    void FormatDataHeader(const CFlatDataHeader& dh)   { }
    void FormatData      (const CFlatData& data)       { }
    void FormatContig    (const CFlatContig& contig)   { }
    void FormatWGSRange  (const CFlatWGSRange& range)  { }
    void FormatGenomeInfo(const CFlatGenomeInfo& g)    { }
    void EndSequence     (void)                        { }

private:
    CRef<IFlatTextOStream> m_Stream;
};


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.4  2003/10/09 15:53:58  ucko
* Remember to define GetDatabase (-> NCBI)
*
* Revision 1.3  2003/06/02 16:01:39  dicuccio
* Rearranged include/objects/ subtree.  This includes the following shifts:
*     - include/objects/alnmgr --> include/objtools/alnmgr
*     - include/objects/cddalignview --> include/objtools/cddalignview
*     - include/objects/flat --> include/objtools/flat
*     - include/objects/objmgr/ --> include/objmgr/
*     - include/objects/util/ --> include/objmgr/util/
*     - include/objects/validator --> include/objtools/validator
*
* Revision 1.2  2003/04/10 20:08:22  ucko
* Arrange to pass the item as an argument to IFlatTextOStream::AddParagraph
*
* Revision 1.1  2003/03/28 19:04:23  ucko
* Add a formatter for 5-column tabular output.
*
*
* ===========================================================================
*/

#endif  /* OBJECTS_FLAT___FLAT_TABLE_FORMATTER__HPP */
