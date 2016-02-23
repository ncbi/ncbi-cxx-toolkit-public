#ifndef GPIPE_COMMON___TABULAR_FORMAT__HPP
#define GPIPE_COMMON___TABULAR_FORMAT__HPP

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
 * Authors:  Mike DiCuccio, Wratko Hlavina, Eyal Mozes
 *
 * File Description:
 *   Sample for the command-line arguments' processing ("ncbiargs.[ch]pp"):
 *
 */

#include <objmgr/util/sequence.hpp>
#include <objmgr/util/create_defline.hpp>

#include <objects/genomecoll/genome_collection__.hpp>
#include <objects/seqalign/Seq_align.hpp>

#include <algo/align/util/score_lookup.hpp>

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
///
/// Simple tabular alignment formatter
/// This is a replacement for the BLAST tabular formatter that supports a set
/// of custom gpipe extensions and also provides for better performance
///

class CTabularFormatter
{
public:
    class IFormatter : public CObject
    {
    public:
        virtual ~IFormatter() {}
        void SetScoreLookup(objects::CScoreLookup *scores)
            { m_Scores = scores; }
        virtual void SetGencoll(CConstRef<objects::CGC_Assembly> gencoll)
            {}
        virtual void PrintHelpText(CNcbiOstream& ostr) const = 0;
        virtual void PrintHeader(CNcbiOstream& ostr) const = 0;
        virtual void Print(CNcbiOstream& ostr,
                           const objects::CSeq_align& align) = 0;

    protected:
        objects::CScoreLookup *m_Scores;
    };

    typedef map< string, CIRef<IFormatter> > TFormatterMap;

    CTabularFormatter(CNcbiOstream& ostr, objects::CScoreLookup &scores);
    void SetFormat(const string& format);

    void SetGencoll(CConstRef<objects::CGC_Assembly> gencoll);

    void RegisterField(const string &field_name, IFormatter* field_formatter)
    {
        m_FormatterMap[field_name] = CIRef<IFormatter>(field_formatter);
    }

    void WriteHeader();
    void Format(const objects::CSeq_align& align);

    const TFormatterMap &AvailableFormatters() const
    { return m_FormatterMap; }

    const list< CIRef<IFormatter> > &Formatters() const
    { return m_Formatters; }

private:
    list< CIRef<IFormatter> > m_Formatters;
    objects::CScoreLookup *m_Scores;
    CNcbiOstream &m_Ostr;
    TFormatterMap m_FormatterMap;

    static void s_RegisterStandardFields(CTabularFormatter &formatter);
};


/////////////////////////////////////////////////////////////////////////////


class CTabularFormatter_AllSeqIds : public CTabularFormatter::IFormatter
{
public:
    CTabularFormatter_AllSeqIds(int row);
    void PrintHelpText(CNcbiOstream& ostr) const;
    void PrintHeader(CNcbiOstream& ostr) const;
    void Print(CNcbiOstream& ostr,
               const objects::CSeq_align& align);

private:
    int m_Row;
};

/////////////////////////////////////////////////////////////////////////////

class CTabularFormatter_SeqId : public CTabularFormatter::IFormatter
{
public:
    CTabularFormatter_SeqId(int row, objects::sequence::EGetIdType id_type);
    void PrintHelpText(CNcbiOstream& ostr) const;
    void PrintHeader(CNcbiOstream& ostr) const;
    void Print(CNcbiOstream& ostr,
               const objects::CSeq_align& align);

private:
    int m_Row;
    objects::sequence::EGetIdType m_GetIdType;
};

/////////////////////////////////////////////////////////////////////////////

class CTabularFormatter_AlignStart : public CTabularFormatter::IFormatter
{
public:
    CTabularFormatter_AlignStart(int row, bool nominus=false);
    void PrintHelpText(CNcbiOstream& ostr) const;
    void PrintHeader(CNcbiOstream& ostr) const;
    void Print(CNcbiOstream& ostr,
               const objects::CSeq_align& align);

private:
    int m_Row;
    bool m_NoMinus;
};

/////////////////////////////////////////////////////////////////////////////

