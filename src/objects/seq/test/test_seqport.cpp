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
 * Author:  Clifford Clausen
 *          (also reviewed/fixed/groomed by Denis Vakatov)
 * File Description:
 *   
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.4  2002/01/10 20:34:15  clausen
 * Added tests for GetIupacaa3, GetCode, and GetIndex
 *
 * Revision 1.3  2001/12/07 18:52:05  grichenk
 * Updated "#include"-s and forward declarations to work with the
 * new datatool version.
 *
 * Revision 1.2  2001/11/13 12:14:29  clausen
 * Changed call to CGencode::Translate to reflect new type for code breaks
 *
 * Revision 1.1  2001/08/24 00:44:05  vakatov
 * Initial revision
 *
 * ===========================================================================
 */

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>

#include <serial/serial.hpp>
#include <serial/objistr.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/NCBI2na.hpp>
#include <objects/seq/NCBI4na.hpp>
#include <objects/seq/IUPACna.hpp>
#include <objects/seq/IUPACaa.hpp>
#include <objects/seq/NCBIeaa.hpp>
#include <objects/seq/NCBIstdaa.hpp>
#include <objects/seq/Seq_inst.hpp>

#include <objects/seq/gencode.hpp>
#include <objects/seq/seqport_util.hpp>

#include <time.h>


USING_NCBI_SCOPE;
USING_SCOPE(objects);

#define MAX_DISPLAY 48


/////////////////////////////////////////////////////////////////////////////
//

class CSeqportTestApp : public CNcbiApplication
{
public:
    virtual void Init();
    virtual int  Run(void);
    virtual void Exit();

    void DisplaySeq(const CSeq_data& seq, unsigned int uSize=48);

    void Test(const CSeq_data&     in_seq,
              CSeq_data::E_Choice  to_code,
              bool                 bAmbig);

    void ManualTest();

    void AsnTest();

    void GetSeqentryData(vector<CRef<CSeq_data> >&  seqs, 
                         CRef<CSeq_entry>&          seq_entry);

    void GetBioseqData(vector<CRef<CSeq_data> >&    seqs,
                       CRef<CBioseq>&               seq); 
                       
    void GetBioseqSetData(vector<CRef<CSeq_data> >& seqs,
                          CRef<CBioseq_set>&        seq_set); 
};




/////////////////////////////////////////////////////////////////////////////
//


void CSeqportTestApp::Init()
{
    return;
}

int CSeqportTestApp::Run()
{
    cout << "Manual or ASN (0-Exit, 1-Manual, 2-ASN)? ";
    int nResponse = 1;
    cin >> nResponse;
    switch(nResponse){
    case 1:
        ManualTest();
        break;
    case 2:
        AsnTest();
        break;
    default:
        break;
    }
    
    return 0;
}


void CSeqportTestApp::AsnTest()
{
    //Load ASN file. Only works for Seq-entry
    string fname;
    cout << "Enter file name: ";
    cin >> fname;
    auto_ptr<CObjectIStream>
        asn_in(CObjectIStream::Open(fname.c_str(), eSerial_AsnText));
    CRef<CSeq_entry> seq_entry(new CSeq_entry);
    *asn_in >> *seq_entry;
    
    //Get a vector of CSeq_data
    vector<CRef<CSeq_data> > seqs;
    GetSeqentryData(seqs, seq_entry);
    
    //Loop through seqs and test.
    iterate (vector<CRef<CSeq_data> >, i_seqs, seqs) {
	    
        //Display in_seq type
        switch((*i_seqs)->Which()){
        case CSeq_data::e_Iupacna:
            cout << "in_seq is Iupacna" << endl;
            break;
        case CSeq_data::e_Ncbi2na:
            cout << "in_seq is Ncbi2na" << endl;
            break;
        case CSeq_data::e_Ncbi4na:
            cout << "in_seq is Ncbi4na" << endl;
            break;
        case CSeq_data::e_Ncbieaa:
            cout << "in_seq is Ncbieaa" << endl;
            break;
        case CSeq_data::e_Ncbistdaa:
            cout << "in_seq is Ncbistdaa" << endl;
            break;
        case CSeq_data::e_Iupacaa:
            cout << "in_seq is Iupacaa" << endl;
            break;
        default:
            cout << "in_seq type not supported" << endl;
            break;
        }
	
	
        cout << "Choice? (0-Quit, 1-Continue, 2-Skip): ";
        int nResponse = 1;
        cin >> nResponse;
        bool bSkip;
        switch(nResponse){
        case 0:
            return;
        case 1:
            bSkip = false;
            break;
        case 2:
            bSkip = true;
            break;
        default:
            bSkip = true;
            break;
        }
	
        if(bSkip)
            continue;
	    
        //if seq is Ncbi2na or Ncbi4na, ask if want 
        //converted to iupacna for Translate
        if(((*i_seqs)->Which() == CSeq_data::e_Ncbi2na)  ||
           ((*i_seqs)->Which() == CSeq_data::e_Ncbi4na)) {
            cout << "Convert in_seq to Iupacna? (0-No, 1-Yes): ";
            cin >> nResponse;
            if (nResponse == 1) {
                CRef<CSeq_data> in_seq(new CSeq_data);
                CSeqportUtil::GetCopy(**i_seqs, in_seq.GetPointer(), 0, 0);
                CSeqportUtil::Convert(*in_seq.GetPointer(), 
                           (*i_seqs).GetPointer(), 
                           CSeq_data::e_Iupacna,
                           0,
                           0);
            }
        }
	    
        //Enter out sequence type
        int nOutSeqType = -1;
        while(nOutSeqType<0) {
            cout << "Enter out sequence type " << endl;
            cout <<
                "(0=ncbi2na, 1=ncbi4na, 2=iupacna, 3=ncbieaa, "
                "4=ncbistdaa, 5=iupacaa): ";
            cin >> nOutSeqType;
            if(nOutSeqType > 5)
                nOutSeqType = -1;
        } 
        bool bAmbig = false;
        cout << "Randomly disambiguate (1 = Yes, 0 = No)? ";
        unsigned int nAmbig = 0;
        cin >> nAmbig;
        if(nAmbig != 0)
            bAmbig = true;
	  
        CSeq_data::E_Choice to_code;	 
        switch(nOutSeqType) {
        case 0:
            to_code = CSeq_data::e_Ncbi2na;
            break;
        case 1:
            to_code = CSeq_data::e_Ncbi4na;
            break;
        case 2:
            to_code = CSeq_data::e_Iupacna;
            break;
        case 3:
            to_code = CSeq_data::e_Ncbieaa;
            break;
        case 4:
            to_code = CSeq_data::e_Ncbistdaa;
            break;
        case 5:
            to_code = CSeq_data::e_Iupacaa;
            break;
        }	    
    
        Test(**i_seqs, to_code, bAmbig);
    }
}


void CSeqportTestApp::GetSeqentryData(vector<CRef<CSeq_data> >&    seqs, 
                                      CRef<CSeq_entry>&            seq_entry)
{
    //Determine type of seq_entry
    CSeq_entry::E_Choice eType = seq_entry->Which();
    
    switch(eType){  
    case CSeq_entry::e_Seq:
        {
            CRef<CBioseq> seq(&seq_entry->GetSeq());
            GetBioseqData(seqs, seq);
            break;
        }
    case CSeq_entry::e_Set:
        {
            CRef<CBioseq_set> seq_set(&seq_entry->GetSet());
            GetBioseqSetData(seqs, seq_set);
            break;
        }
    default:
        break;
    }  
}


void CSeqportTestApp::GetBioseqData(vector<CRef<CSeq_data> >&   seqs,
                                    CRef<CBioseq>&              seq)
{
    //Get the CSeq_inst
    CRef<CSeq_inst> seq_inst(&seq->SetInst());
    
    //If CSeq_data is set get the data and put into seqs
    if(seq_inst->IsSetSeq_data())
        {
            CRef<CSeq_data> seq_data(&seq_inst->SetSeq_data());
            seqs.push_back(seq_data);
        }
}


void CSeqportTestApp::GetBioseqSetData(vector<CRef<CSeq_data> >&     seqs,
                                       CRef<CBioseq_set>&            seq_set)
{
    //Get the list of seq-entries
    list<CRef<CSeq_entry> >& entry_list = seq_set->SetSeq_set();
    
    //Loop through the entry_list and call GetSeqentryData
    //for each item in the list
    non_const_iterate (list<CRef<CSeq_entry> >, i_set, entry_list) {
        GetSeqentryData(seqs, *i_set);
    }
}


void CSeqportTestApp::ManualTest()
{
    //Declare local variables
    string strInSeq;
    unsigned int nTimes=0, uSeqLen=0, uCnt=0, uOverhang=0;
    int nInSeqType, nOutSeqType;
    CSeq_data::E_Choice to_code;
    bool bAmbig;
    
  
    //Create in CSeq_data 
    CRef<CSeq_data> in_seq(new CSeq_data); 
    
    //Enter in sequence type
    nInSeqType = -1;
    while(nInSeqType<0) {
        cout << "Enter in sequence type " << endl;
        cout <<
            "(0=ncbi2na, 1=ncbi4na, 2=iupacna, 3=ncbieaa,"
            " 4=ncbistdaa, 5=iupacaa): ";
        cin >> nInSeqType;
        if(nInSeqType > 5)
            nInSeqType = -1;
    }  
  
    //Enter out sequence type
    nOutSeqType = -1;
    while(nOutSeqType<0) {
        cout << "Enter out sequence type " << endl;
        cout <<
            "(0=ncbi2na, 1=ncbi4na, 2=iupacna, 3=ncbieaa,"
            " 4=ncbistdaa, 5=iupacaa): ";
        cin >> nOutSeqType;
        if(nOutSeqType > 5)
            nOutSeqType = -1;
    } 
    bAmbig = false;
    if(((nInSeqType==1) || (nInSeqType==2)) && (nOutSeqType==0))
        {
            cout << "Randomly disambiguate (1 = Yes, 0 = No)? ";
            unsigned int nAmbig = 0;
            cin >> nAmbig;
            if(nAmbig!=0)
                bAmbig = true;
        }

    switch(nOutSeqType){
    case 0:
        to_code = CSeq_data::e_Ncbi2na;
        break;
    case 1:
        to_code = CSeq_data::e_Ncbi4na;
        break;
    case 2:
        to_code = CSeq_data::e_Iupacna;
        break;
    case 3:
        to_code = CSeq_data::e_Ncbieaa;
        break;
    case 4:
        to_code = CSeq_data::e_Ncbistdaa;
        break;
    case 5:
        to_code = CSeq_data::e_Iupacaa;
        break;
    }
  
    //Enter desired sequence length
    uSeqLen = 0;
    cout << "Enter desired sequence length: ";
    cin >> uSeqLen;
      
    //Get input depending on type of input sequence
    if(nInSeqType == 2 || nInSeqType == 3 || nInSeqType == 5) {
        //Enter an in sequence string
        string str = "";
        cout << "Enter an input sequence string: " << endl;
        cin >> str;
        if(str.size() > 0) {
            nTimes = uSeqLen/str.size();
            uOverhang = uSeqLen % str.size();
        }
        in_seq->Reset();
        int nSize = str.size();
        const char* s = str.c_str();
        switch(nInSeqType) {
        case 2:
            in_seq->SetIupacna().Set().resize(uSeqLen);
            for(unsigned int i=0; i<nTimes; i++){               
                in_seq->SetIupacna().Set().replace
                    (i*nSize,nSize,s);
                string tst = in_seq->SetIupacna().Set();
            } 
            if (uOverhang > 0) {         
                in_seq->SetIupacna().Set().replace
                    (nTimes*str.size(),uOverhang,str.substr(0,uOverhang)); 
            } 
            break;
        case 3:
            in_seq->SetNcbieaa().Set().resize(uSeqLen);
            for(unsigned int i=0; i<nTimes; i++) {
                in_seq->SetNcbieaa().Set().replace
                    (i*nSize,nSize,s);
            }
            if (uOverhang > 0) {         
                in_seq->SetNcbieaa().Set().replace
                    (nTimes*str.size(),uOverhang,str.substr(0,uOverhang)); 
            }    
            break;
        case 5:
            in_seq->SetIupacaa().Set().resize(uSeqLen);
            for(unsigned int i=0; i<nTimes; i++) {
                in_seq->SetIupacaa().Set().replace
                    (i*nSize,nSize,s);
            }            
            if (uOverhang > 0) {         
                in_seq->SetIupacaa().Set().replace
                    (nTimes*str.size(),uOverhang,str.substr(0,uOverhang));
            }
            break;
        }
    }
    else {
        //Enter an in sequence vector
        cout << "Enter an input sequence vector in hex (end with -1): "
             << endl;
        int nIn = 0;
        vector<char> v;
        uCnt = 0;
        while(uCnt < uSeqLen)
            {
                cin >> std::hex >> nIn;
                if(nIn < 0 || nIn > 255) {
                    break;
                }
                v.push_back(nIn);
                switch(nInSeqType){
                case 0:
                    uCnt+=4;
                    break;
                case 1:
                    uCnt+=2;
                    break;
                default:
                    uCnt++;
                }
            }
        if(v.size() == 0) {
            cout << "No input sequence given. Setting to 0." << endl;
            v.push_back(0);
        }
        unsigned int uInSize;
        switch(nInSeqType) {
        case 0:
            if((uSeqLen % 4) != 0) {
                uSeqLen += (4-(uSeqLen % 4));
            }
            uInSize = 4*v.size();
            if(uSeqLen % uInSize == 0) {
                nTimes = uSeqLen/uInSize;
            }
            else {
                nTimes = uSeqLen/uInSize + 1;
            }
            break;
        case 1:
            if((uSeqLen % 2) != 0) {
                uSeqLen += (2-(uSeqLen % 2));
            }
            uInSize = 2*v.size();
            if(uSeqLen % uInSize == 0) {
                nTimes = uSeqLen/uInSize;
            }
            else {
                nTimes = uSeqLen/uInSize + 1;
            }
            break;
        case 4:
            if(uSeqLen % v.size() == 0) {
                nTimes = uSeqLen/v.size();
            }
            else {
                nTimes = uSeqLen/v.size() + 1;
            }
            break;
        }
        
        switch(nInSeqType){
        case 0:
            for(unsigned int i=0; i<nTimes; i++) {
                iterate (vector<char>, j, v) {
                    in_seq->SetNcbi2na().Set().push_back(*j);
                }
            }
            break;
        case 1:
            for(unsigned int i=0; i<nTimes; i++) {
                iterate (vector<char>, j, v) {
                    in_seq->SetNcbi4na().Set().push_back(*j);
                }
            }
            break;
        case 4:
            for(unsigned int i=0; i<nTimes; i++) {
                iterate (vector<char>, j, v) {
                    in_seq->SetNcbistdaa().Set().push_back(*j);
                }
            }
            break;
        }
    }
  
    Test(*in_seq, to_code, bAmbig);
}

 
void CSeqportTestApp::Test(const CSeq_data&    in_seq,
                           CSeq_data::E_Choice to_code,
                           bool                bAmbig)
{
    string strInSeq;
    unsigned int uSeqLen=0;
    unsigned int uBeginIdx, uLength, uIdx1, uIdx2;
    clock_t s_time, e_time;
    double d_time;
    int nResponse = 1;
  
    //Create out_seq
    CRef<CSeq_data> out_seq(new CSeq_data); 
             
    //Enter Begin Index (relative to biological sequence)
    uBeginIdx = 0;
    cout << "Enter begin index to Convert: ";
    cin >> std::dec >> uBeginIdx;
    
    //Enter Length of sequence to Convert
    uLength = 0;
    cout << "Enter length of sequence to Convert: ";
    cin >> uLength;
  
    // Test CSeqportUtil::GetIupacaa3
    while (true) {
        cout << "Test GetIupacaa3() (1=Yes, 0=No): ";
        cin >> nResponse;
        if (!nResponse) {
            break;
        }
        int ncbistdaa;
        cout << "Enter Ncbistdaa code: ";
        cin >> ncbistdaa;
        string iupacaa3;
        iupacaa3 = CSeqportUtil::GetIupacaa3(ncbistdaa);
        cout << "Iupacaa3 is : " << iupacaa3 << endl;
    }

    //Test CSeqportUtil.GetCode()
    while (true) {
        cout << "Test GetCode (1=Yes, 0=No): ";
        cin >> nResponse;
        if (!nResponse) {
            break;
        }
        
        cout << "Enter code type (0=Iupacna, 1=Iupacaa, 2=Ncibeaa, 3=Ncbistdaa): ";
        int ctype;
        cin >> ctype;
        CSeq_data::E_Choice code_type;
        switch (ctype) {
        case 0:
            code_type = CSeq_data::e_Iupacna;
            break;
        case 1:
            code_type = CSeq_data::e_Iupacaa;
            break;
        case 2:
            code_type = CSeq_data::e_Ncbieaa;
            break;
        case 3:
            code_type = CSeq_data::e_Ncbistdaa;
            break;
        default:
            code_type = CSeq_data::e_not_set;
        }
        if (code_type == CSeq_data::e_not_set) {
            continue;
        }
        
        int idx;
        cout << "Enter index: ";
        cin >> idx;
        string code;
        code = CSeqportUtil::GetCode(code_type, idx);
        cout << "Code is : " << code << endl;
    }

    //Test CSeqportUtil.GetIndex()
    while (true) {
        cout << "Test GetIndex (1=Yes, 0=No): ";
        cin >> nResponse;
        if (!nResponse) {
            break;
        }
        
        cout << "Enter code type (0=Iupacna, 1=Iupacaa, 2=Ncibeaa, 3=Ncbistdaa): ";
        int ctype;
        cin >> ctype;
        CSeq_data::E_Choice code_type;
        switch (ctype) {
        case 0:
            code_type = CSeq_data::e_Iupacna;
            break;
        case 1:
            code_type = CSeq_data::e_Iupacaa;
            break;
        case 2:
            code_type = CSeq_data::e_Ncbieaa;
            break;
        case 3:
            code_type = CSeq_data::e_Ncbistdaa;
            break;
        default:
            code_type = CSeq_data::e_not_set;
        }
        if (code_type == CSeq_data::e_not_set) {
            continue;
        }
        
        string code;
        cout << "Enter code: ";
        cin >> code;
        int idx = CSeqportUtil::GetIndex(code_type, code);
        cout << "Index is : " << idx << endl;
    }

  
    //Do ambigutity checking
    cout << "Get Ambigs (1 = Yes, 0 = No)? ";
    cin >> nResponse;
    if (nResponse) {
        try {
            vector<unsigned int> out_indices;
            uIdx1 = uBeginIdx;
            uIdx2 = uLength;
            s_time = clock();
        
            unsigned int uLen = CSeqportUtil::GetAmbigs
                (in_seq,
                 out_seq,
                 &out_indices,
                 to_code,
                 uIdx1,
                 uIdx2);
        
            e_time = clock();
            d_time = ((double)e_time -(double)s_time)/(double)CLOCKS_PER_SEC;
        
            cout << endl << "Results for GetAmbigs()" << endl;
            cout << "uSeqLen = " << uSeqLen
                 << " GetAmbigs() Seconds = " << d_time << endl;
            cout << "(uBeginIdx, uLength) = (" << uIdx1
                 << ", " << uIdx2 << ")" << endl;
            cout << "Num ambigs = " << out_indices.size() << endl;
            cout << "Return length = " << uLen << endl;
      
            if(out_seq->Which() == CSeq_data::e_Ncbi4na) {
                if (out_indices.size() <= 24) {
                    iterate (vector<unsigned int>, i_idx, out_indices) {
                        cout << (*i_idx) << " ";
                    }
                    cout << endl;
                    const vector<char>& out_seq_data
                        = out_seq->GetNcbi4na().Get();
                    iterate (vector<char>, i_out, out_seq_data) {
                        cout << std::hex
                             << (unsigned short)(unsigned char) *i_out << " ";
                    }
                    cout << endl;
                }
            }
            else if(out_seq->Which() == CSeq_data::e_Iupacna) {
                if(out_indices.size() <= 24) {
                    iterate (vector<unsigned int>, i_idx, out_indices) {
                        cout << (*i_idx) << " ";
                    }
                    cout << endl;
                    string out_seq_data = out_seq->GetIupacna().Get();
                    cout << out_seq_data << endl;
                }
            
            }
            out_indices.clear();
            out_seq->Reset();	  
        }
        STD_CATCH("");
    }
  

    //Do translation
    cout << "Translate (1 = Yes, 0 = No)? ";
    cin >> nResponse;
    if (nResponse) {
        try{
            uIdx1 = 0;
            uIdx2 = 0;
    
            bool bCheck_first = false;
            cout << "Check first codon (1-Yes, 0-No)? ";
            cin >> nResponse;
            bCheck_first = (nResponse == 1);
          
            cout << "Is the start codon possibly missing? (1-Yes, 0-No)? ";
            cin >> nResponse;
            bool bPartial_start = (nResponse == 1);
    
            cout << "Strand (0-Unknown, 1-plus, 2-minus, 3-both,"
                " 4-both_rev, 5-other)? ";
            cin >> nResponse;
            ENa_strand strand;
            switch(nResponse){
            case 0:
                strand = eNa_strand_unknown;
                break;
            case 1:
                strand = eNa_strand_plus;
                break;
            case 2:
                strand = eNa_strand_minus;
                break;
            case 3:
                strand = eNa_strand_both;
                break;
            case 4:
                strand = eNa_strand_both_rev;
                break;
            default:
                strand = eNa_strand_other;
                break;
            }
          
            cout << "Stop translation at first stop codon (1-Yes, 0-No)? ";
            cin >> nResponse;
            bool stop = (nResponse == 1);
            
            cout << "Remove trainling Xs (1-Yes, 0-No)? ";
            cin >> nResponse;
            bool bRemove_trailing_x = (nResponse == 1);
            
            //Enter genetic code and start codon string
            CGenetic_code gencode;
            CGenetic_code::C_E* ce;
            nResponse = 1;
            CRef<CGenetic_code::C_E> rce;
            while (nResponse > 0) {
                cout<< "Enter (0-done, 1-Id, 2-Name, 3-Ncbieaa, 4-Sncbieaa): ";
                cin >> nResponse;
                switch(nResponse){
                case 0:
                    break;
                case 1:
                    {
                        cout << "Ener Id: ";
                        int nId; cin >> nId;
                        ce = new CGenetic_code::C_E;
                        ce->SetId() = nId;
                        rce.Reset(ce);
                        gencode.Set().push_back(rce);
                        break;
                    }
                case 2:
                    {
                        cout << "Enter Name: ";
                        string name; cin >> name;
                        ce = new CGenetic_code::C_E;
                        ce->SetName() = name;
                        rce.Reset(ce);
                        gencode.Set().push_back(rce);
                        break;
                    }
                case 3:
                    {
                        cout << "Enter Ncbieaa string: " << endl;
                        string ncbieaa; cin >> ncbieaa;
                        ce = new CGenetic_code::C_E;
                        ce->SetNcbieaa() = ncbieaa;
                        rce.Reset(ce);
                        gencode.Set().push_back(rce);
                        break;
                    }
                case 4:
                    {
                        cout << "Enter Sncbieaa string: " << endl;
                        string sncbieaa; cin >> sncbieaa;
                        ce = new CGenetic_code::C_E;
                        ce->SetSncbieaa() = sncbieaa;
                        rce.Reset(ce);
                        gencode.Set().push_back(rce);
                        break;
                    }
                default:
                    cout << "Please enter a choice betwee 0 and 4." << endl;
                }
            }
            CRef<CGenetic_code> genetic_code(&gencode);
    
            CGencode::TCodeBreaks code_breaks;
            nResponse =1;
            while(nResponse >= 1) {
                cout << "Enter a code break index (-1 to quit): ";
                cin >> nResponse;
                if(nResponse >= 0) {
                    cout << "Enter an aa: ";
                    char aa;
                    cin >> aa;
                    code_breaks.push_back
                        (make_pair(static_cast<unsigned int>(nResponse), aa));
                }
            }       
          
            s_time = clock();

            CGencode::Translate
                (in_seq,
                 out_seq,
                 *genetic_code,
                 code_breaks,
                 uBeginIdx, 
                 uLength,
                 bCheck_first,
                 bPartial_start,
                 strand,
                 stop,
                 bRemove_trailing_x); 
    
    
            e_time = clock();
            d_time = ((double)e_time -(double)s_time)/(double)CLOCKS_PER_SEC;
            cout << endl << "Translate Results" << endl;
            cout << "uLength = " << uLength  << " Seconds = " << d_time
                 << endl;
      
            //Print the out sequence
            cout << "(uBeginIdx, uLength) = (" << uBeginIdx
                 << ", " << uLength << ")" << endl;
            cout << "out_seq is: " << endl;
            DisplaySeq(*out_seq, MAX_DISPLAY);
            
            out_seq->Reset();
            code_breaks.clear();
        }
        STD_CATCH("");
    }
    
    //Do conversion
    cout << "Convert (1 = Yes, 0 = No)? ";
    cin >> nResponse;
    if(nResponse) {
        try{ 
            uIdx1 = uBeginIdx;
            uIdx2 = uLength; 
            s_time = clock();
            unsigned int uLen = CSeqportUtil::Convert
                (in_seq, out_seq, to_code, uIdx1, uIdx2, bAmbig);

            e_time = clock();
            d_time = (double) (e_time - s_time) / (double) CLOCKS_PER_SEC;
            cout << endl << "Conversion Results" << endl;
            cout << "uSeqLen = " << uSeqLen  << " Seconds = " << d_time
                 << endl;
            cout << "Return length = " << uLen << endl;
            
            //Write input and ouput sequences if uSeqLen <= 50
            //Print uBeginIdx and uLength
            cout << "(uBeginIdx, uLength) = (" << uBeginIdx
                 << ", " << uLength << ")" << endl;
            
            //Print the in sequence
            cout << "Input sequence is: " << endl;
            DisplaySeq(in_seq, MAX_DISPLAY);
          
            //Print the out sequence
            cout << "Output sequence is: " << endl;
            DisplaySeq(*out_seq, MAX_DISPLAY);
                 
            //Reset output sequence
            out_seq->Reset();
          
        } 
        STD_CATCH("");
    }
    
  
    //Do Append
    cout << "Append (1 = Yes, 0 = No)?";
    cin >> nResponse;
    if(nResponse) {
        try{
            cout << "(uBeginIdx1, uLength1) = (" << uBeginIdx << "," << uLength << ")" << endl;
            cout << "Enter uBeginIdx2: ";
            cin >> uIdx1;
            cout << "Enter uLength2: ";
            cin >> uIdx2;
            s_time = clock();
            CRef<CSeq_data> in_seq1(new CSeq_data);
            CRef<CSeq_data> in_seq2(new CSeq_data);
            unsigned int uIdxa=0, uIdxb=0;
            CSeqportUtil::GetCopy(in_seq, in_seq1, uIdxa, uIdxb);
            CSeqportUtil::GetCopy(in_seq, in_seq2, uIdxa, uIdxb);
            unsigned int uLen = CSeqportUtil::Append(out_seq.GetPointer(),
                                          *in_seq1.GetPointer(),
                                          uBeginIdx, uLength,
                                          *in_seq2.GetPointer(), uIdx1, uIdx2);
            e_time = clock();
            d_time = ((double)e_time -(double)s_time)/(double)CLOCKS_PER_SEC;
        
            cout << endl << "Append Results" << endl;
            cout << "uSeqLen = " << uSeqLen 
                 << " Append Seconds = " << d_time << endl;
            cout << "(uBeginIdx1, uLength1) = (" << uBeginIdx
                 << ", " << uLength << ")" << endl;
            cout << "(uBeginIdx2, uLength2) = (" << uIdx1
                 << ", " << uIdx2 << ")" << endl;
            cout << "Return length = " << uLen << endl;
      
            //Print the Append out_seq
            cout << "Append output sequence is: " << endl;
            DisplaySeq(*out_seq, MAX_DISPLAY);
            
            //Reset out_seq to free memory
            out_seq->Reset();
        }
        STD_CATCH("");
    }
  
    //Do in-place reverse-complement
    cout << "In-place reverse-complement (1 = Yes, 0 = No)?";
    cin >> nResponse;
    if(nResponse) {
        try{
            uIdx1 = uIdx2 = 0;
            CSeqportUtil::GetCopy(in_seq, out_seq, uIdx1, uIdx2);
            uIdx1 = uBeginIdx;
            uIdx2 = uLength;
            s_time = clock();
            unsigned int uLen
                = CSeqportUtil::ReverseComplement(out_seq, uIdx1, uIdx2);
            e_time = clock();
            d_time = (double)(e_time - s_time) / (double)CLOCKS_PER_SEC;
        
            cout << endl << "In-place reverse complement results" << endl;
            cout << "uSeqLen = " << uSeqLen
                 << " RevComp Seconds = " << d_time << endl;
            cout << "(uBeginIdx, uLength) = (" << uIdx1
                 << ", " << uIdx2 << ")" << endl;
            cout << "Return length = " << uLen << endl;
      
            //Print the In-place reverse-complement sequence
            cout << "In-place reverse-complement sequence is: " << endl;
            DisplaySeq(*out_seq, MAX_DISPLAY);
            
            //Reset out_seq to free memory
            out_seq->Reset();
        }
        STD_CATCH("");
    }
  
    //Do reverse-complement in a copy
    cout << "Reverse-complement in a copy (1 = Yes, 0 = No)?";
    cin >> nResponse;
    if(nResponse) {
        try{
            uIdx1 = uBeginIdx;
            uIdx2 = uLength;
            s_time = clock();
            unsigned int uLen = CSeqportUtil::ReverseComplement
                (in_seq, out_seq, uIdx1, uIdx2);
            e_time = clock();
            d_time = ((double)e_time -(double)s_time)/(double)CLOCKS_PER_SEC;
        
            cout << endl << "Reverse-complement Results" << endl;
            cout << "uSeqLen = " << uSeqLen
                 << " Rev_Comp Seconds = " << d_time << endl;
            cout << "(uBeginIdx, uLength) = (" << uIdx1
                 << ", " << uIdx2 << ")" << endl;
            cout << "Return length = " << uLen << endl;
      
            //Print the reverse-complement sequence
            cout << "Reverse-complement sequence is: " << endl;
            DisplaySeq(*out_seq, MAX_DISPLAY);
            
            //Reset out_seq to free memory
            out_seq->Reset();
        }
        STD_CATCH("");
    }
  
  
    //Do in-place reverse
    cout << "In-place reverse (1 = Yes, 0 = No)?";
    cin >> nResponse;
    if(nResponse) {
        try{
            uIdx1 = uIdx2 = 0;
            CSeqportUtil::GetCopy(in_seq, out_seq, uIdx1, uIdx2);
            uIdx1 = uBeginIdx;
            uIdx2 = uLength;
            s_time = clock();
            unsigned int uLen = CSeqportUtil::Reverse(out_seq, uIdx1, uIdx2);
            e_time = clock();
            d_time = ((double)e_time -(double)s_time)/(double)CLOCKS_PER_SEC;
        
            cout << endl << "In-place Reverse Results" << endl;
            cout << "uSeqLen = " << uSeqLen
                 << " Reverse Seconds = " << d_time << endl;
            cout << "(uBeginIdx, uLength) = (" << uIdx1
                 << ", " << uIdx2 << ")" << endl;
            cout << "Return length = " << uLen << endl;
      
            //Print the In-place reverse sequence
            cout << "In-place reverse sequence is: " << endl;
            DisplaySeq(*out_seq, MAX_DISPLAY);
            
            //Reset out_seq to free memory
            out_seq->Reset();
        }
        STD_CATCH("");
    }
  
    //Do reverse in a copy
    cout << "Reverse in a copy (1 = Yes, 0 = No)?";
    cin >> nResponse;
    if(nResponse) {
        try{
            uIdx1 = uBeginIdx;
            uIdx2 = uLength;
            s_time = clock();
            unsigned int uLen = CSeqportUtil::Reverse
                (in_seq, out_seq, uIdx1, uIdx2);
            e_time = clock();
            d_time = (double) (e_time - s_time) / (double)CLOCKS_PER_SEC;
        
            cout << endl << "Reverse Results" << endl;
            cout << "uSeqLen = " << uSeqLen
                 << " Reverse Seconds = " << d_time << endl;
            cout << "(uBeginIdx, uLength) = (" << uIdx1 <<
                ", " << uIdx2 << ")" << endl;
            cout << "Return length = " << uLen << endl;
      
            //Print the reverse sequence
            cout << "Reverse sequence is: " << endl;
            DisplaySeq(*out_seq, MAX_DISPLAY);
            
            //Reset out_seq to free memory
            out_seq->Reset();
        }
        STD_CATCH("");
    }
  

    //Do in-place complement
    cout << "In-place complement (1 = Yes, 0 = No)?";
    cin >> nResponse;
    if(nResponse) {
        try{
            uIdx1 = uIdx2 = 0;
            CSeqportUtil::GetCopy(in_seq, out_seq, uIdx1, uIdx2);
            uIdx1 = uBeginIdx;
            uIdx2 = uLength;
            s_time = clock();
            unsigned int uLen = CSeqportUtil::Complement
                (out_seq, uIdx1, uIdx2);
            e_time = clock();
            d_time = ((double)e_time -(double)s_time)/(double)CLOCKS_PER_SEC;
        
            cout << endl << "In-place Complement Results" << endl;
            cout << "uSeqLen = " << uSeqLen
                 << " Complement Seconds = " << d_time << endl;
            cout << "(uBeginIdx, uLength) = (" << uIdx1 <<
                ", " << uIdx2 << ")" << endl;
            cout << "Return length = " << uLen << endl;
      
            //Print the In-place complement sequence
            cout << "In-place complement sequence is: " << endl;
            DisplaySeq(*out_seq, MAX_DISPLAY);
            
            //Reset out_seq to free memory
            out_seq->Reset();
        }
        STD_CATCH("");
    }
  
    //Do complement in a copy
    cout << "Complement in a copy (1 = Yes, 0 = No)?";
    cin >> nResponse;
    if(nResponse) {
        try{
            uIdx1 = uBeginIdx;
            uIdx2 = uLength;
            s_time = clock();
            unsigned int uLen = CSeqportUtil::Complement
                (in_seq, out_seq, uIdx1, uIdx2);
            e_time = clock();
            d_time = ((double)e_time -(double)s_time)/(double)CLOCKS_PER_SEC;
        
            cout << endl << "Complement Results" << endl;
            cout << "uSeqLen = " << uSeqLen
                 << " Complement Seconds = " << d_time << endl;
            cout << "(uBeginIdx, uLength) = (" << uIdx1 <<
                ", " << uIdx2 << ")" << endl;
            cout << "Return length = " << uLen << endl;
      
            //Print the complement sequence
            cout << "Complement sequence is: " << endl;
            DisplaySeq(*out_seq, MAX_DISPLAY);
            
            //Reset out_seq to free memory
            out_seq->Reset();
        }
        STD_CATCH("");
    }
      
  
    //Do fast validation of input sequence
    cout << "FastValidate (1 = Yes, 0 = No)? ";
    cin >> nResponse;
    if(nResponse) {
        try{
            uIdx1 = uBeginIdx;
            uIdx2 = uLength;
            s_time = clock();
            bool isValid;
        
            isValid = CSeqportUtil::FastValidate(in_seq, uIdx1, uIdx2);
            e_time = clock();
            d_time = ((double)e_time -(double)s_time)/(double)CLOCKS_PER_SEC;
        
            cout << endl << "Fast Validation Results" << endl;
            cout << "uSeqLen = " << uSeqLen
                 << " FastValidate Seconds = " << d_time << endl;
            cout << "(uBeginIdx, uLength) = (" << uIdx1
                 << ", " << uIdx2 << ")" <<
                endl;
            if(isValid)
                cout << "in_seq is VALID" << endl;
            else
                cout << "in_seq is NOT VALID" << endl;
        }
        STD_CATCH("");
    }
    
  
  
    //Do validation
    cout << "Validate (1 = Yes, 0 = No)? ";
    cin >> nResponse;
    if(nResponse) {
        try{
            uIdx1 = uBeginIdx;
            uIdx2 = uLength;
            s_time = clock();
            vector<unsigned int> badIdx;
            
            CSeqportUtil::Validate(in_seq, &badIdx, uIdx1, uIdx2);
        
            e_time = clock();
            d_time = ((double)e_time -(double)s_time)/(double)CLOCKS_PER_SEC;
        
            cout << endl << "Validation Results" << endl;
            cout << "uSeqLen = " << uSeqLen
                 << " Validate Seconds = " << d_time << endl;
            cout << "(uBeginIdx, uLength) = (" << uIdx1 <<
                ", " << uIdx2 << ")" <<
                endl;
            if(badIdx.size() <= 50 && badIdx.size() > 0)
                {
                    cout << "Bad indices are:" << endl;
                    vector<unsigned int>::iterator itor;
                    for(itor = badIdx.begin(); itor != badIdx.end(); ++itor)
                        cout << *itor << " ";
                    cout << endl;
                }
            else
                {
                    cout << "The number of bad indices are: " << badIdx.size()
                         << endl;
                }
            badIdx.clear();
        }
        STD_CATCH("");
    }
    
  
    //Get copy 
    cout << "Copy (1 = Yes, 0 = No)? ";
    cin >> nResponse;
    if(nResponse) {
        try{
            uIdx1 = uBeginIdx;
            uIdx2 = uLength;
            s_time = clock();
      
            unsigned int uLen = CSeqportUtil::GetCopy
                (in_seq, out_seq, uIdx1, uIdx2);
      
            e_time = clock();
            d_time = ((double)e_time -(double)s_time)/(double)CLOCKS_PER_SEC;
            cout << endl << "Copy Results" << endl;
            cout << "uSeqLen = " << uSeqLen  << " Seconds = " << d_time
                 << endl;
            cout << "Return length = " << uLen << endl;
      
            //Print the out sequence
            cout << "(uBeginIdx, uLength) = (" << uIdx1 <<
                ", " << uIdx2 << ")" << endl;
            cout << "out_seq is: " << endl;
            DisplaySeq(*out_seq, MAX_DISPLAY);
            
            out_seq->Reset();
        }
        STD_CATCH("");
    }
  
    //Get copy 
    cout << "Keep (1 = Yes, 0 = No)? ";
    cin >> nResponse;
    if(nResponse) {
        try{
            uIdx1 = 0;
            uIdx2 = 0;
            CSeqportUtil::GetCopy(in_seq, out_seq, uIdx1, uIdx2);
            s_time = clock();
      
            unsigned int uLen
                = CSeqportUtil::Keep(out_seq.GetPointer(), uBeginIdx, uLength);
      
            e_time = clock();
            d_time = ((double)e_time -(double)s_time)/(double)CLOCKS_PER_SEC;
            cout << endl << "Keep Results" << endl;
            cout << "uLength = " << uLength  << " Seconds = " << d_time
                 << endl;
            cout << "Return length = " << uLen << endl;
      
            //Print the out sequence
            cout << "(uBeginIdx, uLength) = (" << uBeginIdx <<
                ", " << uLength << ")" << endl;
            cout << "out_seq is: " << endl;
            DisplaySeq(*out_seq, MAX_DISPLAY);
            out_seq->Reset();
        }
        STD_CATCH("");
    }
    
  
    //Pack
    cout << "Pack (1 = Yes, 0 = No)? ";
    cin >> nResponse;
    if(nResponse) {
        try{
            uIdx1 = 0;
            uIdx2 = 0;
            CSeqportUtil::GetCopy(in_seq, out_seq, uIdx1, uIdx2);
            s_time = clock();
      
            unsigned int uLen = CSeqportUtil::Pack(out_seq.GetPointer(),
                                        uBeginIdx, uLength);
      
            e_time = clock();
            d_time = ((double)e_time -(double)s_time)/(double)CLOCKS_PER_SEC;
            cout << endl << "Pack Results" << endl;
            cout << "uLength = " << uLength  << " Seconds = " << d_time
                 << endl;
            cout << "Return length = " << uLen << endl;
      
            //Print the out sequence
            cout << "(uBeginIdx, uLength) = (" << uBeginIdx <<
                ", " << uLength << ")" << endl;
            switch(out_seq->Which()){
            case CSeq_data::e_Iupacna:
                cout << "out_seq is Iupacna" << endl;
                break;
            case CSeq_data::e_Ncbi4na:
                cout << "out_seq is Ncbi4na" << endl;
                break;
            case CSeq_data::e_Ncbi2na:
                cout << "out_seq is Ncbi2na" << endl;
                break;
            case CSeq_data::e_Ncbieaa:
                cout << "out_seq is Ncbieaa" << endl;
                break;
            case CSeq_data::e_Ncbistdaa:
                cout << "out_seq is Ncbistdaa" << endl;
                break;
            case CSeq_data::e_Iupacaa:
                cout << "out_seq is Iupacaa" << endl;
                break;
            default:
                cout << "out_seq type is not supported." << endl;
                break;
            }
            cout << "out_seq is: " << endl;
            DisplaySeq(*out_seq, MAX_DISPLAY);
            
            out_seq->Reset();
        }
        STD_CATCH("");
    }  

}


