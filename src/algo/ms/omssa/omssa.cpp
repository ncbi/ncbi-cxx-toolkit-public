/* 
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
*  Please cite the authors in any work or product based on this material.
*
* ===========================================================================
*
* Authors:  Lewis Y. Geer, Douglas J. Slotta
*  
* File Description:
*    code to do the ms/ms search and score matches
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <util/miscmath.h>
#include <algo/blast/core/ncbi_math.h>
#include <util/compress/bzip2.hpp> 



#include "SpectrumSet.hpp"
#include "omssa.hpp"
#include "pepxml.hpp"

#include <fstream>
#include <string>
#include <list>
#include <deque>
#include <algorithm>

#include <math.h>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(omssa);



int 
CSearchHelper::ReadModFiles(const string& ModFileName,
                          const string& UserModFileName,
                          const string& Path,
                          CRef <CMSModSpecSet> Modset)
{  
    CDirEntry DirEntry(Path);
    string FileName;
    try {
        if(ModFileName == "")
            ERR_POST(Critical << "modification filename is blank!");
        if(!CDirEntry::IsAbsolutePath(ModFileName))
            FileName = DirEntry.GetDir() + ModFileName;
        else FileName = ModFileName;
        auto_ptr<CObjectIStream> 
            modsin(CObjectIStream::Open(FileName.c_str(), eSerial_Xml));
        if(modsin->fail()) {	    
            ERR_POST(Fatal << "ommsacl: unable to open modification file" << 
                     FileName);
            return 1;
        }
        modsin->Read(ObjectInfo(*Modset));
        modsin->Close();
    
    } catch (NCBI_NS_STD::exception& e) {
        ERR_POST(Fatal << "Unable to read modification file " <<
                 FileName << " with error " << e.what());
    }

    // read in user mod file, if any
    if(UserModFileName != "") {
        try {
            CRef <CMSModSpecSet> UserModset(new CMSModSpecSet);
            if(!CDirEntry::IsAbsolutePath(UserModFileName))
                FileName = DirEntry.GetDir() + UserModFileName;
            else FileName = UserModFileName;
            auto_ptr<CObjectIStream> 
             usermodsin(CObjectIStream::Open(FileName.c_str(), eSerial_Xml));
            if(usermodsin->fail()) {	    
                 ERR_POST(Warning << "ommsacl: unable to open user modification file" << 
                          ModFileName);
                 return 0;
             }
            usermodsin->Read(ObjectInfo(*UserModset));
            usermodsin->Close();
            Modset->Append(*UserModset);
        } catch (NCBI_NS_STD::exception& e) {
             ERR_POST(Fatal << "Unable to read user modification file " <<
                      FileName << " with error " << e.what());
        }
    }
    return 0;
}


void 
CSearchHelper::ReadTaxFile(string& Filename, TTaxNameMap& TaxNameMap)
{
    ifstream taxnames(Filename.c_str());
    string line;
    list<string> linelist;
    list<string>::iterator ilist;
    while(taxnames && !taxnames.eof()) {
        getline(taxnames, line);
        linelist.clear();
        NStr::Split(line, ",", linelist);
        if(!linelist.empty()) {
            ilist = linelist.begin();
            ilist++;
            TaxNameMap[NStr::StringToInt(*ilist)] = *(linelist.begin());
        }
    }
}   

void 
CSearchHelper::ConditionXMLStream(CObjectOStreamXml *xml_out)
{
    if(!xml_out) return;
    // turn on xml schema
    xml_out->SetReferenceSchema();
    // turn off names in named integers
    xml_out->SetWriteNamedIntegersByValue(true);
}



int 
CSearchHelper::ReadFile(const string& Filename,
                     const EMSSpectrumFileType FileType,
                     CMSSearch& MySearch)
{
    CRef <CMSRequest> Request (new CMSRequest);
    MySearch.SetRequest().push_back(Request);
//    CRef <CMSResponse> Response (new CMSResponse);
//    MySearch.SetResponse().push_back(Response);

    CNcbiIfstream PeakFile(Filename.c_str());
    if(!PeakFile) {
        ERR_POST(Fatal <<" omssacl: not able to open spectrum file " <<
                 Filename);
        return 1;
    }

    CRef <CSpectrumSet> SpectrumSet(new CSpectrumSet);
    (*MySearch.SetRequest().begin())->SetSpectra(*SpectrumSet);
    return SpectrumSet->LoadFile(FileType, PeakFile);
}   

int 
CSearchHelper::ReadSearchRequest(const string& Filename,
                                 const ESerialDataFormat DataFormat,
                                 CMSSearch& MySearch)
{   
    CRef <CMSRequest> Request (new CMSRequest);
    MySearch.SetRequest().push_back(Request);
//    CRef <CMSResponse> Response (new CMSResponse);
//    MySearch.SetResponse().push_back(Response);

    auto_ptr<CObjectIStream> 
        in(CObjectIStream::Open(Filename.c_str(), DataFormat));
    in->Open(Filename.c_str(), DataFormat);
    if(in->fail()) {	    
        ERR_POST(Warning << "omssacl: unable to search file" << 
                 Filename);
        return 1;
    }
    in->Read(ObjectInfo(*Request));
    in->Close();
    return 0;
}


int 
CSearchHelper::ReadCompleteSearch(const string& Filename,
                               const ESerialDataFormat DataFormat,
                               bool bz2,
                               CMSSearch& MySearch)
{
    auto_ptr <CNcbiIfstream> raw_in;
    auto_ptr <CCompressionIStream> compress_in;
    auto_ptr <CObjectIStream> in;

    if( bz2 ) {
        raw_in.reset(new CNcbiIfstream(Filename.c_str()));
        compress_in.reset( new CCompressionIStream (*raw_in, 
                                                    new CBZip2StreamDecompressor(), 
                                                    CCompressionStream::fOwnProcessor)); 
        in.reset(CObjectIStream::Open(DataFormat, *compress_in)); 
    }
    else {
        in.reset(CObjectIStream::Open(Filename.c_str(), DataFormat));
    }
    if(in->fail()) {	    
        ERR_POST(Warning << "omssacl: unable to search file" << 
                 Filename);
        return 1;
    }
    in->Read(ObjectInfo(MySearch));
    in->Close();
    return 0;
}


int 
CSearchHelper::LoadAnyFile(CMSSearch& MySearch, 
                           CConstRef <CMSInFile> InFile,
                           bool* SearchEngineIterative)
{
    string Filename(InFile->GetInfile());
    EMSSpectrumFileType DataFormat =
        static_cast <EMSSpectrumFileType> (InFile->GetInfiletype());
                               
    switch (DataFormat) {
    case eMSSpectrumFileType_dta:
    case eMSSpectrumFileType_dtablank:
    case eMSSpectrumFileType_dtaxml:
    case eMSSpectrumFileType_pkl:
    case eMSSpectrumFileType_mgf:
    return CSearchHelper::ReadFile(Filename, DataFormat, MySearch);
    break;
    case eMSSpectrumFileType_oms:
    if(SearchEngineIterative) *SearchEngineIterative = true;
    return CSearchHelper::ReadCompleteSearch(Filename, eSerial_AsnBinary, false, MySearch);
    break;
    case eMSSpectrumFileType_omx:
    if(SearchEngineIterative) *SearchEngineIterative = true;
    return CSearchHelper::ReadCompleteSearch(Filename, eSerial_Xml, false, MySearch);
    break;
    case eMSSpectrumFileType_xml:
    return CSearchHelper::ReadSearchRequest(Filename, eSerial_Xml, MySearch);
    break;
    case eMSSpectrumFileType_omxbz2 :
    return CSearchHelper::ReadCompleteSearch(Filename, eSerial_Xml, true, MySearch);
    break;
    case eMSSpectrumFileType_asc:
    case eMSSpectrumFileType_pks:
    case eMSSpectrumFileType_sciex:
    case eMSSpectrumFileType_unknown:
    default:
        break;
    }
    return 1;  // not supported
}


void CSearchHelper::SaveOneFile(CMSSearch &MySearch,
                                const string Filename, 
                                ESerialDataFormat FileFormat,
                                bool IncludeRequest,
                                bool bz2) 
{
    auto_ptr <CNcbiOfstream> raw_out;
    auto_ptr <CCompressionOStream> compress_out;
    auto_ptr <CObjectOStream> txt_out;

    if( bz2 ) {
        raw_out.reset(new CNcbiOfstream(Filename.c_str()));
        compress_out.reset( new CCompressionOStream (*raw_out, 
                                                    new CBZip2StreamCompressor(), 
                                                    CCompressionStream::fOwnProcessor)); 
        txt_out.reset(CObjectOStream::Open(FileFormat, *compress_out)); 
    }
    else {
        txt_out.reset(CObjectOStream::Open(Filename.c_str(), FileFormat));
    }

    if(FileFormat == eSerial_Xml) {
        CObjectOStreamXml *xml_out = dynamic_cast <CObjectOStreamXml *> (txt_out.get());
        CSearchHelper::ConditionXMLStream(xml_out);
    }
    if(IncludeRequest)
        txt_out->Write(ObjectInfo(MySearch));
    else
        txt_out->Write(ObjectInfo(**MySearch.SetResponse().begin()));
}   


int 
CSearchHelper::SaveAnyFile(CMSSearch& MySearch, 
                           CMSSearchSettings::TOutfiles OutFiles,
                           CRef <CMSModSpecSet> Modset)
{
    CMSSearchSettings::TOutfiles::const_iterator iOutFile;

    for(iOutFile = OutFiles.begin(); iOutFile != OutFiles.end(); ++iOutFile) {
        string Filename((*iOutFile)->GetOutfile());
        EMSSerialDataFormat DataFormat =
            static_cast <EMSSerialDataFormat> ((*iOutFile)->GetOutfiletype());
        ESerialDataFormat FileFormat(eSerial_AsnText);

        auto_ptr <CObjectOStream> txt_out;
        if(DataFormat == eMSSerialDataFormat_asntext)
            FileFormat = eSerial_AsnText;
        if(DataFormat == eMSSerialDataFormat_asnbinary)
              FileFormat = eSerial_AsnBinary;
        if(DataFormat == eMSSerialDataFormat_xml)
               FileFormat = eSerial_Xml;
        if(DataFormat == eMSSerialDataFormat_xmlbz2)
                FileFormat = eSerial_Xml;

        switch (DataFormat) {
        case eMSSerialDataFormat_asntext:
        case eMSSerialDataFormat_asnbinary:
        case eMSSerialDataFormat_xml:
        CSearchHelper::SaveOneFile(MySearch,
                                   Filename, 
                                   FileFormat, 
                                   (*iOutFile)->GetIncluderequest(),
                                   false);
        break;
        case eMSSerialDataFormat_xmlbz2:
        CSearchHelper::SaveOneFile(MySearch,
                                   Filename, 
                                   FileFormat, 
                                   (*iOutFile)->GetIncluderequest(),
                                   true);
        break;
        case eMSSerialDataFormat_pepxml:
        {
            CPepXML outPepXML;
            outPepXML.ConvertFromOMSSA(MySearch, Modset, Filename, Filename);
            auto_ptr<CObjectOStream> file_out(CObjectOStream::Open(Filename, eSerial_Xml));
            *file_out << outPepXML;
        }
        break;
        case eMSSerialDataFormat_csv:    
        {
            CNcbiOfstream oscsv;
            oscsv.open(Filename.c_str());
            (*MySearch.SetResponse().begin())->PrintCSV(oscsv, Modset);
            oscsv.close();
        }
        break;
        case eMSSerialDataFormat_none:
        default:
        {
            ERR_POST(Error << "Unknown output file format " << DataFormat);
        }
        return 1;
        break;
        }
    }
    return 0;
}

void 
CSearchHelper::ValidateSearchSettings(CRef<CMSSearchSettings> &Settings)
{
    list <string> ValidError;
	if(Settings->Validate(ValidError) != 0) {
	    list <string>::iterator iErr;
	    for(iErr = ValidError.begin(); iErr != ValidError.end(); iErr++)
		ERR_POST(Warning << *iErr);
	    ERR_POST(Fatal << "Unable to validate settings");
	}
}


void 
CSearchHelper::CreateSearchSettings(string FileName,
                                    CRef<CMSSearchSettings> &Settings)
{
    if(FileName != "" ) {
        try {
            auto_ptr<CObjectIStream> 
                paramsin(CObjectIStream::Open(FileName.c_str(), eSerial_Xml));
            if(paramsin->fail()) {	    
                ERR_POST(Fatal << "ommsacl: unable to open parameter file" << 
                         FileName);
                return;
            }
            paramsin->Read(ObjectInfo(*Settings));
            paramsin->Close();

        } catch (NCBI_NS_STD::exception& e) {
            ERR_POST(Fatal << "Unable to read parameter file " <<
                     FileName << " with error " << e.what());
        }
    }
}




/////////////////////////////////////////////////////////////////////////////
//
//  CSearch::
//
//  Performs the ms/ms search
//


CSearch::CSearch(int tNum): 
UseRankScore(false),
Iterative(false),
RestrictedSearch(false)
{
    ThreadNum = tNum;
}


void CSearch::ResetGlobals(void)
{
    iSearchGlobal = -1;
    MaxMZ = 0;
    SharedPeakSet.Reset(0);
}


int CSearch::InitBlast(const char *blastdb, bool use_mmap)
{
    if (!blastdb) return 0;
    rdfp.Reset(new CSeqDB(blastdb, CSeqDB::eProtein, 
    0, 0, use_mmap));
    numseq = rdfp->GetNumOIDs();
    return 0;   
}


// create the ladders from sequence

int CSearch::CreateLadders(const char *Sequence,
                           int iSearch,
                           int position,
                           int endposition,
                           int *Masses, 
                           int iMissed,
                           CAA& AA, 
                           int iMod,
                           CMod ModList[],
                           int NumMod)
{
    TLadderMap::iterator Iter;
    SetLadderContainer().Begin(Iter);
    while(Iter != SetLadderContainer().SetLadderMap().end()) {
        bool NoProline = find(GetSettings()->GetNoprolineions().begin(),
                              GetSettings()->GetNoprolineions().end(),
                              CMSMatchedPeakSetMap::Key2Series(Iter->first)) != 
            GetSettings()->GetNoprolineions().end();
        if (!(*(Iter->second))[iMod]->
            CreateLadder(CMSMatchedPeakSetMap::Key2Series(Iter->first),
                         CMSMatchedPeakSetMap::Key2Charge(Iter->first),
                         Sequence,
                         iSearch,
                         position,
                         endposition,
                         Masses[iMissed], 
                         MassArray,
                         AA,
                         SetMassAndMask(iMissed, iMod).Mask,
                         ModList,
                         NumMod,
                         *SetSettings(),
                         NoProline
                         )) return 1;
        SetLadderContainer().Next(Iter);
    }

    return 0;
}


// compare ladders to experiment
int CSearch::CompareLadders(int iMod,
                            CMSPeak *Peaks,
                            bool OrLadders,
                            const TMassPeak *MassPeak)
{
    EMSPeakListTypes Which = Peaks->GetWhich(MassPeak->Charge);

    int ChargeLimitLo(0), ChargeLimitHi(0);
    if (MassPeak) {
        if(MassPeak->Charge < Peaks->GetConsiderMult()) { 
            ChargeLimitLo = 1;
            ChargeLimitHi = 1;
        }
        else {
            ChargeLimitLo = 0;
            ChargeLimitHi = 0;
        }
    }

    TLadderMap::iterator Iter;
    SetLadderContainer().Begin(Iter, ChargeLimitLo, ChargeLimitHi);
    vector<bool> usedPeaks(Peaks->SetPeakLists()[Which]->GetNum(), false);
    while(Iter != SetLadderContainer().SetLadderMap().end()) {
        Peaks->CompareSortedRank(*((*(Iter->second))[iMod]), Which, usedPeaks);
        SetLadderContainer().Next(Iter, ChargeLimitLo, ChargeLimitHi);
    }
    return 0;
}


// compare ladders to experiment
bool CSearch::CompareLaddersTop(int iMod,
                                CMSPeak *Peaks,
                                const TMassPeak *MassPeak)
{
    int ChargeLimitLo(0), ChargeLimitHi(0);
    if (MassPeak) {
        if(MassPeak->Charge < Peaks->GetConsiderMult()) { 
            ChargeLimitLo = 1;
            ChargeLimitHi = 1;
        }
        else {
            ChargeLimitLo = 0;
            ChargeLimitHi = 0;
        }
    }

    TLadderMap::iterator Iter;
    SetLadderContainer().Begin(Iter, ChargeLimitLo, ChargeLimitHi);
    while(Iter != SetLadderContainer().SetLadderMap().end()) {
        if(Peaks->CompareTop(*((*(Iter->second))[iMod]))) return true;
        SetLadderContainer().Next(Iter, ChargeLimitLo, ChargeLimitHi);
    }
    return false;
}


const bool
CSearch::ReSearch(const int Number) const
{
    if ( GetSettings()->GetIterativesettings().GetResearchthresh() != 0.0) {
        // look for hitset
        CRef <CMSHitSet> HitSet;
        HitSet = GetResponse()->FindHitSet(Number);
        if (HitSet.IsNull()) return true;
        if (HitSet->GetHits().empty()) return true;
        if ((*HitSet->GetHits().begin())->GetEvalue() <= 
            GetSettings()->GetIterativesettings().GetResearchthresh())
            return false;
        else return true;
    }
    return true;
}

int PositiveSign(int input) 
{ 
    return abs(input); 
}

// loads spectra into peaks
//void CSearch::Spectrum2Peak(CMSPeakSet& PeakSet)
void CSearch::Spectrum2Peak(CRef<CMSPeakSet> PeakSet)
{
    CSpectrumSet::Tdata::const_iterator iSpectrum;
    CMSPeak* Peaks;

    iSpectrum = GetRequest()->GetSpectra().Get().begin();
    for (; iSpectrum != GetRequest()->GetSpectra().Get().end(); iSpectrum++) {
        CRef <CMSSpectrum> Spectrum =  *iSpectrum;
        if (!Spectrum) {
            ERR_POST(Error << "omssa: unable to find spectrum");
            return;
        }
        
        // reset charges so that they are absolute values.  The charge sign is indicated
        // by GetSettings()->GetChargehandling().GetNegative()
        transform(Spectrum->SetCharge().begin(), Spectrum->SetCharge().end(), Spectrum->SetCharge().begin(), PositiveSign);

        // if iterative search and spectrum should not be re-search, skip
        if (GetIterative() && !ReSearch(Spectrum->GetNumber()))
            continue;

        Peaks = new CMSPeak(GetSettings()->GetHitlistlen());
        if (!Peaks) {
            ERR_POST(Error << "omssa: unable to allocate CMSPeak");
            return;
        }

        Peaks->ReadAndProcess(*Spectrum, *GetSettings());
#if 0
        {
            ofstream os("test.dta");
            Peaks->Write(os, eMSSpectrumFileType_dta, eMSPeakListCharge1);
        }
#endif
        PeakSet->AddPeak(Peaks);

    }
    int Numisotopes(0);
    if(GetSettings()->CanGetNumisotopes()) 
        Numisotopes = GetSettings()->GetNumisotopes();
    bool Pepppm(false);
    if(GetSettings()->CanGetPepppm())
        Pepppm = GetSettings()->GetPepppm();
    MaxMZ = PeakSet->SortPeaks(MSSCALE2INT(GetSettings()->GetPeptol()),
                              GetSettings()->GetZdep(),
                              Numisotopes, Pepppm, GetSettings()->GetChargehandling().GetNegative());

}

// compares TMassMasks.  Lower m/z first in sort.
struct CMassMaskCompare {
    bool operator() (const TMassMask& x, const TMassMask& y)
    {
        if (x.Mass < y.Mass) return true;
        return false;
    }
};

/**
 *  delete variable mods that overlap with fixed mods
 * @param NumMod the number of modifications
 * @param ModList modification information
 */
