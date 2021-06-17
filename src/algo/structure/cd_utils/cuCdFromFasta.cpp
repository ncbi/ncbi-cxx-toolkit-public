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

#include <algo/structure/cd_utils/cuCD.hpp>
#include <algo/structure/cd_utils/cuCdReadWriteASN.hpp>
#include <algo/structure/cd_utils/cuSeqAnnotFromFasta.hpp>
#include <algo/structure/cd_utils/cuCdFromFasta.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)


// destructor
CCdFromFasta::~CCdFromFasta(void) {
    CleanUpFastaIO();
}

// constructor
CCdFromFasta::CCdFromFasta()
{
    m_fastaInputErrorMsg = "";
    m_ownsFastaIO = true;
    InitializeParameters();
}

CCdFromFasta::CCdFromFasta(const Fasta2CdParams& params) 
{
    m_fastaInputErrorMsg = "";
    m_ownsFastaIO = true;
    InitializeParameters(&params);
}
    
CCdFromFasta::CCdFromFasta(const string& fastaFile, const Fasta2CdParams& params, CBasicFastaWrapper* fastaIOWrapper)
{

    m_fastaInputErrorMsg = "";
    m_fastaIO = fastaIOWrapper;
    m_ownsFastaIO = (m_fastaIO == NULL);  // if m_fastaIO is null, we'll create it and then need to destroy it
    InitializeParameters(&params);
    ImportAlignmentData(fastaFile);

}

void CCdFromFasta::ResetInitialMasterSequenceIndex() 
{
    m_initialMasterSequenceIndex = (unsigned int) CSeqAnnotFromFasta::eUnassignedMaster;
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
    ResetInitialMasterSequenceIndex();
}



bool CCdFromFasta::ImportAlignmentData(const string& fastaFile, bool cleanUp)
{
    unsigned int len, nSeq = 1;
    string test, err;
    TReadFastaFlags fastaFlags = CFastaReader::fAssumeProt | CFastaReader::fForceType;

    if (m_fastaIO == NULL) {
        m_fastaIO = new CBasicFastaWrapper(fastaFlags, false);
        if (!m_fastaIO) {
            m_fastaInputErrorMsg = "Unable to create fasta file I/O object\n";
            return false;
        }
    }

    if (m_parameters.useLocalIds) fastaFlags |= CFastaReader::fNoParseID;
    m_fastaIO->SetFastaFlags(fastaFlags);

//    fastaFlags |= fReadFasta_AllSeqIds;
//    fastaFlags |= fReadFasta_RequireID;

    if (fastaFile.size() == 0) {
        m_fastaInputErrorMsg = "Unable to open file:  filename has size zero (?) \n";
        if (cleanUp) CleanUpFastaIO();
        return false;
    }

    CSeqAnnotFromFasta fastaSeqAnnot(!m_parameters.useAsIs, false, false);
    CNcbiIfstream ifs(fastaFile.c_str(), IOS_BASE::in);

    //  This parses the fasta file and constructs a Seq-annot object.  Alignment coordinates
    //  are indexed to the *DEGAPPED* FASTA-input sequences cached in 'fastaSeqAnnot'.  Note 
    //  that the Seq-entry in the 'fastaIO' object remains unaltered.
    if (!fastaSeqAnnot.MakeSeqAnnotFromFasta(ifs, *m_fastaIO, m_parameters.masterMethod, m_parameters.masterIndex)) {
        m_fastaInputErrorMsg = "Unable to extract an alignment from file " + fastaFile + "\n" + m_fastaIO->GetError() + "\n";
        if (cleanUp) CleanUpFastaIO();
        return false;
    }
    m_initialMasterSequenceIndex = fastaSeqAnnot.GetMasterIndex();

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

    if (cleanUp) CleanUpFastaIO();

    return true;
}

string CCdFromFasta::GetDeflineReadFromFile(unsigned int index) const
{
    string result = "";
    unsigned int nRead = (m_fastaIO) ? m_fastaIO->GetNumRead() : 0;
    
    if (index < nRead) {
        result = m_fastaIO->GetActiveDefline(index);
    }
    return result;
}

