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
* Author:  Aaron Ucko, Mati Shomrat, NCBI
*
* File Description:
*   fasta-file generator application
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_entry_ci.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <objtools/format/flat_file_config.hpp>
#include <objtools/format/flat_file_generator.hpp>
#include <objtools/format/flat_expt.hpp>
#include <objects/seqset/gb_release_file.hpp>

#include <objects/entrez2/Entrez2_boolean_element.hpp>
#include <objects/entrez2/Entrez2_boolean_reply.hpp>
#include <objects/entrez2/Entrez2_boolean_exp.hpp>
#include <objects/entrez2/Entrez2_eval_boolean.hpp>
#include <objects/entrez2/Entrez2_id_list.hpp>
#include <objects/entrez2/entrez2_client.hpp>

#include <objtools/cleanup/cleanup.hpp>

#include <util/compress/zlib.hpp>
#include <util/compress/stream.hpp>
#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ----------------------------------------------------------------------------
class CAnnotRecord
//  ----------------------------------------------------------------------------
{
public:
    CAnnotRecord();
    ~CAnnotRecord() {};
    
    bool SetRecord(
        const CRef< CSeq_feat > );
        
    void DumpRecord(
        CNcbiOstream& );
        
protected:
    bool CAnnotRecord::AssignType(
        const CRef< CSeq_feat > pFeature );
    bool CAnnotRecord::AssignSeqId(
        const CRef< CSeq_feat > pFeature );
    bool CAnnotRecord::AssignStart(
        const CRef< CSeq_feat > pFeature );
    bool CAnnotRecord::AssignStop(
        const CRef< CSeq_feat > pFeature );
    bool CAnnotRecord::AssignSource(
        const CRef< CSeq_feat > pFeature );
    bool CAnnotRecord::AssignScore(
        const CRef< CSeq_feat > pFeature );
    bool CAnnotRecord::AssignStrand(
        const CRef< CSeq_feat > pFeature );
    bool CAnnotRecord::AssignPhase(
        const CRef< CSeq_feat > pFeature );
    bool CAnnotRecord::AssignAttributesCore(
        const CRef< CSeq_feat > pFeature );
    bool CAnnotRecord::AssignAttributesExtended(
        const CRef< CSeq_feat > pFeature );

    string m_strSeqId;
    string m_strSource;
    string m_strType;
    string m_strStart;
    string m_strEnd;
    string m_strScore;
    string m_strStrand;
    string m_strPhase;
    string m_strAttributes;
};

//  ----------------------------------------------------------------------------
CAnnotRecord::CAnnotRecord():
//  ----------------------------------------------------------------------------
    m_strSeqId( "unknown" ),
    m_strSource( "Genbank" ),
    m_strType( "region" ),
    m_strStart( "1" ),
    m_strEnd( "0" ),
    m_strScore( "." ),
    m_strStrand( "." ),
    m_strPhase( "." ),
    m_strAttributes( "" )
{
}

//  ----------------------------------------------------------------------------
bool CAnnotRecord::AssignSeqId(
    const CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    m_strSeqId = "<unknown>";

    if ( pFeature->CanGetLocation() ) {
        const CSeq_loc& location = pFeature->GetLocation();
        const CSeq_id* pId = location.GetId();
        switch ( pId->Which() ) {
            
            case CSeq_id::e_Local:
                if ( pId->GetLocal().IsId() ) {
                    m_strSeqId = NStr::UIntToString( pId->GetLocal().GetId() );
                }
                else {
                    m_strSeqId = pId->GetLocal().GetStr();
                }
                break;

            case CSeq_id::e_Gi:
                m_strSeqId = NStr::IntToString( pId->GetGi() );
                break;

            case CSeq_id::e_Other:
                if ( pId->GetOther().CanGetAccession() ) {
                    m_strSeqId = pId->GetOther().GetAccession();
                    if ( pId->GetOther().CanGetVersion() ) {
                        m_strSeqId += ".";
                        m_strSeqId += NStr::UIntToString( 
                            pId->GetOther().GetVersion() ); 
                    }
                }
                break;

            default:
                break;
        }
    }

    return true;
}

