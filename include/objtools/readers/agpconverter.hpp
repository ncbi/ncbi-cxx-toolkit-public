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
 * Authors:  Josh Cherry, Michael Kornbluh
 *
 * File Description:
 *   Read an AGP file, build Seq-entry's or Seq-submit's,
 *   and do optionally some validation
 *
 */

#ifndef __OBJTOOLS_READERS_AGPCONVERT_HPP__
#define __OBJTOOLS_READERS_AGPCONVERT_HPP__

#include <corelib/ncbiobj.hpp>
#include <serial/serialdef.hpp>
#include <objtools/readers/agp_seq_entry.hpp>

BEGIN_NCBI_SCOPE

namespace objects {
    class CBioseq;
    class CBioseq_set;
    class CSubmit_block;
}

class NCBI_XOBJEDIT_EXPORT CAgpConverter 
{
public:
    enum EOutputFlags {
        fOutputFlags_Fuzz100              = (1<<0), ///< For gaps of length 100, put an Int-fuzz = unk in the literal
        fOutputFlags_FastaId              = (1<<1), ///< Parse object ids (col. 1) as fasta-style ids if they contain '|'
        fOutputFlags_SetGapInfo           = (1<<2), ///< Set Seq-gap (gap type and linkage) in delta sequence.
        // if you add more, please update OutputFlagStringToEnum

        // this must be last
        fOutputFlags_LAST_PLUS_ONE
    };
    /// Bitwise-OR of EOutputFlags
    typedef int TOutputFlags;

    /// The different kinds of errors that could occur while processing.
    enum EError {
        eError_OutputDirNotFoundOrNotADir,
        eError_ComponentNotFound,
        eError_ComponentTooShort,
        eError_ChromosomeMapIgnoredBecauseChromosomeSubsourceAlreadyInTemplate,
        eError_ChromosomeFileBadFormat,
        eError_ChromosomeIsInconsistent,
        eError_WrongNumberOfSourceDescs,
        eError_SubmitBlockIgnoredWhenOneBigBioseqSet,
        eError_EntrySkippedDueToFailedComponentValidation,
        eError_EntrySkipped,
        eError_SuggestUsingFastaIdOption,
        eError_AGPMessage,
        eError_AGPErrorCode,
        // if you add more, please update ErrorStringToEnum

        // this must be last
        eError_END
    };

    /// Subclass this to override how errors are handled (example: to stop
    /// early on some kinds of errors)
    class NCBI_XOBJEDIT_EXPORT CErrorHandler : public CObject {
    public:
        virtual ~CErrorHandler(void) { }

        /// Default is to print to cerr, but feel free to override in a subclass.
        /// In particular, it's reasonable to throw an exception here to halt
        /// the processing.
        virtual void HandleError(EError eError, const string & sMessage ) const { 
            cerr << "Error: " << sMessage << endl; }
    };

    /// Constructor.
    ///
    /// @param pTemplateBioseq
    ///   This holds the template bioseq that the output is built on.
    /// @param pSubmitBlock
    ///   The Seq-submit Submit-block, which can be NULL to just output a Seq-entry.
    /// @param 
    /// @param fFlags
    ///   Flags to control the behavior of the conversion
    CAgpConverter( 
        CConstRef<objects::CBioseq> pTemplateBioseq,
        const objects::CSubmit_block * pSubmitBlock = NULL,
        TOutputFlags fOutputFlags = 0,
        CRef<CErrorHandler> pErrorHandler = CRef<CErrorHandler>() );

    /// Give a bioseq-set containing all the components pieces, for verification.
    void SetComponentsBioseqSet(CConstRef<objects::CBioseq_set> pComponentsBioseqSet);

    /// Map id to chromosome name
    typedef map<string, string>  TChromosomeMap;

    /// Give the chromosomes to this object.  
    void SetChromosomesInfo(const TChromosomeMap & mapChromosomeNames );

    /// Input has 2 tab-delimited columns: id, then chromosome name
    void LoadChromosomeMap(CNcbiIstream & chromosomes_istr );

    /// Outputs the result from the AGP file names as one big Bioseq-set.  
    /// Data format can only be ASN.1 text for now, but this may change in the future.
    void OutputAsBioseqSet(
        CNcbiOstream & ostrm,
        const std::vector<std::string> & vecAgpFileNames ) const;

    /// This gets called after each file is written, so the caller
    /// can do useful things like run asnval on every file that's output.
    class NCBI_XOBJEDIT_EXPORT IFileWrittenCallback {
    public:
        virtual ~IFileWrittenCallback(void) { }

        virtual void Notify(const string & file) = 0;
    };

    /// Outputs the results of each Seq-entry (or Seq-submit if Submit-block was given)
    /// into its own file in the given directory.
    /// 
    /// @param sDirName
    ///   The directory to put the output files into.
    /// @param sSuffix
    ///   The suffix for each file.  If empty, it defaults to "sqn" 
    ///   for Seq-submits and "ent" for Seq-entrys.
    /// @param pFileWrittenCallback
    ///   if set, its Notify function is called after each file is written.
    void OutputOneFileForEach(
        const string & sDirName,
        const std::vector<std::string> & vecAgpFileNames,
        const string & sSuffix = kEmptyStr,
        IFileWrittenCallback * pFileWrittenCallback = NULL ) const;

    /// Convert string to flag
    static TOutputFlags OutputFlagStringToEnum(const string & sEnumAsString);

    /// Convert string to EError enum
    static EError ErrorStringToEnum(const string & sEnumAsString);

private:

    // forbid copying and assignment
    CAgpConverter(const CAgpConverter &);
    CAgpConverter & operator=(const CAgpConverter &);

    // members
    CConstRef<objects::CBioseq> m_pTemplateBioseq;
    CConstRef<objects::CSubmit_block> m_pSubmitBlock;
    TOutputFlags m_fOutputFlags;
    CRef<CErrorHandler> m_pErrorHandler;

    typedef map<string, TSeqPos> TCompLengthMap;
    TCompLengthMap m_mapComponentLength;

    TChromosomeMap m_mapChromosomeNames;

    void x_ReadAgpEntries( 
        const string & sAgpFileName,
        CAgpToSeqEntry::TSeqEntryRefVec & out_agp_entries ) const;

    CRef<objects::CSeq_entry> x_InitializeAndCheckCopyOfTemplate(
        const objects::CBioseq & agp_bioseq,
        string & out_id_str ) const;

    CRef<objects::CSeq_entry> x_InitializeCopyOfTemplate(
        const objects::CBioseq& agp_seq,
        string & out_unparsed_id_str,
        string & out_id_str ) const;

    bool x_VerifyComponents(
        CConstRef<objects::CSeq_entry> new_entry, 
        const string & id_str) const;

    void x_SetChromosomeNameInSourceSubtype(
        CRef<objects::CSeq_entry> new_entry, 
        const string & unparsed_id_str ) const;

    void x_SetCreateAndUpdateDatesToToday(
        CRef<objects::CSeq_entry> new_entry ) const;
};

END_NCBI_SCOPE

#endif  // end of "include-guard"