string CCdFromFasta::GetSequenceReadFromFile(unsigned int index) const
{
    string result = "";
    unsigned int nRead = (m_fastaIO) ? m_fastaIO->GetNumRead() : 0;
    
    if (index < nRead) {
        result = m_fastaIO->GetActiveSequence(index);
    }
    return result;
}


string CCdFromFasta::GetColumnReadFromFile(unsigned int column) const
{
    static const string gap("-");
    string result = "";
    string rowSequence;
    unsigned int row = 0, nNoSuchColumn = 0;
    unsigned int nRead = (m_fastaIO) ? m_fastaIO->GetNumRead() : 0;

    for (; row < nRead; ++row) {
        rowSequence = m_fastaIO->GetActiveSequence(row);
        if (column < rowSequence.length()) {
            column += rowSequence[column];
        } else {
            result += gap;
            ++nNoSuchColumn;
        }
    }

    //  If column index out of range for all rows, return empty string.
    if (nNoSuchColumn == nRead) {
        result.erase();
    }

    return result;
}

unsigned int CCdFromFasta::GetAllColumnsReadFromFile(map<unsigned int, string>& columns) const {
    return GetColumnsReadFromFile(columns, false);
}
unsigned int CCdFromFasta::GetGaplessColumnsReadFromFile(map<unsigned int, string>& gaplessColumns) const {
    return GetColumnsReadFromFile(gaplessColumns, true);
}

unsigned int CCdFromFasta::GetColumnsReadFromFile(map<unsigned int, string>& columns, bool skipGappedColumns) const
{
    static const string gap("-");
    static const string letters("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");

    string result = "";
    string rowSequence, columnString;
    unsigned int nColumns, col;  
    unsigned int row = 0, nNoSuchColumn = 0;
    unsigned int nRead = (m_fastaIO) ? m_fastaIO->GetNumRead() : 0;
    ncbi::SIZE_TYPE nonLetterPosition;
    map<unsigned int, string> activeSequences;

    columns.clear();

    for (; row < nRead; ++row) {
//        rowSequence = m_fastaIO->GetActiveSequence(row);
        activeSequences[row] = m_fastaIO->GetActiveSequence(row, true);
    }

    //  by convention, each character in the first row of the FASTA is a column.
    nColumns = activeSequences[0].length();
    for (col = 0; col < nColumns; ++col) {

        columnString.erase();
        for (row = 0; row < nRead; ++row) {
            if (col < activeSequences[row].length()) {
                columnString += activeSequences[row][col];
            } else {
                columnString += gap;
                ++nNoSuchColumn;
            }
        }

        if (nNoSuchColumn == nRead) {
            columnString.erase();
        }

        if (skipGappedColumns) {
            nonLetterPosition = columnString.find_first_not_of(letters);
            if (nonLetterPosition != NPOS) {
//                cerr << "found gap in column " << col << " (" << columnString[nonLetterPosition] << " at position " << nonLetterPosition << "); column string:\n" << columnString << endl;
                columnString.erase();
            }
        }
            
        if (columnString.length() > 0) {
            columns[col] = columnString;
        }
    }

    return columns.size();
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


bool CCdFromFasta::WriteToFile(const string& outputFile) const
{
    static const string cdExt = ".cn3";
    bool result = true;
    string cdOutFile, cdOutExt, err;

    cdOutFile = (outputFile.size() > 0) ? outputFile : "fastaCd";

    //  Find the last file extension...
    CFile::SplitPath(cdOutFile, NULL, NULL, &cdOutExt);
    if (cdOutFile.length() > 0 && cdOutExt != cdExt) {
        cdOutFile += cdExt;
    }

    if (!WriteASNToFile(cdOutFile.c_str(), *this, false, &err)) {
        result = false;
        cerr << "Error writing cd to file " << cdOutFile << endl << err << endl;
    }
    return result;
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE
