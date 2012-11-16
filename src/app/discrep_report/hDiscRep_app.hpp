#ifndef _HDiscRepApp_HPP
#define _HDiscRepApp_HPP

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
 *   main headfile of cpp discrepancy report
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbiexpt.hpp>

#include <connect/ncbi_core_cxx.hpp>

// Objects includes
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/macro/Suspect_rule.hpp>
#include <objects/macro/Suspect_rule_set.hpp>
#include <objects/submit/Submit_block.hpp>

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

#include <common/test_assert.h>

using namespace ncbi;
using namespace objects;

BEGIN_NCBI_SCOPE

namespace DiscRepNmSpc{

     typedef map < string, string> Str2Str;
     typedef map < string, vector < const CSerialObject* > > Str2Objs;
     typedef map < string, vector < string > > Str2Strs;

     class CDiscRepApp : public CNcbiApplication
     {
       public:
         virtual void Init(void);
         virtual int  Run (void);
         
         void CheckThisSeqEntry(CRef <CSeq_entry> seq_entry);
     };

     typedef struct loginfo 
     {
       ofstream fout;
       bool data_in_log;
       string display_title;
       string path;
     } LogInfo;


     typedef void (*ClickableCallback) (list <void *> item_list, void *userdata);
     typedef void (*ClickableCallbackDataFree) (void * userdata);

     class CClickableItem  : public CObject
     {
       public:
          CClickableItem () : item_list(), callback_data(0),chosen(false), 
                              subcategories(),
                              expanded(false), level(0),
                              autofix_data(0), next_sibling(false) {};
          ~CClickableItem () {};

          string                               setting_name;
          string                               description;
          vector < string >                    item_list;
          void *                               callback_data;
          bool                                 chosen;
          vector < CRef <CClickableItem > >    subcategories;
          bool                                 expanded;
          int                                  level;
          void *                               autofix_data; 
          bool                                 next_sibling;
          void (*autofix_func)(list <void *> item_list, void *userdata, 
                                                             LogInfo& lip);  
          void (*callback_func)(list <void *> item_list, void *userdata); 
          void (*datafree_func)(void *userdata); 
     };
   
     class COutputConfig {
 
        public:
           bool     use_flag;
           ofstream output_f;
           bool     summary_report;
     };

     typedef map<string, unsigned>  Str2UInt;
     class CDiscRepInfo
     {
        public:
 
           static CRef < CScope >                   scope;
           static string                            infile;
           static COutputConfig                      output_config;
           static vector < CRef < CClickableItem > > disc_report_data;
           static Str2Strs                           test_item_list;
           static CRef < CSuspect_rule_set>          suspect_rules;
           static vector <string> 		     weasels;
           static CConstRef <CSubmit_block>          submit_block;
           static string                             expand_defline_on_set;
           static string                             report_lineage;
           static vector <string>                    strandsymbol;
           static bool                               exclude_dirsub;
           static string                             report;
           static Str2UInt                           rRNATerms;

           static vector <string>                    bad_gene_names_contained;
           static vector <string>                    no_multi_qual;
           static vector <string>                    rrna_standard_name; 
           static vector <string>                    short_auth_nms; 
           static vector <string>                    spec_words_biosrc; 
           static vector <string>                    suspicious_notes;
           static vector <string>                    trna_list; 
           static Str2UInt                           desired_aaList;
     };

/*
     class CRuleProperties :: CObject
     {
        public:
           CRuleProperties () {};
           ~CRuleProperties () {};

           static vector <bool>                      sch_func_empty;
           bool IsSrchFuncEmpty();

     };
*/
};

END_NCBI_SCOPE

#endif
