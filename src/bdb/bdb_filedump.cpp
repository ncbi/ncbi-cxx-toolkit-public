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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:  BDB File covertion into text.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistre.hpp>

#include <bdb/bdb_file.hpp>
#include <bdb/bdb_cursor.hpp>
#include <bdb/bdb_filedump.hpp>
#include <bdb/bdb_blob.hpp>

#include <bdb/bdb_query.hpp>
#include <bdb/bdb_query_parser.hpp>


#include <iomanip>

BEGIN_NCBI_SCOPE

CBDB_FileDumper::CBDB_FileDumper(const string& col_separator)
: m_ColumnSeparator(col_separator),
  m_PrintNames(ePrintNames),
  m_ValueFormatting(eNoQuotes),
  m_BlobFormat(eBlobSummary | eBlobAsHex),
  m_RecordsDumped(0),
  m_Query(0),
  m_OutFile(0)
{
}

CBDB_FileDumper::CBDB_FileDumper(const CBDB_FileDumper& fdump)
: m_ColumnSeparator(fdump.m_ColumnSeparator),
  m_BlobDumpFname(fdump.m_BlobDumpFname),
  m_PrintNames(fdump.m_PrintNames),
  m_ValueFormatting(fdump.m_ValueFormatting),
  m_BlobFormat(fdump.m_BlobFormat),
  m_RecordsDumped(0),
  m_QueryStr(fdump.m_QueryStr),
  m_Query(0),
  m_OutFile(0)
{
}

CBDB_FileDumper::~CBDB_FileDumper()
{
    delete m_Query;
}

CBDB_FileDumper& CBDB_FileDumper::operator=(const CBDB_FileDumper& fdump)
{
    m_ColumnSeparator = fdump.m_ColumnSeparator;
    m_BlobDumpFname = fdump.m_BlobDumpFname;
    m_PrintNames = fdump.m_PrintNames;
    m_ValueFormatting = fdump.m_ValueFormatting;
    m_BlobFormat = fdump.m_BlobFormat;
    
    m_RecordsDumped = 0;
    m_QueryStr = fdump.m_QueryStr;
    
    delete m_Query; m_Query = 0;
    
    m_OutFile = 0;
    
    return *this;
}

void CBDB_FileDumper::SetQuery(const string& query_str)
{
    auto_ptr<CBDB_Query>  q(new CBDB_Query);
    BDB_ParseQuery(query_str.c_str(), q.get());
    m_QueryStr = query_str;
    m_Query = q.release();

    // BDB_PrintQueryTree(cout, *m_Query);
}

void CBDB_FileDumper::Dump(const string& dump_file_name, CBDB_File& db)
{
    CNcbiOfstream out(dump_file_name.c_str());
    if (!out) {
        string err = "Cannot open text file:";
        err.append(dump_file_name);
        BDB_THROW(eInvalidValue, err);
    }

    Dump(out, db);
}

void CBDB_FileDumper::Dump(CNcbiOstream& out, CBDB_File& db)
{
    // Print values
    CBDB_FileCursor cur(db);
    cur.SetCondition(CBDB_FileCursor::eFirst);
	
    Dump(out, cur);
}

static const char* kNullStr = "NULL";

/// Query scanner
///
/// @internal
class CBDB_DumpScanner : public CBDB_FileScanner
{
public:
    CBDB_DumpScanner(CBDB_File& dbf) : CBDB_FileScanner(dbf) {}
    virtual EScanAction OnRecordFound() { return eContinue; }
};        

