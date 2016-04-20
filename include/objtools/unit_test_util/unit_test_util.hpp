#ifndef OBJMGR__TEST__UNIT_TEST_UTIL__HPP
#define OBJMGR__TEST__UNIT_TEST_UTIL__HPP

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
 * Author:  Colleen Bollin
 *
 * File Description:
 *   functions for creating Bioseqs, Bioseq-sets, and other objects for
 *   for testing purposes
 *   .......
 *
 */
#include <corelib/ncbistd.hpp>
#include <corelib/ncbifile.hpp>
#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <corelib/ncbidiag.hpp>
#include <serial/objectinfo.hpp>
#include <serial/serialbase.hpp>
#include <objmgr/scope.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/biblio/Author.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/Cit_jour.hpp>
#include <objects/biblio/Cit_gen.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/general/Person_id.hpp>
#include <objects/general/Name_std.hpp>
#include <objects/general/Object_id.hpp>


#include <map>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

BEGIN_SCOPE(unit_test_util)

// Dbxrefs, for sources and features
NCBI_UNIT_TEST_UTIL_EXPORT void SetDbxref (objects::CBioSource& src, string db, objects::CObject_id::TId id);
NCBI_UNIT_TEST_UTIL_EXPORT void SetDbxref (objects::CBioSource& src, string db, string id);
NCBI_UNIT_TEST_UTIL_EXPORT void SetDbxref (CRef<objects::CSeq_entry> entry, string db, objects::CObject_id::TId id);
NCBI_UNIT_TEST_UTIL_EXPORT void SetDbxref (CRef<objects::CSeq_entry> entry, string db, string id);
NCBI_UNIT_TEST_UTIL_EXPORT void SetDbxref (CRef<objects::CSeq_feat> feat, string db, objects::CObject_id::TId id);
NCBI_UNIT_TEST_UTIL_EXPORT void SetDbxref (CRef<objects::CSeq_feat> feat, string db, string id);

NCBI_UNIT_TEST_UTIL_EXPORT void RemoveDbxref (objects::CBioSource& src, string db, objects::CObject_id::TId id);
NCBI_UNIT_TEST_UTIL_EXPORT void RemoveDbxref (CRef<objects::CSeq_entry> entry, string db, objects::CObject_id::TId id);
NCBI_UNIT_TEST_UTIL_EXPORT void RemoveDbxref (CRef<objects::CSeq_feat> feat, string db, objects::CObject_id::TId id);

// BioSource
NCBI_UNIT_TEST_UTIL_EXPORT void SetSubSource (objects::CBioSource& src, objects::CSubSource::TSubtype subtype, string val);
NCBI_UNIT_TEST_UTIL_EXPORT void SetSubSource (CRef<objects::CSeq_entry> entry, objects::CSubSource::TSubtype subtype, string val);

// MolInfo
NCBI_UNIT_TEST_UTIL_EXPORT void SetCompleteness(CRef<objects::CSeq_entry> entry, objects::CMolInfo::TCompleteness completeness);


