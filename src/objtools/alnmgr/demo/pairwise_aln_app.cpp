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
* Author:  Kamen Todorov
*
* File Description:
*   Demo of extracting a pairwise alignment from a file with Seq-align(s).
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <connect/ncbi_core_cxx.hpp>

#include <common/test_assert.h>

#include <serial/objistr.hpp>
#include <serial/iterator.hpp>

/// Obj Manager
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

/// Aln Manager
#include <objtools/alnmgr/aln_asn_reader.hpp>
#include <objtools/alnmgr/pairwise_aln.hpp>
#include <objtools/alnmgr/aln_container.hpp>
#include <objtools/alnmgr/aln_tests.hpp>
#include <objtools/alnmgr/aln_stats.hpp>


using namespace ncbi;
using namespace objects;


/// Types we use here:
// typedef CSeq_align::TDim TDim;
// typedef vector<const CSeq_align*> TAlnVector;
// typedef const CSeq_id* TSeqIdPtr;
// typedef vector<TSeqIdPtr> TSeqIdVector;
// typedef SCompareOrdered<TSeqIdPtr> TComp;
// typedef CAlnSeqIdVector<TAlnVector, TComp> TAlnSeqIdVector;
// typedef CSeqIdAlnBitmap<TAlnSeqIdVector> TSeqIdAlnBitmap;
// typedef CAlnStats<TAlnVector, TSeqIdVector, TAlnSeqIdVector> TAlnStats;
// typedef TAlnStats::TBaseWidths TBaseWidths;
// typedef TAlnStats::TAnchorRows TAnchorRows;


class CPairwiseAlnApp : public CNcbiApplication
{
public:
    virtual void Init         (void);
    virtual int  Run          (void);
    CScope&      GetScope     (void) const;
    void         LoadInputAlns(void);
    bool         InsertAln    (const CSeq_align* aln) {
        aln->Validate(true);
        m_AlnContainer.insert(*aln);
        return true;
    }

private:
    mutable CRef<CObjectManager> m_ObjMgr;
    mutable CRef<CScope>         m_Scope;
    CAlnContainer                m_AlnContainer;
    int                          m_QueryRow;
    int                          m_SubjectRow;
};


void CPairwiseAlnApp::Init(void)
{
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->AddDefaultKey
        ("in", "InputFileName",
         "Name of file to read from (standard input by default)",
         CArgDescriptions::eInputFile, "-");

    arg_desc->AddDefaultKey
        ("b", "bin_obj_type",
         "This forces the input file to be read in binary ASN.1 mode\n"
         "and specifies the type of the top-level ASN.1 object.\n",
         CArgDescriptions::eString, "");

    arg_desc->AddDefaultKey
        ("q", "QueryRow",
         "Query (anchor) row (zero-based)",
         CArgDescriptions::eInteger, "0");

    arg_desc->AddDefaultKey
        ("s", "SubjectRow",
         "Subject row (zero-based)",
         CArgDescriptions::eInteger, "1");

    // Program description
    string prog_description = "Alignment build application.\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    SetupArgDescriptions(arg_desc.release());
}


void CPairwiseAlnApp::LoadInputAlns(void)
{
    const CArgs& args = GetArgs();
    string sname = args["in"].AsString();
    
    /// get the asn type of the top-level object
    string asn_type = args["b"].AsString();
    bool binary = !asn_type.empty();
    unique_ptr<CObjectIStream> in
        (CObjectIStream::Open(binary?eSerial_AsnBinary:eSerial_AsnText, sname));
    
    CAlnAsnReader reader;
    reader.Read(in.get(),
                bind1st(mem_fun(&CPairwiseAlnApp::InsertAln), this),
                asn_type);
}


CScope& CPairwiseAlnApp::GetScope(void) const
{
    if (!m_Scope) {
        m_ObjMgr = CObjectManager::GetInstance();
        CGBDataLoader::RegisterInObjectManager(*m_ObjMgr);
        
        m_Scope = new CScope(*m_ObjMgr);
        m_Scope->AddDefaults();
    }
    return *m_Scope;
}