void CSearch::DeleteVariableOverlap(int& NumMod,
                                    CMod ModList[])
{
    int i, j;
    for (i = 0; i < NumMod; i++) {
        // if variable mod
        if (ModList[i].GetFixed() != 1) {
            // iterate thru all mods for comparison
            for (j = 0; j < NumMod; j++) {
                // if fixed and at same site
                if (ModList[j].GetFixed() == 1 && 
                    ModList[i].GetSite() == ModList[j].GetSite()) {
                    // mark mod for deletion
                    ModList[i].SetFixed() = -1;
                }
            } // j loop
        } // IsFixed
    } // i loop

    // now do the deletion
    for (i = 0; i < NumMod;) {
        if (ModList[i].GetFixed() == -1) {
            NumMod--;
            // if last mod, then just return
            if (i == NumMod) return;
            // otherwise, delete the modification
            for (j=i; j < NumMod; ++j) {
                ModList[j] = ModList[j+1];
            }
        }
        else i++;
    }
    return;
}

// update sites and masses for new peptide
void CSearch::UpdateWithNewPep(int Missed,
                               const char *PepStart[],
                               const char *PepEnd[], 
                               int NumMod[], 
                               CMod ModList[][MAXMOD],
                               int Masses[],
                               int EndMasses[],
                               int NumModSites[],
                               CRef <CMSModSpecSet> &Modset)
{
    // iterate over missed cleavages
    int iMissed;
    // maximum mods allowed
    //int ModMax; 
    // iterate over mods
    int iMod;


    // update the longer peptides to add the new peptide (Missed-1) on the end
    for (iMissed = 0; iMissed < Missed - 1; iMissed++) {
        // skip start
        if (PepStart[iMissed] == (const char *)-1) continue;
        // reset the end sequences
        PepEnd[iMissed] = PepEnd[Missed - 1];

        // update new mod masses to add in any new mods from new peptide

        // first determine the maximum value for updated mod list
        //if(NumMod[iMissed] + NumMod[Missed-1] >= MAXMOD)
        //    ModMax = MAXMOD - NumMod[iMissed];
        //else ModMax = NumMod[Missed-1];

        // now interate thru the new entries
        const char *OldSite(0);
        int NumModSitesCount(0), NumModCount(0);
        for (iMod = 0; iMod < NumMod[Missed-1]; iMod++) {

            // don't do more than the maximum number of modifications
            if (NumModCount + NumMod[iMissed] >= MAXMOD) break;

            // if n-term peptide mod and not at the start of the peptide, don't copy
            if ((Modset->GetModType(ModList[Missed-1][iMod].GetEnum()) == eMSModType_modnp || 
                 Modset->GetModType(ModList[Missed-1][iMod].GetEnum()) == eMSModType_modnpaa) &&
                PepStart[iMissed] != ModList[Missed-1][iMod].GetSite()) {
                continue;
            }

            // if n-term protein mod, don't copy
             if (Modset->GetModType(ModList[Missed-1][iMod].GetEnum()) == eMSModType_modn || 
                  Modset->GetModType(ModList[Missed-1][iMod].GetEnum()) == eMSModType_modnaa) {
                 continue;
             }

            // copy the mod to the old peptide
            ModList[iMissed][NumModCount + NumMod[iMissed]] = 
            ModList[Missed-1][iMod];

            // increment site count if not fixed mod and not the same site
            if (OldSite != ModList[iMissed][NumModCount + NumMod[iMissed]].GetSite() &&
                ModList[iMissed][NumModCount + NumMod[iMissed]].GetFixed() != 1) {
                NumModSitesCount++;
                OldSite = ModList[iMissed][NumModCount + NumMod[iMissed]].GetSite();
            }

            // increment number of mods
            NumModCount++;


        }

        // update old masses
        Masses[iMissed] += Masses[Missed - 1];

        // update end masses
        EndMasses[iMissed] = EndMasses[Missed - 1];

        // update number of Mods
        NumMod[iMissed] += NumModCount;

        // update number of Modification Sites
        NumModSites[iMissed] += NumModSitesCount;
    }       
}


