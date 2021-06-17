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
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>

#include <serial/serial.hpp>
#include <serial/objistr.hpp>

#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/NCBI2na.hpp>
#include <objects/seq/NCBI4na.hpp>
#include <objects/seq/IUPACna.hpp>
#include <objects/seq/IUPACaa.hpp>
#include <objects/seq/NCBIeaa.hpp>
#include <objects/seq/NCBIstdaa.hpp>
#include <objects/seq/Seq_inst.hpp>

#include <objects/seq/seqport_util.hpp>



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

    // Testing methods that require CSeq_data
    void SeqDataTest();
    void GetAmbigsTest(const CSeq_data&     in_seq,
              CSeq_data*           out_seq,
              CSeq_data::E_Choice  to_code,
              TSeqPos              uBeginIdx,
              TSeqPos              uLength);
    void ConvertTest(const CSeq_data&     in_seq,
              CSeq_data*           out_seq,
              CSeq_data::E_Choice  to_code,
              TSeqPos              uBeginIdx,
              TSeqPos              uLength,
              bool                 bAmbig);
    void AppendTest(const CSeq_data&     in_seq,
              CSeq_data*           out_seq,
              TSeqPos              uBeginIdx,
              TSeqPos              uLength);
    void InPlaceReverseComplementTest(const CSeq_data&     in_seq,
              CSeq_data*           out_seq,
              TSeqPos              uBeginIdx,
              TSeqPos              uLength);
    void InCopyReverseComplementTest(const CSeq_data&     in_seq,
              CSeq_data*           out_seq,
              TSeqPos              uBeginIdx,
              TSeqPos              uLength);
    void InPlaceReverseTest(const CSeq_data&     in_seq,
              CSeq_data*           out_seq,
              TSeqPos              uBeginIdx,
              TSeqPos              uLength);
    void InCopyReverseTest(const CSeq_data&     in_seq,
              CSeq_data*           out_seq,
              TSeqPos              uBeginIdx,
              TSeqPos              uLength);
    void InPlaceComplementTest(const CSeq_data&     in_seq,
              CSeq_data*           out_seq,
              TSeqPos              uBeginIdx,
              TSeqPos              uLength);
    void InCopyComplementTest(const CSeq_data&     in_seq,
              CSeq_data*           out_seq,
              TSeqPos              uBeginIdx,
              TSeqPos              uLength);
    void FastValidateTest(const CSeq_data&     in_seq,
              TSeqPos              uBeginIdx,
              TSeqPos              uLength);
    void ValidateTest(const CSeq_data&     in_seq,
              TSeqPos              uBeginIdx,
              TSeqPos              uLength);
    void GetCopyTest(const CSeq_data&     in_seq,
              CSeq_data*           out_seq,
              TSeqPos              uBeginIdx,
              TSeqPos              uLength);
    void KeepTest(const CSeq_data&     in_seq,
              CSeq_data*           out_seq,
              TSeqPos         uBeginIdx,
              TSeqPos         uLength);
    void PackTest(const CSeq_data&     in_seq,
              CSeq_data*           out_seq,
              TSeqPos              uBeginIdx,
              TSeqPos              uLength);
    
    
    // Testing methods that do not require CSeq_data
    void NonSeqDataTest();
    void GetIupacaa3();
    void GetCodeEChoice();
    void GetCodeESeq();
    void GetNameEChoice();    
    void GetNameESeq();
    void GetIndexEChoice();
    void GetIndexESeq();
    void IsCodeAvailableEChoice();
    void IsCodeAvailableESeq();
    void GetCodeIndexFromToEChoice();
    void GetCodeIndexFromToESeq();
    void GetIndexComplementEChoice();
    void GetIndexComplementESeq();
    void GetMapToIndexEChoice();
    void GetMapToIndexESeq();       
};




/////////////////////////////////////////////////////////////////////////////
//


void CSeqportTestApp::Init()
{
    return;
}