int CPairwiseAlnApp::Run(void)
{
    // Setup application registry, error log, and MT-lock for CONNECT library
    CONNECT_Init(&GetConfig());

    m_QueryRow = GetArgs()["q"].AsInteger();
    m_SubjectRow = GetArgs()["s"].AsInteger();

    LoadInputAlns();

//     /// Create a vector of alignments
//     TAlnVector aln_vector(m_AlnContainer.size());
//     aln_vector.assign(m_AlnContainer.begin(), m_AlnContainer.end());


//     /// Create a comparison functor
//     TComp comp;


//     /// Create a vector of seq-ids per seq-align
//     TAlnSeqIdVector aln_seq_id_vector(aln_vector, comp);


//     /// Create an alignment bitmap to obtain statistics.
//     TSeqIdAlnBitmap id_aln_bitmap(aln_seq_id_vector, GetScope());
//     id_aln_bitmap.Dump(cout);

//     /// Determine anchor row for each alignment
//     TBaseWidths base_widths;
//     bool translated = id_aln_bitmap.GetTranslatedAlnCount();
//     if (translated) {
//         base_widths.resize(id_aln_bitmap.GetAlnCount());
//         for (size_t aln_idx = 0;  aln_idx < aln_seq_id_vector.size();  ++aln_idx) {
//             const TSeqIdVector& ids = aln_seq_id_vector[aln_idx];
//             base_widths[aln_idx].resize(ids.size());
//             for (size_t row = 0; row < ids.size(); ++row)   {
//                 CBioseq_Handle bioseq_handle = m_Scope->GetBioseqHandle(*ids[row]);
//                 if (bioseq_handle.IsProtein()) {
//                     base_widths[aln_idx][row] = 3;
//                 } else if (bioseq_handle.IsNucleotide()) {
//                     base_widths[aln_idx][row] = 1;
//                 } else {
//                     string err_str =
//                         string("Cannot determine molecule type for seq-id: ")
//                         + ids[row]->AsFastaString();
//                     NCBI_THROW(CSeqalignException, eInvalidSeqId, err_str);
//                 }
//             }
//         }
//     }


//     /// Determine anchor rows;
//     TAnchorRows anchor_rows;
//     bool anchored = id_aln_bitmap.IsQueryAnchored();
//     if (anchored) {
//         TSeqIdPtr anchor_id = id_aln_bitmap.GetAnchorHandle().GetSeqId();
//         anchor_rows.resize(id_aln_bitmap.GetAlnCount(), -1);
//         for (size_t aln_idx = 0;  aln_idx < anchor_rows.size();  ++aln_idx) {
//             const TSeqIdVector& ids = aln_seq_id_vector[aln_idx];
//             for (size_t row = 0; row < ids.size(); ++row)   {
//                 if ( !(comp(ids[row], anchor_id) ||
//                        comp(anchor_id, ids[row])) ) {
//                     anchor_rows[aln_idx] = row;
//                     break;
//                 }
//             }
//             _ASSERT(anchor_rows[aln_idx] >= 0);
//         }
//     }


//     /// Store all retrieved statistics in the aln hints
//     TAlnStats aln_stats(aln_vector,
//                         aln_seq_id_vector,
//                         anchored ? &anchor_rows : 0,
//                         translated ? &base_widths : 0);
//     aln_stats.Dump(cout);


//     /// Construct pairwise alignmenst based on the aln hints
//     for (size_t aln_idx = 0;  
//          aln_idx < aln_stats.GetAlnCount();
//          ++aln_idx) {

//         CPairwiseAln 
//             pairwise_aln(*aln_stats.GetAlnVector()[aln_idx],
//                          m_QueryRow,
//                          m_SubjectRow,
//                          aln_stats.GetBaseWidthForAlnRow(aln_idx, m_QueryRow),
//                          aln_stats.GetBaseWidthForAlnRow(aln_idx, m_SubjectRow));

//         pairwise_aln.Dump(cout);
//     }
//     cout << endl;


    return 0;
}


int main(int argc, const char* argv[])
{
    return CPairwiseAlnApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
