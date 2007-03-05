/* $Id$
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
 * Author:  Chris Lanczycki
 *
 * File Description:
 *
 *       Subclass of CCdCore to build & represent a CD instantiated via mFASTA.
 *
 * ===========================================================================
 */


#include <ncbi_pch.hpp>

#include <corelib/ncbifile.hpp>

#include <objects/biblio/PubMedId.hpp>
#include <objects/pub/Pub.hpp>
#include <algo/structure/cd_utils/cuCD.hpp>
#include <algo/structure/cd_utils/cuCdReadWriteASN.hpp>
#include <algo/structure/cd_utils/cuReadFastaWrapper.hpp>
#include <algo/structure/cd_utils/cuSeqAnnotFromFasta.hpp>
#include <algo/structure/cd_utils/cuCdFromFasta.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)


// destructor
CCdFromFasta::~CCdFromFasta(void) {
}

// constructor
CCdFromFasta::CCdFromFasta()
{
    m_fastaInputErrorMsg = "";
    InitializeParameters();
}

CCdFromFasta::CCdFromFasta(const Fasta2CdParams& params) 
{
    m_fastaInputErrorMsg = "";
    InitializeParameters(&params);
}
    
CCdFromFasta::CCdFromFasta(const string& fastaFile, const Fasta2CdParams& params, CBasicFastaWrapper* fastaIOWrapper)
{

    m_fastaInputErrorMsg = "";
    m_fastaIO = fastaIOWrapper;
    InitializeParameters(&params);
    ImportAlignmentData(fastaFile);

}

    
void CCdFromFasta::InitializeParameters(const Fasta2CdParams* params)
{
    bool isNull = (!params);
    m_parameters.cdAcc = (isNull || params->cdAcc.length() == 0) ? "cdFrom_" : params->cdAcc;
    m_parameters.cdName = (isNull || params->cdName.length() == 0) ? m_parameters.cdAcc : params->cdName;

    //  Fill in name for the CD.
    SetName(m_parameters.cdName);

    //  SetAcccession will make a new accession object since none exists.
    //  Force the CD to have version zero.
    SetAccession(m_parameters.cdAcc, 0);

    m_parameters.useLocalIds = (isNull) ? false : params->useLocalIds;
    m_parameters.useAsIs = (isNull) ? true : params->useAsIs;
    m_parameters.masterMethod = (isNull) ? CSeqAnnotFromFasta::eMostAlignedAndFewestGaps : params->masterMethod;
    m_parameters.masterIndex = (isNull) ? 0 : params->masterIndex;

//    cerr << m_parameters.Print() << endl;
}