NCBI_UNIT_TEST_UTIL_EXPORT void SetTaxon (objects::CBioSource& src, size_t taxon);
NCBI_UNIT_TEST_UTIL_EXPORT void SetTaxon (CRef<objects::CSeq_entry> entry, size_t taxon);
NCBI_UNIT_TEST_UTIL_EXPORT void AddGoodSource (CRef<objects::CSeq_entry> entry);
NCBI_UNIT_TEST_UTIL_EXPORT void AddFeatAnnotToSeqEntry (CRef<objects::CSeq_annot> annot, CRef<objects::CSeq_entry> entry);
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CSeq_feat> AddProtFeat(CRef<objects::CSeq_entry> entry) ;
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CSeq_feat> AddGoodSourceFeature(CRef<objects::CSeq_entry> entry);
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CSeq_feat> MakeMiscFeature(CRef<objects::CSeq_id> id, size_t right_end = 10, size_t left_end = 0);
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CSeq_id> IdFromEntry(CRef<objects::CSeq_entry> entry);
NCBI_UNIT_TEST_UTIL_EXPORT void SetTaxname (CRef<objects::CSeq_entry> entry, string taxname);
NCBI_UNIT_TEST_UTIL_EXPORT void SetSebaea_microphylla(CRef<objects::CSeq_entry> entry);
NCBI_UNIT_TEST_UTIL_EXPORT void SetSynthetic_construct(CRef<objects::CSeq_entry> entry);
NCBI_UNIT_TEST_UTIL_EXPORT void SetDrosophila_melanogaster(CRef<objects::CSeq_entry> entry);
NCBI_UNIT_TEST_UTIL_EXPORT void SetCommon (CRef<objects::CSeq_entry> entry, string common);
NCBI_UNIT_TEST_UTIL_EXPORT void SetLineage (CRef<objects::CSeq_entry> entry, string lineage);
NCBI_UNIT_TEST_UTIL_EXPORT void SetDiv (CRef<objects::CSeq_entry> entry, string div);
NCBI_UNIT_TEST_UTIL_EXPORT void SetOrigin (CRef<objects::CSeq_entry> entry, objects::CBioSource::TOrigin origin);
NCBI_UNIT_TEST_UTIL_EXPORT void SetGcode (CRef<objects::CSeq_entry> entry, objects::COrgName::TGcode gcode);
NCBI_UNIT_TEST_UTIL_EXPORT void SetMGcode (CRef<objects::CSeq_entry> entry, objects::COrgName::TGcode mgcode);
NCBI_UNIT_TEST_UTIL_EXPORT void SetPGcode (CRef<objects::CSeq_entry> entry, objects::COrgName::TGcode pgcode);
NCBI_UNIT_TEST_UTIL_EXPORT void ResetOrgname (CRef<objects::CSeq_entry> entry);
NCBI_UNIT_TEST_UTIL_EXPORT void SetFocus (CRef<objects::CSeq_entry> entry);
NCBI_UNIT_TEST_UTIL_EXPORT void ClearFocus (CRef<objects::CSeq_entry> entry);
NCBI_UNIT_TEST_UTIL_EXPORT void SetGenome (CRef<objects::CSeq_entry> entry, objects::CBioSource::TGenome genome);
NCBI_UNIT_TEST_UTIL_EXPORT void SetChromosome (objects::CBioSource& src, string chromosome) ;
NCBI_UNIT_TEST_UTIL_EXPORT void SetChromosome (CRef<objects::CSeq_entry> entry, string chromosome);
NCBI_UNIT_TEST_UTIL_EXPORT void SetTransgenic (objects::CBioSource& src, bool do_set) ;
NCBI_UNIT_TEST_UTIL_EXPORT void SetTransgenic (CRef<objects::CSeq_entry> entry, bool do_set);
NCBI_UNIT_TEST_UTIL_EXPORT void SetOrgMod (objects::CBioSource& src, objects::COrgMod::TSubtype subtype, string val);
NCBI_UNIT_TEST_UTIL_EXPORT void SetOrgMod (CRef<objects::CSeq_entry> entry, objects::COrgMod::TSubtype subtype, string val);
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CSeqdesc> BuildGoodPubSeqdesc();
NCBI_UNIT_TEST_UTIL_EXPORT void AddGoodPub (CRef<objects::CSeq_entry> entry);
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CAuthor> BuildGoodAuthor();
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CPub> BuildGoodArticlePub();
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CPub> BuildGoodCitGenPub(CRef<objects::CAuthor> author, int serial_number);
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CPub> BuildGoodCitSubPub();
NCBI_UNIT_TEST_UTIL_EXPORT void MakeSeqLong(objects::CBioseq& seq);
NCBI_UNIT_TEST_UTIL_EXPORT void SetBiomol (CRef<objects::CSeq_entry> entry, objects::CMolInfo::TBiomol biomol);
NCBI_UNIT_TEST_UTIL_EXPORT void SetTech (CRef<objects::CSeq_entry> entry, objects::CMolInfo::TTech tech);
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CSeq_entry> MakeProteinForGoodNucProtSet (string id);
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CSeq_feat> MakeCDSForGoodNucProtSet (string nuc_id, string prot_id);
NCBI_UNIT_TEST_UTIL_EXPORT void AdjustProtFeatForNucProtSet(CRef<objects::CSeq_entry> entry);
NCBI_UNIT_TEST_UTIL_EXPORT void SetNucProtSetProductName (CRef<objects::CSeq_entry> entry, string new_name);
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CSeq_entry> GetNucleotideSequenceFromGoodNucProtSet (CRef<objects::CSeq_entry> entry);
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CSeq_entry> GetProteinSequenceFromGoodNucProtSet (CRef<objects::CSeq_entry> entry);
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CSeq_feat> GetProtFeatFromGoodNucProtSet (CRef<objects::CSeq_entry> entry);
NCBI_UNIT_TEST_UTIL_EXPORT void RetranslateCdsForNucProtSet (CRef<objects::CSeq_entry> entry, objects::CScope &scope);
NCBI_UNIT_TEST_UTIL_EXPORT void SetNucProtSetPartials (CRef<objects::CSeq_entry> entry, bool partial5, bool partial3);
NCBI_UNIT_TEST_UTIL_EXPORT void ChangeNucProtSetProteinId (CRef<objects::CSeq_entry> entry, CRef<objects::CSeq_id> id);
NCBI_UNIT_TEST_UTIL_EXPORT void ChangeNucProtSetNucId (CRef<objects::CSeq_entry> entry, CRef<objects::CSeq_id> id);
NCBI_UNIT_TEST_UTIL_EXPORT void MakeNucProtSet3Partial (CRef<objects::CSeq_entry> entry);
NCBI_UNIT_TEST_UTIL_EXPORT void ChangeId(CRef<objects::CSeq_annot> annot, CRef<objects::CSeq_id> id);
NCBI_UNIT_TEST_UTIL_EXPORT void ChangeProductId(CRef<objects::CSeq_annot> annot, CRef<objects::CSeq_id> id);
NCBI_UNIT_TEST_UTIL_EXPORT void ChangeNucId(CRef<objects::CSeq_entry> np_set, CRef<objects::CSeq_id> id);
NCBI_UNIT_TEST_UTIL_EXPORT void ChangeProtId(CRef<objects::CSeq_entry> np_set, CRef<objects::CSeq_id> id);
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CSeq_id> BuildRefSeqId(void);
NCBI_UNIT_TEST_UTIL_EXPORT void ChangeId(CRef<objects::CSeq_entry> entry, CRef<objects::CSeq_id> id);
NCBI_UNIT_TEST_UTIL_EXPORT void ChangeId(CRef<objects::CSeq_annot> annot, string suffix);
NCBI_UNIT_TEST_UTIL_EXPORT void ChangeId(CRef<objects::CSeq_entry> entry, string suffix);
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CSeq_entry> BuildGenProdSetNucProtSet (CRef<objects::CSeq_id> nuc_id, CRef<objects::CSeq_id> prot_id);
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CSeq_feat> MakemRNAForCDS (CRef<objects::CSeq_feat> feat);
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CSeq_entry> BuildGoodGenProdSet();
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CSeq_entry> GetGenomicFromGenProdSet (CRef<objects::CSeq_entry> entry);
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CSeq_feat> GetmRNAFromGenProdSet(CRef<objects::CSeq_entry> entry);
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CSeq_entry> GetNucProtSetFromGenProdSet(CRef<objects::CSeq_entry> entry);
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CSeq_feat> GetCDSFromGenProdSet (CRef<objects::CSeq_entry> entry);
NCBI_UNIT_TEST_UTIL_EXPORT void RevComp (objects::CBioseq& bioseq);
NCBI_UNIT_TEST_UTIL_EXPORT void RevComp (objects::CSeq_loc& loc, size_t len);
NCBI_UNIT_TEST_UTIL_EXPORT void RevComp (CRef<objects::CSeq_entry> entry);
NCBI_UNIT_TEST_UTIL_EXPORT void RemoveDeltaSeqGaps(CRef<objects::CSeq_entry> entry) ;
NCBI_UNIT_TEST_UTIL_EXPORT void AddToDeltaSeq(CRef<objects::CSeq_entry> entry, string seq);
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CSeq_entry> BuildSegSetPart(string id_str);
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CSeq_entry> BuildGoodSegSet(void);
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CSeq_entry> BuildGoodEcoSet();
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CSeq_align> BuildGoodAlign();
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CSeq_annot> BuildGoodGraphAnnot(string id);
NCBI_UNIT_TEST_UTIL_EXPORT void RemoveDescriptorType (CRef<objects::CSeq_entry> entry, objects::CSeqdesc::E_Choice desc_choice);
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CSeq_feat> BuildtRNA(CRef<objects::CSeq_id> id);
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CSeq_feat> BuildGoodtRNA(CRef<objects::CSeq_id> id);
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CSeq_loc> MakeMixLoc (CRef<objects::CSeq_id> id);
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CSeq_feat> MakeIntronForMixLoc (CRef<objects::CSeq_id> id);
NCBI_UNIT_TEST_UTIL_EXPORT void SetSpliceForMixLoc (objects::CBioseq& seq);
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CSeq_feat> MakeGeneForFeature (CRef<objects::CSeq_feat> feat);
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CSeq_feat> AddGoodImpFeat (CRef<objects::CSeq_entry> entry, string key);


