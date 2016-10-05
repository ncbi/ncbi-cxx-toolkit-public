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
 *   and optionally do some validation
 *
 */
#include <ncbi_pch.hpp>

#include <corelib/ncbifile.hpp>

#include <objtools/readers/agp_converter.hpp>

#include <serial/objostr.hpp>
#include <serial/objostrasn.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/general/Int_fuzz.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/submit/Submit_block.hpp>

#include <sstream>

#include <util/static_map.hpp>

using namespace std;

BEGIN_NCBI_SCOPE

USING_SCOPE(objects);

CAgpConverter::CAgpConverter(
    CConstRef<CBioseq> pTemplateBioseq,
    const CSubmit_block * pSubmitBlock,
    TOutputFlags fOutputFlags,
    CRef<CErrorHandler> pErrorHandler )
    : m_pTemplateBioseq(pTemplateBioseq),
      m_fOutputFlags(fOutputFlags)
{
    if( pSubmitBlock ) {
        m_pSubmitBlock.Reset(pSubmitBlock);
    }

    if( pErrorHandler ) {
        m_pErrorHandler = pErrorHandler;
    } else {
        // use default error handler if none supplied
        m_pErrorHandler.Reset( new CErrorHandler );
    }
}

void 
CAgpConverter::SetComponentsBioseqSet(
    CConstRef<CBioseq_set> pComponentsBioseqSet)
{
    m_mapComponentLength.clear();
    ITERATE (CBioseq_set::TSeq_set, ent, pComponentsBioseqSet->GetSeq_set()) {
        TSeqPos length = (*ent)->GetSeq().GetInst().GetLength();
        ITERATE (CBioseq::TId, id, (*ent)->GetSeq().GetId()) {
            m_mapComponentLength[(*id)->AsFastaString()] = length;
        }
    }
}

void
CAgpConverter::SetChromosomesInfo(
    const TChromosomeMap & mapChromosomeNames )
{
    // Make sure there's not already a chromosome in the template
    ITERATE (CSeq_descr::Tdata, desc,
        m_pTemplateBioseq->GetDescr().Get()) {
            if ((*desc)->IsSource() && (*desc)->GetSource().IsSetSubtype()) {
                ITERATE (CBioSource::TSubtype, sub_type,
                    (*desc)->GetSource().GetSubtype()) {
                        if ((*sub_type)->GetSubtype() ==
                            CSubSource::eSubtype_chromosome) {
                                m_pErrorHandler->HandleError(
                                    eError_ChromosomeMapIgnoredBecauseChromosomeSubsourceAlreadyInTemplate,
                                    "chromosome info ignored because template "
                                    "contains a chromosome SubSource");
                                return;
                        }
                }
            }
    }

    m_mapChromosomeNames = mapChromosomeNames;
}

/// Input has 2 tab-delimited columns: id, then chromosome name
void 
CAgpConverter::LoadChromosomeMap(CNcbiIstream & chromosomes_istr )
{
    TChromosomeMap mapChromosomeNames;

    string line;
    while (!chromosomes_istr.eof()) {
        NcbiGetlineEOL(chromosomes_istr, line);
        if (line.empty()) {
            continue;
        }
        list<string> split_line;
        NStr::Split(line, " \t", split_line, NStr::fSplit_Tokenize);
        if (split_line.size() != 2) {
            m_pErrorHandler->HandleError(
                eError_ChromosomeFileBadFormat,
                "line of chromosome file does not have "
                "two columns: " + line);
            return;
        }
        string id = split_line.front();
        string chr = split_line.back();
        if (mapChromosomeNames.find(id) != mapChromosomeNames.end()
            && mapChromosomeNames[id] != chr) 
        {
            m_pErrorHandler->HandleError(
                eError_ChromosomeIsInconsistent,
                "inconsistent chromosome for " + id +
                " in chromosome file");
            return;
        }
        mapChromosomeNames[id] = chr;
    }

    SetChromosomesInfo(mapChromosomeNames);
}