/**
 *  count the number of unique sites modified
 * 
 * @param NumModSites the number of unique mod sites
 * @param NumMod the number of mods
 * @param ModList modification information
 */
void CSearch::CountModSites(int &NumModSites,
                            int NumMod,
                            CMod ModList[])
{
    NumModSites = 0;
    int i;
    const char *OldSite(0);

    for (i = 0; i < NumMod; i++) {
        // skip repeated sites and fixed mods
        if (ModList[i].GetSite() != OldSite && ModList[i].GetFixed() != 1 ) {
            NumModSites++;
            OldSite = ModList[i].GetSite();
        }
    }
}


// create the various combinations of mods
void CSearch::CreateModCombinations(int Missed,
                                    const char *PepStart[],
                                    int Masses[],
                                    int EndMasses[],
                                    int NumMod[],
                                    int NumMassAndMask[],
                                    int NumModSites[],
                                    CMod ModList[][MAXMOD]
                                   )
{
    // need to iterate thru combinations that have iMod.
    // i.e. iMod = 3 and NumMod=5
    // 00111, 01011, 10011, 10101, 11001, 11010, 11100, 01101,
    // 01110
    // i[0] = 0 --> 5-3, i[1] = i[0]+1 -> 5-2, i[3] = i[1]+1 -> 5-1
    // then construct bool mask

    // holders for calculated modification mask and modified peptide masses
    unsigned Mask, MassOfMask;
    // iterate thru active mods
    int iiMod;
    // keep track of the number of unique masks created.  each corresponds to a ladder
    int iModCount;
    // missed cleavage
    int iMissed;
    // number of mods to consider
    int iMod;
    // positions of mods
    int ModIndex[MAXMOD];

    // go thru missed cleaves
    for (iMissed = 0; iMissed < Missed; iMissed++) {
        // skip start
        if (PepStart[iMissed] == (const char *)-1) continue;
        iModCount = 0;

        // set up non-modified mass
        SetMassAndMask(iMissed, iModCount).Mass = 
        Masses[iMissed] + EndMasses[iMissed];
        SetMassAndMask(iMissed, iModCount).Mask = 0;

        int NumVariable(NumMod[iMissed]);  // number of variable mods
        int NumFixed;
        // add in fixed mods
        for (iMod = 0; iMod < NumMod[iMissed]; iMod++) {
            if (ModList[iMissed][iMod].GetFixed()) {
                SetMassAndMask(iMissed, iModCount).Mass += ModList[iMissed][iMod].GetPrecursorDelta();
                SetMassAndMask(iMissed, iModCount).Mask |= 1 << iMod;
                NumVariable--;
            }
        }
        iModCount++;
        NumFixed = NumMod[iMissed] - NumVariable;

        // go thru number of mods allowed
//	for(iMod = 0; iMod < NumVariable && iModCount < MaxModPerPep; iMod++) {
        for (iMod = 0; iMod < NumModSites[iMissed] && iModCount < MaxModPerPep; iMod++) {

            // initialize ModIndex that points to mod sites

            // todo: ModIndex must always include fixed mods

            InitModIndex(ModIndex, iMod, NumMod[iMissed],
                         NumModSites[iMissed], ModList[iMissed]);
            do {

                // calculate mass
                MassOfMask = SetMassAndMask(iMissed, 0).Mass;
                for (iiMod = 0; iiMod <= iMod; iiMod++ )
                    MassOfMask += ModList[iMissed][ModIndex[iiMod + NumFixed]].GetPrecursorDelta();
                // make bool mask
                Mask = MakeBoolMask(ModIndex, iMod + NumFixed);
                // put mass and mask into storage
                SetMassAndMask(iMissed, iModCount).Mass = MassOfMask;
                SetMassAndMask(iMissed, iModCount).Mask = Mask;
#if 0
                printf("NumMod = %d iMod = %d, Mask = \n", NumMod[iMissed], iMod);
                int iii;
                for (iii=NumMod[iMissed]-1; iii >= 0; iii--) {
                    if (Mask & 1 << iii) printf("1");
                    else printf("0");
                }
                printf("\n");
#endif
                // keep track of the  number of ladders
                iModCount++;

            } while (iModCount < MaxModPerPep &&
                     CalcModIndex(ModIndex, iMod, NumMod[iMissed], NumFixed,
                                  NumModSites[iMissed], ModList[iMissed]));
        } // iMod

        // if exact mass, add neutrons as appropriate
        if (SetSettings()->GetPrecursorsearchtype() == eMSSearchType_exact) {
            int ii;
            for (ii = 0; ii < iModCount; ++ii) {
                SetMassAndMask(iMissed, ii).Mass += 
                SetMassAndMask(iMissed, ii).Mass /
                MSSCALE2INT(GetSettings()->GetExactmass()) * 
                MSSCALE2INT(kNeutron);
            }
        }


        // sort mask and mass by mass
        sort(MassAndMask.get() + iMissed*MaxModPerPep, MassAndMask.get() + iMissed*MaxModPerPep + iModCount,
             CMassMaskCompare());
        // keep track of number of MassAndMask
        NumMassAndMask[iMissed] = iModCount;

    } // iMissed
}