// Adding Features
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CSeq_feat> AddMiscFeature(CRef<objects::CSeq_entry> entry);
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CSeq_feat> AddMiscFeature(CRef<objects::CSeq_entry> entry, size_t right_end);
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CSeq_annot> AddFeat (CRef<objects::CSeq_feat> feat, CRef<objects::CSeq_entry> entry);
NCBI_UNIT_TEST_UTIL_EXPORT CRef<CSeq_feat> BuildGoodFeat ();

// Building known good Seq-entries
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CSeq_entry> BuildGoodNucProtSet(void);
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CSeq_entry> BuildGoodSeq(void);
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CSeq_entry> BuildGoodProtSeq(void);
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CSeq_entry> BuildGoodDeltaSeq(void);
NCBI_UNIT_TEST_UTIL_EXPORT CRef<objects::CSeq_feat> GetCDSFromGoodNucProtSet (CRef<objects::CSeq_entry> entry);

class NCBI_UNIT_TEST_UTIL_EXPORT ITestRunner
{
public:
    virtual ~ITestRunner(void) { }

    typedef map<string, CFile> TMapSuffixToFile;

    /// This function is called for each test.
    /// For example, let's say we have test_cases/foo/bar/baz/
    /// with BasicTest.input.asn and BasicTest.expected_output.asn
    /// (and files for other tests, which we'll ignore), then
    /// this would have a mapping from "input.asn" to the file test_cases/foo/bar/baz/BasicTest.input.asn
    /// and "expected_output.asn" to the file test_cases/foo/bar/baz/BasicTest.expected_output.asn
    virtual void RunTest(const string & sTestNAme,
        const TMapSuffixToFile & mapSuffixToFile ) = 0;

