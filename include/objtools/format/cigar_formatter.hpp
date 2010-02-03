#ifndef OBJTOOLS_FORMAT___CIGAR_FORMATTER__HPP
#define OBJTOOLS_FORMAT___CIGAR_FORMATTER__HPP

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
 * Authors:  Aaron Ucko, Aleksey Grichenko
 *
 * File Description:
 *   Base class for CIGAR formatters.
 *
 */

/// @file cigar_formatter.hpp
/// Base class for CIGAR formatters.


#include <objtools/alnmgr/alnmap.hpp>
#include <objtools/format/items/alignment_item.hpp>
#include <objtools/format/flat_file_config.hpp>
#include <objtools/format/context.hpp>


/** @addtogroup Miscellaneous
 *
 * @{
 */


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/// The base class for alignment formatters which use CIGAR format
class NCBI_FORMAT_EXPORT CCIGAR_Formatter
{
public:
    CCIGAR_Formatter(const CAlignmentItem& aln);
    virtual ~CCIGAR_Formatter(void);

    void FormatAlignmentRows(void);

protected:
    typedef CAlnMap::TNumrow      TNumrow;
    typedef CAlnMap::TSignedRange TRange;

    virtual void StartAlignment(void) {}
    virtual void EndAlignment(void) {}
    virtual void StartSubAlignment(void) {}
    virtual void EndSubAlignment(void) {}
    virtual void StartRows(void) {}
    virtual void EndRows(void) {}
    virtual void StartRow(void) {}
    virtual void AddRow(const string& cigar) {}
    virtual void EndRow(void) {}
    virtual void AddSegment(CNcbiOstream& cigar,
                            char seg_type,
                            TSeqPos seg_len);

    const CAlignmentItem& GetAlignmentItem(void) const;
    const CSeq_align& GetSeq_align(void) const;
    CBioseqContext& GetContext(void) const;
    CScope& GetScope(void) const;
    const CFlatFileConfig& GetConfig(void) const;

    bool IsFirstSubalign(void) const { return m_IsFirstSubalign; }
    bool IsTrivial(void) const { return m_IsTrivial; }
    char GetLastType(void) const { return m_LastType; }
    int GetFrame(void) const { return m_Frame; }
    const CDense_seg& GetDense_seg(void) const { return *m_DenseSeg; }
    const CAlnMap& GetAlnMap(void) const { return *m_AlnMap; }

    TNumrow GetRefRow(void) const { return m_RefRow; }
    const CSeq_id& GetRefId(void) const { return *m_RefId; }
    const TRange& GetRefRange(void) const { return m_RefRange; }
    int GetRefSign(void) const { return m_RefSign; }
    TSeqPos GetRefWidth(void) const { return m_RefWidth; }

    TNumrow GetTargetRow(void) const { return m_TargetRow; }
    const CSeq_id& GetTargetId(void) const { return *m_TargetId; }
    const TRange& GetTargetRange(void) const { return m_TargetRange; }
    int GetTargetSign(void) const { return m_TargetSign; }
    TSeqPos GetTargetWidth(void) const { return m_TargetWidth; }

private:
    void x_FormatAlignmentRows(const CSeq_align& sa,
                               bool              width_inverted);
    void x_FormatDensegRows(const CDense_seg& ds,
                            bool              width_inverted);

    const CAlignmentItem& m_Alignment;
    CConstRef<CDense_seg> m_DenseSeg;
    CRef<CAlnMap>         m_AlnMap;

    bool                m_IsFirstSubalign;
    bool                m_IsTrivial;
    char                m_LastType;
    int                 m_Frame;
    TNumrow             m_RefRow;
    CConstRef<CSeq_id>  m_RefId;
    TRange              m_RefRange;
    int                 m_RefSign;
    TSeqPos             m_RefWidth;
    TNumrow             m_TargetRow;
    CConstRef<CSeq_id>  m_TargetId;
    TRange              m_TargetRange;
    int                 m_TargetSign;
    TSeqPos             m_TargetWidth;
};


inline
const CAlignmentItem& CCIGAR_Formatter::GetAlignmentItem(void) const
{
    return m_Alignment;
}


inline
const CSeq_align& CCIGAR_Formatter::GetSeq_align(void) const
{
    return GetAlignmentItem().GetAlign();
}


inline
CBioseqContext& CCIGAR_Formatter::GetContext(void) const
{
    return *GetAlignmentItem().GetContext();
}


inline
CScope& CCIGAR_Formatter::GetScope(void) const
{
    return GetContext().GetScope();
}


inline
const CFlatFileConfig& CCIGAR_Formatter::GetConfig(void) const
{
    return GetContext().Config();
}


END_SCOPE(objects)
END_NCBI_SCOPE


/* @} */

#endif  /* OBJTOOLS_FORMAT___CIGAR_FORMATTER__HPP */