bool CCdFromFasta::ImportAlignmentData(const string& fastaFile)
{
    bool cleanUpFastaIO = (m_fastaIO == NULL);
    unsigned int len, masterIndex, nSeq = 1;
    string test, err;
    TReadFastaFlags fastaFlags = fReadFasta_AssumeProt;

    if (m_fastaIO == NULL) {
        m_fastaIO = new CBasicFastaWrapper(fastaFlags, false);
        if (!m_fastaIO) {
            m_fastaInputErrorMsg = "Unable to create fasta file I/O object\n";
            return false;
        }
    }

    if (m_parameters.useLocalIds) fastaFlags |= fReadFasta_NoParseID;
    m_fastaIO->SetFastaFlags(fastaFlags);

//    fastaFlags |= fReadFasta_AllSeqIds;
//    fastaFlags |= fReadFasta_RequireID;

    //  Set master as row 0 by default.
    masterIndex = 0;

    if (fastaFile.size() == 0) {
        m_fastaInputErrorMsg = "Unable to open file:  filename has size zero (?) \n";
        if (cleanUpFastaIO) delete m_fastaIO;
        return false;
    }

    CSeqAnnotFromFasta fastaSeqAnnot(!m_parameters.useAsIs, false, false);
    CNcbiIfstream ifs(fastaFile.c_str(), IOS_BASE::in);

    //  This parses the fasta file and constructs a Seq-annot object.  Alignment coordinates
    //  are indexed to the *DEGAPPED* FASTA-input sequences cached in 'fastaSeqAnnot'.  Note 
    //  that the Seq-entry in the 'fastaIO' object remains unaltered.
    if (!fastaSeqAnnot.MakeSeqAnnotFromFasta(ifs, *m_fastaIO, m_parameters.masterMethod, m_parameters.masterIndex)) {
        m_fastaInputErrorMsg = "Unable to extract an alignment from file " + fastaFile + "\n" + m_fastaIO->GetError() + "\n";
        if (cleanUpFastaIO) delete m_fastaIO;
        return false;
    }

    //  Testing....
/*
    unsigned int nr, nRead = m_fastaIO->GetNumRead();
    cout << "Num read = " << nRead << endl;
    for (nr = 0; nr < nRead; ++nr) {
        cout << "nr = " << nr << " defline = " << m_fastaIO->GetActiveDefline(nr) << endl;
        cout << "         sequence = " << m_fastaIO->GetActiveSequence(nr) << endl;
    }
*/


    //  Put the sequence data into the CD, after removing any gap characters.
    CRef < CSeq_entry > se(new CSeq_entry);
    se->Assign(*m_fastaIO->GetSeqEntry());

    //  Degap the sequence data *after* the seq-annot was made,
    //  and do some post-processing of the Bioseqs.
    //  Assuming we have some identifiers followed by text that ends
    //  up as a 'title' type of Seqdesc.
    for (list<CRef <CSeq_entry > >::iterator i = se->SetSet().SetSeq_set().begin();
         i != se->SetSet().SetSeq_set().end(); ++i, ++nSeq) {

        CBioseq& bs = (*i)->SetSeq();
        CSeqAnnotFromFasta::PurgeNonAlphaFromSequence(bs);

        //  Avoid writing out an empty 'descr' field.  (Calling bs.SetDescr()
        //  w/o passing the test automatically creates an empty 'descr' field!)
        if (!bs.IsSetDescr()) continue;

        if (bs.SetDescr().Set().size() == 0) {
            cerr << "sequence " << nSeq << ":  resetting description field due to no entries" << endl;
            bs.ResetDescr();
            continue;
        }

        if (!bs.SetDescr().Set().front()->IsTitle()) continue;

        //  For some cases, the description can come in w/ bad character...
        //  remove trailing non-printing characters from the description.
        test = bs.SetDescr().Set().front()->SetTitle();
        len = test.length();

        while (len > 0 && !ncbi::GoodVisibleChar(test[len-1])) {
            _TRACE("Description of seq " << nSeq  << " has " << len << " characters;");
            _TRACE("    the last one is not a printable char; truncating it!\n");
            bs.SetDescr().Set().front()->SetTitle().resize(len-1);  
            test = bs.SetDescr().Set().front()->SetTitle();
            len = test.length();
        }

    }

    SetSequences(*se);

    //  Add the alignment to the CD
    CRef<CSeq_annot> alignment(fastaSeqAnnot.GetSeqAnnot());
    SetSeqannot().push_back(alignment);

    if (cleanUpFastaIO) delete m_fastaIO;

    return true;
}


bool CCdFromFasta::AddComment(const string& comment)
{
    bool result = (comment.length() > 0);

    //  Don't add an identical comment.
    if (result && IsSetDescription()) {
        for (TDescription::Tdata::const_iterator cit = GetDescription().Get().begin(); result && cit != GetDescription().Get().end(); ++cit) {
            if ((*cit)->IsComment() && (*cit)->GetComment() == comment) {
                result = false;
            }
        }
    }

    if (result) {
        CRef<CCdd_descr> descr(new CCdd_descr);
        descr->SetComment(comment);
        result = AddCddDescr(descr);
    }
    return result;
}

