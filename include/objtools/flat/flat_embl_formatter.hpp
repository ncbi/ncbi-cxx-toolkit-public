#ifndef OBJECTS_FLAT___FLAT_EMBL_FORMATTER__HPP
#define OBJECTS_FLAT___FLAT_EMBL_FORMATTER__HPP

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
*   new (early 2003) flat-file generator -- EMBL(PEPT) output class
*
*/

#include <objects/flat/flat_text_formatter.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CFlatEMBLFormatter : public CFlatTextFormatter
{
public:
    typedef CFlatTextFormatter TParent;

    CFlatEMBLFormatter(IFlatTextOStream& stream, CScope& scope, EMode mode,
                       EStyle style = eStyle_Normal, TFlags flags = 0)
        : TParent(stream, scope, mode, style, flags)
        { Pad(kEmptyStr, m_FeatIndent, eFeat); }

protected:    
    EDatabase GetDatabase(void) const { return eDB_EMBL; }

    void FormatHead      (const CFlatHead& head);
    void FormatKeywords  (const CFlatKeywords& keys);
    void FormatSegment   (const CFlatSegment& seg);
    void FormatSource    (const CFlatSource& source);
    void FormatReference (const CFlatReference& ref);
    void FormatComment   (const CFlatComment& comment);
    void FormatPrimary   (const CFlatPrimary& prim); // TPAs
    void FormatFeatHeader(void);
    // void FormatFeature   (const CFlatFeature& feat);
    void FormatDataHeader(const CFlatDataHeader& dh);
    void FormatData      (const CFlatData& data);
    // alternatives to DataHeader + Data...
    void FormatContig    (const CFlatContig& contig);
    void FormatWGSRange  (const CFlatWGSRange& range);
    void FormatGenomeInfo(const CFlatGenomeInfo& g); // NS_*

    string& Pad(const string& s, string& out, EPadContext where);

private:
    list<string>& Wrap(list<string>& l, const string& tag, const string& body,
                       EPadContext where = ePara);
    void x_AddXX(void);
};


inline
list<string>& CFlatEMBLFormatter::Wrap(list<string>& l, const string& tag,
                                       const string& body, EPadContext where)
{
    NStr::TWrapFlags flags = DoHTML() ? NStr::fWrap_HTMLPre : 0;
    string tag2;
    Pad(tag, tag2, where);
    NStr::Wrap(body, m_Stream->GetWidth(), l, flags, tag2);
    return l;
}


inline
void CFlatEMBLFormatter::x_AddXX(void)
{
    static list<string> l;
    if (l.empty()) {
        string tmp;
        l.push_back(Pad("XX", tmp, ePara));
    }
    m_Stream->AddParagraph(l);
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
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

#endif  /* OBJECTS_FLAT___FLAT_EMBL_FORMATTER__HPP */
