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
 * Authors:  Sergiy Gotvyanskyy
 *
 * File Description:
 *    testing C++ 20 coroutine capacities
 *
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistl.hpp>
#include <util/regexp/ctre/replace.hpp>
#include <util/regexp/ctre/ctre.hpp>

#include "co_fasta.hpp"
#include "co_fasta_compat.hpp"
#include "co_gff.hpp"
#include <objtools/huge_asn/huge_file.hpp>
#include "impl/co_readers.hpp"

#include <objtools/readers/fasta.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <corelib/test_boost.hpp>
#include <cassert>

#include <common/test_assert.h>  /* This header must go last */

using namespace ncbi;
using namespace ncbi::objtools::hugeasn;


Generator<std::uint64_t>
fibonacci_sequence(unsigned n)
{
    if (n == 0)
        co_return;

    if (n > 94)
        throw std::runtime_error("Too big Fibonacci sequence. Elements would overflow.");

    co_yield 0;

    if (n == 1)
        co_return;

    co_yield 1;

    if (n == 2)
        co_return;

    std::uint64_t a = 0;
    std::uint64_t b = 1;

    for (unsigned i = 2; i < n; ++i)
    {
        std::uint64_t s = a + b;
        co_yield s;
        a = b;
        b = s;
    }
}

void RunFibonacciTest()
{
    auto gen = fibonacci_sequence(20);
    for (int j = 0; gen; ++j)
        std::cout << "fib(" << j << ")=" << gen() << '\n';
}

void RunFastaTest1(std::string_view blob)
{
    auto gen = BasicReadLinesGenerator(1, blob);
    for (int j = 0; gen; ++j) {
        auto res = gen();
        std::cout << "Line:" << res.first << ":" << res.second << "\n";
    }
    std::cout << "Finished\n";
}

void RunTest1(const std::string& filename)
{
    auto file = std::make_shared<objects::edit::CHugeFile>();
    file->Open(filename, nullptr);
    std::cout << "File opened: " << filename <<  ":" << file->m_filesize << ":" << file->m_format << "\n";
    if (file->m_format == CFormatGuess::eFasta) {
        RunFastaTest1(std::string_view(file->m_memory, file->m_filesize));
    }
}

void RunTest2(const std::string& filename)
{
    CFastaSource reader(filename);
    auto gen = reader.ReadBlobs();
    for (int j = 0; gen; ++j) {
        auto blob = gen();
        std::cout << "Blob:" << blob->m_data.size() << ":" << blob->m_defline << ":" << blob->m_seq_length << "\n";
    }
    std::cout << "Finished\n";
}

void RunTest2Gaps(const std::string& filename)
{
    CFastaSource reader(filename);
    reader.SetFlags({CFastaSource::ParseFlags::IsDeltaSeq});
    auto gen = reader.ReadBlobs();
    for (int j = 0; gen; ++j) {
        auto blob = gen();
        std::cout << "Blob:" << blob->m_data.size() << ":" << blob->m_defline << ":" << blob->m_seq_length << "\n";
    }
    std::cout << "Finished\n";
}

BOOST_AUTO_TEST_CASE(test_basic_line_reader)
{
    //RunTest1("file1.fsa");
}

BOOST_AUTO_TEST_CASE(test_fasta_reader_basic)
{
    //RunTest2("file1.fsa");
}

BOOST_AUTO_TEST_CASE(test_fasta_reader_gaps)
{
    //RunTest2("file2.fsa");
    //RunTest2Gaps("file2.fsa");
}

BOOST_AUTO_TEST_CASE(test_fasta_reader_literals1)
{
    //RunTest3("file1.fsa");
    //RunTest3("file2.fsa");
}

BOOST_AUTO_TEST_CASE(test_fasta_reader_literals2)
{
    //RunTest4("file1.fsa", 300);
    //RunTest4("file2.fsa", 300);
}

BOOST_AUTO_TEST_CASE(test_fasta_reader_pack1)
{
    //RunTestPack("file1.fsa");
    //RunTest4("file2.fsa", 300);
}

void RunTestGffBasic(const std::string& filename)
{
    auto blob_gen = CGffSource::ReadBlobs(filename);
    size_t total_lines = 0;
    for (int j = 0; blob_gen; ++j) {
        auto blob = blob_gen();
        total_lines += blob->m_lines.size();
        std::cout << "Blob: " << (int)blob->m_blob_type << ":" << blob->m_seqid << ":" << blob->m_lines.size() << "\n";
    }
    std::cout << "Finished with " << total_lines << "\n";
}

BOOST_AUTO_TEST_CASE(test_gff_reader_basic)
{
    //RunTestGffBasic("uud-1.gff3");
    //RunTestGffBasic("/netmnt/vast01/gp/home/stropepk/JIRA/GP-39570/GeneMarkETP/genemark_supported.gtf_2");
    //RunTestGffBasic("CMM_BatrDend_JEL423_V3.annotation.gff3");
    //RunTestPack("file1.fsa");
    //RunTest4("file2.fsa", 300);
}

void ParseFastaBlob(CFastaSource::TFastaBlob blob, bool parse_gaps)
{
    objects::CFastaReader::TFlags flags = 0;

    if (blob.Flags().test(CFastaSource::ParseFlags::IsDeltaSeq) || parse_gaps) {
        flags |= objects::CFastaReader::fParseGaps;
    }

    CCompatLineReader clr{std::move(blob)};
    objects::CFastaReader r{clr, flags};
    auto seq = r.ReadSet();
    std::cout << MSerial_AsnText << *seq;
}

void RunTestFullFasta(const std::string& filename, bool delta_seq)
{
    CFastaSource reader(filename);
    if (delta_seq) {
        reader.SetFlags({CFastaSource::ParseFlags::IsDeltaSeq});
    }
    auto gen = reader.ReadBlobs();
    for (int j = 0; gen; ++j) {
        auto blob = gen();
        std::cout << "Blob:" << blob->m_data.size() << ":" << blob->m_defline << ":" << blob->m_seq_length << "\n";
        ParseFastaBlob(*blob, false);
        ParseFastaBlob(*blob, true);
    }
    std::cout << "Finished\n";
}

BOOST_AUTO_TEST_CASE(test_fasta_reader_full)
{
    RunTestFullFasta("file2.fsa", false);
    RunTestFullFasta("file2.fsa", true);
}