void CBDB_FileDumper::Dump(CNcbiOstream& out, CBDB_FileCursor& cur)
{
    CBDB_File& db = cur.GetDBFile();
    CBDB_BLobFile* blob_db = dynamic_cast<CBDB_BLobFile*>(&db);

    const CBDB_BufferManager* key  = db.GetKeyBuffer();
    const CBDB_BufferManager* data = db.GetDataBuffer();
    
   
    // Regular data file 	
	
    vector<unsigned> key_quote_flags;
    vector<unsigned> data_quote_flags;

    if (key) {
        x_SetQuoteFlags(&key_quote_flags, *key);
    }
    if (data) {
        x_SetQuoteFlags(&data_quote_flags, *data);
    }
	
	
    // Print header
    if (m_PrintNames == ePrintNames) {
        PrintHeader(out, key, data);
    }
    
    m_RecordsDumped = 0;
    
    CBDB_DumpScanner scan_filter(db);

    string blob_sz;
    for ( ;cur.Fetch() == eBDB_Ok; ++m_RecordsDumped) {
        
        if (m_Query) {  // Filtered output
            bool query_res = scan_filter.StaticEvaluate(*m_Query);
            
            //BDB_PrintQueryTree(cout, *m_Query);
            
            m_Query->ResetQueryClause();
            if (!query_res) {
                --m_RecordsDumped;
                continue;
            }
        }
        
        
        if (key) {
            x_DumpFields(out, *key, key_quote_flags, true/*key*/);
        }
        
        // BLOB dump
        if (blob_db) {
            unsigned char buf[2048];
            
            unsigned size = blob_db->LobSize();
            out << m_ColumnSeparator;
            if (size) {
                
                auto_ptr<CBDB_BLobStream> blob_stream( 
                        blob_db->CreateStream());
                
                if ((m_BlobFormat & eBlobAll) == 0) { // BLOB summary
                    out << "[LOB, size= " 
                        << size 
                        << " {";
                    
                    size_t bytes_read;
                    blob_stream->Read(buf, 128, &bytes_read);
                    unsigned sp_counter = 0;
                    for (unsigned int i = 0; i < bytes_read; ++i) {
                        if (m_BlobFormat & eBlobAsHex) {
                          out << setfill('0') << hex << setw(2) 
                              << (unsigned)buf[i];
                           if (++sp_counter == 4) {
                               cout << " ";
                               sp_counter = 0;
                           }
                        } else {
                           out << (char)buf[i];
                        }
                    }
                    if (bytes_read < size) {
                        out << " ...";
                    }
                    out << "}]";
                                        
                } else {  // All BLOB
                    
                    out << "BLOB. size=" << size << "\n";
                    
                    size_t bytes_read;
                    do {
                        blob_stream->Read(buf, 2048, &bytes_read);
                        if (m_BlobFormat & eBlobAsHex) {
                            unsigned sp_counter = 0;
                            for (unsigned int i = 0; i < bytes_read; ++i) {
                               out << setfill('0') << hex << setw(2) 
                                   << (unsigned)buf[i];
                               if (++sp_counter == 4) {
                                   cout << " ";
                                   sp_counter = 0;
                               }
                               if (i > 0 && (i % 79 == 0)) {
                                   out << "\n";
                               }
                            }
                        } else { // BLOB as text
                            for (unsigned int i = 0; i < bytes_read; ++i) {
                                out << (char)buf[i];
                            }
                        }
                    } while (bytes_read);

                    out << "\n<<END BLOB>>\n";
                    
                }
                
                // Dump BLOB to a file 
                // (works only for the first record)
                
                if (m_BlobDumpFname.length() && m_RecordsDumped == 0) {
                    CNcbiOfstream ofs(m_BlobDumpFname.c_str(), 
                                      IOS_BASE::out | 
                                      IOS_BASE::trunc | 
                                      IOS_BASE::binary);
                    if (ofs.is_open()) {
                        blob_stream.reset(blob_db->CreateStream());
                        char buffer[2048];

                        out << "BLOB. size=" << size << "\n";

                        size_t bytes_read;
                        do {
                            blob_stream->Read(buffer, 2048, &bytes_read);
                            ofs.write(buffer, bytes_read);
                        } while (bytes_read);
                    }
                }
                
                
            } else {
                out << kNullStr;
            }
        }
        
        if (data) {
            x_DumpFields(out, *data, data_quote_flags, false/*not key*/);
        }
        
        // Copy recors to output file
        if (m_OutFile) {
            m_OutFile->CopyFrom(db);
            m_OutFile->Insert();
        }
        
        out << NcbiEndl;
    } // for
	
}


void CBDB_FileDumper::x_DumpFields(CNcbiOstream&             out, 
                                   const CBDB_BufferManager& bman, 
                                   const vector<unsigned>&   quote_flags, 
                                   bool                      is_key)
{
    for (unsigned i = 0; i < bman.FieldCount(); ++i) {
        const CBDB_Field& fld = bman.GetField(i);
        if (is_key) {
            if (i != 0)
                out << m_ColumnSeparator;
        } else {
            out << m_ColumnSeparator;
        }
        
	unsigned qf = quote_flags[i];
    	if (qf) {
	    out << '"';
    	}
        out << (fld.IsNull() ? kNullStr : fld.GetString());
        
    	if (qf) {    
	    out << '"';
    	}
    }
}        


void CBDB_FileDumper::PrintHeader(CNcbiOstream& out,
		                          const CBDB_BufferManager* key,
				 				  const CBDB_BufferManager* data)
{
    if (key) {
        for (unsigned i = 0; i < key->FieldCount(); ++i) {
            const CBDB_Field& fld = key->GetField(i);
            if (i != 0)
                out << m_ColumnSeparator;
            out << fld.GetName();
        }
    }

    if (data) {
        for (unsigned i = 0; i < data->FieldCount(); ++i) {
            const CBDB_Field& fld = data->GetField(i);
            out << m_ColumnSeparator << fld.GetName();
        }
    }
    out << NcbiEndl;
}		

void CBDB_FileDumper::x_SetQuoteFlags(vector<unsigned>*         flags, 
 		                              const CBDB_BufferManager& bman)
{
    flags->resize(0);
    for (unsigned i = 0; i < bman.FieldCount(); ++i) {
        switch (m_ValueFormatting) {
        case eNoQuotes:
            flags->push_back(0);
            break;
        case eQuoteAll:
            flags->push_back(1);
            break;
        case eQuoteStrings:
            {
            const CBDB_Field& fld = bman.GetField(i);
            const CBDB_FieldStringBase* bstr = 
		            dynamic_cast<const CBDB_FieldStringBase*>(&fld);
            flags->push_back(bstr ? 1 : 0);
            }
            break;
        default:
            _ASSERT(0);
        } // switch

    } // for
	
}
		
		
END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.11  2005/02/02 19:49:54  grichenk
 * Fixed more warnings
 *
 * Revision 1.10  2004/06/30 16:27:22  kuznets
 * Fixed casting for correct BLOB printing
 *
 * Revision 1.9  2004/06/29 12:28:02  kuznets
 * Added option to copy all db records to another file
 *
 * Revision 1.8  2004/06/28 12:17:42  kuznets
 * Improved BLOB dumping
 *
 * Revision 1.7  2004/06/23 19:37:47  kuznets
 * Added counter for dumped records
 *
 * Revision 1.6  2004/06/23 14:10:27  kuznets
 * Improved BLOB dumping
 *
 * Revision 1.5  2004/06/22 18:27:46  kuznets
 * Implemented BLOB dumping(summary)
 *
 * Revision 1.4  2004/06/21 15:08:46  kuznets
 * file dumper changed to work with cursors
 *
 * Revision 1.3  2004/05/17 20:55:11  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.2  2003/10/28 14:57:13  kuznets
 * Implemeneted field names printing
 *
 * Revision 1.1  2003/10/27 14:18:22  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