void CSeqportTestApp::Exit() {
    return;
}


void CSeqportTestApp::DisplaySeq(const CSeq_data& seq, unsigned int uSize)
{
    //Print the sequence
    switch (seq.Which()) {
    case CSeq_data::e_Ncbi2na:
        {
            const vector<char>& v = seq.GetNcbi2na().Get();
            if (v.size() <= 12) {
                iterate (vector<char>, i, v) {
                    cout << std::hex << (unsigned short)(unsigned char) *i
                         << " " << endl;
                }
            }
            cout << "seq size = " << v.size() << endl;
            break;
        }
    case CSeq_data::e_Ncbi4na:
        {
            const vector<char>& v = seq.GetNcbi4na().Get();
            if (v.size() <= 24) {
                iterate (vector<char>, i, v) {
                    cout << std::hex << (unsigned short)(unsigned char) *i
                         << " " << endl;
                }
            }
            cout << "seq size = " << v.size() << endl;
            break;
        }
    case CSeq_data::e_Iupacna:
        if(seq.GetIupacna().Get().size() <= uSize)
            cout << seq.GetIupacna().Get() << endl;
        cout << "seq size = " << seq.GetIupacna().Get().size() << endl;
        break;
    case CSeq_data::e_Ncbieaa:
        if(seq.GetNcbieaa().Get().size() <= uSize)
            cout << seq.GetNcbieaa().Get() << endl;;
        cout << "seq size = " << seq.GetNcbieaa().Get().size() << endl;
        break;
    case CSeq_data::e_Ncbistdaa:
        {
            const vector<char>& v = seq.GetNcbistdaa().Get();
            if (v.size() <= uSize) {
                iterate (vector<char>, i, v) {
                    cout << std::hex << (unsigned short)(unsigned char) *i
                         << " " << endl;
                }
            }
            cout << "seq size = " << v.size() << endl;
            break;
        }
    case CSeq_data::e_Iupacaa:
        if(seq.GetIupacaa().Get().size() <= uSize)
            cout << seq.GetIupacaa().Get() << endl;
        cout << "seq size = " << seq.GetIupacaa().Get().size() << endl;
        break;
    default: 
        cout << "Display of requested sequence type not supported" << endl;
    }

}


int main(int argc, const char* argv[])
{
    CSeqportTestApp theApp;
    return theApp.AppMain(argc, argv, 0, eDS_Default, 0, "seqport_test");
}
