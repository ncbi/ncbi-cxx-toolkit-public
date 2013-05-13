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
* Author:  Michael Kornbluh, based on template by Pavel Ivanov, NCBI
*
* File Description:
*   Unit test functions in agpconvert.cpp (the library, not the app which shares its name)
*
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <connect/ncbi_pipe.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/rwstream.hpp>
#include <corelib/stream_utils.hpp>
#include <objects/misc/sequence_macros.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objtools/readers/agpconverter.hpp>
#include <objtools/unit_test_util/unit_test_util.hpp>
#include <map>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

#include <util/xregexp/regexp.hpp>

BEGIN_NCBI_SCOPE

USING_SCOPE(objects);
USING_SCOPE(unit_test_util);

namespace {

    typedef pair<CAgpConverter::EError,string> TErrInfo;
    typedef vector<TErrInfo> TErrInfoVec;

    class CErrorCollector : public CAgpConverter::CErrorHandler
    {
    public:
        virtual ~CErrorCollector() { }
        virtual void HandleError(CAgpConverter::EError eError, const string & sMessage ) const {
            m_errInfoVec.push_back(make_pair(eError, sMessage));
        }

        typedef pair<CAgpConverter::EError,string> TErrInfo;
        typedef vector<TErrInfo> TErrInfoVec;

        const TErrInfoVec & GetErrInfoVec(void) const { return m_errInfoVec; }

    private:
        mutable TErrInfoVec m_errInfoVec;
    };

    string s_NormalizeErrorString( const string & sErrMsg )
    {
        // build answer here
        string sAnswer;
        ITERATE(string, ch_iter, sAnswer) {
            const char ch = *ch_iter;
            if( isspace(ch) ) {
                // all whitespace becomes ' '
                sAnswer += ' ';
            } else if( isalpha(ch) ) {
                sAnswer += tolower(ch);
            } else {
                // ignore all other characters
            }
        }

        NStr::TruncateSpacesInPlace(sAnswer);
        return sAnswer;
    }

    template<class TClass>
    CRef<TClass> s_Asn1TextToClass(const char *pchAsn1Text)
    {
        CRef<TClass> pAnswer( new TClass );

        string sAsn1Text(pchAsn1Text);
        CStringReader sAsn1TextReader(sAsn1Text);
        CRStream rstrm(&sAsn1TextReader);
        rstrm >> MSerial_AsnText >> *pAnswer;
        return pAnswer;
    }

    template<class TClass>
    CRef<TClass> s_Asn1TextFileToClass(const CFile & aFile )
    {
        CRef<TClass> pAnswer( new TClass );

        CNcbiIfstream ifstrm( aFile.GetPath().c_str() );
        ifstrm >> MSerial_AsnText >> *pAnswer;
        return pAnswer;
    }

    string s_ErrInfoToStr(const TErrInfo *pErrInfo)
    {
        if( ! pErrInfo ) {
            return "(NO-ERROR)";
        }

        string sAnswer;
        sAnswer += '(';
        sAnswer += NStr::NumericToString(static_cast<int>(pErrInfo->first));
        sAnswer += ',';
        sAnswer += pErrInfo->second;
        sAnswer += ')';

        return sAnswer;
    }

    // This function is needed because the create and update date
    // are set to *now*, so they're time dependent so we have to
    // set them to the same value in both the expected and the
    // resulting CBioseq-set's.
    void s_UpdateCreateAndUpdateDate(CBioseq_set & bioseq_set)
    {
        // single-threaded, so don't worry about locks
        static CRef<CDate> s_pTodaysDate;
        if( ! s_pTodaysDate ) {
            s_pTodaysDate.Reset( 
                new CDate(CTime(CTime::eCurrent),
                CDate::ePrecision_day) );
        }

        for (CTypeIterator<CSeqdesc> desc_it(Begin(bioseq_set)); desc_it; ++desc_it ) {
            CSeqdesc & desc = *desc_it;
            if( desc.IsCreate_date() ) {
                desc.SetCreate_date(*s_pTodaysDate);
            } else if( desc.IsUpdate_date() ) {
                desc.SetUpdate_date( *s_pTodaysDate );
            }
        }
    }