class CTabularFormatter_AlignEnd : public CTabularFormatter::IFormatter
{
public:
    CTabularFormatter_AlignEnd(int row, bool nominus=false);
    void PrintHelpText(CNcbiOstream& ostr) const;
    void PrintHeader(CNcbiOstream& ostr) const;
    void Print(CNcbiOstream& ostr,
               const objects::CSeq_align& align);

private:
    int m_Row;
    bool m_NoMinus;
};

/////////////////////////////////////////////////////////////////////////////

class CTabularFormatter_AlignStrand : public CTabularFormatter::IFormatter
{
public:
    CTabularFormatter_AlignStrand(int row);
    void PrintHelpText(CNcbiOstream& ostr) const;
    void PrintHeader(CNcbiOstream& ostr) const;
    void Print(CNcbiOstream& ostr,
               const objects::CSeq_align& align);

private:
    int m_Row;
};

/////////////////////////////////////////////////////////////////////////////

class CTabularFormatter_SeqLength: public CTabularFormatter::IFormatter
{
public:
    CTabularFormatter_SeqLength(int row);
    void PrintHelpText(CNcbiOstream& ostr) const;
    void PrintHeader(CNcbiOstream& ostr) const;
    void Print(CNcbiOstream& ostr,
               const objects::CSeq_align& align);

private:
    int m_Row;
};

/////////////////////////////////////////////////////////////////////////////

class CTabularFormatter_AlignLength: public CTabularFormatter::IFormatter
{
public:
    void PrintHelpText(CNcbiOstream& ostr) const;
    void PrintHeader(CNcbiOstream& ostr) const;
    void Print(CNcbiOstream& ostr,
               const objects::CSeq_align& align);
};

/////////////////////////////////////////////////////////////////////////////

class CTabularFormatter_AlignLengthUngap: public CTabularFormatter::IFormatter
{
public:
    void PrintHelpText(CNcbiOstream& ostr) const;
    void PrintHeader(CNcbiOstream& ostr) const;
    void Print(CNcbiOstream& ostr,
               const objects::CSeq_align& align);
};

/////////////////////////////////////////////////////////////////////////////


class CTabularFormatter_PercentId : public CTabularFormatter::IFormatter
{
public:
    CTabularFormatter_PercentId(bool gapped = false);
    void PrintHelpText(CNcbiOstream& ostr) const;
    void PrintHeader(CNcbiOstream& ostr) const;
    void Print(CNcbiOstream& ostr,
               const objects::CSeq_align& align);

private:
    bool m_Gapped;
};

/////////////////////////////////////////////////////////////////////////////

class CTabularFormatter_PercentCoverage : public CTabularFormatter::IFormatter
{
public:
    void PrintHelpText(CNcbiOstream& ostr) const;
    void PrintHeader(CNcbiOstream& ostr) const;
    void Print(CNcbiOstream& ostr,
               const objects::CSeq_align& align);
};

/////////////////////////////////////////////////////////////////////////////

class CTabularFormatter_GapCount : public CTabularFormatter::IFormatter
{
public:
    void PrintHelpText(CNcbiOstream& ostr) const;
    void PrintHeader(CNcbiOstream& ostr) const;
    void Print(CNcbiOstream& ostr,
               const objects::CSeq_align& align);
};

/////////////////////////////////////////////////////////////////////////////

class CTabularFormatter_IdentityCount : public CTabularFormatter::IFormatter
{
public:
    void PrintHelpText(CNcbiOstream& ostr) const;
    void PrintHeader(CNcbiOstream& ostr) const;
    void Print(CNcbiOstream& ostr,
               const objects::CSeq_align& align);
};

/////////////////////////////////////////////////////////////////////////////

class CTabularFormatter_MismatchCount : public CTabularFormatter::IFormatter
{
public:
    void PrintHelpText(CNcbiOstream& ostr) const;
    void PrintHeader(CNcbiOstream& ostr) const;
    void Print(CNcbiOstream& ostr,
               const objects::CSeq_align& align);
};

/////////////////////////////////////////////////////////////////////////////

class CTabularFormatter_MismatchPositions : public CTabularFormatter::IFormatter
{
public:
    CTabularFormatter_MismatchPositions(int row);
    void PrintHelpText(CNcbiOstream& ostr) const;
    void PrintHeader(CNcbiOstream& ostr) const;
    void Print(CNcbiOstream& ostr,
               const objects::CSeq_align& align);
private:
    int m_Row;
};

/////////////////////////////////////////////////////////////////////////////