int CSeqportTestApp::Run()
{
    while ( true ) {
    
        cout << "Enter type tests to run: " << endl
             << "0) Done" << endl
             << "1) Tests requiring CSeq_dat" << endl
             << "2) Tests not requiring CSeq_dat" << endl; 

        int resp;
        cin >> resp;
        
        switch (resp) {
        case 0:
            return 0;
        case 1:
            SeqDataTest();
            break;
        case 2:
            NonSeqDataTest();
            break;
        }
    }
}


static int GetChoice()
{
    int choice = -1;
    while ( (choice < 0 || choice > 11) && (choice != 99) ) {
        cout << "Enter code type choice:" << endl
             << "0:  Not set" << endl
             << "1:  Iupacna" << endl
             << "2:  Iupacaa" << endl
             << "3:  Ncbi2na" << endl
             << "4:  Ncbi4na" << endl
             << "5:  Ncbi8na" << endl
             << "6:  Ncbipna" << endl
             << "7:  Ncbi8aa" << endl
             << "8:  Ncbieaa" << endl
             << "9:  Ncbipaa" << endl
             << "10: Ncbistdaa" << endl
             << "11: Iupacaa3" << endl
             << "99: Done" << endl;
        cin >> choice;
    }    
    return choice;  
}

static CSeq_data::E_Choice GetEChoice()
{
    int choice = GetChoice();
    
    switch (choice) {
    case 0:
        return CSeq_data::e_not_set;
    case 1:
        return CSeq_data::e_Iupacna;
    case 2:
        return CSeq_data::e_Iupacaa;
    case 3:
        return CSeq_data::e_Ncbi2na;
    case 4:
        return CSeq_data::e_Ncbi4na;
    case 5:
        return CSeq_data::e_Ncbi8na;
    case 6:
        return CSeq_data::e_Ncbipna;
    case 7:
        return CSeq_data::e_Ncbi8aa;
    case 8:
        return CSeq_data::e_Ncbieaa;
    case 9:
        return CSeq_data::e_Ncbipaa;
    case 10:
        return CSeq_data::e_Ncbistdaa;
    case 99:
        throw runtime_error("Done");
    }
    cout << "Requested code not CSeq_data:E_Choice" << endl;
    cout << "Using CSeq_data::e_not_set" << endl;
    return CSeq_data::e_not_set;
}

static ESeq_code_type GetESeqCodeType()
{
    int choice = GetChoice();
    
    switch (choice) {
    case 1:
        return eSeq_code_type_iupacna;
    case 2:
        return eSeq_code_type_iupacaa;
    case 3:
        return eSeq_code_type_ncbi2na;
    case 4:
        return eSeq_code_type_ncbi4na;
    case 5:
        return eSeq_code_type_ncbi8na;
    case 6:
        return eSeq_code_type_ncbipna;
    case 7:
        return eSeq_code_type_ncbi8aa;
    case 8:
        return eSeq_code_type_ncbieaa;
    case 9:
        return eSeq_code_type_ncbipaa;
    case 10:
        return eSeq_code_type_ncbistdaa;
    case 11:
        return eSeq_code_type_iupacaa3;
    case 99:
        throw runtime_error("Done");
    }
    cout << "Requested code no an ESeq_code_type. Using iupacna." << endl;
    return eSeq_code_type_iupacna;
}


void CSeqportTestApp::GetIupacaa3()
{
    unsigned int ncbistdaa;
    cout << "Enter Ncbistdaa index: ";
    cin >> ncbistdaa;
    string iupacaa3;
    iupacaa3 = CSeqportUtil::GetIupacaa3(ncbistdaa);
    cout << "Iupacaa3 is : " << iupacaa3 << endl;
    throw runtime_error("Done");
}

void CSeqportTestApp::GetCodeEChoice()
{   
    CSeq_data::E_Choice code_type = GetEChoice();  
    CSeqportUtil::TIndex idx;
    cout << "Enter index: ";
    cin >> idx;
    string code;
    code = CSeqportUtil::GetCode(code_type, idx);
    cout << "Code is : " << code << endl;
}

void CSeqportTestApp::GetCodeESeq()
{
    ESeq_code_type code_type = GetESeqCodeType();
    int idx;
    cout << "Enter index: ";
    cin >> idx;
    string code;
    code = CSeqportUtil::GetCode(code_type, idx);
    cout << "Code is : " << code << endl;
}

void CSeqportTestApp::GetNameEChoice()
{
    CSeq_data::E_Choice code_type = GetEChoice();
    int idx;
    cout << "Enter index: ";
    cin >> idx;
    string code;
    code = CSeqportUtil::GetName(code_type, idx);
    cout << "Name is : " << code << endl;
}

void CSeqportTestApp::GetNameESeq()
{
    ESeq_code_type code_type = GetESeqCodeType();
    int idx;
    cout << "Enter index: ";
    cin >> idx;
    string code;
    code = CSeqportUtil::GetName(code_type, idx);
    cout << "Name is : " << code << endl;
}

void CSeqportTestApp::GetIndexEChoice()
{
    CSeq_data::E_Choice code_type = GetEChoice();
    string code;
    cout << "Enter code: ";
    cin >> code;
    int idx = CSeqportUtil::GetIndex(code_type, code);
    cout << "Index is : " << idx << endl;
}

void CSeqportTestApp::GetIndexESeq()
{
    ESeq_code_type code_type = GetESeqCodeType();
    string code;
    cout << "Enter code: ";
    cin >> code;
    int idx = CSeqportUtil::GetIndex(code_type, code);
    cout << "Index is : " << idx << endl;
}

void CSeqportTestApp::IsCodeAvailableEChoice()
{
    CSeq_data::E_Choice code_type = GetEChoice();
    bool avail = CSeqportUtil::IsCodeAvailable(code_type);
    cout << "Code availabe is " << avail << endl;
}

void CSeqportTestApp::IsCodeAvailableESeq()
{
    ESeq_code_type code_type = GetESeqCodeType();
    bool avail= CSeqportUtil::IsCodeAvailable(code_type);
    cout << "Code availabe is " << avail << endl;
}

void CSeqportTestApp::GetCodeIndexFromToEChoice()
{
    CSeq_data::E_Choice code_type = GetEChoice();
    CSeqportUtil::TPair from_to = 
        CSeqportUtil::GetCodeIndexFromTo(code_type);
    cout << "From is " << from_to.first
         << ": To is " << from_to.second << endl;
}

void CSeqportTestApp::GetCodeIndexFromToESeq()
{
    ESeq_code_type code_type = GetESeqCodeType();
    CSeqportUtil::TPair from_to = 
        CSeqportUtil::GetCodeIndexFromTo(code_type);
    cout << "From is " << from_to.first
         << ": To is " << from_to.second << endl;
}

void CSeqportTestApp::GetIndexComplementEChoice()
{
    CSeq_data::E_Choice code_type = GetEChoice();
    cout << "Enter index: ";
    int idx;
    cin >> idx;
    int c_idx = CSeqportUtil::GetIndexComplement(code_type, idx);
    cout << "Complement for index " << idx << " is " << c_idx << endl;
}

void CSeqportTestApp::GetIndexComplementESeq()
{
    ESeq_code_type code_type = GetESeqCodeType();
    cout << "Enter index: ";
    int idx;
    cin >> idx;
    int c_idx = CSeqportUtil::GetIndexComplement(code_type, idx);
    cout << "Complement for index " << idx << " is " << c_idx << endl;
}

void CSeqportTestApp::GetMapToIndexEChoice()
{
    cout << "From type: " << endl;
    CSeq_data::E_Choice from_type = GetEChoice();
    cout << "To type: " << endl;
    CSeq_data::E_Choice to_type = GetEChoice();
    cout << "Enter from index: ";
    int from_idx;
    cin >> from_idx;
    int to_idx = CSeqportUtil::GetMapToIndex(from_type, to_type, from_idx);
    cout << "From index maps to " << to_idx << endl;
}

void CSeqportTestApp::GetMapToIndexESeq()
{
    cout << "From type: " << endl;
    ESeq_code_type from_type = GetESeqCodeType();
    cout << "To type: " << endl;
    ESeq_code_type to_type = GetESeqCodeType();
    cout << "Enter from index: ";
    int from_idx;
    cin >> from_idx;
    int to_idx = CSeqportUtil::GetMapToIndex(from_type, to_type, from_idx);
    cout << "From index maps to " << to_idx << endl;
}

void CSeqportTestApp::NonSeqDataTest()
{
    while (true) {
        cout << "Enter test to run: " << endl
             << "0)  Done " << endl
             << "1)  GetIupacaa3() " << endl
             << "2)  GetCode(E_Choice) " << endl
             << "3)  GetCode(ESeq_code_type) " << endl
             << "4)  GetName(E_Choice) " << endl
             << "5)  GetName(ESeq_code_type) " << endl
             << "6)  GetIndex(E_Choice) " << endl
             << "7)  GetIndex(ESeq_code_type) " << endl
             << "8)  IsCodeAvailable(E_Choice) " << endl
             << "9)  IsCodeAvailable(ESeq_code_type) " << endl
             << "10) GetCodeIndexFromTo(E_Choice) " << endl
             << "11) GetCodeIndexFromTo(ESeq_code_type) " << endl
             << "12) GetIndexComplement(E_Choice) " << endl
             << "13) GetIndexComplement(ESeq_code_type) " << endl
             << "14) GetMapToIndex(E_Choice) " << endl
             << "15) GetMapToIndex(ESeq) " << endl
             << endl;
        int resp;
        cin >> resp;
        while (true) {
            try {
                switch (resp) {
                case 0:
                    return;
                case 1:   
                    GetIupacaa3();
                    break;
                case 2:
                    GetCodeEChoice();
                    break;
                case 3:
                    GetCodeESeq();
                    break;
                case 4:
                    GetNameEChoice();
                    break;
                case 5:
                    GetNameESeq();
                    break;
                case 6:
                    GetIndexEChoice();
                    break;
                case 7:
                    GetIndexESeq();
                    break;
                case 8:
                    IsCodeAvailableEChoice();
                    break;
                case 9:
                    IsCodeAvailableESeq();
                    break;
                case 10:
                    GetCodeIndexFromToEChoice();
                    break;
                case 11:
                    GetCodeIndexFromToESeq();
                    break;
                case 12:
                    GetIndexComplementEChoice();
                    break;
                case 13:
                    GetIndexComplementESeq();
                    break;
                case 14:
                    GetMapToIndexEChoice();
                    break;
                case 15:
                    GetMapToIndexESeq();
                    break;
                }
            } catch (runtime_error e) {
                string msg(e.what());
                if (msg == "Done") {
                    break;
                }
                cout << msg << endl;
            } 
        }
    }    
}


void CSeqportTestApp::SeqDataTest()
{
    //Declare local variables
    unsigned int nTimes=0, uOverhang=0;
    TSeqPos uSeqLen = 0, uCnt = 0;
    int nInSeqType, nOutSeqType;
    CSeq_data::E_Choice to_code = CSeq_data::e_not_set;
    bool bAmbig;

    //Create in CSeq_data 
    CRef<CSeq_data> in_seq(new CSeq_data); 
    
    //Enter in sequence type
    nInSeqType = -1;
    while(nInSeqType<0) {
        cout << "Enter input sequence type " << endl;
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
        cout << "Enter output sequence type " << endl;
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
        TSeqPos uInSize;
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
                ITERATE (vector<char>, j, v) {
                    in_seq->SetNcbi2na().Set().push_back(*j);
                }
            }
            break;
        case 1:
            for(unsigned int i=0; i<nTimes; i++) {
                ITERATE (vector<char>, j, v) {
                    in_seq->SetNcbi4na().Set().push_back(*j);
                }
            }
            break;
        case 4:
            for(unsigned int i=0; i<nTimes; i++) {
                ITERATE (vector<char>, j, v) {
                    in_seq->SetNcbistdaa().Set().push_back(*j);
                }
            }
            break;
        }
    }
  
    //Enter Begin Index (relative to biological sequence)
    TSeqPos uBeginIdx = 0, uLength = 0;
    cout << "Enter begin index to Convert: ";
    cin >> std::dec >> uBeginIdx;
    
    //Enter Length of sequence to Convert
    cout << "Enter length of sequence to Convert: ";
    cin >> uLength;
    
    //Create out_seq
    CRef<CSeq_data> out_seq(new CSeq_data);
    
    int nResponse;
    
    while (true) {
        cout << "Enter test to run:" << endl
             << "0)  Quit" << endl
             << "1)  GetAmbigsTest" << endl
             << "2)  ConvertTest" << endl
             << "3)  AppendTest" << endl
             << "4)  InPlaceReverseComplementTest" << endl
             << "5)  InCopyReverseComplementTest" << endl
             << "6)  InPlaceReverseTest" << endl
             << "7)  InCopyReverseTest" << endl
             << "8)  InPlaceComplementTest" << endl
             << "9) InCopyComplementTest" << endl
             << "10) FastValidateTest" << endl
             << "11) ValidateTest" << endl
             << "12) GetCopyTest" << endl
             << "13) KeepTest" << endl
             << "14) PackTest" << endl; 
        cin >> nResponse;   
         
        switch (nResponse) {
        case 0:
            return;
        case 1:
            GetAmbigsTest
                (*in_seq, out_seq, to_code, uBeginIdx, uLength);
            break;
        case 2:
            ConvertTest(*in_seq, out_seq, to_code, uBeginIdx, uLength, bAmbig);
            break;
        case 3:
            AppendTest(*in_seq, out_seq, uBeginIdx, uLength);
            break;
        case 4:
            InPlaceReverseComplementTest
                (*in_seq, out_seq, uBeginIdx, uLength);
            break;
        case 5:
            InCopyReverseComplementTest
                (*in_seq, out_seq, uBeginIdx, uLength);
            break;
        case 6:
            InPlaceReverseTest
                (*in_seq, out_seq, uBeginIdx, uLength);
            break;
        case 7:
            InCopyReverseTest
                (*in_seq, out_seq, uBeginIdx, uLength);
            break;
        case 8:
            InPlaceComplementTest
                (*in_seq, out_seq, uBeginIdx, uLength);
            break;
        case 9:
            InCopyComplementTest
                (*in_seq, out_seq, uBeginIdx, uLength);
            break;
        case 10:
            FastValidateTest
                (*in_seq, uBeginIdx, uLength);
            break;
        case 11:
            ValidateTest
                (*in_seq, uBeginIdx, uLength);
            break;
        case 12:
            GetCopyTest(*in_seq, out_seq, uBeginIdx, uLength);
            break;
        case 13:
            KeepTest(*in_seq, out_seq, uBeginIdx, uLength);
            break;
        case 14:
            PackTest(*in_seq, out_seq, uBeginIdx, uLength);
            break;
        default:
            break;
        }
    }        
}

 
void CSeqportTestApp::GetAmbigsTest(const CSeq_data&     in_seq,
              CSeq_data*           out_seq,
              CSeq_data::E_Choice  to_code,
              TSeqPos              uBeginIdx,
              TSeqPos              uLength)
{
    TSeqPos uSeqLen=0;
    
    try {
        vector<TSeqPos> out_indices;
    
        TSeqPos uLen = CSeqportUtil::GetAmbigs
            (in_seq,
             out_seq,
             &out_indices,
             to_code,
             uBeginIdx,
             uLength);
    
    
        cout << endl << "Results for GetAmbigs()" << endl;
        cout << "uSeqLen = " << uSeqLen << endl;
        cout << "(uBeginIdx, uLength) = (" << uBeginIdx
             << ", " << uLength << ")" << endl;
        cout << "Num ambigs = " << out_indices.size() << endl;
        cout << "Return length = " << uLen << endl;
  
        if(out_seq->Which() == CSeq_data::e_Ncbi4na) {
            if (out_indices.size() <= 24) {
                ITERATE (vector<TSeqPos>, i_idx, out_indices) {
                    cout << (*i_idx) << " ";
                }
                cout << endl;
                const vector<char>& out_seq_data
                    = out_seq->GetNcbi4na().Get();
                ITERATE (vector<char>, i_out, out_seq_data) {
                    cout << std::hex
                         << (unsigned short)(unsigned char) *i_out << " ";
                }
                cout << endl;
            }
        }
        else if(out_seq->Which() == CSeq_data::e_Iupacna) {
            if(out_indices.size() <= 24) {
                ITERATE (vector<TSeqPos>, i_idx, out_indices) {
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


void CSeqportTestApp::ConvertTest(const CSeq_data&     in_seq,
              CSeq_data*           out_seq,
              CSeq_data::E_Choice  to_code,
              TSeqPos              uBeginIdx,
              TSeqPos              uLength,
              bool                 bAmbig)
{
    TSeqPos uSeqLen=0;
    
    try{ 
        TSeqPos uLen = CSeqportUtil::Convert
            (in_seq, out_seq, to_code, uBeginIdx, uLength, bAmbig);

        cout << endl << "Conversion Results" << endl;
        cout << "uSeqLen = " << uSeqLen  << endl;
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

void CSeqportTestApp::AppendTest(const CSeq_data&     in_seq,
              CSeq_data*           out_seq,
              TSeqPos              uBeginIdx,
              TSeqPos              uLength)
{
    TSeqPos uSeqLen=0;
    
    try{
        TSeqPos uBeginIdx2, uLength2;
        cout << "(uBeginIdx1, uLength1) = (" 
             << uBeginIdx << "," << uLength << ")" << endl;
        cout << "Enter uBeginIdx2: ";
        cin >> uBeginIdx2;
        cout << "Enter uLength2: ";
        cin >> uLength2;
        CRef<CSeq_data> in_seq1(new CSeq_data);
        CRef<CSeq_data> in_seq2(new CSeq_data);
        CSeqportUtil::GetCopy(in_seq, in_seq1, uBeginIdx, uLength);
        CSeqportUtil::GetCopy(in_seq, in_seq2, 0, 0);
        TSeqPos uLen = CSeqportUtil::Append(out_seq,
                                      *in_seq1.GetPointer(),
                                      uBeginIdx, uLength,
                                      *in_seq2.GetPointer(), 
                                      uBeginIdx2, uLength2);
    
        cout << endl << "Append Results" << endl;
        cout << "uSeqLen = " << uSeqLen << endl;
        cout << "(uBeginIdx1, uLength1) = (" << uBeginIdx
             << ", " << uLength << ")" << endl;
        cout << "(uBeginIdx2, uLength2) = (" << uBeginIdx2
             << ", " << uLength2 << ")" << endl;
        cout << "Return length = " << uLen << endl;
  
        //Print the Append out_seq
        cout << "Append output sequence is: " << endl;
        DisplaySeq(*out_seq, MAX_DISPLAY);
        
        //Reset out_seq to free memory
        out_seq->Reset();
    }
    STD_CATCH("");
}

void CSeqportTestApp::InPlaceReverseComplementTest(const CSeq_data&     in_seq,
              CSeq_data*           out_seq,
              TSeqPos              uBeginIdx,
              TSeqPos              uLength)
{
    TSeqPos uSeqLen=0;

    try{
        CSeqportUtil::GetCopy(in_seq, out_seq, 0, 0);
        TSeqPos uLen
            = CSeqportUtil::ReverseComplement(out_seq, uBeginIdx, uLength);
        cout << endl << "In-place reverse complement results" << endl;
        cout << "uSeqLen = " << uSeqLen << endl;
        cout << "(uBeginIdx, uLength) = (" << uBeginIdx
             << ", " << uLength << ")" << endl;
        cout << "Return length = " << uLen << endl;
  
        //Print the In-place reverse-complement sequence
        cout << "In-place reverse-complement sequence is: " << endl;
        DisplaySeq(*out_seq, MAX_DISPLAY);
        
        //Reset out_seq to free memory
        out_seq->Reset();
    }
    STD_CATCH("");
}

void CSeqportTestApp::InCopyReverseComplementTest(const CSeq_data&     in_seq,
              CSeq_data*           out_seq,
              TSeqPos              uBeginIdx,
              TSeqPos              uLength)
{
    TSeqPos uSeqLen=0;

    try{
        TSeqPos uLen = CSeqportUtil::ReverseComplement
            (in_seq, out_seq, uBeginIdx, uLength);
    
        cout << endl << "Reverse-complement Results" << endl;
        cout << "uSeqLen = " << uSeqLen << endl;
        cout << "(uBeginIdx, uLength) = (" << uBeginIdx
             << ", " << uLength << ")" << endl;
        cout << "Return length = " << uLen << endl;
  
        //Print the reverse-complement sequence
        cout << "Reverse-complement sequence is: " << endl;
        DisplaySeq(*out_seq, MAX_DISPLAY);
        
        //Reset out_seq to free memory
        out_seq->Reset();
    }
    STD_CATCH("");
}

void CSeqportTestApp::InPlaceReverseTest(const CSeq_data&     in_seq,
              CSeq_data*           out_seq,
              TSeqPos              uBeginIdx,
              TSeqPos              uLength)
{
    TSeqPos uSeqLen=0;

    try{
        CSeqportUtil::GetCopy(in_seq, out_seq, 0, 0);
        TSeqPos uLen = 
            CSeqportUtil::Reverse(out_seq, uBeginIdx, uLength);
    
        cout << endl << "In-place Reverse Results" << endl;
        cout << "uSeqLen = " << uSeqLen << endl;
        cout << "(uBeginIdx, uLength) = (" << uBeginIdx
             << ", " << uLength << ")" << endl;
        cout << "Return length = " << uLen << endl;
  
        //Print the In-place reverse sequence
        cout << "In-place reverse sequence is: " << endl;
        DisplaySeq(*out_seq, MAX_DISPLAY);
        
        //Reset out_seq to free memory
        out_seq->Reset();
    }
    STD_CATCH("");
}

void CSeqportTestApp::InCopyReverseTest(const CSeq_data&     in_seq,
              CSeq_data*           out_seq,
              TSeqPos              uBeginIdx,
              TSeqPos              uLength)
{
    TSeqPos uSeqLen=0;

    try{
        TSeqPos uLen = CSeqportUtil::Reverse
            (in_seq, out_seq, uBeginIdx, uLength);
    
        cout << endl << "Reverse Results" << endl;
        cout << "uSeqLen = " << uSeqLen << endl;
        cout << "(uBeginIdx, uLength) = (" << uBeginIdx <<
            ", " << uLength << ")" << endl;
        cout << "Return length = " << uLen << endl;
  
        //Print the reverse sequence
        cout << "Reverse sequence is: " << endl;
        DisplaySeq(*out_seq, MAX_DISPLAY);
        
        //Reset out_seq to free memory
        out_seq->Reset();
    }
    STD_CATCH("");
}

void CSeqportTestApp::InPlaceComplementTest(const CSeq_data&     in_seq,
              CSeq_data*           out_seq,
              TSeqPos              uBeginIdx,
              TSeqPos              uLength)
{
    TSeqPos uSeqLen=0;

    try{
        CSeqportUtil::GetCopy(in_seq, out_seq, 0, 0);
        TSeqPos uLen = CSeqportUtil::Complement
            (out_seq, uBeginIdx, uLength);
    
        cout << endl << "In-place Complement Results" << endl;
        cout << "uSeqLen = " << uSeqLen << endl;
        cout << "(uBeginIdx, uLength) = (" << uBeginIdx <<
            ", " << uLength << ")" << endl;
        cout << "Return length = " << uLen << endl;
  
        //Print the In-place complement sequence
        cout << "In-place complement sequence is: " << endl;
        DisplaySeq(*out_seq, MAX_DISPLAY);
        
        //Reset out_seq to free memory
        out_seq->Reset();
    }
    STD_CATCH("");
}
 
void CSeqportTestApp::InCopyComplementTest(const CSeq_data&     in_seq,
              CSeq_data*           out_seq,
              TSeqPos              uBeginIdx,
              TSeqPos              uLength)
{
    TSeqPos uSeqLen=0;

    try{
        TSeqPos uLen = CSeqportUtil::Complement
            (in_seq, out_seq, uBeginIdx, uLength);
    
        cout << endl << "Complement Results" << endl;
        cout << "uSeqLen = " << uSeqLen << endl;
        cout << "(uBeginIdx, uLength) = (" << uBeginIdx <<
            ", " << uLength << ")" << endl;
        cout << "Return length = " << uLen << endl;
  
        //Print the complement sequence
        cout << "Complement sequence is: " << endl;
        DisplaySeq(*out_seq, MAX_DISPLAY);
        
        //Reset out_seq to free memory
        out_seq->Reset();
    }
    STD_CATCH("");      
}

void CSeqportTestApp::FastValidateTest(const CSeq_data&     in_seq,
              TSeqPos              uBeginIdx,
              TSeqPos              uLength)
{
    TSeqPos uSeqLen=0;

    try{
        bool isValid;
    
        isValid = 
            CSeqportUtil::FastValidate(in_seq, uBeginIdx, uLength);
    
        cout << endl << "Fast Validation Results" << endl;
        cout << "uSeqLen = " << uSeqLen << endl;
        cout << "(uBeginIdx, uLength) = (" << uBeginIdx
             << ", " << uLength << ")" <<
            endl;
        if(isValid)
            cout << "in_seq is VALID" << endl;
        else
            cout << "in_seq is NOT VALID" << endl;
    }
    STD_CATCH("");    
}

void CSeqportTestApp::ValidateTest(const CSeq_data&     in_seq,
              TSeqPos              uBeginIdx,
              TSeqPos              uLength)
{
    TSeqPos uSeqLen=0;

    try{
        vector<TSeqPos> badIdx;
        
        CSeqportUtil::Validate(in_seq, &badIdx, uBeginIdx, uLength);
    
    
        cout << endl << "Validation Results" << endl;
        cout << "uSeqLen = " << uSeqLen << endl;
        cout << "(uBeginIdx, uLength) = (" << uBeginIdx <<
            ", " << uLength << ")" << endl;
        if(badIdx.size() <= 50 && badIdx.size() > 0)
            {
                cout << "Bad indices are:" << endl;
                vector<TSeqPos>::iterator itor;
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

void CSeqportTestApp::GetCopyTest(const CSeq_data&     in_seq,
              CSeq_data*           out_seq,
              TSeqPos              uBeginIdx,
              TSeqPos              uLength)
{
    TSeqPos uSeqLen=0;

    try{      
        TSeqPos uLen = CSeqportUtil::GetCopy
            (in_seq, out_seq, uBeginIdx, uLength);
  
        cout << endl << "Copy Results" << endl;
        cout << "uSeqLen = " << uSeqLen  << endl;
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

void CSeqportTestApp::KeepTest(const CSeq_data&     in_seq,
              CSeq_data*           out_seq,
              TSeqPos              uBeginIdx,
              TSeqPos              uLength)
{

    try{
        CSeqportUtil::GetCopy(in_seq, out_seq, 0, 0);
  
        TSeqPos uLen
            = CSeqportUtil::Keep(out_seq, uBeginIdx, uLength);
  
        cout << endl << "Keep Results" << endl;
        cout << "uLength = " << uLength  << endl;
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

void CSeqportTestApp::PackTest(const CSeq_data&     in_seq,
              CSeq_data*           out_seq,
              TSeqPos              uBeginIdx,
              TSeqPos              uLength)
{

    try{
        CSeqportUtil::GetCopy(in_seq, out_seq, 0, 0);
  
        TSeqPos uLen = CSeqportUtil::Pack(out_seq, uLength);
  
        cout << endl << "Pack Results" << endl;
        cout << "uLength = " << uLength  << endl;
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

void CSeqportTestApp::Exit() {
    return;
}


void CSeqportTestApp::DisplaySeq(const CSeq_data& seq, TSeqPos uSize)
{
    //Print the sequence
    switch (seq.Which()) {
    case CSeq_data::e_Ncbi2na:
        {
            const vector<char>& v = seq.GetNcbi2na().Get();
            if (v.size() <= 12) {
                ITERATE (vector<char>, i, v) {
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
                ITERATE (vector<char>, i, v) {
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
                ITERATE (vector<char>, i, v) {
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
    return theApp.AppMain(argc, argv);
}