void CSearch::SetIons(list <EMSIonSeries> & Ions)
{
    if (GetSettings()->GetIonstosearch().size() < 1) {
        ERR_POST(Fatal << "omssa: at least one ions series to search need to be specified");
    }
    CMSSearchSettings::TIonstosearch::const_iterator i;
    i = GetSettings()->GetIonstosearch().begin();
    for(; i != GetSettings()->GetIonstosearch().end(); ++i) {
        Ions.push_back(static_cast <EMSIonSeries> (*i));
    }
}


void CSearch::InitLadders(list <EMSIonSeries> & Ions)
{

    int MaxLadderSize = GetSettings()->GetMaxproductions();
    if (MaxLadderSize == 0) MaxLadderSize = kMSLadderMax;

    int i;
    SetLadderContainer().SetSeriesChargePairList().clear();
    list <EMSIonSeries> ::const_iterator iIons;

    for (iIons = Ions.begin(); iIons != Ions.end(); ++iIons) {
        for(i = 1; i <= GetSettings()->GetChargehandling().GetMaxproductcharge(); ++i) {
            SetLadderContainer().SetSeriesChargePairList().
                push_back(TSeriesChargePairList::value_type(i, *iIons));
         }
    }
    SetLadderContainer().CreateLadderArrays(MaxModPerPep, MaxLadderSize);
}


void CSearch::MakeOidSet(void)
{
    SetOidSet().clear();
    if (GetSettings()->GetIterativesettings().GetSubsetthresh() != 0.0) {
        SetRestrictedSearch() = true;
        GetResponse()->
        GetOidsBelowThreshold(
                             SetOidSet(),
                             GetSettings()->GetIterativesettings().GetSubsetthresh());
    }
}

int CSearch::iSearchGlobal = -1;
int CSearch::MaxMZ = 0;
CRef<CMSPeakSet> CSearch::SharedPeakSet = null;
DEFINE_STATIC_FAST_MUTEX(iSearchMutex);
DEFINE_STATIC_FAST_MUTEX(PeakSetMutex);
DEFINE_STATIC_FAST_MUTEX(PeaksExaminedMutex);
 
void CSearch::SetupSearch(CRef <CMSRequest> MyRequestIn,
			  CRef <CMSResponse> MyResponseIn,
			  CRef <CMSModSpecSet> Modset,
			  CRef <CMSSearchSettings> SettingsIn,
			  TOMSSACallback Callback,
			  void *CallbackData)
{
  initRequestIn = MyRequestIn;
  initResponseIn = MyResponseIn;
  initModset = Modset;
  initSettingsIn = SettingsIn;
  initCallback = Callback;
  initCallbackData = CallbackData;
}
 
void* CSearch::Main(void)
{
   Search(initRequestIn,
          initResponseIn,
          initModset,
          initSettingsIn,
          initCallback);

    return new bool(true);
}
 
void CSearch::OnExit(void)
{
}

void CSearch::CopySettings(CRef <CSearch> fromObj)
{
  initRequestIn = fromObj->initRequestIn;
  initResponseIn = fromObj->initResponseIn;
  initModset = fromObj->initModset;
  initSettingsIn = fromObj->initSettingsIn;
  initCallback = fromObj->initCallback;
  initCallbackData = fromObj->initCallbackData;
  UseRankScore = fromObj->UseRankScore;
  Iterative = fromObj->Iterative;
  numseq = fromObj->numseq;
  rdfp = fromObj->rdfp;
  
}