class CTabularFormatter_GapRanges : public CTabularFormatter::IFormatter
{
public:
    CTabularFormatter_GapRanges(int row);
    void PrintHelpText(CNcbiOstream& ostr) const;
    void PrintHeader(CNcbiOstream& ostr) const;
    void Print(CNcbiOstream& ostr,
               const objects::CSeq_align& align);
private:
    int m_Row;
};

/////////////////////////////////////////////////////////////////////////////

class CTabularFormatter_GapBaseCount : public CTabularFormatter::IFormatter
{
public:
    void PrintHelpText(CNcbiOstream& ostr) const;
    void PrintHeader(CNcbiOstream& ostr) const;
    void Print(CNcbiOstream& ostr,
               const objects::CSeq_align& align);
};

/////////////////////////////////////////////////////////////////////////////

class CTabularFormatter_EValue : public CTabularFormatter::IFormatter
{
public:
    void PrintHelpText(CNcbiOstream& ostr) const;
    void PrintHeader(CNcbiOstream& ostr) const;
    void Print(CNcbiOstream& ostr,
               const objects::CSeq_align& align);
};

/////////////////////////////////////////////////////////////////////////////

class CTabularFormatter_EValue_Mantissa : public CTabularFormatter::IFormatter
{
public:
    void PrintHelpText(CNcbiOstream& ostr) const;
    void PrintHeader(CNcbiOstream& ostr) const;
    void Print(CNcbiOstream& ostr,
               const objects::CSeq_align& align);
};

/////////////////////////////////////////////////////////////////////////////

class CTabularFormatter_EValue_Exponent : public CTabularFormatter::IFormatter
{
public:
    void PrintHelpText(CNcbiOstream& ostr) const;
    void PrintHeader(CNcbiOstream& ostr) const;
    void Print(CNcbiOstream& ostr,
               const objects::CSeq_align& align);
};

/////////////////////////////////////////////////////////////////////////////

class CTabularFormatter_BitScore : public CTabularFormatter::IFormatter
{
public:
    void PrintHelpText(CNcbiOstream& ostr) const;
    void PrintHeader(CNcbiOstream& ostr) const;
    void Print(CNcbiOstream& ostr,
           const objects::CSeq_align& align);
};

/////////////////////////////////////////////////////////////////////////////

class CTabularFormatter_Score : public CTabularFormatter::IFormatter
{
public:
    void PrintHelpText(CNcbiOstream& ostr) const;
    void PrintHeader(CNcbiOstream& ostr) const;
    void Print(CNcbiOstream& ostr,
               const objects::CSeq_align& align);
};

/////////////////////////////////////////////////////////////////////////////

/// formatter for dumping any score in an alignment
class CTabularFormatter_AnyScore : public CTabularFormatter::IFormatter
{
public:
    CTabularFormatter_AnyScore(const string& score_name,
                               const string& col_name);
    void PrintHelpText(CNcbiOstream& ostr) const;
    void PrintHeader(CNcbiOstream& ostr) const;
    void Print(CNcbiOstream& ostr,
               const objects::CSeq_align& align);

private:
    string m_ScoreName;
    string m_ColName;
};


/////////////////////////////////////////////////////////////////////////////
///
/// formatter for dumping alignment identifiers
class CTabularFormatter_AlignId : public CTabularFormatter::IFormatter
{
public:
    void PrintHelpText(CNcbiOstream& ostr) const;
    void PrintHeader(CNcbiOstream& ostr) const;
    void Print(CNcbiOstream& ostr,
               const objects::CSeq_align& align);
};


/////////////////////////////////////////////////////////////////////////////
///
/// formatter for dumping alignment identifiers
class CTabularFormatter_BestPlacementGroup : public CTabularFormatter::IFormatter
{
public:
    void PrintHelpText(CNcbiOstream& ostr) const;
    void PrintHeader(CNcbiOstream& ostr) const;
    void Print(CNcbiOstream& ostr,
               const objects::CSeq_align& align);
};


/////////////////////////////////////////////////////////////////////////////
///
/// formatter for dumping sequence deflines
class CTabularFormatter_Defline : public CTabularFormatter::IFormatter
{
public:
    CTabularFormatter_Defline(int row);
    void PrintHelpText(CNcbiOstream& ostr) const;
    void PrintHeader(CNcbiOstream& ostr) const;
    void Print(CNcbiOstream& ostr,
               const objects::CSeq_align& align);

private:
    int m_Row;
    objects::sequence::CDeflineGenerator generator;
};


