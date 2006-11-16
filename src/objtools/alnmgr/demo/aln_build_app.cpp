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
*   Demo of alignment building.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <connect/ncbi_core_cxx.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <test/test_assert.h>

#include <serial/objistr.hpp>
#include <serial/iterator.hpp>

#include <objtools/alnmgr/aln_asn_reader.hpp>
#include <objtools/alnmgr/aln_container.hpp>
#include <objtools/alnmgr/aln_tests.hpp>
#include <objtools/alnmgr/aln_hints.hpp>
#include <objtools/alnmgr/pairwise_aln.hpp>
#include <objtools/alnmgr/aln_converters.hpp>
#include <objtools/alnmgr/sparse_aln.hpp> //< Temp, just to test it


using namespace ncbi;
using namespace objects;


/// Types we use here:
typedef CSeq_align::TDim TDim;
typedef vector<const CSeq_align*> TAlnVector;
typedef const CSeq_id* TSeqIdPtr;
typedef vector<TSeqIdPtr> TSeqIdVector;
typedef SCompareOrdered<TSeqIdPtr> TComp;
typedef CAlnSeqIdVector<TAlnVector, TComp> TAlnSeqIdVector;
typedef CSeqIdAlnBitmap<TAlnSeqIdVector> TSeqIdAlnBitmap;
typedef CAlnHints<TAlnVector, TSeqIdVector, TAlnSeqIdVector> TAlnHints;
typedef TAlnHints::TBaseWidths TBaseWidths;
typedef TAlnHints::TAnchorRows TAnchorRows;


class CAlnBuildApp : public CNcbiApplication
{
public:
    virtual void Init         (void);
    virtual int  Run          (void);
    CScope&      GetScope     (void) const;
    void         LoadInputAlns(void);
    bool         InsertAln    (const CSeq_align* aln) {
        m_AlnContainer.insert(*aln);
        aln->Validate(true);
        return true;
    }

private:
    mutable CRef<CObjectManager> m_ObjMgr;
    mutable CRef<CScope>         m_Scope;
    CAlnContainer                m_AlnContainer;
};


void CAlnBuildApp::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->AddDefaultKey
        ("in", "InputFileName",
         "Name of file to read from (standard input by default)",
         CArgDescriptions::eInputFile, "-");

    arg_desc->AddDefaultKey
        ("b", "bin_obj_type",
         "This forces the input file to be read in binary ASN.1 mode\n"
         "and specifies the type of the top-level ASN.1 object.\n",
         CArgDescriptions::eString, "");

    // Program description
    string prog_description = "Alignment build application.\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    SetupArgDescriptions(arg_desc.release());
}


void CAlnBuildApp::LoadInputAlns(void)
{
    const CArgs& args = GetArgs();
    string sname = args["in"].AsString();
    
    /// get the asn type of the top-level object
    string asn_type = args["b"].AsString();
    bool binary = !asn_type.empty();
    auto_ptr<CObjectIStream> in
        (CObjectIStream::Open(binary?eSerial_AsnBinary:eSerial_AsnText, sname));
    
    CAlnAsnReader reader(&GetScope());
    reader.Read(in.get(),
                bind1st(mem_fun(&CAlnBuildApp::InsertAln), this),
                asn_type);
}


CScope& CAlnBuildApp::GetScope(void) const
{
    if (!m_Scope) {
        m_ObjMgr = CObjectManager::GetInstance();
        CGBDataLoader::RegisterInObjectManager(*m_ObjMgr);
        
        m_Scope = new CScope(*m_ObjMgr);
        m_Scope->AddDefaults();
    }
    return *m_Scope;
}


int CAlnBuildApp::Run(void)
{
    // Setup application registry, error log, and MT-lock for CONNECT library
    CONNECT_Init(&GetConfig());

    LoadInputAlns();


    /// Create a vector of alignments
    TAlnVector aln_vector(m_AlnContainer.size());
    aln_vector.assign(m_AlnContainer.begin(), m_AlnContainer.end());


    /// Create a comparison functor
    TComp comp;


    /// Create a vector of seq-ids per seq-align
    TAlnSeqIdVector aln_seq_id_vector(aln_vector, comp);


    /// Create an alignment bitmap to obtain statistics.
    TSeqIdAlnBitmap id_aln_bitmap(aln_seq_id_vector, GetScope());
    id_aln_bitmap.Dump(cout);
    

    /// Determine anchor row for each alignment
    TBaseWidths base_widths;
    bool translated = id_aln_bitmap.GetTranslatedAlnCount();
    if (translated) {
        base_widths.resize(id_aln_bitmap.GetAlnCount());
        for (size_t aln_idx = 0;  aln_idx < aln_seq_id_vector.size();  ++aln_idx) {
            const TSeqIdVector& ids = aln_seq_id_vector[aln_idx];
            base_widths[aln_idx].resize(ids.size());
            for (size_t row = 0; row < ids.size(); ++row)   {
                CBioseq_Handle bioseq_handle = m_Scope->GetBioseqHandle(*ids[row]);
                if (bioseq_handle.IsProtein()) {
                    base_widths[aln_idx][row] = 3;
                } else if (bioseq_handle.IsNucleotide()) {
                    base_widths[aln_idx][row] = 1;
                } else {
                    string err_str =
                        string("Cannot determine molecule type for seq-id: ")
                        + ids[row]->AsFastaString();
                    NCBI_THROW(CSeqalignException, eInvalidSeqId, err_str);
                }
            }
        }
    }


    /// Determine anchor rows;
    TAnchorRows anchor_rows;
    bool anchored = id_aln_bitmap.IsQueryAnchored();
    if (anchored) {
        TSeqIdPtr anchor_id = id_aln_bitmap.GetAnchorHandle().GetSeqId();
        anchor_rows.resize(id_aln_bitmap.GetAlnCount(), -1);
        for (size_t aln_idx = 0;  aln_idx < anchor_rows.size();  ++aln_idx) {
            const TSeqIdVector& ids = aln_seq_id_vector[aln_idx];
            for (size_t row = 0; row < ids.size(); ++row)   {
                if ( !(comp(ids[row], anchor_id) ||
                       comp(anchor_id, ids[row])) ) {
                    anchor_rows[aln_idx] = row;
                    break;
                }
            }
            _ASSERT(anchor_rows[aln_idx] >= 0);
        }
    }


    /// Store all retrieved statistics in the aln hints
    TAlnHints aln_hints(aln_vector,
                        aln_seq_id_vector,
                        anchored ? &anchor_rows : 0,
                        translated ? &base_widths : 0);
    aln_hints.Dump(cout);


    /// Construct a vector of anchored alignments containing pairs of rows
    typedef vector<CConstRef<CAnchoredAln> > TAnchoredAlnVector;
    TAnchoredAlnVector anchored_aln_vector;
    for (size_t aln_idx = 0;  
         aln_idx < aln_hints.GetAlnCount();
         ++aln_idx) {

        CConstRef<CAnchoredAln> anchored_aln =
            CreateAnchoredAlnFromAln(aln_hints, aln_idx);
        anchored_aln->Dump(cout);
        anchored_aln_vector.push_back(anchored_aln);
    }


    /// Build a single anchored_aln
    CAnchoredAln built_anchored_aln;
    for (size_t aln_idx = 0; 
         aln_idx < aln_hints.GetAlnCount();
         ++aln_idx) {

        const CAnchoredAln& anchored_aln = *anchored_aln_vector[aln_idx];

        TDim dim = anchored_aln.GetDim();
        for (TDim row = 0; row < dim; ++row) {

            if (row < built_anchored_aln.GetDim()) {

//                 const CPairwiseAln& minuend = *built_anchored_aln.m_PairwiseAlns[row];
//                 const CPairwiseAln& subtrahend = *anchored_aln.m_PairwiseAlns[row];
//                 CDiagRngColl diff;

//                 cout << "Minuend:" << endl;
//                 minuend.Dump(cout);
//                 cout << "Subtrahend:" << endl;
//                 subtrahend.Dump(cout);
//                 minuend.Diff(subtrahend, diff);
//                 cout << "Diff:" << endl;
//                 diff.Dump(cout);
                
//                 CPairwiseAln& built_pairwise_aln = *built_anchored_aln.m_PairwiseAlns[row];
//                 ITERATE(CDiagRngColl, rng_it, diff) {
//                     built_pairwise_aln.insert(*rng_it);
//                 }
//                 cout << "Result:" << endl;
//                 built_pairwise_aln.Dump(cout);

                CPairwiseAln& built_pairwise_aln = *built_anchored_aln.SetPairwiseAlns()[row];
                ITERATE(CPairwiseAln, rng_it, *anchored_aln.GetPairwiseAlns()[row]) {
                    built_pairwise_aln.insert(*rng_it);
                }

            } else {
                _ASSERT(row == built_anchored_aln.GetDim());

                CRef<CPairwiseAln> pairwise_aln
                    (new CPairwiseAln(*anchored_aln.GetPairwiseAlns()[row]));
                built_anchored_aln.SetPairwiseAlns().push_back(pairwise_aln);
                built_anchored_aln.SetSeqIds().push_back(anchored_aln.GetSeqIds()[row]);
            }
        }
        cout << "Added alignment " << aln_idx << ":" << endl;
        built_anchored_aln.Dump(cout);
    }
    built_anchored_aln.Dump(cout);

    /// Get sequence:
    CSparseAln sparse_aln(built_anchored_aln, GetScope());
    for (TDim row = 0;  row < sparse_aln.GetDim();  ++row) {
        string sequence;
        sparse_aln.GetAlnSeqString
            (row, 
             sequence, 
             sparse_aln.GetSeqAlnRange(row));
        cout << sequence;
    }

    return 0;
}


int main(int argc, const char* argv[])
{
    return CAlnBuildApp().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
* ===========================================================================
*
* $Log$
* Revision 1.6  2006/11/16 13:51:10  todorov
* Using some CSparseAln.
*
* Revision 1.5  2006/11/14 20:42:33  todorov
* build without using CDiagRngColl.
*
* Revision 1.4  2006/11/09 00:14:27  todorov
* Working version of building a vector of anchored alignments.
*
* Revision 1.3  2006/11/08 17:42:10  todorov
* Working version extracting complete stats.
*
* Revision 1.2  2006/11/06 19:57:36  todorov
* Added comments.
*
* Revision 1.1  2006/10/19 17:12:41  todorov
* Initial revision.
*
* ===========================================================================
*/
