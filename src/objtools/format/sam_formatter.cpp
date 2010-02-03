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
* Author:  Aaron Ucko, Aleksey Grichenko
*
* File Description:
*   Flat formatter for Sequence Alignment/Map (SAM).
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
/*
#include <objtools/format/items/alignment_item.hpp>
*/
#include <objtools/error_codes.hpp>
#include <objtools/format/flat_expt.hpp>
#include <objtools/format/text_ostream.hpp>
#include <objtools/format/cigar_formatter.hpp>
#include <objtools/format/sam_formatter.hpp>


#define NCBI_USE_ERRCODE_X   Objtools_Fmt_SAM

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CSAM_CIGAR_Formatter : public CCIGAR_Formatter
{
public:
    CSAM_CIGAR_Formatter(const CAlignmentItem& aln,
                         IFlatTextOStream& os);
    virtual ~CSAM_CIGAR_Formatter(void) {}

protected:
    virtual void StartAlignment(void);
    virtual void EndAlignment(void);
    virtual void StartSubAlignment(void) {}
    virtual void EndSubAlignment(void) {}
    virtual void StartRows(void) {}
    virtual void EndRows(void) {}
    virtual void StartRow(void) {}
    virtual void AddRow(const string& cigar);
    virtual void EndRow(void) {}

private:
    enum EReadFlags {
        fRead_Default       = 0x0000,
        fRead_Reverse       = 0x0010  // minus strand
    };
    typedef unsigned int TReadFlags;

    string x_GetRefIdString(void) const;
    string x_GetTargetIdString(void) const;

    typedef list<string> TLines;

    IFlatTextOStream&   m_Out;
    TLines              m_Head;
    TLines              m_Rows;
    CBioseq_Handle      m_RefSeq;
};


CSAM_CIGAR_Formatter::CSAM_CIGAR_Formatter(const CAlignmentItem& aln,
                                           IFlatTextOStream& os)
    : CCIGAR_Formatter(aln),
      m_Out(os)
{
    m_RefSeq = GetScope().GetBioseqHandle(GetRefId());
}


inline
string CSAM_CIGAR_Formatter::x_GetRefIdString(void) const
{
    // ???
    return GetRefId().AsFastaString();
}


inline
string CSAM_CIGAR_Formatter::x_GetTargetIdString(void) const
{
    // ???
    return GetTargetId().AsFastaString();
}


void CSAM_CIGAR_Formatter::StartAlignment(void)
{
    m_Head.push_back("@HD\tVN:1.2\tGO:query");
    // m_Lines.push_back("@PG\tID:" + app_name + "\tVN:" + app_version);
    // ??? Is AsFastaString() good for SAM?
    m_Head.push_back("@SQ\tSN:" + x_GetRefIdString() +
        "\tLN:" + NStr::UInt8ToString(m_RefSeq.GetBioseqLength()));
}


void CSAM_CIGAR_Formatter::EndAlignment(void)
{
    m_Out.AddParagraph(m_Head);
    m_Out.AddParagraph(m_Rows);
    m_Head.clear();
    m_Rows.clear();
}


void CSAM_CIGAR_Formatter::AddRow(const string& cigar)
{
    string id = x_GetTargetIdString();

    TReadFlags flags = fRead_Default;
    if ( GetTargetSign() < 0 ) {
        flags |= fRead_Reverse;
    }

    const TRange& ref_rg = GetRefRange();
    const TRange& tgt_rg = GetTargetRange();
    string clip_front, clip_back;
    if (tgt_rg.GetFrom() > 0) {
        clip_front = NStr::UInt8ToString(tgt_rg.GetFrom()) + "H";
    }

    //string seq_data;
    CBioseq_Handle h = GetScope().GetBioseqHandle(GetTargetId());
    if ( h ) {
        //CSeqVector vect = h.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
        //vect.GetSeqData(tgt_rg.GetFrom(), tgt_rg.GetTo(), seq_data);
        if ( TSeqPos(tgt_rg.GetToOpen()) < h.GetBioseqLength() ) {
            clip_back = NStr::UInt8ToString(
                h.GetBioseqLength() - tgt_rg.GetToOpen()) + "H";
        }
    }
    /*
    else {
        seq_data = string(tgt_rg.GetLength(), 'N'); // ???
    }
    */

    m_Rows.push_back(
        id + "\t" +
        NStr::UIntToString(flags) + "\t" +
        x_GetRefIdString() + "\t" +
        NStr::UInt8ToString(ref_rg.GetFrom() + 1) + "\t" + // position, 1-based
        "255\t" + // ??? mapping quality
        clip_front + cigar + clip_back + "\t" +
        "*\t" + // ??? mate reference sequence
        "0\t" + // mate position, 1-based
        "0\t" + // inferred insert size
        /*seq_data + */ "*\t" +
        "*" // query quality
        );
}


void CSAM_Formatter::Start(IFlatTextOStream& text_os)
{
}


void CSAM_Formatter::End(IFlatTextOStream& text_os)
{
}


void CSAM_Formatter::Format(const IFlatItem& item, IFlatTextOStream& text_os)
{
}


void CSAM_Formatter::FormatAlignment(const CAlignmentItem& aln,
                                     IFlatTextOStream&     text_os)
{
    CSAM_CIGAR_Formatter fmt(aln, text_os);
    fmt.FormatAlignmentRows();
}


END_SCOPE(objects)
END_NCBI_SCOPE
