#ifndef OBJMGR_GC_ASSEMBLY_PARSER__HPP
#define OBJMGR_GC_ASSEMBLY_PARSER__HPP

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
* Authors:
*           Aleksey Grichenko
*
* File Description:
*           GC-Assembly parser used by CScope and CSeq_loc_Mapper to
*           convert assemblies to seq-entries.
*
*/

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objects/genomecoll/GC_Assembly.hpp>
#include <corelib/ncbiobj.hpp>
#include <set>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/** @addtogroup ObjectManagerCore
 *
 * @{
 */


// fwd decl
class CGC_AssemblyDesc;


/////////////////////////////////////////////////////////////////////////////
///
///  CGC_Assembly_Parser --
///
///    GC-Assembly parser used by CScope and CSeq_loc_Mapper to
///    convert assemblies to seq-entries.
///

class NCBI_XOBJMGR_EXPORT CGC_Assembly_Parser : public CObject
{
public:
    /// Parser options.
    enum FParserFlags {
        /// Do not add local private ids to bioseqs.
        fIgnoreLocalIds             = 1 << 0,
        /// Do not add external ids to bioseqs.
        fIgnoreExternalIds          = 1 << 1,
        /// Do not add annotations to seq-entries and bioseqs.
        fIgnoreAnnots               = 1 << 2,
        /// Do not add descriptions to seq-entries and bioseqs.
        fIgnoreDescr                = 1 << 3,
        /// Skip duplicate sequences (all synonyms are checked).
        fSkipDuplicates             = 1 << 4,

        fDefault = fIgnoreLocalIds | fIgnoreAnnots | fIgnoreDescr | fSkipDuplicates
    };
    typedef int TParserFlags;

    /// Parse the assembly, convert it to seq-entry, collect additional
    /// information (top-level sequences etc).
    CGC_Assembly_Parser(const CGC_Assembly& assembly,
                        TParserFlags        flags = fDefault);

    virtual ~CGC_Assembly_Parser(void);

    /// Create seq-entry with all parsed sequences, annotations etc.
    CRef<CSeq_entry> GetTSE(void) const { return m_TSE; }

    typedef set<CSeq_id_Handle> TSeqIds;

    /// Get seq-ids for all top-level sequences in the assembly.
    const TSeqIds& GetTopLevelSequences(void) const { return m_TopSeqs; }

private:
    void x_ParseGCAssembly(const CGC_Assembly& gc_assembly,
                           CRef<CSeq_entry>    parent_entry);
    void x_ParseGCSequence(const CGC_Sequence& gc_seq,
                           const CGC_Sequence* parent_seq,
                           CRef<CSeq_entry>    parent_entry,
                           CRef<CSeq_id>       override_id);
    void x_AddBioseq(CRef<CSeq_entry>  parent_entry,
                     const TSeqIds&    synonyms,
                     const CDelta_ext* delta);
    void x_InitSeq_entry(CRef<CSeq_entry> entry,
                         CRef<CSeq_entry> parent);
    void x_CopyData(const CGC_AssemblyDesc& assm_desc,
                      CSeq_entry&              entry);

    TParserFlags     m_Flags;
    CRef<CSeq_entry> m_TSE;
    TSeqIds          m_TopSeqs;
    TSeqIds          m_AllSeqs;
};


/// Seq-loc and seq-align mapper exceptions
class NCBI_XOBJMGR_EXPORT CAssemblyParserException : public CException
{
public:
    enum EErrCode {
        eUnsupported,    ///< Unsupported type/flag.
        eOtherError
    };
    virtual const char* GetErrCodeString(void) const;
    NCBI_EXCEPTION_DEFAULT(CAssemblyParserException, CException);
};


/////////////////////////////////////////////////////////////////////////////
// CGC_Assembly_Parser inline methods
/////////////////////////////////////////////////////////////////////////////




/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

#endif//OBJMGR_GC_ASSEMBLY_PARSER__HPP
