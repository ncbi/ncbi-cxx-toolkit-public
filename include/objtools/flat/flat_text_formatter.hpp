#ifndef OBJECTS_FLAT___FLAT_TEXT_FORMATTER__HPP
#define OBJECTS_FLAT___FLAT_TEXT_FORMATTER__HPP

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
*   new (early 2003) flat-file generator -- base class for traditional
*   line-oriented formats (GenBank, EMBL, DDBJ but not GBSeq)
*
*/

#include <objtools/flat/flat_formatter.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class IFlatTextOStream : public CObject
{
public:
    virtual void NewSequence (void) { } // switch to a new window/tab in GUIs?
    // NB: item may be null, or appear for multiple paragraphs.
    virtual void AddParagraph(const list<string>&  lines,
                              const IFlatItem*     item = 0,
                              const CSerialObject* topic = 0) = 0;

    virtual SIZE_TYPE GetWidth(void) const { return 79; }
};


class NCBI_FLAT_EXPORT CFlatTextOStream : public IFlatTextOStream
{
public:
    CFlatTextOStream(CNcbiOstream& stream, bool trace_topics = false);
    virtual void AddParagraph(const list<string>&  lines,
                              const IFlatItem*     item = 0,
                              const CSerialObject* topic = 0);

private:
    CNcbiOstream&            m_Stream;
    auto_ptr<CObjectOStream> m_TraceStream;
};


// Standard behavior is GB/DDBJ-ish.
class NCBI_FLAT_EXPORT CFlatTextFormatter : public IFlatFormatter
{
public:
    static CFlatTextFormatter* New(IFlatTextOStream& stream, CScope& scope,
                                   EMode mode, EDatabase db,
                                   EStyle style = eStyle_Normal,
                                   TFlags flags = 0);

protected:
    CFlatTextFormatter(IFlatTextOStream& stream, CScope& scope, EMode mode,
                       EStyle style = eStyle_Normal, TFlags flags = 0)
        : IFlatFormatter(scope, mode, style, flags), m_Stream(&stream),
          m_Indent(12, ' '), m_FeatIndent(21, ' ')
        { }

    void BeginSequence   (CFlatContext& ctx);
    void FormatHead      (const CFlatHead& head);
    void FormatKeywords  (const CFlatKeywords& keys);
    void FormatSegment   (const CFlatSegment& seg);
    void FormatSource    (const CFlatSource& source);
    void FormatReference (const CFlatReference& ref);
    void FormatComment   (const CFlatComment& comment);
    void FormatPrimary   (const CFlatPrimary& prim); // TPAs
    void FormatFeatHeader(const CFlatFeatHeader& fh);
    void FormatFeature   (const IFlattishFeature& f);
    void FormatDataHeader(const CFlatDataHeader& dh);
    void FormatData      (const CFlatData& data);
    // alternatives to DataHeader + Data...
    void FormatContig    (const CFlatContig& contig);
    void FormatWGSRange  (const CFlatWGSRange& range);
    void FormatGenomeInfo(const CFlatGenomeInfo& g); // NS_*
    void EndSequence     (void);

    enum EPadContext {
        ePara,
        eSubp,
        eFeatHead,
        eFeat
    };
    static  string& Pad(const string& s, string& out, SIZE_TYPE width,
                        const string& indent = kEmptyStr);
    virtual string& Pad(const string& s, string& out, EPadContext where);

    CRef<IFlatTextOStream> m_Stream;
    string                 m_Indent, m_FeatIndent;

private:
    list<string>& Wrap(list<string>& l, const string& tag, const string& body,
                       EPadContext where = ePara);
};


///////////////////////////////////////////////////////////////////////////
// INLINE METHODS


inline
string& CFlatTextFormatter::Pad(const string& s, string& out, SIZE_TYPE width,
                                const string& indent)
{
    out.assign(indent);
    out += s;
    out.resize(width, ' ');
    return out;
}


inline
list<string>& CFlatTextFormatter::Wrap(list<string>& l, const string& tag,
                                       const string& body, EPadContext where)
{
    NStr::TWrapFlags flags = DoHTML() ? NStr::fWrap_HTMLPre : 0;
    string tag2;
    Pad(tag, tag2, where);
    NStr::Wrap(body, m_Stream->GetWidth(), l, flags,
               where == eFeat ? m_FeatIndent : m_Indent, tag2);
    return l;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.6  2003/10/09 17:01:49  dicuccio
* Added export specifiers
*
* Revision 1.5  2003/06/02 16:01:39  dicuccio
* Rearranged include/objects/ subtree.  This includes the following shifts:
*     - include/objects/alnmgr --> include/objtools/alnmgr
*     - include/objects/cddalignview --> include/objtools/cddalignview
*     - include/objects/flat --> include/objtools/flat
*     - include/objects/objmgr/ --> include/objmgr/
*     - include/objects/util/ --> include/objmgr/util/
*     - include/objects/validator --> include/objtools/validator
*
* Revision 1.4  2003/04/10 20:08:22  ucko
* Arrange to pass the item as an argument to IFlatTextOStream::AddParagraph
*
* Revision 1.3  2003/03/28 17:45:36  dicuccio
* Added (very judicious) use of Win32 exports - only needed in external classes
* CFlatTextFormatter and IFlatFormatter
*
* Revision 1.2  2003/03/21 18:47:47  ucko
* Turn most structs into (accessor-requiring) classes; replace some
* formerly copied fields with pointers to the original data.
*
* Revision 1.1  2003/03/10 16:39:08  ucko
* Initial check-in of new flat-file generator
*
*
* ===========================================================================
*/

#endif  /* OBJECTS_FLAT___FLAT_TEXT_FORMATTER__HPP */
