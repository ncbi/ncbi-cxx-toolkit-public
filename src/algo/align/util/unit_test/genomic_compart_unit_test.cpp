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
* Author:  Eyal Mozes
*
* File Description:
*   Unit tests for FindCompartments()
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>
#include <corelib/ncbifile.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <algo/align/util/genomic_compart.hpp>
#include <algo/align/util/align_filter.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);

struct SCompartmentData {
    unsigned num_alignments;
    TSeqRange query_range;
    TSeqRange subject_range;

    SCompartmentData() : num_alignments(0) {}
    SCompartmentData(const string &line);
    SCompartmentData(const CSeq_align_set &compartment)
    : num_alignments(compartment.Get().size())
    {
       CSeq_align align;
       align.SetSegs().SetDisc(const_cast<CSeq_align_set &>(compartment));
       query_range = align.GetSeqRange(0);
       subject_range = align.GetSeqRange(1);
    }

    bool operator==(const SCompartmentData &o) const
    {
        return num_alignments == o.num_alignments &&
               query_range == o.query_range &&
               subject_range == o.subject_range;
    }

    bool operator<(const SCompartmentData &o) const
    {
        return num_alignments < o.num_alignments ||
               (num_alignments == o.num_alignments &&
                (query_range < o.query_range ||
                 (query_range == o.query_range &&
                  subject_range < o.subject_range)));
    }
};

SCompartmentData::SCompartmentData(const string &line)
{
    size_t blank1 = line.find(' '),
           blank2 = line.rfind(' ');
    size_t dots1 = line.find(".."),
           dots2 = line.rfind("..");
    num_alignments = NStr::StringToUInt(line.substr(0,blank1));
    query_range.SetFrom(NStr::StringToUInt(
                            line.substr(blank1+1, dots1-blank1-1))-1);
    query_range.SetToOpen(NStr::StringToUInt(
                              line.substr(dots1+2, blank2-dots1-2)));
    subject_range.SetFrom(NStr::StringToUInt(
                              line.substr(blank2+1, dots2-blank2-1))-1);
    subject_range.SetToOpen(NStr::StringToUInt(line.substr(dots2+2)));
}

CNcbiOstream &operator<<(CNcbiOstream &ostr, const SCompartmentData &d)
{
    ostr << d.num_alignments << ' ' << d.query_range.GetFrom()+1 << ".."
         << d.query_range.GetTo()+1 << ' ' << d.subject_range.GetFrom()+1
         << ".." << d.subject_range.GetTo()+1;
    return ostr;
}

NCBITEST_INIT_CMDLINE(arg_desc)
{
    // Here we make descriptions of command line parameters that we are
    // going to use.

    arg_desc->AddKey("expected-results", "ResultsFile",
                     "File containing FindCompartments() results",
                     CArgDescriptions::eInputFile);
    arg_desc->AddKey("input-dir", "InputDirectory",
                     "Directory containint input alignment sets",
                     CArgDescriptions::eString);
}

BOOST_AUTO_TEST_CASE(Test_Align_Filter)
{
    const CArgs& args = CNcbiApplication::Instance()->GetArgs();

    CNcbiIstream& istr = args["expected-results"].AsInputFile();
    string line;
    while (NcbiGetlineEOL(istr, line)) {
        string input_name, intersections;
        NStr::SplitInTwo(line, " ", input_name, intersections);
        int opts;
        if (intersections == "none") {
            opts = fCompart_Defaults;
        } else if (intersections == "either") {
            opts = fCompart_AllowIntersections;
        } else if (intersections == "both") {
            opts = fCompart_AllowIntersectionsBoth;
        } else if (intersections == "query") {
            opts = fCompart_AllowIntersectionsQuery;
        } else if (intersections == "subject") {
            opts = fCompart_AllowIntersectionsSubject;
        } else {
            NCBI_THROW(CException, eUnknown,
                       "Intersection option not recognized: " +
                       intersections);
        }

        set<SCompartmentData> expected_compartments;
        while (NcbiGetlineEOL(istr, line) && !line.empty()) {
           expected_compartments.insert(line);
        }
        list< CRef<CSeq_align> > alignments;
        CNcbiIfstream align_istr(
            CDirEntry::MakePath(args["input-dir"].AsString(),
                                input_name, "asn").c_str());
        for (;;) {
            try {
                CRef<CSeq_align> alignment(new CSeq_align);
                align_istr >> MSerial_AsnText >> *alignment;
                alignments.push_back(alignment);
            }
            catch (CEofException&) {
                break;
            }
         }

         list< CRef<CSeq_align_set> > comparts;
         FindCompartments(alignments, comparts, opts);
         set<SCompartmentData> actual_compartments;
         ITERATE (list< CRef<CSeq_align_set> >, it, comparts) {
             actual_compartments.insert(**it);
         }
         BOOST_CHECK_EQUAL(expected_compartments.size(),
                           actual_compartments.size());
         for(set<SCompartmentData>::const_iterator expected_it =
                 expected_compartments.begin(),
             actual_it = actual_compartments.begin();
             expected_it != expected_compartments.end();
             ++expected_it, ++actual_it)
          {
             BOOST_CHECK_EQUAL(*expected_it, *actual_it);
          }
    }
}
