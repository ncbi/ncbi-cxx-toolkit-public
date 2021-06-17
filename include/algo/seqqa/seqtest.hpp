#ifndef ALGO_SEQQA___SEQTEST__HPP
#define ALGO_SEQQA___SEQTEST__HPP

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
 * Authors:  Mike DiCuccio, Josh Cherry
 *
 * File Description:
 *
 */

#include <corelib/ncbiobj.hpp>
#include <objects/seqtest/SeqTestResults.hpp>
#include <objects/seqtest/Seq_test_result_set.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/feat_ci.hpp>
#include <objects/seqtest/Seq_test_result.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE


///
/// CSeqTestContext defines any contextual information that a derived class
/// might need.  It is empty and is provided for run-time binding.
///
class NCBI_XALGOSEQQA_EXPORT CSeqTestContext : public CObject
{
public:
    CSeqTestContext(objects::CScope& scope)
        : m_Scope(&scope) { }

    /// Return the scope
    objects::CScope& GetScope() const {
        // a const_cast here is preferable to making m_Scope mutable
        // or always passing non-const pointers to this class
        return const_cast<objects::CScope&>(*m_Scope);
    }

    /// Is there an attribute by this name?
    bool HasKey(const string& key) const {
        return m_Values.find(key) != m_Values.end();
    }
    /// read/write access to named attribute
    string& operator[](const string& key) {
        return m_Values[key];
    }
    /// read-only access to named attribute
    /// for const instance
    string operator[](const string& key) const {
        map<string, string>::const_iterator iter = m_Values.find(key);
        if (iter == m_Values.end()) {
            throw runtime_error("CSeqTestContext: attempt to access "
                                "nonexistent attribute \""
                                + key + "\"");
        }
        return iter->second;
    }
private:
    // scope for resolving ids
    CRef<objects::CScope> m_Scope;
    // key/value string pairs
    map<string, string> m_Values;
};


///
/// Class CSeqTest provides a general API for performing a given test on
/// 
///
class NCBI_XALGOSEQQA_EXPORT CSeqTest : public CObject
{
public:

    /// Test to see whether the given object *can* be used in this test
    virtual bool CanTest(const CSerialObject& obj,
                         const CSeqTestContext* ctx) const = 0;

    /// RunTest() is called for each registered object.  Derived code has the
    /// guarantee that the CSerialObject can be down-cast to the bound
    /// registered type.  Derived code should also assume that the context
    /// object may be NULL.
    virtual CRef<objects::CSeq_test_result_set>
        RunTest(const CSerialObject& obj,
                const CSeqTestContext* ctx) = 0;

protected:
    /// A fuction type that analyzes coding regions.
    typedef void (*TCdregionTester)(const objects::CSeq_id&,
                                    const CSeqTestContext* ctx,
                                    objects::CFeat_CI,
                                    objects::CSeq_test_result&);

    /// Create a Seq-test-result with some fields filled in,
    /// including a name for this test, which must be supplied
    /// as an argument.
    CRef<objects::CSeq_test_result>
        x_SkeletalTestResult(const string& test_name);
    /// Given a Seq-id and a context, analyze all coding regions
    /// by calling a supplied function.  If the serial object
    /// passed in is not a Seq-id, the result is null.
    CRef<objects::CSeq_test_result_set>
        x_TestAllCdregions(const CSerialObject& obj,
                           const CSeqTestContext* ctx,
                           const string& test_name,
                           TCdregionTester cdregion_tester);
};


class NCBI_XALGOSEQQA_EXPORT CSeqTestManager
{
public:
    typedef list< CRef<objects::CSeqTestResults> > TResults;

    virtual ~CSeqTestManager() {}

    /// Provide a standard battery of quality checks that should always
    /// be run
    virtual void RegisterStandardTests();

    /// Register a test to be run for a given object sub-type
    void RegisterTest(const CTypeInfo* info, CSeqTest* test);

    /// "Un-register" a test for a given object sub-type.
    ///
    /// This is useful in conjunction with RegisterStandardTests
    /// for excluding particular standard tests.
    /// This undoes, for this object type, all registrations
    /// of CSeqTest objects of the same subclass as parameter "test"
    /// (usually there is at most one).
    void UnRegisterTest(const CTypeInfo* info, CSeqTest* test);

    /// Run tests for a given serial object
    virtual CRef<objects::CSeq_test_result_set>
        RunTests(const CSerialObject& obj,
                 const CSeqTestContext* ctx = NULL);

    /// Run tests for a given set of serial objects.  The seria objects are
    /// obtained by iterating looking for a given type
    ///
    /// For example, to run all seq-feat tests on a given seq-entry, one
    /// could call:
    ///
    ///    CSeqTestManager::RunTests(entry, *CSeq_feat::GetTypeInfo());
    ///
    /// where entry is a CSeq_entry object
    virtual TResults
        RunTests(const CSerialObject& obj,
                 const CTypeInfo& info,
                 const CSeqTestContext* ctx = NULL);

protected:
    typedef multimap<const CTypeInfo*, CRef<CSeqTest> > TTests;
    TTests m_Tests;

};


END_NCBI_SCOPE

#endif  // ALGO_SEQQA___SEQTEST__HPP