void CSearch::Search(CRef <CMSRequest> MyRequestIn,
                                 CRef <CMSResponse> MyResponseIn,
                                 CRef <CMSModSpecSet> Modset,
                                 CRef <CMSSearchSettings> SettingsIn,
                                 TOMSSACallback Callback,
                                 void *CallbackData)
{
    try {
        SetSettings().Reset(SettingsIn);
        SetRequest().Reset(MyRequestIn);
        SetResponse().Reset(MyResponseIn);

        // force the mass scale settings to what is currently used.
        SetSettings()->SetScale(MSSCALE);
        SetResponse()->SetScale(MSSCALE);
        
        // set up automatic number of peaks per bin for noise filter
        if (GetSettings()->GetSinglenum() == 0) {
            SetSettings()->SetSinglenum() = GetSettings()->GetIonstosearch().size();
        }
        if (GetSettings()->GetDoublenum() == 0) {
            SetSettings()->SetDoublenum() = GetSettings()->GetIonstosearch().size();
        }
        
        SetEnzyme() = CCleaveFactory::CleaveFactory(static_cast <EMSEnzymes> 
                                                    (GetSettings()->GetEnzyme()));

        // do iterative search setup
        if (GetIterative()) {
            // check to see if the same sequence library
            if (GetResponse()->GetDbversion() != Getnumseq())
                ERR_POST(Fatal << 
                         "number of sequences in search library is not the same as previously searched. Unable to do iterative search.");
            // if restricted sequence search
            // scan thru hits and make map of oids
            MakeOidSet();
        }

        // set maximum number of ladders to calculate per peptide
        MaxModPerPep = GetSettings()->GetMaxmods();
        if (MaxModPerPep > MAXMOD2) MaxModPerPep = MAXMOD2;

        list <EMSIonSeries> Ions;
        SetIons(Ions);
        InitLadders(Ions);

        LadderCalc.reset(new Int1[MaxModPerPep]);
        CAA AA;

        int Missed;  // number of missed cleaves allowed + 1
        if (GetEnzyme()->GetNonSpecific()) Missed = 1;
        else Missed = GetSettings()->GetMissedcleave()+1;

        int iMissed; // iterate thru missed cleavages

        int iSearch, hits;
        int endposition, position;

        // initialize fixed mods
        FixedMods.Init(GetSettings()->GetFixed(), Modset);
        MassArray.Init(FixedMods, GetSettings()->GetProductsearchtype(), Modset);
        PrecursorMassArray.Init(FixedMods, 
                                GetSettings()->GetPrecursorsearchtype(), Modset);
        // initialize variable mods and set enzyme to use n-term methionine cleavage
        SetEnzyme()->SetNMethionine() = 
            VariableMods.Init(GetSettings()->GetVariable(), Modset) ||
            SetSettings()->GetNmethionine();

        const int *IntMassArray = MassArray.GetIntMass();
        const int *PrecursorIntMassArray = PrecursorMassArray.GetIntMass();
        const char *PepStart[MAXMISSEDCLEAVE];
        const char *PepEnd[MAXMISSEDCLEAVE];

        // contains informations on individual mod sites
        CMod ModList[MAXMISSEDCLEAVE][MAXMOD];

        int NumMod[MAXMISSEDCLEAVE];
        // the number of modification sites.  always less than NumMod.
        int NumModSites[MAXMISSEDCLEAVE];


        // calculated masses and masks
        MassAndMask.reset(new TMassMask[MAXMISSEDCLEAVE*MaxModPerPep]);

        // the number of masses and masks for each peptide
        int NumMassAndMask[MAXMISSEDCLEAVE];

        // set up mass array, indexed by missed cleavage
        // note that EndMasses is the end mass of peptide, kept separate to allow
        // reuse of Masses array in missed cleavage calc
        int Masses[MAXMISSEDCLEAVE];
        int EndMasses[MAXMISSEDCLEAVE];

        int iMod;   // used to iterate thru modifications

        bool SequenceDone;  // are we done iterating through the sequences?

        const CMSSearchSettings::TTaxids& Tax = GetSettings()->GetTaxids();
        CMSSearchSettings::TTaxids::const_iterator iTax;

        CMSHit NewHit;  // a new hit of a ladder to an m/z value
        CMSHit *NewHitOut;  // copy of new hit

        const TMassPeak *MassPeak; // peak currently in consideration
        CMSPeak* Peaks;
        CIntervalTree::const_iterator im; // iterates over interval tree

        // iterates over ladders
        TLadderMap::iterator Iter;

        {{
           CFastMutexGuard guard(PeakSetMutex);
           if (SharedPeakSet == null) {
              SharedPeakSet = new CMSPeakSet();
              Spectrum2Peak(SharedPeakSet);
           }
        }}
        vector <int> taxids;
        vector <int>::iterator itaxids;
        bool TaxInfo(false);  // check to see if any tax information in blast library
	    bool iSearchNotDone(true);

        // iterate through sequences
	    //for (iSearch = 0; rdfp->CheckOrFindOID(iSearch); iSearch++) {
	    while (iSearchNotDone) {
            {{
                CFastMutexGuard guard(iSearchMutex);
                iSearchGlobal++;
                if (!rdfp->CheckOrFindOID(iSearchGlobal)) {
                    iSearchNotDone = false;
                    continue;
                }                
                iSearch = iSearchGlobal;
                if (iSearch % 10000 == 0) {
                   if(Callback) Callback(Getnumseq(), iSearch, CallbackData);
                }
            }}
            
            // if oid restricted search, check to see if oid is in set
            if (GetRestrictedSearch() && SetOidSet().find(iSearch) == SetOidSet().end())
                continue;
            
            if (SetSettings()->IsSetTaxids()) {
                rdfp->GetTaxIDs(iSearch, taxids, false);
                for (itaxids = taxids.begin(); itaxids != taxids.end(); ++itaxids) {
                    if (*itaxids == 0) continue;
                    TaxInfo = true;
                    for (iTax = Tax.begin(); iTax != Tax.end(); ++iTax) {
                        if (*itaxids == *iTax) goto TaxContinue;
                    } 
                }
                continue;
            }
            TaxContinue:
            CSeqDBSequence Sequence(rdfp.GetPointer(), iSearch);
            SequenceDone = false;

            // initialize missed cleavage matrix
            for (iMissed = 0; iMissed < Missed; iMissed++) {
                PepStart[iMissed] = (const char *)-1; // mark start
                PepEnd[iMissed] = Sequence.GetData();
                Masses[iMissed] = 0;
                EndMasses[iMissed] = 0;
                NumMod[iMissed] = 0;
                NumModSites[iMissed] = 0;

                ModList[iMissed][0].Reset();
            }
            PepStart[Missed - 1] = Sequence.GetData();

            // if non-specific enzyme, set stop point
            if (SetEnzyme()->GetNonSpecific()) {
                SetEnzyme()->SetStop() = Sequence.GetData() + SetSettings()->GetMinnoenzyme() - 1;
            }

            // iterate thru the sequence by digesting it
            while (!SequenceDone) {


                // zero out no missed cleavage peptide mass and mods
                // note that Masses and EndMass are separate to reuse
                // masses during the missed cleavage calculation
                Masses[Missed - 1] = 0;
                EndMasses[Missed - 1] = 0;
                NumMod[Missed - 1] = 0;
                NumModSites[Missed - 1] = 0;
                // init no modification elements
                ModList[Missed - 1][0].Reset();

                // calculate new stop and mass
                SequenceDone = 
                SetEnzyme()->CalcAndCut(Sequence.GetData(),
                                   Sequence.GetData() + Sequence.GetLength() - 1, 
                                   &(PepEnd[Missed - 1]),
                                   &(Masses[Missed - 1]),
                                   NumMod[Missed - 1],
                                   MAXMOD,
                                   &(EndMasses[Missed - 1]),
                                   VariableMods, FixedMods,
                                   ModList[Missed - 1],
                                   IntMassArray,
                                   PrecursorIntMassArray,
                                   Modset,
                                   SetSettings()->GetMaxproductions()
                                  );

                // delete variable mods that overlap with fixed mods
                DeleteVariableOverlap(NumMod[Missed - 1],
                                      ModList[Missed - 1]);

                // count the number of unique sites modified
                CountModSites(NumModSites[Missed - 1],
                              NumMod[Missed - 1],
                              ModList[Missed - 1]);

                UpdateWithNewPep(Missed, PepStart, PepEnd, NumMod, ModList,
                                 Masses, EndMasses, NumModSites, Modset);

                CreateModCombinations(Missed, PepStart, Masses,
                                      EndMasses, NumMod, NumMassAndMask,
                                      NumModSites, ModList);


                int OldMass;  // keeps the old peptide mass for comparison
                bool NoMassMatch;  // was there a match to the old mass?

                for (iMissed = 0; iMissed < Missed; iMissed++) {
                    if (PepStart[iMissed] == (const char *)-1) continue;  // skip start

                    // get the start and stop position, inclusive, of the peptide
                    position =  PepStart[iMissed] - Sequence.GetData();
                    endposition = PepEnd[iMissed] - Sequence.GetData();

                    // init bool for "Has ladder been calculated?"
                    ClearLadderCalc(NumMassAndMask[iMissed]);

                    OldMass = 0;
                    NoMassMatch = true;

                    // go thru total number of mods
                    for (iMod = 0; iMod < NumMassAndMask[iMissed]; iMod++) {

                        // have we seen this mass before?
                        if (SetMassAndMask(iMissed, iMod).Mass == OldMass &&
                            NoMassMatch) continue;
                        NoMassMatch = true;
                        OldMass = SetMassAndMask(iMissed, iMod).Mass;

                        // return peaks where theoretical mass is <= precursor mass + tol
                        // and >= precursor mass - tol
                        if (!SetEnzyme()->GetTopDown())
                            im = SharedPeakSet->SetIntervalTree().IntervalsContaining(OldMass);
                        // if top-down enzyme, skip the interval tree match
                        else
                            im = SharedPeakSet->SetIntervalTree().AllIntervals();

                        for (; im; ++im ) {
                            MassPeak = static_cast <const TMassPeak *> (im.GetValue().GetPointerOrNull());

                            Peaks = MassPeak->Peak;
                            // make sure we look thru other mod masks with the same mass
                            NoMassMatch = false;

                            if (!GetLadderCalc(iMod)) {
                                if (CreateLadders(Sequence.GetData(), 
                                                  iSearch,
                                                  position,
                                                  endposition,
                                                  Masses,
                                                  iMissed, 
                                                  AA,
                                                  iMod,
                                                  ModList[iMissed],
                                                  NumMod[iMissed]) != 0) continue;
                                SetLadderCalc(iMod) = true; 
                                // continue to next sequence if ladders not successfully made
                            }
                            else {
                                TLadderMap::iterator Iter;
                                SetLadderContainer().Begin(Iter);
                                while(Iter != SetLadderContainer().SetLadderMap().end()) {
                                    (*(Iter->second))[iMod]->ClearHits();
                                    SetLadderContainer().Next(Iter);
                                }
                            }

                            if (UseRankScore) {
                                {{
                                    CFastMutexGuard guard(PeaksExaminedMutex);   
                                    Peaks->SetPeptidesExamined(MassPeak->Charge)++;
                                }}
                            }
                            if (CompareLaddersTop(iMod, 
                                                  Peaks,
                                                  MassPeak)
                               ) {
                                if (!UseRankScore) {
                                    {{
                                        CFastMutexGuard guard(PeaksExaminedMutex);
                                        Peaks->SetPeptidesExamined(MassPeak->Charge)++;
                                    }}
                                }
                                CompareLadders(iMod, 
                                               Peaks,
                                               false,
                                               MassPeak);
                                hits = 0;
                                SetLadderContainer().Begin(Iter);
                                while(Iter != SetLadderContainer().SetLadderMap().end()) {
                                    hits += (*(Iter->second))[iMod]->HitCount();
                                    SetLadderContainer().Next(Iter);
                                }
                                
                                    
                                {{
                                   CFastMutexGuard guard(PeakSetMutex);
                                   if (hits >= SetSettings()->GetMinhit()) {
                                      // need to save mods.  bool map?
                                      NewHit.SetHits() = hits;   
                                      NewHit.SetCharge() = MassPeak->Charge;
                                      // only record if hit kept
                                      if (Peaks->AddHit(NewHit, NewHitOut)) {
                                         NewHitOut->SetStart() = position;
                                         NewHitOut->SetStop() = endposition;
                                         NewHitOut->SetSeqIndex() = iSearch;
                                         NewHitOut->SetExpMass() = MassPeak->Mass;
                                         // record the hits
                                         NewHitOut->
                                            RecordMatches(SetLadderContainer(),
                                                          iMod,
                                                          Peaks,
                                                          SetMassAndMask(iMissed, iMod).Mask,
                                                          ModList[iMissed],
                                                          NumMod[iMissed],
                                                          PepStart[iMissed],
                                                          SetSettings()->GetSearchctermproduct(),
                                                          SetSettings()->GetSearchb1(),
                                                          SetMassAndMask(iMissed, iMod).Mass
                                                          );
                                      }
                                   }
                                }}
                            } // new addition
                        } // MassPeak
                    } //iMod
                } // iMissed
                if (SetEnzyme()->GetNonSpecific()) {
                    int NonSpecificMass(Masses[0] + EndMasses[0]);
                    PartialLoop:

                    // check that stop is within bounds
                    //// upper bound is max precursor mass divided by lightest AA
                    ////      if(enzyme->GetStop() - PepStart[0] < MaxMZ/MonoMass[7]/MSSCALE &&
                    // upper bound redefined so that minimum mass of existing peptide
                    // is less than the max precursor mass minus the mass of glycine
                    // assumes that any mods have positive mass

                    // argghh, doesn't work for semi-tryptic, which resets the mass
                    // need to use different criterion if semi-tryptic and  start position was
                    // moved.  otherwise this criterion is OK
                    if (NonSpecificMass < MaxMZ /*- MSSCALE2INT(MonoMass[7]) */&&
                        SetEnzyme()->GetStop() < Sequence.GetData() + Sequence.GetLength() - 1 /*-1 added*/ &&
                        (SetSettings()->GetMaxnoenzyme() == 0 ||
                         SetEnzyme()->GetStop() - PepStart[0] + 1 < SetSettings()->GetMaxnoenzyme())
                       ) {
                        SetEnzyme()->SetStop()++;
                        NonSpecificMass += PrecursorIntMassArray[AA.GetMap()[*(SetEnzyme()->GetStop())]];
                    }
                    // reset to new start with minimum size
                    else if ( PepStart[0] < Sequence.GetData() + Sequence.GetLength() - 
                              SetSettings()->GetMinnoenzyme()) {
                        PepStart[0]++;
                        SetEnzyme()->SetStop() = PepStart[0] + SetSettings()->GetMinnoenzyme() - 1;

                        // reset mass
                        NonSpecificMass = 0;
                        const char *iSeqChar;
                        for (iSeqChar = PepStart[0]; iSeqChar <= SetEnzyme()->GetStop(); iSeqChar++)
                           NonSpecificMass += PrecursorIntMassArray[AA.GetMap()[*iSeqChar]];
                        // reset sequence done flag if at end of sequence
                        SequenceDone = false;
                    }
                    else SequenceDone = true;

                    // if this is partial tryptic, loop back if one end or the other is not tryptic
                    // for start, need to check sequence before (check for start of seq)
                    // for end, need to deal with end of protein case
                    if (!SequenceDone && SetEnzyme()->GetCleaveNum() > 0 &&
                        PepStart[0] != Sequence.GetData() &&
                        SetEnzyme()->GetStop() != Sequence.GetData() + Sequence.GetLength() - 1 /* -1 added */ ) {
                        if (!SetEnzyme()->CheckCleaveChar(PepStart[0]-1) &&
                            !SetEnzyme()->CheckCleaveChar(SetEnzyme()->GetStop()))
                            goto PartialLoop;
                    }

                    PepEnd[0] = PepStart[0];
                }
                else {
                    if (!SequenceDone) {
                        int NumModCount;
                        const char *OldSite;
                        int NumModSitesCount;
                        // get rid of longest peptide and move the other peptides down the line
                        for (iMissed = 0; iMissed < Missed - 1; iMissed++) {
                            // move masses to next missed cleavage
                            Masses[iMissed] = Masses[iMissed + 1];
                            // don't move EndMasses as they are recalculated

                            // move the modification data
                            NumModCount = 0;
                            OldSite = 0;
                            NumModSitesCount = 0;
                            for (iMod = 0; iMod < NumMod[iMissed + 1]; iMod++) {
                                // throw away the c term peptide mods as we have a new c terminus
                                if (Modset->GetModType(ModList[iMissed + 1][iMod].GetEnum()) != eMSModType_modcp  && 
                                    Modset->GetModType(ModList[iMissed + 1][iMod].GetEnum()) != eMSModType_modcpaa) {
                                    ModList[iMissed][NumModCount] = ModList[iMissed + 1][iMod];
                                    NumModCount++;
                                    // increment mod site count if new site and not fixed mod
                                    if (OldSite != ModList[iMissed + 1][iMod].GetSite() &&
                                        ModList[iMissed + 1][iMod].GetFixed() != 1) {
                                        NumModSitesCount++;
                                        OldSite = ModList[iMissed + 1][iMod].GetSite();
                                    }
                                }
                            }
                            NumMod[iMissed] = NumModCount;
                            NumModSites[iMissed] = NumModSitesCount;

                            // copy starts to next missed cleavage
                            PepStart[iMissed] = PepStart[iMissed + 1];
                        }

                        // init new start from old stop
                        PepEnd[Missed-1] += 1;
                        PepStart[Missed-1] = PepEnd[Missed-1];
                    }
                }

            }


        }


        if (GetSettings()->IsSetTaxids() && !TaxInfo)
            ERR_POST(Warning << 
                     "Taxonomically restricted search specified and no matching organisms found in sequence library.  Did you use a sequence library with taxonomic information?");

    }
    catch (NCBI_NS_STD::exception& e) {
        ERR_POST(Info << "Exception caught in CSearch::Search: " << e.what());
        throw;
    }

    //return PeakSet;
}