/////////////////////////////////////////////////////////////////////////////
///
/// formatter for dumping sequence Prot-refs (protein only)
class CTabularFormatter_ProtRef : public CTabularFormatter::IFormatter
{
public:
    CTabularFormatter_ProtRef(int row);
    void PrintHelpText(CNcbiOstream& ostr) const;
    void PrintHeader(CNcbiOstream& ostr) const;
    void Print(CNcbiOstream& ostr,
               const objects::CSeq_align& align);

private:
    int m_Row;
};


/////////////////////////////////////////////////////////////////////////////
///
/// formatter for dumping tax-ids
class CTabularFormatter_TaxId : public CTabularFormatter::IFormatter
{
public:
    CTabularFormatter_TaxId(int row);
    void PrintHelpText(CNcbiOstream& ostr) const;
    void PrintHeader(CNcbiOstream& ostr) const;
    void Print(CNcbiOstream& ostr,
               const objects::CSeq_align& align);

private:
    int m_Row;
};

/////////////////////////////////////////////////////////////////////////////
///
/// formatter for dumping exons
class CTabularFormatter_ExonIntrons : public CTabularFormatter::IFormatter
{
public:
    enum EIntervalType { e_Exons, e_Introns };
    enum EInfoType { e_Range, e_Length };
    CTabularFormatter_ExonIntrons(unsigned sequence, EIntervalType interval,
                                  EInfoType info)
    : m_Sequence(sequence)
    , m_Interval(interval)
    , m_Info(info)
    {}

    void PrintHelpText(CNcbiOstream& ostr) const;
    void PrintHeader(CNcbiOstream& ostr) const;
    void Print(CNcbiOstream& ostr,
               const objects::CSeq_align& align);

private:
    unsigned m_Sequence;
    EIntervalType m_Interval;
    EInfoType m_Info;
};

/////////////////////////////////////////////////////////////////////////////
// Size, in bp, of the largest gap in the Seq-align
class CTabularFormatter_BiggestGapBases : public CTabularFormatter::IFormatter
{
public:
    enum ERow { e_All = -1 };
    CTabularFormatter_BiggestGapBases(int row);
    void PrintHelpText(CNcbiOstream& ostr) const;
    void PrintHeader(CNcbiOstream& ostr) const;
    void Print(CNcbiOstream& ostr,
               const objects::CSeq_align& align);
private:
    TSeqPos x_CalcBiggestGap(const objects::CSeq_align& align);
    int m_Row;
};

/////////////////////////////////////////////////////////////////////////////
// Bioseq.descr.source.subtype.chromosome/name
class CTabularFormatter_SeqChrom : public CTabularFormatter::IFormatter
{
public:
    CTabularFormatter_SeqChrom(int row);
    void PrintHelpText(CNcbiOstream& ostr) const;
    void PrintHeader(CNcbiOstream& ostr) const;
    void Print(CNcbiOstream& ostr,
               const objects::CSeq_align& align);
private:
    int m_Row;
};

/////////////////////////////////////////////////////////////////////////////
// Bioseq.descr.source.subtype.clone/name
class CTabularFormatter_SeqClone : public CTabularFormatter::IFormatter
{
public:
    CTabularFormatter_SeqClone(int row);
    void PrintHelpText(CNcbiOstream& ostr) const;
    void PrintHeader(CNcbiOstream& ostr) const;
    void Print(CNcbiOstream& ostr,
               const objects::CSeq_align& align);
private:
    int m_Row;
};

/////////////////////////////////////////////////////////////////////////////
// Bioseq.descr.molinfo.tech
class CTabularFormatter_Tech : public CTabularFormatter::IFormatter
{
public:
    CTabularFormatter_Tech(int row);
    void PrintHelpText(CNcbiOstream& ostr) const;
    void PrintHeader(CNcbiOstream& ostr) const;
    void Print(CNcbiOstream& ostr,
               const objects::CSeq_align& align);
private:
    int m_Row;
};

/////////////////////////////////////////////////////////////////////////////

