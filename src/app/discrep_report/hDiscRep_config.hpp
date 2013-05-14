#ifndef _DiscRepConfig_HPP
#define _DiscRepConfig_HPP


/*  $Id$
 *===========================================================================
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
 *===========================================================================
 *
 * Author:  Jie Chen
 *
 * File Description:
 *   Report configuration headfile for discrepancy report
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <connect/ncbi_core_cxx.hpp>

// Objects includes
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seq/Seq_inst.hpp>

// Object Manager includes
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <serial/objistr.hpp>
#include <serial/serial.hpp>

#include "hDiscRep_app.hpp"
#include "hDiscRep_tests.hpp"

#include <common/test_assert.h>

using namespace ncbi;
using namespace objects;

BEGIN_NCBI_SCOPE

namespace DiscRepNmSpc {

   enum ETestCategoryFlags {
      fDiscrepancy = 1 << 0,
      fOncaller = 1 << 1,
      fMegaReport = 1 << 2,
      fTSA = 1 << 3
   };

   struct s_test_property {
      string name;
      int category;
   };

   class CRepConfig 
   {
     public:
        virtual ~CRepConfig() {};

        void Init(const string& report_type);

        static CRepConfig* factory(const string& report_tp);
        virtual void ConfigRep() = 0; 
        virtual void Export() = 0;
        void CollectTests();
  
        static vector < CRef < CTestAndRepData > > tests_on_Bioseq;
        static vector < CRef < CTestAndRepData > > tests_on_Bioseq_na;
        static vector < CRef < CTestAndRepData > > tests_on_Bioseq_aa;
        static vector < CRef < CTestAndRepData > > tests_on_Bioseq_CFeat; 
        static vector < CRef < CTestAndRepData > > tests_on_Bioseq_CFeat_NotInGenProdSet; 
        static vector < CRef < CTestAndRepData > > tests_on_Bioseq_NotInGenProdSet;
        static vector < CRef < CTestAndRepData > > tests_on_Bioseq_CFeat_CSeqdesc;
        static vector < CRef < CTestAndRepData > > tests_on_SeqEntry;
        static vector < CRef < CTestAndRepData > > tests_on_SeqEntry_feat_desc;
        static vector < CRef < CTestAndRepData > > tests_4_once;
        static vector < CRef < CTestAndRepData > > tests_on_BioseqSet;
        static vector < CRef < CTestAndRepData > > tests_on_SubmitBlk;

     protected:
        set <string> tests_run;

        void WriteDiscRepSummary();
        void WriteDiscRepSubcategories(const vector <CRef <CClickableItem> >& subcategories);
        void WriteDiscRepDetails(vector <CRef < CClickableItem > > disc_rep_dt, 
                                 bool use_flag, bool IsSubcategory=false);
        void WriteDiscRepItems(CRef <CClickableItem> c_item, const string& prefix);
        bool SuppressItemListForFeatureTypeForOutputFiles(const string& setting_name);
        void StandardWriteDiscRepItems(COutputConfig& oc, const CClickableItem* c_item, const string& prefix, bool list_features_if_subcat);
        bool RmTagInDescp(CRef <CClickableItem> c_item, const string& tag);
   };

   class CRepConfDiscrepancy : public CRepConfig
   {
        friend class CRepConfig;

      public:
        virtual ~CRepConfDiscrepancy () {};
        virtual void ConfigRep();
        virtual void Export();
   };

   class CRepConfOncaller : public CRepConfig
   {
        friend class CRepConfig;

      public:
        virtual ~CRepConfOncaller () {};
        virtual void ConfigRep();
        virtual void Export() { };
   };

   class CRepConfAll : public CRepConfig 
   {
        friend class CRepConfig;

      public:
        virtual ~CRepConfAll () {};
        virtual void ConfigRep();
        virtual void Export() { };
   };
};


END_NCBI_SCOPE

#endif
