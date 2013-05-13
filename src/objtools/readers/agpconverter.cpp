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

#include <objtools/readers/agpconverter.hpp>

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
        NStr::Split(line, " \t", split_line);
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

void CAgpConverter::OutputAsBioseqSet(
    CNcbiOstream & ostrm,
    const std::vector<std::string> & vecAgpFileNames ) const
{
    if( m_pSubmitBlock ) {
        // some kind of warning about submit block being ignored
        m_pErrorHandler->HandleError(
            eError_SubmitBlockIgnoredWhenOneBigBioseqSet,
            "The Submit-block is ignored when outputting as one big bioseq-set because "
            "bioseq-sets cannot contain Submit-blocks.");
    }

    ostrm << "Bioseq-set ::= { seq-set {" << endl;

    {{
        CObjectOStreamAsn obj_writer(ostrm);

        // Iterate over AGP files
        bool bFirstEntry = true;
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
                        "Entry skipped and reason probably given in a previous error" );
                    continue;
                }


                if( bFirstEntry ) {
                    bFirstEntry = false;
                } else {
                    obj_writer.Flush();
                    ostrm << "," << endl;
                }

                obj_writer.WriteObject(new_entry.GetPointer(), new_entry->GetThisTypeInfo());
            }
        }
    }}

    ostrm << "} }"  << endl; // close Bioseq-set
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
                new_submit->SetData().SetEntrys().front().Reset(new_entry.GetPointer());
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
    _ASSERT( (fOutputFlags_LAST_PLUS_ONE - 1) == fOutputFlags_SetGapInfo );

    typedef SStaticPair<const char*, CAgpConverter::TOutputFlags> TStrFlagPair;
    static const TStrFlagPair kStrFlagPairs[] = {
        STRING_AND_VAR_PAIR(fOutputFlags_FastaId),
        STRING_AND_VAR_PAIR(fOutputFlags_Fuzz100),
        STRING_AND_VAR_PAIR(fOutputFlags_SetGapInfo)
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
    _ASSERT( eError_END == (eError_AGPErrorCode + 1) );

    typedef SStaticPair<const char*, CAgpConverter::EError> TStrErrorPair;
    static const TStrErrorPair kStrErrorPairs[] = {
        STRING_AND_VAR_PAIR(eError_AGPErrorCode),
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
    CAgpToSeqEntry agp_reader( fAgpReaderFlags, eAgpVersion_auto, pErrHandler.GetPointer(), true );
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
            out_id_str.clear();
            // need version, no db name from id general
            CSeq_id::TLabelFlags flags = CSeq_id::fLabel_GeneralDbIsContent|CSeq_id::fLabel_Version;
            ids.front()->GetLabel(&out_id_str, CSeq_id::eContent, flags);
        } else {
            m_pErrorHandler->HandleError(
                eError_SuggestUsingFastaIdOption,
                "** ID " + out_id_str +
                " contains a '|'; consider using the -fasta_id option");
        }
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
            NStr::UIntToString(source_descs.size()) +
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

END_NCBI_SCOPE