CAgpConverter::IIdTransformer::~IIdTransformer(void)
{
    // nothing required yet.
}

void CAgpConverter::OutputBioseqs(
    CNcbiOstream & ostrm,
    const std::vector<std::string> & vecAgpFileNames,
    TOutputBioseqsFlags fFlags,
    size_t uMaxBioseqsToWrite ) const
{
    // put some flags into easier-to-read variables
    const bool bOneObjectPerBioseq = (fFlags & fOutputBioseqsFlags_OneObjectPerBioseq);

    // we get the first AGP entries to help with
    // determining whether to use Bioseq-sets
    CAgpToSeqEntry::TSeqEntryRefVec agp_entries;
    if( ! vecAgpFileNames.empty() ) {
        x_ReadAgpEntries( vecAgpFileNames[0], agp_entries );
    }
    const bool bOnlyOneBioseqInAllAGPFiles =
        ( agp_entries.size() == 1 && vecAgpFileNames.size() == 1 );


    // Each top-level object we write out is prepended with sObjectOpeningString
    // and appended with sObjectClosingString.
    string sObjectOpeningString;
    string sObjectClosingString;
    // set up sObjectOpeningString and sObjectClosingString
    x_SetUpObjectOpeningAndClosingStrings(
        sObjectOpeningString,
        sObjectClosingString,
        fFlags,
        bOnlyOneBioseqInAllAGPFiles );

    ostrm << sObjectOpeningString << endl;

    {{

        CObjectOStreamAsn obj_writer(ostrm);

        // Iterate over AGP files
        bool bFirstEntry = true;
        ITERATE( std::vector<std::string>, file_name_it, vecAgpFileNames ) {

            // We got entries for the first AGP file earlier in this func
            if( ! bFirstEntry ) {
                agp_entries.clear();
                x_ReadAgpEntries( *file_name_it, agp_entries );
            }

            ITERATE (CAgpToSeqEntry::TSeqEntryRefVec, ent, agp_entries) {

                string id_str;

                CRef<CBioseq> new_bioseq;

                // set new_bioseq
                {{
                    CRef<CSeq_entry> new_entry = 
                        x_InitializeAndCheckCopyOfTemplate(
                        (*ent)->GetSeq(),
                        id_str );
                    if( ! new_entry ) {
                        m_pErrorHandler->HandleError(
                            eError_EntrySkipped,
                            "Entry skipped and reason probably given in a previous error" );
                        continue;
                    }
                    new_bioseq.Reset( &new_entry->SetSeq() );
                }}

                if( bFirstEntry ) {
                    bFirstEntry = false;
                } else {
                    if( bOneObjectPerBioseq ) {
                        // if one object per bioseq, we close the
                        // previous one and open up the new one
                        ostrm << sObjectClosingString << endl;
                        ostrm << sObjectOpeningString << endl;
                    } else if( ! sObjectOpeningString.empty() ) {
                        // all the bioseqs are in one Bioseq-set,
                        // so just comma-separate them
                        ostrm << "," << endl;
                    }
                }

                if( sObjectOpeningString.empty() ) {
                    // Bioseq has to stand on its own
                    ostrm << "Bioseq ::= " << endl;
                } else {
                    // Bioseq is inside some other object
                    ostrm << "seq " << endl;
                }
                obj_writer.WriteObject(new_bioseq.GetPointer(), new_bioseq->GetThisTypeInfo());
                // flush after every write in case the object writer has its own
                // buffering that can cause corruption when intermixed with direct 
                // stringstream "operator<<" calls.
                obj_writer.Flush();
            }
        }
    }}

    ostrm << sObjectClosingString << endl;
}

