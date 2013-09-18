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
* Author:  Igor Filippov
*
* File Description:
*   Simple program to analyze possible shift
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <sstream>
#include <iomanip>

#include <cstdlib>
#include <iostream>

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objectio.hpp>
#include <serial/streamiter.hpp>
#include <util/compress/stream_util.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objects/variation/Variation.hpp>
#include <objects/variation/VariantPlacement.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/misc/sequence_macros.hpp>

#include <util/line_reader.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/data_loaders/blastdb/bdbloader.hpp>
#include <connect/services/neticache_client.hpp>
#include <corelib/rwstream.hpp>

#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/seq_loc_util.hpp>

#include <objtools/variation/variation_utils.hpp>

#include <misc/hgvs/hgvs_reader.hpp>
#include <misc/hgvs/variation_util2.hpp>
#include <objects/genomecoll/genome_collection__.hpp>
#include <objects/genomecoll/genomic_collections_cli.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
using namespace std;


class CAnalyzeShiftApp : public CNcbiApplication 
{
private:
    virtual void Init(void);
    virtual int Run ();
    virtual void Exit(void);
    bool ProcessHGVS(string &expression, CRef<CScope> scope, CHgvsReader &reader,int &pos_left, int &pos_right);
};


void CAnalyzeShiftApp::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),"Analyze Shift in HGVS objects");
    arg_desc->AddDefaultKey("i", "input", "Input file",CArgDescriptions::eInputFile, "-", CArgDescriptions::fPreOpen);
    arg_desc->AddDefaultKey("o", "output", "Output file",CArgDescriptions::eOutputFile,"-",CArgDescriptions::fPreOpen);
    SetupArgDescriptions(arg_desc.release());
}

void CAnalyzeShiftApp::Exit(void)
{
}


bool CAnalyzeShiftApp::ProcessHGVS(string &expression, CRef<CScope> scope, CHgvsReader &reader, int &pos_left, int &pos_right)
{
  vector<CRef<CSeq_annot> > annots;
  stringstream line;
  line << expression;

  reader.ReadSeqAnnots(annots, line);
  if (annots.size() != 1) return false;
  CRef<CSeq_annot> a(new CSeq_annot);
  a->Assign(*annots.front());
//  cout <<  MSerial_AsnText << *a;
  CVariationNormalization::NormalizeVariation(a,CVariationNormalization::eDbSnp2,*scope);
  if (a->GetData().GetFtable().empty() || a->GetData().GetFtable().size() != 1) return false;
  if (!a->GetData().GetFtable().front()->IsSetLocation()) return false;
  const CSeq_loc &loc = a->GetData().GetFtable().front()->GetLocation();
  string ref;
  if (a->GetData().GetFtable().front()->GetData().GetVariation().GetData().IsSet())
  {
      for (CVariation_ref::TData::TSet::TVariations::const_iterator inst = a->GetData().GetFtable().front()->GetData().GetVariation().GetData().GetSet().GetVariations().begin(); 
           inst != a->GetData().GetFtable().front()->GetData().GetVariation().GetData().GetSet().GetVariations().end(); ++inst)
          if ( (*inst)->IsSetData() && (*inst)->GetData().IsInstance() && (*inst)->GetData().GetInstance().IsSetDelta() && (*inst)->GetData().GetInstance().GetDelta().size() == 1 && 
               (*inst)->GetData().GetInstance().GetDelta().front()->IsSetSeq() && (*inst)->GetData().GetInstance().GetDelta().front()->GetSeq().IsLiteral() &&
               (*inst)->GetData().GetInstance().GetDelta().front()->GetSeq().GetLiteral().IsSetSeq_data() && (*inst)->GetData().GetInstance().GetDelta().front()->GetSeq().GetLiteral().GetSeq_data().IsIupacna())
              ref = (*inst)->GetData().GetInstance().GetDelta().front()->GetSeq().GetLiteral().GetSeq_data().GetIupacna().Get();
  }
  else if (a->GetData().GetFtable().front()->GetData().GetVariation().GetData().IsInstance())
      ref = a->GetData().GetFtable().front()->GetData().GetVariation().GetData().GetInstance().GetDelta().front()->GetSeq().GetLiteral().GetSeq_data().GetIupacna().Get();
  else
      return false;

  if (ref.empty()) return false;

  if (loc.IsPnt())
  {
      pos_left = loc.GetPnt().GetPoint();
      pos_right = pos_left;
  }
  else  if (loc.IsInt())
  {
      pos_left = loc.GetInt().GetFrom();
      pos_right = loc.GetInt().GetTo();
  }
  else return false;

  return true;
}


int CAnalyzeShiftApp::Run() 
{

    CArgs args = GetArgs();
    CNcbiIstream& istr = args["i"].AsInputFile();
    CNcbiOstream& ostr = args["o"].AsOutputFile();

    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
    CRef<CScope> scope(new CScope(*object_manager));
    CGBDataLoader::RegisterInObjectManager(*object_manager, NULL, CObjectManager::eDefault, 2);
    scope->AddDefaults();
    
    variation::CVariationUtil util(*scope);
    CGenomicCollectionsService gcservice;
    CConstRef<CGC_Assembly> assembly( gcservice.GetAssembly("GCF_000001405.22", CGCClient_GetAssemblyRequest::eLevel_scaffold)  );
    CHgvsReader reader(*assembly);


    CStreamLineReader line_reader(istr);
    while(!line_reader.AtEOF())
    {
        string line = *(++line_reader);
        if (!line.empty())
        {
            vector<string> tokens;
            NStr::Tokenize(line,"|",tokens);
            string snp_id = tokens[0];
            string hgvs = tokens[1];
            string gi = tokens[2];
            string start = tokens[3];
            string stop	 = tokens[4];
            string isClinical = tokens[5];
            int pos_left = NStr::StringToInt(start);
            int pos_right = NStr::StringToInt(stop);
            int new_pos_left = pos_left;
            int new_pos_right = pos_right;
            if (ProcessHGVS(hgvs,scope,reader,new_pos_left,new_pos_right))
            {
                bool is_shiftable = false;
                if (new_pos_left != pos_left || new_pos_right != pos_right)
                    is_shiftable = true;
                ostr <<snp_id<<"|"<<hgvs<<"|"<<isClinical<<"|"<<is_shiftable<<"|";
                if (is_shiftable)
                    ostr  << pos_left-new_pos_left<<"|"<<new_pos_right-pos_right<<"|"<<new_pos_left<<"|"<<new_pos_right<<"|";
                ostr<<endl;
            }
            else
                ostr << line <<"ERROR_HGVS_PROCESSING"<<endl;
        }
    }

    return 0;
}

int main(int argc, const char* argv[])
{
    CAnalyzeShiftApp AnalyzeShiftApp;
    return AnalyzeShiftApp.AppMain(argc, argv);
}


