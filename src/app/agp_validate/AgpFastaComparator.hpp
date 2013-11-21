#ifndef AGP_VALIDATE_AgpFastaComparator
#define AGP_VALIDATE_AgpFastaComparator

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
 * Authors:  Mike DiCuccio, Michael Kornbluh
 *
 * File Description:
 *     Makes sure an AGP file builds the same sequence found in a FASTA
 *     file.
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbifile.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <util/format_guess.hpp>

BEGIN_NCBI_SCOPE

class CAgpFastaComparator 
{
public:
    CAgpFastaComparator( void );
    
    enum FDiffsToHide {
        fDiffsToHide_AGPOnly     = (1 << 0),
        fDiffsToHide_ObjfileOnly = (1 << 1),
        // this is NOT the same as 
        // "fDiffsToHide_AGPOnly & fDiffsToHide_ObjfileOnly".
        // It is a different kind of message.
        fDiffsToHide_InBoth      = (1 << 2)
    };
    typedef int TDiffsToHide;

    enum EResult {
        eResult_Failure,
        eResult_Success
    };
    EResult Run( const std::list<std::string> & files,
                 const std::string & loadlog,
                 const std::string & agp_as_fasta_file,
                 TDiffsToHide diffsToHide,
                 int diffs_to_find);

    typedef set<objects::CSeq_id_Handle> TSeqIdSet;
private:

    class CTmpSeqVecStorage {
    public:
        CTmpSeqVecStorage(void);
        ~CTmpSeqVecStorage(void);

        enum EType {
            eType_AGP,          // AGP sequences
            eType_Obj           // object (usually fasta) sequences
        };
        void WriteData( EType type, objects::CSeq_entry_Handle seh );
        string GetFileName( EType type, objects::CSeq_id_Handle idh );

    private:
        CDir m_dir;

        string x_GetTmpDir(void);
    };

    // after constructor, only set to false
    bool m_bSuccess; 

    // where we write detailed loading logs
    // This is unset if we're not writing anywhere
    auto_ptr<CNcbiOfstream> m_pLoadLogFile;

    // where we output AGP in FASTA format (unset if we're not
    // supposed to write it)
    auto_ptr<CNcbiOfstream> m_pAgpAsFastaFile;

    // TKey pair is: MD5 checksum and sequence length
    typedef pair<string, TSeqPos> TKey;
    typedef map<TKey, TSeqIdSet> TUniqueSeqs;

    bool x_IsLogFileOpen(void) { 
        return m_pLoadLogFile.get() != NULL; }

    bool x_GetCompAndObjSeqIds( 
        TSeqIdSet & out_compSeqIds, // components
        TSeqIdSet & out_objSeqIds,  // objects
        const std::list<std::string> & agpFiles );

    void x_ProcessObjects( const list<string> & filenames,
                           TUniqueSeqs& fasta_ids,
                           CTmpSeqVecStorage *temp_dir );
    void x_ProcessAgps(const list<string> & filenames,
                       TUniqueSeqs& agp_ids,
                       CTmpSeqVecStorage *temp_dir );

    void x_Process(const objects::CSeq_entry_Handle seh,
                   TUniqueSeqs& seqs,
                   int * in_out_pUniqueBioseqsLoaded,
                   int * in_out_pBioseqsSkipped,
                   CNcbiOfstream *pDataOutFile );

    void x_WriteDataAsFasta( CNcbiOfstream & dataOutFile,
                             const objects::CSeq_id_Handle & idh,
                             const std::string & data );

    void x_PrintDetailsOfLengthIssue( objects::CBioseq_Handle bioseq_h );

    void x_OutputDifferingSeqIds(
        const TSeqIdSet & vSeqIdFASTAOnly,
        const TSeqIdSet & vSeqIdAGPOnly,
        TDiffsToHide diffs_to_hide,
        TSeqIdSet & out_seqIdIntersection );

    void x_CheckForDups( TUniqueSeqs & unique_ids,
                         const string & file_type );

    void x_OutputSeqDifferences(
        int diffs_to_find,
        const TSeqIdSet & seqIdIntersection,
        CTmpSeqVecStorage & temp_dir );

    void x_SetBinaryVsText( CNcbiIstream & file_istrm, 
        CFormatGuess::EFormat guess_format );

    enum EFileType {
        eFileType_FASTA = 1,
        eFileType_ASN1,
        eFileType_AGP,

        eFileType_Unknown
    };
    EFileType x_GuessFileType( const string & filename );
};

END_NCBI_SCOPE

#endif /* AGP_VALIDATE_AgpFastaComparator */


