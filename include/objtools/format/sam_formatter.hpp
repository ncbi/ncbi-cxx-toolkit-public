#ifndef OBJTOOLS_FORMAT___SAM_FORMATTER__HPP
#define OBJTOOLS_FORMAT___SAM_FORMATTER__HPP

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
 */

/// @file sam_formatter.hpp
/// Flat formatter for Sequence Alignment/Map (SAM).


#include <corelib/ncbistre.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objmgr/scope.hpp>
#include <list>


/** @addtogroup Miscellaneous
 *
 * @{
 */


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class NCBI_FORMAT_EXPORT CSAM_Formatter
{
public:
    enum EFlags {
        fSAM_AlignmentScore     = 1 << 0,  ///< Generate AS tags
        fSAM_ExpectationValue   = 1 << 1,  ///< Generate EV tags
        fSAM_NumNucDiff         = 1 << 2,  ///< Generate NM tags
        fSAM_PercentageIdentity = 1 << 3,  ///< Generate PI tags
        fSAM_BitScore           = 1 << 4,  ///< Generate BS tags
        fSAM_PlainSeqIds        = 1 << 5,  ///< Drop type prefix in seq-ids

        //? Include all tags by default
        fSAM_Default            = fSAM_AlignmentScore     |
                                  fSAM_ExpectationValue   |
                                  fSAM_NumNucDiff         |
                                  fSAM_PercentageIdentity |
                                  fSAM_BitScore
    };
    typedef int TFlags;  ///< bitwise OR of EFlags

    CSAM_Formatter(CNcbiOstream& out,
                   CScope& scope,
                   TFlags flags = fSAM_Default);
    ~CSAM_Formatter(void);

    void SetOutputStream(CNcbiOstream& out) { m_Out = &out; }
    void SetScope(CScope& scope) { m_Scope.Reset(&scope); }
    void SetFlags (TFlags flags) { m_Flags = flags; }
    void SetFlag  (EFlags flag)  { m_Flags |= flag; }
    void UnsetFlag(EFlags flag)  { m_Flags &= ~flag; }

    /// @PG tag fields
    struct SProgramInfo {
        SProgramInfo(const string& id = kEmptyStr,
                     const string& ver = kEmptyStr,
                     const string& cmd = kEmptyStr)
                     : m_Id(id), m_Version(ver), m_CmdLine(cmd) {}

        string m_Id;         ///< ID - program id
        string m_Version;    ///< VN - version
        string m_CmdLine;    ///< CL - command line
        string m_Desc;       ///< DS - description
        string m_Name;       ///< PN - program name
    };

    void SetProgram(const SProgramInfo& pg) { m_ProgramInfo = pg; }
    SProgramInfo& SetProgram(void) { return m_ProgramInfo; }
    const SProgramInfo& GetProgram(void) const { return m_ProgramInfo; }

    enum ESortOrder {
        eSO_Skip,            ///< Do not print SO tag (default)
        eSO_Unsorted,        ///< SO:unsorted
        eSO_QueryName,       ///< SO:queryname
        eSO_Coordinate,      ///< SO:coordinate
        eSO_User             ///< User-provided string
    };

    /// Set SO tag value.
    /// @param so
    ///   Predefined SO tag value.
    /// @param so_value
    ///   User-defined SO tag value. Used only if 'so' is set to eSO_User.
    void SetSortOrder(ESortOrder so, const string& so_value = kEmptyStr)
    {
        m_SO = so;
        m_SO_Value = so_value;
    }

    enum EGroupOrder {
        eGO_Skip,            ///< Do not print GO tag
        eGO_None,            ///< GO:none
        eGO_Query,           ///< GO:query (default)
        eGO_Reference,       ///< GO:reference
        eGO_User             ///< User-provided string
    };

    /// Set GO tag value.
    /// @param go
    ///   Predefined GO tag value.
    /// @param go_value
    ///   User-defined GO tag value. Used only if 'go' is set to eGO_User.
    void SetGroupOrder(EGroupOrder go, const string& go_value = kEmptyStr)
    {
        m_GO = go;
        m_GO_Value = go_value;
    }

    CSAM_Formatter& Print(const CSeq_align&     aln,
                          const CSeq_id&        query_id);
    CSAM_Formatter& Print(const CSeq_align&     aln,
                          CSeq_align::TDim      query_row);
    CSAM_Formatter& Print(const CSeq_align_set& aln,
                          const CSeq_id&        query_id);
    CSAM_Formatter& Print(const CSeq_align_set& aln,
                          CSeq_align::TDim      query_row);

    // Write all data to the output, start a new file.
    void Flush(void);

private:
    CNcbiOstream* m_Out;
    CRef<CScope>  m_Scope;
    TFlags        m_Flags;
    SProgramInfo  m_ProgramInfo;
    ESortOrder    m_SO;
    string        m_SO_Value;
    EGroupOrder   m_GO;
    string        m_GO_Value;

    void x_PrintSOTag(void) const;
    void x_PrintGOTag(void) const;

    friend class CSAM_CIGAR_Formatter;

    typedef list<string> TLines;

    class CSAM_Headers
    {
    public:
        CSAM_Headers(void) {}
        void AddSequence(CSeq_id_Handle id, const string& line);
        typedef list<pair<CSeq_id_Handle, string> > TData;
        TData m_Data;
    };

    CSAM_Headers  m_Header;
    TLines        m_Body;
};


END_SCOPE(objects)
END_NCBI_SCOPE


/* @} */

#endif  /* OBJTOOLS_FORMAT___SAM_FORMATTER__HPP */