    /// This is called when an error occurs, and if ITestRunner is using boost
    /// it should indicate that using, for example, BOOST_ERROR.
    virtual void OnError(const string & sErrorText) = 0;
};

/// Flags that control TraverseAndRunTestCases, if needed.
enum ETraverseAndRunTestCasesFlags {
    /// Overrides default behavior (which is to disregard files whose prefix is "README")
    fTraverseAndRunTestCasesFlags_DoNOTIgnoreREADMEFiles = (1 << 0) 
};
typedef int TTraverseAndRunTestCasesFlags;

/// This is for running data-driven test cases below the given top-level
/// test directory.  In a given directory,
/// each file is checked for its prefix and suffix, where the prefix is
/// the part before the first '.' and the suffix is the part after.
/// (e.g. "foo.input.asn" has prefix "foo" and suffix "input.asn").
/// Files with the same prefix are assumed to belong to the same test.
///
/// There can be more than one test case in a test-case descendent dir and 
/// test-cases is even permitted to have zero subdirectories and have all
/// the files be at the top level (though this could get messy if you
/// have many test cases)
///
/// Any errors are handled through ITestRunner::OnError except for a NULL
/// ITestRunner which, by necessity, is handled by throwing an exception.
///
/// @param pTestRunner
///   This class's "RunTest" func is called for each test.
/// @param dirWithTestCases
///   This is the directory holding the test cases.  Sub-directories of
///   this are also checked for test-cases, although hidden subdirectories
///   and files (that is, those starting with a period, such as ".svn") 
///   are skipped.
/// @param setOfRequiredSuffixes
///   Each test must have exactly one file with the given suffix.  If any
///   test is missing any required suffix, this function will halt before
///   running any tests.   At least one required suffix must be specified.
/// @param setOfOptionalSuffixes
///   These are the suffixes permitted but not required for each test.
/// @param setOfIgnoredSuffixes
///   This is the set of suffixes that are just ignored and not passed
///   to the pTestRunner.  If a file is found whose suffix is not
///   in one of the "setOf[FOO]Suffixes" parameters, then an error
///   will occur, except that files with prefix "README" are also ignored.
/// @param fFlags
///   This controls the behavior of the function (usually not needed).
NCBI_UNIT_TEST_UTIL_EXPORT void TraverseAndRunTestCases(
    ITestRunner *pTestRunner,
    CDir dirWithTestCases,
    const set<string> & setOfRequiredSuffixes,
    const set<string> & setOfOptionalSuffixes = set<string>(),
    const set<string> & setOfIgnoredSuffixes = set<string>(),
    TTraverseAndRunTestCasesFlags fFlags = 0 );

END_SCOPE(unit_test_util)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJMGR__TEST__UNIT_TEST_UTIL__HPP */