    CRef<CBioseq_set> s_RunTests(
        CConstRef<CBioseq> pTemplateBioseq,
        CConstRef<CSubmit_block> pSubmitBlock, // can be NULL
        CAgpConverter::TOutputFlags fOutputFlags,
        const vector<string> & vecAgpFileNames,
        const TErrInfoVec & expectedErrInfoVec
        )
    {
        CRef<CErrorCollector> pErrorCollector( new CErrorCollector );
        CRef<CAgpConverter::CErrorHandler> pErrorHandler( pErrorCollector.GetPointer() );
        CAgpConverter agpConvert(
            pTemplateBioseq,
            pSubmitBlock.GetPointerOrNull(),
            fOutputFlags,
            pErrorHandler );

        stringstream ostrm;
        agpConvert.OutputAsBioseqSet(
            ostrm,
            vecAgpFileNames );

        // check for unexpected errors
        const TErrInfoVec & resultingErrInfoVec = pErrorCollector->GetErrInfoVec();
        const size_t highest_err_idx = 
            max(expectedErrInfoVec.size(),
            resultingErrInfoVec.size());
        for( size_t err_idx = 0; err_idx < highest_err_idx; ++err_idx ) {
            const TErrInfo * pExpectedErrInfo = 
                ( (err_idx < expectedErrInfoVec.size()) ? 
                &expectedErrInfoVec[err_idx] :
                NULL );
            const TErrInfo * pResultingErrInfo = 
                ( (err_idx < resultingErrInfoVec.size()) ? 
                &resultingErrInfoVec[err_idx] :
                NULL );
            // skip this iteration if they match
            if( pExpectedErrInfo && pResultingErrInfo &&
                pExpectedErrInfo->first == pResultingErrInfo->first &&
                s_NormalizeErrorString(pResultingErrInfo->second) ==
                s_NormalizeErrorString(pExpectedErrInfo->second) ) 
            {
                continue;
            }

            // otherwise, print out what we have
            BOOST_ERROR("At index " << err_idx << ", expected error was: " 
                << s_ErrInfoToStr(pExpectedErrInfo) << ", but got: "
                << s_ErrInfoToStr(pResultingErrInfo) );
        }

        CRef<CBioseq_set> pBioseqSetAnswer( new CBioseq_set );
        ostrm >> MSerial_AsnText >> *pBioseqSetAnswer;
        return pBioseqSetAnswer;
    }

    // wrapper for one AGP file
    CRef<CBioseq_set> s_RunTests(
        CConstRef<CBioseq> pTemplateBioseq,
        CConstRef<CSubmit_block> pSubmitBlock, // can be NULL
        CAgpConverter::TOutputFlags fOutputFlags,
        const string & vecAgpFileName,
        const TErrInfoVec & expectedErrInfoVec = TErrInfoVec() // usually we expect no errors
        )
    {
        vector<string> vecAgpFileNames;
        vecAgpFileNames.push_back(vecAgpFileName);
        return s_RunTests(
            pTemplateBioseq,
            pSubmitBlock,
            fOutputFlags,
            vecAgpFileNames,
            expectedErrInfoVec );
    }

    // use this as a sort of ostream operator
    // that prints CConstRef of CSerialObjects (but with
    // the ability to handle NULL by printing "(NULL)")
    struct SSerialObjectPtrToAsnText {
        SSerialObjectPtrToAsnText(
            const CSerialObject * pObject )
            : m_pObject(pObject) { }

        const CSerialObject * m_pObject;
    };
    ostream & operator<<(ostream & ostrm, 
        const SSerialObjectPtrToAsnText & stream_op )
    {
        if( stream_op.m_pObject ) {
            return ostrm << MSerial_AsnText << *stream_op.m_pObject;
        } else {
            return ostrm << "(NULL)";
        }
    }
}

NCBITEST_INIT_CMDLINE(descrs)
{
    descrs->AddDefaultKey(
        "test-cases-dir", 
        "DIRECTORY",
        "Use this to change the directory where test cases are sought.",
        CArgDescriptions::eDirectory,
        "agpconverter_test_cases");
}