class CTabularFormatter_DiscStrand : public CTabularFormatter::IFormatter
{
public:
    CTabularFormatter_DiscStrand(int row);
    void PrintHelpText(CNcbiOstream& ostr) const;
    void PrintHeader(CNcbiOstream& ostr) const;
    void Print(CNcbiOstream& ostr,
               const objects::CSeq_align& align);

private:
    int m_Row;
    void x_RecurseStrands(const objects::CSeq_align& align, bool& Plus, bool& Minus);
};

/////////////////////////////////////////////////////////////////////////////

class CTabularFormatter_FixedText : public CTabularFormatter::IFormatter
{
public:
    CTabularFormatter_FixedText(const string& col_name, const string& text);
    void PrintHelpText(CNcbiOstream& ostr) const;
    void PrintHeader(CNcbiOstream& ostr) const;
    void Print(CNcbiOstream& ostr,
               const objects::CSeq_align& align);

private:
    string m_ColName;
    string m_Text;
};

/////////////////////////////////////////////////////////////////////////////

class CTabularFormatter_AlignLengthRatio : public CTabularFormatter::IFormatter
{
public:
    void PrintHelpText(CNcbiOstream& ostr) const;
    void PrintHeader(CNcbiOstream& ostr) const;
    void Print(CNcbiOstream& ostr,
               const objects::CSeq_align& align);
};


/////////////////////////////////////////////////////////////////////////////
/// formatter for dumping cigar of alignments
class CTabularFormatter_Cigar : public CTabularFormatter::IFormatter
{
public:
    CTabularFormatter_Cigar();
    void PrintHelpText(CNcbiOstream& ostr) const;
    void PrintHeader(CNcbiOstream& ostr) const;
    void Print(CNcbiOstream& ostr,
               const objects::CSeq_align& align);

private:

};


/////////////////////////////////////////////////////////////////////////////
// CGC_AssemblyUnit.desc.name
class CTabularFormatter_AssemblyInfo : public CTabularFormatter::IFormatter
{
public:
    enum EAssemblyType { eFull, eUnit };
    enum EInfo { eName, eAccession, eChainId, eChromosome };

    CTabularFormatter_AssemblyInfo(int row, EAssemblyType type, EInfo info);

    virtual void SetGencoll(CConstRef<objects::CGC_Assembly> gencoll);

    void PrintHelpText(CNcbiOstream& ostr) const;
    void PrintHeader(CNcbiOstream& ostr) const;
    void Print(CNcbiOstream& ostr,
               const objects::CSeq_align& align);
private:
    int m_Row;
    EAssemblyType m_Type;
    EInfo m_Info;
    CConstRef<objects::CGC_Assembly> m_Gencoll;
};

/////////////////////////////////////////////////////////////////////////////
// CGC_Sequence.patch_type
class CTabularFormatter_PatchType : public CTabularFormatter::IFormatter
{
public:
    CTabularFormatter_PatchType(int row, CConstRef<objects::CGC_Assembly> gencoll);
    void PrintHelpText(CNcbiOstream& ostr) const;
    void PrintHeader(CNcbiOstream& ostr) const;
    void Print(CNcbiOstream& ostr,
               const objects::CSeq_align& align);
private:
    int m_Row;
    CConstRef<objects::CGC_Assembly> m_Gencoll;
};

/////////////////////////////////////////////////////////////////////////////
// CGC_Sequence.patch_type
class CTabularFormatter_NearestGap : public CTabularFormatter::IFormatter
{
public:
    CTabularFormatter_NearestGap(int row, CConstRef<objects::CGC_Assembly> gencoll);
    void PrintHelpText(CNcbiOstream& ostr) const;
    void PrintHeader(CNcbiOstream& ostr) const;
    void Print(CNcbiOstream& ostr,
               const objects::CSeq_align& align);
private:
    int m_Row;
    CConstRef<objects::CGC_Assembly> m_Gencoll;
};




class CTabularFormatter_Traceback : public CTabularFormatter::IFormatter
{
public:
    CTabularFormatter_Traceback();
    void PrintHelpText(CNcbiOstream& ostr) const;
    void PrintHeader(CNcbiOstream& ostr) const;
    void Print(CNcbiOstream& ostr,
               const objects::CSeq_align& align);

private:

};


/////////////////////////////////////////////////////////////////////////////

END_NCBI_SCOPE


#endif  // GPIPE_COMMON___TABULAR_FORMAT__HPP