void CAgpConverter::OutputOneFileForEach(
    const string & sDirName,
    const std::vector<std::string> & vecAgpFileNames,
    const string & sSuffix_arg,
    IFileWrittenCallback * pFileWrittenCallback ) const
{
    CDir outputDir(sDirName);
    if( ! outputDir.Exists() || 
        ! outputDir.IsDir() )
    {
        m_pErrorHandler->HandleError( 
            eError_OutputDirNotFoundOrNotADir,
            "The output directory is not a dir or is not found: " + sDirName );
        return;
    }

    const string & sSuffix = (
        sSuffix_arg.empty() ?
        ( m_pSubmitBlock ? "sqn" : "ent" ) :
        sSuffix_arg );

    ITERATE( std::vector<std::string>, file_name_it, vecAgpFileNames ) {

        CAgpToSeqEntry::TSeqEntryRefVec agp_entries;
        x_ReadAgpEntries( *file_name_it, agp_entries );

        ITERATE (CAgpToSeqEntry::TSeqEntryRefVec, ent, agp_entries) {

            string id_str;
            CRef<CSeq_entry> new_entry = 
                x_InitializeAndCheckCopyOfTemplate(
                (*ent)->GetSeq(),
                id_str );
            if( ! new_entry ) {
                m_pErrorHandler->HandleError(
                    eError_EntrySkipped,
                    "Entry skipped and the reason was "
                    "probably given in a previous error" );
                continue;
            }

            // we're in one of the modes where we print a CSerialObject
            // to its own file
            CRef<CSerialObject> pObjectToPrint;
            if( m_pSubmitBlock ) {
                // wrap in seq-submit before writing
                CRef<CSeq_submit> new_submit( new CSeq_submit );
                new_submit->SetSub( *SerialClone(*m_pSubmitBlock) );
                new_submit->SetData().SetEntrys().push_back(new_entry);
                pObjectToPrint = new_submit;
            } else {
                // don't need to wrap the seq-entry in a seq-submit
                pObjectToPrint = new_entry;
            }

            string outfpath = CDirEntry::MakePath(
                outputDir.GetPath(), id_str, sSuffix);
            {{
                CNcbiOfstream ostr(outfpath.c_str());
                ostr << MSerial_AsnText << *pObjectToPrint;
            }}

            // allow caller to perform some custom actions, if desired
            if( pFileWrittenCallback ) {
                pFileWrittenCallback->Notify(outfpath);
            }
        }
    }
}

#ifdef STRING_AND_VAR_PAIR
#  error STRING_AND_VAR_PAIR
#endif