bool CCdFromFasta::AddPmidReference(unsigned int pmid)
{
    //  Don't add a duplicate PMID.
    if (IsSetDescription()) {
        for (TDescription::Tdata::const_iterator cit = GetDescription().Get().begin(); cit != GetDescription().Get().end(); ++cit) {
            if ((*cit)->IsReference() && (*cit)->GetReference().IsPmid()) {
                if (pmid == (unsigned int) (*cit)->GetReference().GetPmid()) {
                    return false;
                }
            }
        }
    }

    //  validate the pmid???
    CRef<CPub> pub(new CPub);
    pub->SetPmid((CPub::TPmid) pmid);

    CRef<CCdd_descr> descr(new CCdd_descr);
    descr->SetReference(*pub);
    return AddCddDescr(descr);
}

bool CCdFromFasta::RemoveCddDescrsOfType(int cddDescrChoice)
{
    if (cddDescrChoice <= CCdd_descr::e_not_set || cddDescrChoice >= CCdd_descr::e_MaxChoice) return false;

    unsigned int count = 0;
    bool reachedEnd = false;
    CCdd_descr_set::Tdata::iterator i, iEnd;
    if (IsSetDescription()) {
        while (!reachedEnd) {
            i = SetDescription().Set().begin();
            iEnd = SetDescription().Set().end();
            for (; i != iEnd; i++) {
                if ((*i)->Which() == cddDescrChoice) {
                    ++count;
                    SetDescription().Set().erase(i);
                    break;
                }
            }
            reachedEnd = (i == iEnd);
        }
    }
    return (count > 0);
}


bool CCdFromFasta::AddSource(const string& source, bool removeExisting)
{
    bool result = (source.length() > 0);

    if (result) {
        if (removeExisting)
            RemoveCddDescrsOfType(CCdd_descr::e_Source);

        CRef<CCdd_descr> descr(new CCdd_descr);
        descr->SetSource(source);
        result = AddCddDescr(descr);
    }
    return result;
}

bool CCdFromFasta::UpdateSourceId(const string& sourceId, int version)
{
    bool foundExisting = false;
    bool result = (sourceId.length() > 0);
    CCdd_descr_set::Tdata::iterator i;
 
    // make a new source-id
    CRef< CCdd_id > ID(new CCdd_id);
    CRef< CGlobal_id > GID(new CGlobal_id);
    GID->SetAccession(sourceId);
    GID->SetVersion(version);
    ID->SetGid(*GID);
 
    // if there is a source-id replace it with the current one
    if (result && IsSetDescription()) {
        for (i=SetDescription().Set().begin(); i!=SetDescription().Set().end(); i++) {
            if ((*i)->IsSource_id()) {
                // reset it, and set the new one
                (*i)->SetSource_id().Reset();
                (*i)->SetSource_id().Set().push_back(ID);
                foundExisting = true;
            }
        }
    }


    // otherwise add another description, this one with a new old-root
    if (result && !foundExisting) {
        CRef < CCdd_descr > descr(new CCdd_descr);
        CRef < CCdd_id_set> setOfIds(new CCdd_id_set);
        setOfIds->Set().push_back(ID);
        descr->SetSource_id(*setOfIds);
        result = AddCddDescr(descr);
    }
    return result;
}

bool CCdFromFasta::AddCreateDate()  
{
    return SetCreationDate(this);
}


bool CCdFromFasta::AddCddDescr(CRef< CCdd_descr >& descr)
{
    if (!IsSetDescription()) {
        CCdd_descr_set* newDescrSet = new CCdd_descr_set();
        if (newDescrSet) 
            SetDescription(*newDescrSet);
        else 
            return false;
    }

    if (descr.NotEmpty()) {
        SetDescription().Set().push_back(descr);
        return true;
    }
    return false;
}



void CCdFromFasta::WriteToFile(const string& outputFile) const
{
    static const string cdExt = ".cn3";
    string cdOutFile, cdOutExt, err;

    cdOutFile = (outputFile.size() > 0) ? outputFile : "fastaCd";

    //  Find the last file extension...
    CFile::SplitPath(cdOutFile, NULL, NULL, &cdOutExt);
    if (cdOutFile.length() > 0 && cdOutExt != cdExt) {
        cdOutFile += cdExt;
    }

    if (!WriteASNToFile(cdOutFile.c_str(), *this, false, &err)) {
        cerr << "Error writing cd to file " << cdOutFile << endl << err << endl;
    }
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE
