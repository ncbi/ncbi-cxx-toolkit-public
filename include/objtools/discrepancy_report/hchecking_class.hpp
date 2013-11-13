#ifndef CHECKING_CLASS
#define CHECKING_CLASS

/*  $Id$
 * ==========================================================================
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
 * ==========================================================================
 *
 * Author:  Jie Chen
 *
 * File Description:
 *   headfile of cpp discrepancy report checking class
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <connect/ncbi_core_cxx.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/seq_feat_handle.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objects/submit/Submit_block.hpp>

#include <objtools/discrepancy_report/hDiscRep_tests.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

   class CCheckingClass 
   {
      public:
         CCheckingClass();

         void CheckBioseq( CBioseq& bioseq );
         void CheckSeqInstMol(CSeq_inst& seq_inst, CBioseq& bioseq); // not used
         void CheckSeqEntry ( CRef <CSeq_entry> seq_entry);
         void CheckBioseqSet ( CBioseq_set& bioseq_set);

         void CollectRepData();

         bool HasLocusTag (void) {
               return has_locus_tag;
         };

         int GetNumBioseq(void) {
             return num_bioseq;
         };

         template < class T >
         void GoTests(vector <CRef < CTestAndRepData> >& test_category, const T& obj) 
         {
            NON_CONST_ITERATE (vector <CRef <CTestAndRepData> >, it, test_category)
{
cerr << "GetNAme " << (*it)->GetName() << endl;
// if ( (*it)->GetName() == "MISSING_STRUCTURED_COMMENT")
                  (*it)->TestOnObj(obj);
 cerr << "done\n";
}
         };

         void GoGetRep(vector < CRef < CTestAndRepData> >& test_category);

         static bool SortByFrom(const CSeq_feat* seqfeat1, const CSeq_feat* seqfeat2);

      private:
         void x_Clean();

         bool CanGetOrgMod(const CBioSource& biosrc);
         void CollectSeqdescFromSeqEntry(const CSeq_entry_Handle& seq_entry_h);

         SAnnotSelector sel_seqfeat, sel_seqfeat_4_seq_entry;
         vector <CSeqdesc :: E_Choice> sel_seqdesc, sel_seqdesc_4_bioseq;

         int num_bioseq;
         bool has_locus_tag;

         vector < vector <const CSeq_feat*> * >  m_vec_sf;
         vector < vector <const CSeqdesc*> * >   m_vec_desc;
         vector < vector <const CSeq_entry*> * > m_vec_entry; 
   };

END_NCBI_SCOPE

#endif // CHECKING_CLASS
