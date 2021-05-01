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
        fOutputFlags_AGPLenMustMatchOrig  = (1<<3), ///< When set, we give an error on AGP objects that don't have the same length as the original template.
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
        eError_AGPLengthMismatchWithTemplateLength,
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
        virtual void HandleError(EError /*eError*/, const string & sMessage ) const {
            cerr << "Error: " << sMessage << endl; }
    };

    /// Constructor.
    ///
    /// @param pTemplateBioseq
    ///   This holds the template bioseq that the output is built on.
    /// @param pSubmitBlock
    ///   The Seq-submit Submit-block, which can be NULL to just output a Seq-entry.
    /// @param fOutputFlags
    ///   Flags to control the behavior of the conversion
    /// @param pErrorHandler
    ///   This is called whenever an error occurs.  The caller will want to give a subclass of CErrorHandler if the caller wants differently functionality from the default (which is to just print to stderr)
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

    class NCBI_XOBJEDIT_EXPORT IIdTransformer : public CObject {
    public:
        virtual ~IIdTransformer(void);
        /// Takes a CSeq_id and optionally transform it.
        ///
        /// @return true, if transformed
        virtual bool Transform(CRef<objects::CSeq_id> pSeqId) const = 0;
    };
    /// When this reads an id, it will use the supplied transformer
    /// (if any) to change the CSeq_id.  It is okay to even change it
    /// to a different type.
    /// Set to NULL to unset it
    void SetIdTransformer(IIdTransformer * pIdTransformer) {
        if( pIdTransformer ) {
            m_pIdTransformer.Reset(pIdTransformer);
        } else {
            m_pIdTransformer.Reset();
        }
    }

    enum EOutputBioseqsFlags {
        /// If set, each AGP Bioseq is written as its own object.
        /// * If Submit_block was given, each object is a Seq-submit
        /// * Otherwise, it's a Bioseq-set, Bioseq or Seq-entry depending on the other flags.
        fOutputBioseqsFlags_OneObjectPerBioseq = (1 << 0),
        /// Bioseqs and Bioseq-sets should always be wrapped in a Seq-entry.
        /// This has no effect if Submit-block was given because
        /// Seq-submits must take Seq-entry's.
        fOutputBioseqsFlags_WrapInSeqEntry    = (1 << 1),
        /// Specify this if Bioseq-sets with just one Bioseq in them
        /// should _NOT_ be unwrapped into a Bioseq.
        fOutputBioseqsFlags_DoNOTUnwrapSingularBioseqSets = (1 << 2),


        // this must be last
        fOutputBioseqsFlags_LAST_PLUS_ONE
    };
    typedef int TOutputBioseqsFlags;

    /// Outputs the result from the AGP file names as ASN.1.  The output
    /// could be a Seq-submit, Seq-entry, Bioseq-set or Bioseq, depending
    /// on the flags and whether a pSubmitBlock was given.
    void OutputBioseqs(
        CNcbiOstream & ostrm,
        const std::vector<std::string> & vecAgpFileNames,
        TOutputBioseqsFlags fFlags = 0,
        size_t uMaxBioseqsToWrite = std::numeric_limits<size_t>::max() ) const;

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
    /// @param vecAgpFileNames
    ///   A list of the AGP filenames to read from.
    /// @param sSuffix
    ///   The suffix for each file.  If empty, it defaults to "sqn"
    ///   for Seq-submits and "ent" for Seq-entrys.
    /// @param pFileWrittenCallback
    ///   If non-NULL, its Notify function is called after each file is
    ///   written so the caller can perform any required custom logic.
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
    CRef<IIdTransformer> m_pIdTransformer;

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

    /// Each Bioseq written out will have the out_sObjectOpeningString
    /// before it and out_sObjectClosingString
    /// after it.  You can use the resulting strings as follows:\n
    /// \n
    /// * print out_sObjectOpeningString
    /// * If out_sObjectOpeningString is empty, print "Bioseq ::= "
    /// * print all Bisoeqs in this object (comma-separated).
    ///   (example: "seq { [...snip...] }"
    /// * print out_sObjectClosingString
    /// \n
    /// If callers are printing more than one group of Bioseqs
    /// (or one bioseq per object), they
    /// can use the openers and closers for each object.
    ///
    /// @param out_sObjectOpeningString
    ///   This will be cleared then set to the string
    ///   that should appear before each text ASN.1
    ///   object that is output.
    /// @param out_sObjectClosingString
    ///   Obvious closing analog to out_sObjectOpeningString
    /// @param fOutputBioseqsFlags
    ///   This function needs to know the flags that will
    ///   be used to output so it can determine information
    ///   such as whether or not a Bioseq-set or Bioseq will be used.
    /// @param bOnlyOneBioseqInAllAGPFiles
    ///   Caller sets this to true when there is only one Bioseq
    ///   in all the AGP files being processed for this call.
    void x_SetUpObjectOpeningAndClosingStrings(
        string & out_sObjectOpeningString,
        string & out_sObjectClosingString,
        TOutputBioseqsFlags fOutputBioseqsFlags,
        bool bOnlyOneBioseqInAllAGPFiles ) const;
};

END_NCBI_SCOPE

#endif  // end of "include-guard"