// This is less error prone because we don't have to 
// worry about getting the string and name out of sync
#define STRING_AND_VAR_PAIR(_value) \
    {  #_value, _value }

CAgpConverter::TOutputFlags 
CAgpConverter::OutputFlagStringToEnum(const string & sEnumAsString)
{
    // check if this func has fallen out of date
    _ASSERT( (fOutputFlags_LAST_PLUS_ONE - 1) == fOutputFlags_AGPLenMustMatchOrig );

    typedef SStaticPair<const char*, CAgpConverter::TOutputFlags> TStrFlagPair;
    static const TStrFlagPair kStrFlagPairs[] = {
        STRING_AND_VAR_PAIR(fOutputFlags_AGPLenMustMatchOrig),
        STRING_AND_VAR_PAIR(fOutputFlags_FastaId),
        STRING_AND_VAR_PAIR(fOutputFlags_Fuzz100),
        STRING_AND_VAR_PAIR(fOutputFlags_SetGapInfo),
    };
    typedef const CStaticPairArrayMap<const char*, CAgpConverter::TOutputFlags, PNocase_CStr> TStrFlagMap;
    DEFINE_STATIC_ARRAY_MAP(TStrFlagMap, kStrFlagMap, kStrFlagPairs);

    TStrFlagMap::const_iterator find_iter =
        kStrFlagMap.find(NStr::TruncateSpaces(sEnumAsString).c_str());
    if( find_iter == kStrFlagMap.end() ) {
        NCBI_USER_THROW_FMT(
            "Bad string given to CAgpConverter::OutputFlagStringToEnum: " 
            << sEnumAsString);
    } else {
        return find_iter->second;
    }
}

CAgpConverter::EError 
CAgpConverter::ErrorStringToEnum(const string & sEnumAsString)
{
    // check if this func has fallen out of date
    _ASSERT( eError_END == (eError_AGPLengthMismatchWithTemplateLength + 1) );

    typedef SStaticPair<const char*, CAgpConverter::EError> TStrErrorPair;
    static const TStrErrorPair kStrErrorPairs[] = {
        STRING_AND_VAR_PAIR(eError_AGPErrorCode),
        STRING_AND_VAR_PAIR(eError_AGPLengthMismatchWithTemplateLength),
        STRING_AND_VAR_PAIR(eError_AGPMessage),
        STRING_AND_VAR_PAIR(eError_ChromosomeFileBadFormat),
        STRING_AND_VAR_PAIR(eError_ChromosomeIsInconsistent),
        STRING_AND_VAR_PAIR(eError_ChromosomeMapIgnoredBecauseChromosomeSubsourceAlreadyInTemplate),
        STRING_AND_VAR_PAIR(eError_ComponentNotFound),
        STRING_AND_VAR_PAIR(eError_ComponentTooShort),
        STRING_AND_VAR_PAIR(eError_EntrySkipped),
        STRING_AND_VAR_PAIR(eError_EntrySkippedDueToFailedComponentValidation),
        STRING_AND_VAR_PAIR(eError_OutputDirNotFoundOrNotADir),
        STRING_AND_VAR_PAIR(eError_SubmitBlockIgnoredWhenOneBigBioseqSet),
        STRING_AND_VAR_PAIR(eError_SuggestUsingFastaIdOption),
        STRING_AND_VAR_PAIR(eError_WrongNumberOfSourceDescs),
    };
    typedef const CStaticPairArrayMap<const char*, CAgpConverter::EError, PNocase_CStr> TStrErrorMap;
    DEFINE_STATIC_ARRAY_MAP(TStrErrorMap, kStrErrorMap, kStrErrorPairs);

    TStrErrorMap::const_iterator find_iter =
        kStrErrorMap.find(NStr::TruncateSpaces(sEnumAsString).c_str());
    if( find_iter == kStrErrorMap.end() ) {
        NCBI_USER_THROW_FMT(
            "Bad string given to CAgpConverter::ErrorStringToEnum: " 
            << sEnumAsString);
    } else {
        return find_iter->second;
    }
}

#undef STRING_AND_VAR_PAIR

void CAgpConverter::x_ReadAgpEntries( 
    const string & sAgpFileName,
    CAgpToSeqEntry::TSeqEntryRefVec & out_agp_entries ) const
{
    // load AGP Seq-entry's into agp_entries

    // set up the AGP to Seq-entry object
    const CAgpToSeqEntry::TFlags fAgpReaderFlags = 
        ( (m_fOutputFlags & fOutputFlags_SetGapInfo) ? CAgpToSeqEntry::fSetSeqGap : 0 );
    stringstream err_strm;
    CRef<CAgpErrEx> pErrHandler( new CAgpErrEx(&err_strm) );
    CAgpToSeqEntry agp_reader( fAgpReaderFlags, eAgpVersion_auto, pErrHandler.GetPointer() );
    CNcbiIfstream istr( sAgpFileName.c_str() );
    const int iErrCode = agp_reader.ReadStream(istr);

    // deal with errors
    const string sErrors = err_strm.str();
    if( ! sErrors.empty() ) {
        m_pErrorHandler->HandleError(
            eError_AGPMessage,
            "AGP parsing returned error message(s): " + sErrors );
    }
    if( iErrCode != 0 ) {
        m_pErrorHandler->HandleError(
            eError_AGPErrorCode,
            "AGP parsing returned error code " + 
            NStr::NumericToString(iErrCode) + " (" + pErrHandler->GetMsg(iErrCode) + ")");
        return;
    }

    // swap is faster than assignment
    out_agp_entries.swap( agp_reader.GetResult() );
}

CRef<CSeq_entry>
CAgpConverter::x_InitializeAndCheckCopyOfTemplate(
    const CBioseq & agp_bioseq,
    string & out_id_str ) const
{
    string unparsed_id_str;
    CRef<CSeq_entry> new_entry = 
        x_InitializeCopyOfTemplate(agp_bioseq,
        unparsed_id_str,
        out_id_str );


    if( m_fOutputFlags & fOutputFlags_AGPLenMustMatchOrig ) {
        // calculate the original template's length
        const TSeqPos uOrigBioseqLen = ( m_pTemplateBioseq->IsSetLength() ?
            m_pTemplateBioseq->GetLength() :
            0 );

        // calculate the new bioseq's length
        const TSeqPos uAGPBioseqLen = (
            agp_bioseq.IsSetLength() ?
            agp_bioseq.GetLength() :
            0 );

        if( uOrigBioseqLen != uAGPBioseqLen ) {
            m_pErrorHandler->HandleError(
                eError_AGPLengthMismatchWithTemplateLength,
                "** Entry " + out_id_str + " has mismatch, but will "
                "be written anyway: "
                "fOutputFlags_AGPLenMustMatchOrig was set and the entry's "
                "length is " +
                NStr::NumericToString(uAGPBioseqLen) + 
                " but the original template's length is " + 
                NStr::NumericToString(uOrigBioseqLen) );
        }
    }

    // if requested, put an Int-fuzz = unk for
    // all literals of length 100
    if ( m_fOutputFlags & fOutputFlags_Fuzz100 ) {
        NON_CONST_ITERATE (CDelta_ext::Tdata, delta,
            new_entry->SetSeq().SetInst()
            .SetExt().SetDelta().Set()) {
                if ((*delta)->IsLiteral() &&
                    (*delta)->GetLiteral().GetLength() == 100) {
                        (*delta)->SetLiteral().SetFuzz().SetLim();
                }
        }
    }

    // if requested, verify against known sequence components
    if ( ! m_mapComponentLength.empty() ) {
        const bool bSuccessfulValidation = x_VerifyComponents(
            new_entry, out_id_str);
        if ( ! bSuccessfulValidation ) {
            // put this error in a better place
            m_pErrorHandler->HandleError(
                eError_EntrySkippedDueToFailedComponentValidation,
                "** Not writing entry " + out_id_str + " due to failed validation");
            return CRef<CSeq_entry>();
        }
    }

    // if requested, set chromosome name in source subtype
    if ( ! m_mapChromosomeNames.empty() ) {
        x_SetChromosomeNameInSourceSubtype(
            new_entry, unparsed_id_str);
    }

    // set create and update dates to today
    x_SetCreateAndUpdateDatesToToday( new_entry );

    return new_entry;
}

CRef<CSeq_entry>
CAgpConverter::x_InitializeCopyOfTemplate(
    const CBioseq& agp_seq,
    string & out_unparsed_id_str,
    string & out_id_str ) const
{
    // insert sequence instance and id into a copy of template
    CRef<CSeq_id> pSeqId( SerialClone(*agp_seq.GetFirstId()) );
    {
        stringstream id_strm;
        pSeqId->GetLocal().AsString(id_strm);
        out_unparsed_id_str = id_strm.str();
        out_id_str = out_unparsed_id_str;
    }

    // "ids" will hold all the ids for this piece,
    // hopefully just one unless we have to fasta_id parse it
    list<CRef<CSeq_id> > ids;
    ids.push_back(pSeqId);

    // if ID contains a pipe, it might be a fasta id
    if (NStr::Find(out_id_str, "|") != NPOS) {
        if ( m_fOutputFlags & fOutputFlags_FastaId ) {
            // parse the id as a fasta id
            ids.clear();
            CSeq_id::ParseFastaIds(ids, out_id_str);
        } else {
            m_pErrorHandler->HandleError(
                eError_SuggestUsingFastaIdOption,
                "** ID " + out_id_str +
                " contains a '|'; consider using the -fasta_id option");
        }
    }

    // perform custom transformations given to us by the caller, if any
    bool bFirstWasTransformed = false;
    if( m_pIdTransformer ) {
        NON_CONST_ITERATE(list<CRef<CSeq_id> >, id_it, ids) {
            const bool bWasTransformed = m_pIdTransformer->Transform(*id_it);
            if( bWasTransformed && id_it == ids.begin() ) {
                bFirstWasTransformed = true;
            }
        }
    }

    // out_id_str might need to be updated
    if( m_fOutputFlags & fOutputFlags_FastaId ||
        bFirstWasTransformed )
    {
        // need version, no db name from id general
        out_id_str.clear();
        CSeq_id::TLabelFlags flags = CSeq_id::fLabel_GeneralDbIsContent|CSeq_id::fLabel_Version;
        ids.front()->GetLabel(&out_id_str, CSeq_id::eContent, flags);
    }

    CRef<CSeq_entry> new_entry( new CSeq_entry );
    new_entry->SetSeq( *SerialClone(*m_pTemplateBioseq) );
    new_entry->SetSeq().SetInst().Assign(agp_seq.GetInst());
    new_entry->SetSeq().ResetId();
    ITERATE (list<CRef<CSeq_id> >, an_id, ids) {
        new_entry->SetSeq().SetId().push_back(*an_id);
    }

    return new_entry;
}

bool CAgpConverter::x_VerifyComponents(
    CConstRef<CSeq_entry> new_entry, 
    const string & id_str) const
{
    bool failure = false;
    ITERATE (CDelta_ext::Tdata, delta,
        new_entry->GetSeq().GetInst()
        .GetExt().GetDelta().Get()) {
            if ((*delta)->IsLoc()) {
                const string comp_id_str =
                    (*delta)->GetLoc().GetInt().GetId().AsFastaString();
                TCompLengthMap::const_iterator find_iter =
                    m_mapComponentLength.find(comp_id_str);
                if ( find_iter == m_mapComponentLength.end()) {
                    failure = true;
                    m_pErrorHandler->HandleError(
                        eError_ComponentNotFound,
                        "** Component " + comp_id_str +
                        " of entry " + id_str + " not found");
                } else {
                    const TSeqPos uCompLen = find_iter->second;

                    const TSeqPos to = (*delta)->GetLoc().GetInt().GetTo();
                    if (to >= uCompLen) {
                        failure = true;
                        m_pErrorHandler->HandleError(
                            eError_ComponentTooShort,
                            "** Component " + comp_id_str +
                            " of entry " + id_str + " not long enough.\n"
                            "** Length is " +
                            NStr::NumericToString(uCompLen) +
                            "; requested \"to\" is " + NStr::NumericToString(to) );
                    }
                }
            }
    }

    return ! failure;
}

void CAgpConverter::x_SetChromosomeNameInSourceSubtype(
    CRef<CSeq_entry> new_entry, 
    const string & unparsed_id_str ) const
{
    TChromosomeMap::const_iterator chr_find_iter =
        m_mapChromosomeNames.find(unparsed_id_str);
    if( chr_find_iter == m_mapChromosomeNames.end() ) {
        // not found, so leave
        return;
    }

    CRef<CSubSource> sub_source(new CSubSource);
    sub_source->SetSubtype(CSubSource::eSubtype_chromosome);
    sub_source->SetName(chr_find_iter->second);
    vector<CRef<CSeqdesc> > source_descs;
    ITERATE (CSeq_descr::Tdata, desc,
        new_entry->GetSeq().GetDescr().Get()) {
            if ((*desc)->IsSource()) {
                source_descs.push_back(*desc);
            }
    }
    if (source_descs.size() != 1) {
        m_pErrorHandler->HandleError(
            eError_WrongNumberOfSourceDescs,
            "found " +
            NStr::SizetToString(source_descs.size()) +
            "Source Desc's; expected exactly one");
        return;
    }
    CSeqdesc& source_desc = *source_descs[0];
    source_desc.SetSource().SetSubtype().push_back(sub_source);
}

void CAgpConverter::x_SetCreateAndUpdateDatesToToday(
    CRef<CSeq_entry> new_entry ) const
{
    CRef<CDate> date(new CDate);
    date->SetToTime(CurrentTime(), CDate::ePrecision_day);

    CRef<CSeqdesc> update_date(new CSeqdesc);
    update_date->SetUpdate_date(*date);
    new_entry->SetSeq().SetDescr().Set().push_back(update_date);

    CRef<CSeqdesc> create_date(new CSeqdesc);
    create_date->SetCreate_date(*date);
    new_entry->SetSeq().SetDescr().Set().push_back(create_date);
}

void CAgpConverter::x_SetUpObjectOpeningAndClosingStrings(
    string & out_sObjectOpeningString,
    string & out_sObjectClosingString,
    TOutputBioseqsFlags fOutputBioseqsFlags,
    bool bOnlyOneBioseqInAllAGPFiles ) const
{
    out_sObjectOpeningString.clear();
    out_sObjectClosingString.clear();

    // See if Bioseqs will be in a Bioseq-set or not:
    bool bUsingBioseqSets = false; // default so we can unwrap where possible
    if( fOutputBioseqsFlags & fOutputBioseqsFlags_DoNOTUnwrapSingularBioseqSets ) {
        // if unwrapping is forbidden, we have no choice but
        // to use Bioseq-sets
        bUsingBioseqSets = true;
    } else if( fOutputBioseqsFlags & fOutputBioseqsFlags_OneObjectPerBioseq ) {
        // There's only one Bioseq per object, so 
        // there's no reason to use Bioseq-sets in each one
        // if we don't have to
        bUsingBioseqSets = false; // redundant assignment, but clarifies
    } else if( ! bOnlyOneBioseqInAllAGPFiles ) 
    {
        // there's only one big object, so using Bioseq-sets
        // depends on whether there exists one Bioseq in all the AGP files
        // (we make the assumption that AGP files will have at least one Bioseq)
        bUsingBioseqSets = true;
    }

    // Each subsequent "if" should append to out_sObjectOpeningString
    // and prepend to out_sObjectClosingString, because we're going from the outside inward.

    // At each step, we check if out_sObjectOpeningString is empty 
    // to see whether or not to add a ASN.1 text header (example header: "Seq-submit :: ")

    // outermost possible level: is a Seq-submit needed?
    if( m_pSubmitBlock ) {
        stringstream seq_sub_header_strm;
        CObjectOStreamAsn submit_block_writer(seq_sub_header_strm);

        // for consistency we put the header-writing line in an
        // "if" even though we know the "if" always succeeds
        if( out_sObjectOpeningString.empty() ) {
            seq_sub_header_strm << "Seq-submit ::= ";
        }
        seq_sub_header_strm << "{" << endl;
        seq_sub_header_strm << "sub ";
        submit_block_writer.WriteObject(m_pSubmitBlock.GetPointer(), m_pSubmitBlock->GetThisTypeInfo());
        submit_block_writer.Flush();
        seq_sub_header_strm << "," << endl;
        seq_sub_header_strm << "data entrys {" << endl;

        out_sObjectOpeningString = seq_sub_header_strm.str();
        out_sObjectClosingString = "} }" + out_sObjectClosingString;
    } 

    // next level inward: is a Seq-entry needed?
    const bool bUsingSeqEntry = ( 
        m_pSubmitBlock || 
        ( fOutputBioseqsFlags & fOutputBioseqsFlags_WrapInSeqEntry ) );
    if( bUsingSeqEntry ) {
        if( out_sObjectOpeningString.empty() ) {
            // add an ASN.1 text header if we're not wrapped in
            // something else
            out_sObjectOpeningString += "Seq-entry ::= ";
        }
        if( bUsingBioseqSets ) {
            out_sObjectOpeningString += "set ";
        }
    }

    // next level inward: is a Bioseq-set needed?
    if( bUsingBioseqSets ) {
        // add an ASN.1 text header if we're not wrapped in
        // something else
        if( out_sObjectOpeningString.empty() ) {
            out_sObjectOpeningString += "Bioseq-set ::= ";
        }
        out_sObjectOpeningString += "{ seq-set { ";
        out_sObjectClosingString = "} }" + out_sObjectClosingString;
    }
}

END_NCBI_SCOPE