BOOST_AUTO_TEST_CASE(MasterTest)
{
    const CArgs & args = CNcbiApplication::Instance()->GetArgs();

    class CAgpConverterTestRunner : public unit_test_util::ITestRunner
    {
    public:
        virtual void RunTest(
            const string & sTestName,
            const TMapSuffixToFile & mapSuffixToFile ) 
        {
            BOOST_MESSAGE("Starting test: " << sTestName);

            CRef<CBioseq> pTemplateBioseq = 
                s_Asn1TextFileToClass<CBioseq>(mapSuffixToFile.find("template.asn")->second);

            TMapSuffixToFile::const_iterator submit_block_find_iter =
                mapSuffixToFile.find("submit_block.asn");
            CRef<CSubmit_block> pSubmitBlock;
            if( submit_block_find_iter != mapSuffixToFile.end() ) {
                pSubmitBlock = s_Asn1TextFileToClass<CSubmit_block>(submit_block_find_iter->second);
            }

            TMapSuffixToFile::const_iterator flags_find_iter =
                mapSuffixToFile.find("flags.txt");
            CAgpConverter::TOutputFlags fOutputFlags = 0;
            if( flags_find_iter != mapSuffixToFile.end() ) {
                CNcbiIfstream flags_istrm( flags_find_iter->second.GetPath().c_str() );
                while( flags_istrm ) {
                    string sFlagName;
                    NcbiGetlineEOL(flags_istrm, sFlagName);
                    NStr::TruncateSpacesInPlace(sFlagName);
                    if( sFlagName.empty() ) {
                        continue;
                    }


                    fOutputFlags |= CAgpConverter::OutputFlagStringToEnum(sFlagName);
                }
            }

            const CFile agpFile = mapSuffixToFile.find("agp")->second;

            TMapSuffixToFile::const_iterator expected_errors_find_iter =
                mapSuffixToFile.find("expected_errors.txt");
            TErrInfoVec errInfoVec;
            if( expected_errors_find_iter != mapSuffixToFile.end() ) {
                CNcbiIfstream exp_errs_istrm( 
                    expected_errors_find_iter->second.GetPath().c_str() );
                // each line is the error type then tab, then error message
                string sLine;
                while( NcbiGetlineEOL(exp_errs_istrm, sLine) ) {
                    NStr::TruncateSpacesInPlace(sLine);
                    if( sLine.empty() ) {
                        continue;
                    }

                    string sErrEnum;
                    string sErrMsg;
                    NStr::SplitInTwo(sLine, "\t", sErrEnum, sErrMsg);

                    CAgpConverter::EError eErrEnum =
                        CAgpConverter::ErrorStringToEnum(sErrEnum);

                    errInfoVec.push_back( TErrInfo(eErrEnum, sErrMsg) );
                }
            }

            CRef<CBioseq_set> pExpectedBioseqSetAnswer;
            BOOST_CHECK_NO_THROW(
                pExpectedBioseqSetAnswer =
                    s_Asn1TextFileToClass<CBioseq_set>( 
                        mapSuffixToFile.find("expected_bioseq_set.asn")->second ) );
            if( pExpectedBioseqSetAnswer ) {
                s_UpdateCreateAndUpdateDate(*pExpectedBioseqSetAnswer);
            }

            CRef<CBioseq_set> pBioseqSetAnswer =
                s_RunTests(
                    pTemplateBioseq,
                    pSubmitBlock,
                    fOutputFlags,
                    agpFile.GetPath(),
                    errInfoVec );
            if( pBioseqSetAnswer ) {
                s_UpdateCreateAndUpdateDate(*pBioseqSetAnswer);
            }

            if( ! pBioseqSetAnswer ||
                ! pExpectedBioseqSetAnswer ||
                ! pBioseqSetAnswer->Equals(*pExpectedBioseqSetAnswer) ) 
            {
                BOOST_ERROR("Result does not match expected.  Expected: " << Endl()
                    << SSerialObjectPtrToAsnText(pExpectedBioseqSetAnswer) << Endl()
                    << "But got: " << Endl()
                    << SSerialObjectPtrToAsnText(pBioseqSetAnswer) );
            }
        }

        virtual void OnError(const string & sErrorText) {
            BOOST_ERROR(sErrorText);
        }
    };
    CAgpConverterTestRunner testRunner;

    set<string> requiredSuffixes;
    requiredSuffixes.insert("template.asn");
    requiredSuffixes.insert("expected_bioseq_set.asn");
    requiredSuffixes.insert("agp");

    set<string> optionalSuffixes;
    optionalSuffixes.insert("submit_block.asn");
    optionalSuffixes.insert("flags.txt");
    optionalSuffixes.insert("expected_errors.txt");

    TraverseAndRunTestCases( 
        &testRunner,
        args["test-cases-dir"].AsDirectory(),
        requiredSuffixes,
        optionalSuffixes );
}

END_NCBI_SCOPE