///
///  Adds modification information to hitset
///

void CSearch::AddModsToHit(CMSHits *Hit, CMSHit *MSHit)
{
    int i;
    for (i = 0; i < MSHit->GetNumModInfo(); i++) {
        // screen out fixed mods
        if (MSHit->GetModInfo(i).GetIsFixed() == 1) continue;
        CRef< CMSModHit > ModHit(new CMSModHit);
        ModHit->SetSite() = MSHit->GetModInfo(i).GetSite();
        ModHit->SetModtype() = MSHit->GetModInfo(i).GetModEnum() ;
        Hit->SetMods().push_back(ModHit);
    }
}


///
///  Adds ion information to hitset
///

void CSearch::AddIonsToHit(CMSHits *Hit, CMSHit *MSHit)
{
    int i;
    for (i = 0; i < MSHit->GetHits(); i++) {
        CRef<CMSMZHit> IonHit(new CMSMZHit);
        IonHit->SetIon() = MSHit->GetHitInfo(i).GetIonSeries();
        IonHit->SetCharge() = MSHit->GetHitInfo(i).GetCharge();
        IonHit->SetNumber() = MSHit->GetHitInfo(i).GetNumber();
        IonHit->SetMz() = MSHit->GetHitInfo(i).GetMZ();
        Hit->SetMzhits().push_back(IonHit);
    }
}


///
///  Makes a string hashed out of the sequence plus mods
///

void CSearch::MakeModString(string& seqstring, string& modseqstring, CMSHit *MSHit)
{
    int i;
    modseqstring = seqstring;
    for (i = 0; i < MSHit->GetNumModInfo(); i++) {
        modseqstring += NStr::IntToString(MSHit->GetModInfo(i).GetSite()) + ":" +
                        NStr::IntToString(MSHit->GetModInfo(i).GetModEnum()) + ",";
    }
}


void CSearch::CreateSequence(int Start,
                             int Stop,
                             string &seqstring, 
                             CSeqDBSequence &Sequence) 
{
    int iseq;
    seqstring.erase();
    
    for (iseq = Start; iseq <= Stop; iseq++) {
        seqstring += UniqueAA[Sequence.GetData()[iseq]];
    }
}   
    

void CSearch::SetResult(CRef<CMSPeakSet> PeakSet)
{

    double ThreshStart = GetSettings()->GetCutlo(); 
    double ThreshEnd = GetSettings()->GetCuthi();
    double ThreshInc = GetSettings()->GetCutinc();
    double Evalcutoff = GetSettings()->GetCutoff();

    CMSPeak* Peaks;

    TScoreList ScoreList;
    TScoreList::iterator iScoreList;
    CMSHit * MSHit;

    // set the search library version
    SetResponse()->SetDbversion(Getnumseq());

    // Reset the oid set for tracking results
    SetOidSet().clear();

    while(!PeakSet->GetPeaks().empty()) {
        Peaks = *(PeakSet->GetPeaks().begin());

        // add to hitset
        CRef< CMSHitSet > HitSet(null);

        // if iterative search, try to find hitset
        if (GetIterative()) {
            HitSet = SetResponse()->FindHitSet(Peaks->GetNumber());
            if (HitSet.IsNull())
                ERR_POST(Warning << "unable to find matching hitset");
        }
        
        // create a hitset if necessary
        if (HitSet.IsNull()) {
            HitSet = new CMSHitSet;
            if (!HitSet) {
                ERR_POST(Error << "omssa: unable to allocate hitset");
                return;
            }
            HitSet->SetNumber(Peaks->GetNumber());
            HitSet->SetIds() = Peaks->GetName();
            SetResponse()->SetHitsets().push_back(HitSet);
        }
        HitSet->SetSettingid() = GetSettings()->GetSettingid();

        // if there weren't enough peaks to do a search, note in error status
        if (Peaks->GetError() == eMSHitError_notenuffpeaks) {
            _TRACE("empty set");
            HitSet->SetError(eMSHitError_notenuffpeaks);
            ScoreList.clear();
            delete *(PeakSet->GetPeaks().begin());
            PeakSet->GetPeaks().pop_front();
            continue;
        }

        double Threshold, MinThreshold(ThreshStart), MinEval(1000000.0L);
        if (!UseRankScore) {
            // now calculate scores and sort
            for (Threshold = ThreshStart; Threshold <= ThreshEnd; 
                Threshold += ThreshInc) {
                CalcNSort(ScoreList, Threshold, Peaks);
                if (!ScoreList.empty()) {
                    _TRACE("Threshold = " << Threshold <<
                           "EVal = " << ScoreList.begin()->first);
                }
                if (!ScoreList.empty() && ScoreList.begin()->first < MinEval) {
                    MinEval = ScoreList.begin()->first;
                    MinThreshold = Threshold;
                }
                ScoreList.clear();
            }
        }
        _TRACE("Min Threshold = " << MinThreshold);
        CalcNSort(ScoreList,
                  MinThreshold,
                  Peaks);

        // if iterative search, check to see if hitset needs to be replaced
        if (GetIterative() && !ScoreList.empty()) {
            if ((GetSettings()->GetIterativesettings().GetReplacethresh() == 0.0 &&
                 (HitSet->GetHits().empty() ||
                  ScoreList.begin()->first <= (*HitSet->GetHits().begin())->GetEvalue()))  || 
                (GetSettings()->GetIterativesettings().GetReplacethresh() != 0.0 &&
                 ScoreList.begin()->first <= GetSettings()->GetIterativesettings().GetReplacethresh())) {
                HitSet->SetHits().clear();
            }
            else {
                ScoreList.clear();
                delete *(PeakSet->GetPeaks().begin());
                PeakSet->GetPeaks().pop_front();
                continue;
            }
        }

        const CMSSearchSettings::TTaxids& Tax = GetSettings()->GetTaxids();
        CMSSearchSettings::TTaxids::const_iterator iTax;

        // keep a list of redundant peptides
        map <string, CMSHits * > PepDone;
        int HitNum(0);
        // add to hitset by score
        for (iScoreList = ScoreList.begin();
            iScoreList != ScoreList.end();
            ++iScoreList,++HitNum) {

            double Score = iScoreList->first;
            if (Score > Evalcutoff)
                continue;
            if(GetSettings()->CanGetReportedhitcount())
                if(GetSettings()->GetReportedhitcount() != 0 && HitNum >= GetSettings()->GetReportedhitcount()) 
                    continue;
            
            CMSHits * Hit;
            CMSPepHit * Pephit;

            MSHit = iScoreList->second;

            CBlast_def_line_set::Tdata::const_iterator iDefLine;
            CRef<CBlast_def_line_set> Hdr = rdfp->GetHdr(MSHit->GetSeqIndex());
            // scan taxids
            for (iDefLine = Hdr->Get().begin();
                iDefLine != Hdr->Get().end();
                ++iDefLine) {
                if (GetSettings()->IsSetTaxids()) {
                    for (iTax = Tax.begin(); iTax != Tax.end(); iTax++) {
                        if ((*iDefLine)->GetTaxid() == *iTax) goto TaxContinue2;
                    } 
                    continue;
                }
                TaxContinue2:
                string seqstring, modseqstring;

                // keep a list of the oids
                SetOidSet().insert(MSHit->GetSeqIndex());
                // get the sequence
                CSeqDBSequence Sequence(rdfp.GetPointer(), MSHit->GetSeqIndex());

                string tempstartstop;
                CreateSequence(MSHit->GetStart(), MSHit->GetStop(),
                               seqstring, Sequence);
                MakeModString(seqstring, modseqstring, MSHit);

                if (PepDone.find(modseqstring) != PepDone.end()) {
                    Hit = PepDone[modseqstring];
                }
                else {
                    Hit = new CMSHits;
                    Hit->SetTheomass(MSHit->GetTheoreticalMass());
                    Hit->SetPepstring(seqstring);
                    // set the start AA, if there is one
                    if (MSHit->GetStart() > 0) {
                        tempstartstop = UniqueAA[Sequence.GetData()[MSHit->GetStart()-1]];
                        Hit->SetPepstart(tempstartstop);
                    }
                    else Hit->SetPepstart("");

                    // set the end AA, if there is one
                    if (MSHit->GetStop() < Sequence.GetLength() - 1) {
                        tempstartstop = UniqueAA[Sequence.GetData()[MSHit->GetStop()+1]];
                        Hit->SetPepstop(tempstartstop);
                    }
                    else Hit->SetPepstop("");

                    if (isnan(Score)) {
                        ERR_POST(Info << "Not a number in hitset " << 
                                 HitSet->GetNumber() <<
                                 " peptide " << modseqstring);
                        Score = kHighEval;
                    }
                    else if (!finite(Score)) {
                        ERR_POST(Info << "Infinite number in hitset " << 
                                 HitSet->GetNumber() <<
                                 " peptide " << modseqstring);
                        Score = kHighEval;
                    }
                    Hit->SetEvalue(Score);
                    Hit->SetPvalue(Score/Peaks->
                                   GetPeptidesExamined(MSHit->
                                                       GetCharge()));      
                    Hit->SetCharge(MSHit->GetCharge());
                    Hit->SetMass(MSHit->GetExpMass());
                    // insert mods here
                    AddModsToHit(Hit, MSHit);
                    // insert ions here
                    AddIonsToHit(Hit, MSHit);
                    CRef<CMSHits> hitref(Hit);
                    HitSet->SetHits().push_back(hitref);  
                    PepDone[modseqstring] = Hit;

                }

                Pephit = new CMSPepHit;

                if ((*iDefLine)->CanGetSeqid()) {
                    // find a gi
                    ITERATE(list< CRef<CSeq_id> >, seqid, (*iDefLine)->GetSeqid()) {
                        if ((**seqid).IsGi()) {
                            Pephit->SetGi((**seqid).GetGi());
                            break;
                        }
                    }

                    Pephit->SetAccession(
                                        FindBestChoice((*iDefLine)->GetSeqid(), CSeq_id::Score)->
                                        GetSeqIdString(false));
                }


                Pephit->SetStart(MSHit->GetStart());
                Pephit->SetStop(MSHit->GetStop());;
                Pephit->SetDefline((*iDefLine)->GetTitle());
                Pephit->SetProtlength(Sequence.GetLength());
                Pephit->SetOid(MSHit->GetSeqIndex());
                CRef<CMSPepHit> pepref(Pephit);
                Hit->SetPephits().push_back(pepref);

            }
        }
        ScoreList.clear();
        delete *(PeakSet->GetPeaks().begin());
        PeakSet->GetPeaks().pop_front();
    }
    // write bioseqs to output
    WriteBioseqs();
}


void CSearch::WriteBioseqs(void)
{
    ITERATE(CMSResponse::TOidSet, iOids, GetOidSet()) {
        CConstRef <CMSBioseq::TSeq> Bioseq(SetResponse()->SetBioseqs().GetBioseqByOid(*iOids));
        if (Bioseq.IsNull()) {
            CRef <CMSBioseq> MSBioseq (new CMSBioseq);
            MSBioseq->SetSeq(*rdfp->GetBioseq(*iOids));
            MSBioseq->SetOid() = *iOids;
            SetResponse()->SetBioseqs().Set().push_back(MSBioseq);
        }
    }
}