//  ----------------------------------------------------------------------------
bool CAnnotRecord::AssignType(
    const CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    m_strType = "region";

    if ( pFeature->CanGetQual() ) {
        const vector< CRef< CGb_qual > >& quals = pFeature->GetQual();
        vector< CRef< CGb_qual > >::const_iterator it = quals.begin();
        while ( it != quals.end() ) {
            if ( (*it)->CanGetQual() && (*it)->CanGetVal() ) {
                if ( (*it)->GetQual() == "standard_name" ) {
                    m_strType = (*it)->GetVal();
                    return true;
                }
            }
            ++it;
        }
    }

    if ( ! pFeature->CanGetData() ) {
        return true;
    }

    switch ( pFeature->GetData().GetSubtype() ) {
    default:
        break;

    case CSeq_feat::TData::eSubtype_cdregion:
        m_strType = "CDS";
        break;

    case CSeq_feat::TData::eSubtype_mRNA:
        m_strType = "mRNA";
        break;

    case CSeq_feat::TData::eSubtype_scRNA:
        m_strType = "scRNA";
        break;

    case CSeq_feat::TData::eSubtype_exon:
        m_strType = "exon";
        break;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CAnnotRecord::AssignStart(
    const CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    if ( pFeature->CanGetLocation() ) {
        const CSeq_loc& location = pFeature->GetLocation();
        unsigned int uStart = location.GetStart( eExtreme_Positional ) + 1;
        m_strStart = NStr::UIntToString( uStart );
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CAnnotRecord::AssignStop(
    const CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    if ( pFeature->CanGetLocation() ) {
        const CSeq_loc& location = pFeature->GetLocation();
        unsigned int uEnd = location.GetStop( eExtreme_Positional ) + 1;
        m_strEnd = NStr::UIntToString( uEnd );
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CAnnotRecord::AssignSource(
    const CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    m_strSource = ".";

    if ( pFeature->CanGetQual() ) {
        const vector< CRef< CGb_qual > >& quals = pFeature->GetQual();
        vector< CRef< CGb_qual > >::const_iterator it = quals.begin();
        while ( it != quals.end() ) {
            if ( (*it)->CanGetQual() && (*it)->CanGetVal() ) {
                if ( (*it)->GetQual() == "source" ) {
                    m_strSource = (*it)->GetVal();
                    return true;
                }
            }
            ++it;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CAnnotRecord::AssignScore(
    const CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    m_strScore = ".";

    if ( pFeature->CanGetQual() ) {
        const vector< CRef< CGb_qual > >& quals = pFeature->GetQual();
        vector< CRef< CGb_qual > >::const_iterator it = quals.begin();
        while ( it != quals.end() ) {
            if ( (*it)->CanGetQual() && (*it)->CanGetVal() ) {
                if ( (*it)->GetQual() == "score" ) {
                    m_strScore = (*it)->GetVal();
                    return true;
                }
            }
            ++it;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CAnnotRecord::AssignStrand(
    const CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    m_strStrand = ".";
    if ( pFeature->CanGetLocation() ) {
        const CSeq_loc& location = pFeature->GetLocation();
        ENa_strand strand = location.GetStrand();
        switch( strand ) {
        default:
            break;
        case eNa_strand_plus:
            m_strStrand = "+";
            break;
        case eNa_strand_minus:
            m_strStrand = "-";
            break;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CAnnotRecord::AssignPhase(
    const CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    m_strPhase = ".";

    if ( ! pFeature->CanGetData() ) {
        return true;
    }
    const CSeq_feat::TData& data = pFeature->GetData();
    if ( data.GetSubtype() != CSeq_feat::TData::eSubtype_cdregion ) {
        return true;
    }

    const CCdregion& cdr = data.GetCdregion();
    CCdregion::TFrame frame = cdr.GetFrame();
    switch ( frame ) {
    default:
        break;
    case CCdregion::eFrame_one:
        m_strPhase = "0";
        break;
    case CCdregion::eFrame_two:
        m_strPhase = "1";
        break;
    case CCdregion::eFrame_three:
        m_strPhase = "2";
        break;
    }

    return true;
}

//  ----------------------------------------------------------------------------
bool CAnnotRecord::AssignAttributesCore(
    const CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    if ( pFeature->CanGetQual() ) {
        const vector< CRef< CGb_qual > >& quals = pFeature->GetQual();
        vector< CRef< CGb_qual > >::const_iterator it = quals.begin();
        while ( it != quals.end() ) {
            if ( (*it)->CanGetQual() && (*it)->CanGetVal() ) {
                if ( (*it)->GetQual() == "ID" ) {
                    if ( ! m_strAttributes.empty() ) {
                        m_strAttributes += ";";
                    }
                    m_strAttributes += "ID=";
                    m_strAttributes += (*it)->GetVal();
                }
                if ( (*it)->GetQual() == "Name" ) {
                    if ( ! m_strAttributes.empty() ) {
                        m_strAttributes += ";";
                    }
                    m_strAttributes += "Name=";
                    m_strAttributes += (*it)->GetVal();
                }
                if ( (*it)->GetQual() == "Var_type" ) {
                    if ( ! m_strAttributes.empty() ) {
                        m_strAttributes += ";";
                    }
                    m_strAttributes += "Var_type=";
                    m_strAttributes += (*it)->GetVal();
                }
            }
            it++;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CAnnotRecord::AssignAttributesExtended(
    const CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    if ( pFeature->CanGetComment() ) {
        if ( ! m_strAttributes.empty() ) {
            m_strAttributes += ";";
        }
        m_strAttributes += "comment=\"";
        m_strAttributes += pFeature->GetComment();
        m_strAttributes += "\"";
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CAnnotRecord::SetRecord(
    const CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    if ( ! AssignType( pFeature ) ) {
        return false;
    }
    if ( ! AssignSeqId( pFeature ) ) {
        return false;
    }
    if ( ! AssignStart( pFeature ) ) {
        return false;
    }
    if ( ! AssignStop( pFeature ) ) {
        return false;
    }
    if ( ! AssignScore( pFeature ) ) {
        return false;
    }
    if ( ! AssignStrand( pFeature ) ) {
        return false;
    }
    if ( ! AssignPhase( pFeature ) ) {
        return false;
    }
    if ( ! AssignAttributesCore( pFeature ) ) {
        return false;
    }
    if ( ! AssignAttributesExtended( pFeature ) ) {
        return false;
    }

    //if ( pFeature->CanGetId() ) {
    //}
    //if ( pFeature->CanGetData() ) {
    //    cerr << ":data:";
    //}
    //if ( pFeature->CanGetPartial() ) {
    //    cerr << ":partial:";
    //}
    //if ( pFeature->CanGetExcept() ) {
    //    cerr << ":except:";
    //}
    //if ( pFeature->CanGetProduct() ) {
    //    cerr << ":product:";
    //}
    //if ( pFeature->CanGetTitle() ) {
    //    cerr << ":title:";
    //}
    //if ( pFeature->CanGetExt() ) {
    //    cerr << ":ext:";
    //}
    //if ( pFeature->CanGetCit() ) {
    //    cerr << ":cit:";
    //}
    //if ( pFeature->CanGetExp_ev() ) {
    //    cerr << ":exp:";
    //}
    //if ( pFeature->CanGetXref() ) {
    //    cerr << ":xref:";
    //}
    //if ( pFeature->CanGetDbxref() ) {
    //}
    //if ( pFeature->CanGetExcept_text() ) {
    //    cerr << ":xpttxt:";
    //}
    //if ( pFeature->CanGetPseudo() ) {
    //    cerr << ":pseudo:";
    //}
    //if ( pFeature->CanGetIds() ) {
    //    cerr << ":ids:";
    //}
    //if ( pFeature->CanGetExts() ) {
    //    cerr << ":exts:";
    //}
    return true;
}

//  ----------------------------------------------------------------------------
void CAnnotRecord::DumpRecord(
    CNcbiOstream& out )
//  ----------------------------------------------------------------------------
{
    out << m_strSeqId + '\t';
    out << m_strSource << '\t';
    out << m_strType << '\t';
    out << m_strStart << '\t';
    out << m_strEnd << '\t';
    out << m_strScore << '\t';
    out << m_strStrand << '\t';
    out << m_strPhase << '\t';
    out << m_strAttributes << endl;
}


//  ----------------------------------------------------------------------------
class CAnnotWriterApp : public CNcbiApplication
//  ----------------------------------------------------------------------------
{
public:
    void Init(void);
    int  Run (void);

    bool HandleSeqAnnot(CRef<CSeq_annot>& se);
        
private:
    bool HandleSeqAnnotFtable(CRef<CSeq_annot>& se);
    bool HandleSeqAnnotAlign(CRef<CSeq_annot>& se);
    CObjectIStream* x_OpenIStream(const CArgs& args);

    // data
    CRef<CObjectManager>        m_Objmgr;       // Object Manager
    CNcbiOstream*               m_Os;           // Output stream
};

//  ----------------------------------------------------------------------------
void CAnnotWriterApp::Init()
//  ----------------------------------------------------------------------------
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(
        GetArguments().GetProgramBasename(),
        "Convert an ASN.1 to alternative file formats",
        false);
    
    // input
    {{
        // name
        arg_desc->AddOptionalKey("i", "InputFile", 
            "Input file name", CArgDescriptions::eInputFile);
    }}

    // output
    {{ 
        // name
        arg_desc->AddOptionalKey("o", "OutputFile", 
            "Output file name", CArgDescriptions::eOutputFile);
    }}
    
    SetupArgDescriptions(arg_desc.release());
}


//  ----------------------------------------------------------------------------
int CAnnotWriterApp::Run()
//  ----------------------------------------------------------------------------
{
	// initialize conn library
	CONNECT_Init(&GetConfig());

    const CArgs&   args = GetArgs();

    // create object manager
    m_Objmgr = CObjectManager::GetInstance();
    if ( !m_Objmgr ) {
        NCBI_THROW(CFlatException, eInternal, "Could not create object manager");
    }

    // open the output stream
    m_Os = args["o"] ? &(args["o"].AsOutputFile()) : &cout;
    if ( m_Os == 0 ) {
        NCBI_THROW(CFlatException, eInternal, "Could not open output stream");
    }

    auto_ptr<CObjectIStream> is;
    is.reset( x_OpenIStream( args ) );
    if (is.get() == NULL) {
        string msg = args["i"]? "Unable to open input file" + args["i"].AsString() :
                        "Unable to read data from stdin";
        NCBI_THROW(CFlatException, eInternal, msg);
    }

    while ( true ) {
        try {
            CRef<CSeq_annot> annot(new CSeq_annot);
            *is >> *annot;
            HandleSeqAnnot( annot );
        }
        catch ( CEofException& ) {
            break;
        }
        catch ( ... ) {
            is.reset( x_OpenIStream( args ) );
            CRef<CSeq_entry> entry( new CSeq_entry );
            *is >> *entry;
            CTypeIterator<CSeq_annot> annot_iter( *entry );
            for ( ;  annot_iter;  ++annot_iter ) {
                CRef< CSeq_annot > annot( annot_iter.operator->() );
                HandleSeqAnnot( annot );
            }
            break;
        }
    } 
    
    m_Os->flush();

    is.reset();
    return 0;
}

//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::HandleSeqAnnot( 
    CRef<CSeq_annot>& annot )
//  -----------------------------------------------------------------------------
{
    if ( annot->IsFtable() ) {
        return HandleSeqAnnotFtable( annot );
    }
    if ( annot->IsAlign() ) {
        return HandleSeqAnnotAlign( annot );
    }
    cerr << "Unexpected!" << endl;
    return false;
}

//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::HandleSeqAnnotFtable( 
    CRef<CSeq_annot>& annot )
//  -----------------------------------------------------------------------------
{
    if ( ! annot->IsFtable() ) {
        return false;
    }
    const list< CRef< CSeq_feat > >& table = annot->GetData().GetFtable();
    list< CRef< CSeq_feat > >::const_iterator it = table.begin();
    while ( it != table.end() ) {
        CAnnotRecord record;
        record.SetRecord( *it );
        record.DumpRecord( *m_Os );
        it++;
    }
    return true;
}

//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::HandleSeqAnnotAlign( 
    CRef<CSeq_annot>& annot )
//  -----------------------------------------------------------------------------
{
    if ( ! annot->IsAlign() ) {
        return false;
    }
//    cerr << "SeqAnnot.Align" << endl;
    return true;
}

//  -----------------------------------------------------------------------------
CObjectIStream* CAnnotWriterApp::x_OpenIStream( 
    const CArgs& args )
//  -----------------------------------------------------------------------------
{
    ESerialDataFormat serial = eSerial_AsnText;
    CNcbiIstream* pInputStream = &NcbiCin;
    bool bDeleteOnClose = false;
    if ( args["i"] ) {
        pInputStream = new CNcbiIfstream( args["i"].AsString().c_str(), ios::binary  );
        bDeleteOnClose = true;
    }
    CObjectIStream* pI = CObjectIStream::Open( serial, *pInputStream, bDeleteOnClose );
//    if ( 0 != pI ) {
//        pI->UseMemoryPool();
//    }
    return pI;
}

END_NCBI_SCOPE

USING_NCBI_SCOPE;

/////////////////////////////////////////////////////////////////////////////
//
// Main

int main(int argc, const char** argv)
{
    return CAnnotWriterApp().AppMain(argc, argv, 0, eDS_ToStderr, 0);
}