CMSMatchedPeakSet * CSearch::PepCharge(CMSHit& Hit,
                                       int SeriesCharge,
                                       int Ion,
                                       int minintensity,
                                       int Which, 
                                       CMSPeak *Peaks,
                                       int Maxproductions)
{
    int iii;
    int lowmz(0), highmz;

    unsigned Size = Hit.GetStop() - Hit.GetStart();
    if (Maxproductions == 0) Maxproductions = kMSLadderMax;


    // decide if there is any terminal bias
    EMSTerminalBias TerminalBias(eMSNoTerminalBias);

    for(iii = 0; iii < GetEnzyme()->GetCleaveNum(); ++iii) {
        // n term
        if(GetEnzyme()->GetCleaveOffset()[iii] == 1 ) {
            // check to see if should be biases on both ends
            if(TerminalBias == eMSNTerminalBias || TerminalBias == eMSNoTerminalBias)
                TerminalBias = eMSNTerminalBias;
            else
                TerminalBias = eMSBothTerminalBias;
        }
        // c term
        else if (GetEnzyme()->GetCleaveOffset()[iii] == 0 ) {            
            // check to see if should be biases on both ends
            if(TerminalBias == eMSCTerminalBias || TerminalBias == eMSNoTerminalBias)
                TerminalBias = eMSCTerminalBias;
            else
                TerminalBias = eMSBothTerminalBias;
        }
    }

//#if 0
    // make a copy of the peptide sequence
    CSeqDBSequence Sequence(rdfp.GetPointer(), Hit.GetSeqIndex());
    string seqstring;
    CreateSequence(Hit.GetStart(),
                   Hit.GetStop(),
                   seqstring,
                   Sequence);
//#endif
    bool NoProline = find(GetSettings()->GetNoprolineions().begin(),
                          GetSettings()->GetNoprolineions().end(),
                          Ion) != 
        GetSettings()->GetNoprolineions().end();
    // fill in the matched ions
    Hit.FillMatchedPeaks(SeriesCharge,
                         Ion,
                         Size,
                         minintensity,
                         false, 
                         TerminalBias, 
                         SeriesCharge*Maxproductions
//#if 0
                         ,
                         seqstring,
                         NoProline
//#endif
                         );
    CMSMatchedPeakSet *MatchPeakSet = Hit.SetIonSeriesMatchMap().SetSeries(SeriesCharge, Ion);
    TMatchedPeakSet::iterator bin, prev, next;

    for ( bin = MatchPeakSet->SetMatchedPeakSet().begin(); bin != MatchPeakSet->SetMatchedPeakSet().end(); ++bin) {
        // need to go thru match info, not hit info.
        if(bin != MatchPeakSet->SetMatchedPeakSet().begin()) {
            lowmz = ((*bin)->GetMZ() + (*prev)->GetMZ())/2;
        }
        next = bin;
        ++next;
        if(next != MatchPeakSet->SetMatchedPeakSet().end()) {
            highmz = ((*bin)->GetMZ() + (*next)->GetMZ())/2;
        }
        else highmz = Hit.GetExpMass()/SeriesCharge;
        (*bin)->SetExpIons() = 
            Peaks->CountMZRange(lowmz,
                                highmz,
                                minintensity,
                                Which) /
            (double)(highmz - lowmz);

        (*bin)->SetMassTolerance() = (Peaks->GetTol())/SeriesCharge;
        prev = bin;
    }
    return MatchPeakSet;
}




void CSearch::MatchAndSort(CMSPeak * Peaks, 
                           CMSHit& Hit, 
                           EMSPeakListTypes Which,
                           int minintensity,
                           const TSeriesChargePairList::const_iterator &iPairList,
                           list<CMSMatchedPeakSet *> &Forward, 
                           list<CMSMatchedPeakSet *> &Backward)
{
    CMSMatchedPeakSet * current;
    
    current = PepCharge(Hit,
                        iPairList->first,
                        iPairList->second,
                        minintensity,
                        Which,
                        Peaks,
                        GetSettings()->GetMaxproductions());

    if (kIonDirection[iPairList->second] == 1)
        Forward.push_back(current);
    else if (kIonDirection[iPairList->second] == -1)
        Backward.push_back(current);
}       


void CSearch::DoubleCompare(list<CMSMatchedPeakSet *> &SingleForward,
                            list<CMSMatchedPeakSet *> &SingleBackward,
                            list<CMSMatchedPeakSet *> &Double,
                            bool DoubleForward) 
{   
    list<CMSMatchedPeakSet *>::iterator iDouble, iFront, iBack;

    for (iDouble = Double.begin(); iDouble != Double.end(); ++iDouble) {
        
        for(iFront = SingleForward.begin(); iFront != SingleForward.end(); ++iFront) {
            (*iDouble)->Compare(*iFront, DoubleForward);
        }
        
        for(iBack = SingleBackward.begin(); iBack != SingleBackward.end(); ++iBack) {
            (*iDouble)->Compare(*iBack, !DoubleForward);
        }
    }             
}


void CSearch::CalcNSort(TScoreList& ScoreList,
                        double Threshold,
                        CMSPeak* Peaks
                       )
{
    int iCharges;
    int iHitList;
    int Tophitnum = GetSettings()->GetTophitnum();

    for (iCharges = 0; iCharges < Peaks->GetNumCharges(); iCharges++) {

        TMSHitList& HitList = Peaks->GetHitList(iCharges);   
        for (iHitList = 0; iHitList != Peaks->GetHitListIndex(iCharges);
            iHitList++) {

            int tempMass = HitList[iHitList].GetExpMass();
            int Charge = HitList[iHitList].GetCharge();
            EMSPeakListTypes Which = Peaks->GetWhich(Charge);

            // set up new score

 
            // minimum intensity
            int minintensity = static_cast <int> (Threshold * Peaks->GetMaxI(Which));


            TSeriesChargePairList::const_iterator iPairList;
            list <CMSMatchedPeakSet *> SingleForward, SingleBackward, DoubleForward, DoubleBackward;

            for (iPairList = SetLadderContainer().GetSeriesChargePairList().begin();
                iPairList != SetLadderContainer().GetSeriesChargePairList().end();
                ++iPairList) {

                // charge 1
                if (iPairList->first == 1) {
                    MatchAndSort(Peaks, HitList[iHitList], Which, minintensity,
                                 iPairList, SingleForward, SingleBackward);
                }
                else if (Charge >= Peaks->GetConsiderMult()) {
                    MatchAndSort(Peaks, HitList[iHitList], Which, minintensity,
                                  iPairList, DoubleForward, DoubleBackward);
                }
            }

            list <CMSMatchedPeakSet *> ::iterator iFront, iBack, iDouble;

            if(GetSettings()->GetNocorrelationscore() == 0) {
                // do the singly charge comparison
                for (iFront = SingleForward.begin(); iFront != SingleForward.end(); ++iFront) {
                    for(iBack = SingleBackward.begin(); iBack != SingleBackward.end(); ++iBack) {
                    (*iFront)->Compare(*iBack, false);
                    }
                }
                if (Charge >= Peaks->GetConsiderMult()) {
                    DoubleCompare(SingleForward, SingleBackward, DoubleForward, true);
                    DoubleCompare(SingleForward, SingleBackward, DoubleBackward, false); 
                }
            }


            double adjust = HitList[iHitList].GetMaxDelta() / 
                MSSCALE2INT(GetSettings()->GetMsmstol());
            if(adjust < GetSettings()->GetAutomassadjust()) 
                adjust = GetSettings()->GetAutomassadjust();
            if(adjust > 1.0) 
                adjust = 1.0;
            double a = 
                HitList[iHitList].CalcPoissonMean(GetSettings()->GetProbfollowingion(),
                                                  GetEnzyme()->GetCleaveNum(),
                                                  GetSettings()->GetProbfollowingion(),
                                                  19,
                                                  adjust);

            if (a == 0) {
                // threshold probably too high
                continue;
            }
            if (a < 0 ) {
                _TRACE("poisson mean is < 0");
                continue;
            }
            else if (isnan(a) || !finite(a)) {
                ERR_POST(Info << "poisson mean is NaN or is infinite");
                continue;
            }

            // keep going if obviously insignificant
            if (HitList[iHitList].GetHits() < a) continue;

            double pval; // statistical p-value
            int N; // number of peptides
            N = Peaks->GetPeptidesExamined(Charge) + 
                (GetSettings()->GetZdep() * (Charge - 1) + 1) *
                GetSettings()->GetPseudocount();

            if (!UseRankScore) {
                int High, Low, NumPeaks, NumLo, NumHi;
                Peaks->HighLow(High, Low, NumPeaks, tempMass, Charge, Threshold, NumLo, NumHi);

                double TopHitProb = ((double)Tophitnum)/NumPeaks;
                // correct for situation where more tophits than experimental peaks
                if (TopHitProb > 1.0) TopHitProb = 1.0;
                int numhits = HitList[iHitList].CountHits(Threshold, Peaks->GetMaxI(Which));
                double Normal = HitList[iHitList].CalcNormalTopHit(a, TopHitProb);
                pval = HitList[iHitList].CalcPvalueTopHit(a, numhits, Normal, TopHitProb);
            }
            else {
                pval = HitList[iHitList].CalcPvalue(a, HitList[iHitList].CountHits(Threshold, Peaks->GetMaxI(Which)));
            }
            if (UseRankScore && !GetPoissonOnly()) {
                if (HitList[iHitList].GetM() != 0.0) {
                    double Perf = HitList[iHitList].CalcRankProb();
                    _TRACE( "Perf=" << Perf << " pval=" << pval << " N=" << N );
                    pval *= Perf;
                    pval *= 10.0;  // correction to scales
                }
                else ERR_POST(Info << "M is zero");
            }
            double eval = 3e3 * pval * N;
//            _TRACE( " pval=" << pval << " eval=" << eval );
            ScoreList.insert(pair<const double, CMSHit *> 
                             (eval, &(HitList[iHitList])));
        }   
    } 
}

CSearch::~CSearch()
{
}


