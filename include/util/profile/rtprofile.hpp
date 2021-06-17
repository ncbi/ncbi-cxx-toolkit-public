// add-hock class to make CStopWatch instancess available as global object
#ifndef UTIL_PROFILE___RTPROFILE_HPP
#define UTIL_PROFILE___RTPROFILE_HPP

#include <sys/types.h>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbitime.hpp> 
#include <corelib/ncbimtx.hpp>

#include <string>
#include <vector>
#include <map>
#include <list>
#include <utility> 

BEGIN_NCBI_SCOPE

#define BLAST_PROF_REPORT CRtProfiler::getInstance()->DoReport()
#define BLAST_PROF_START(sw_name) CRtProfiler::getInstance()->Start( std::string( #sw_name ) ) 
#define BLAST_PROF_START2(sw_name) CRtProfiler::getInstance()->Start( sw_name ) 
#define BLAST_PROF_STOP(sw_name) CRtProfiler::getInstance()->Stop( std::string( #sw_name ) ) 
#define BLAST_PROF_ADD(key_name,key_val) CRtProfiler::getInstance()->AddUserKVMT(std::string( #key_name ), key_val )
#define BLAST_PROF_ADD2(key_name,key_val) CRtProfiler::getInstance()->AddUserKVMT( std::string( #key_name ), std::string( #key_val ) )

#define BLAST_PROF_MARK(marker_name) CRtProfiler::getInstance()->AddMarkerMT( std::string( #marker_name ) )
#define BLAST_PROF_MARK2(marker_val) CRtProfiler::getInstance()->AddMarkerMT(  marker_val  )
class CRtProfiler 
{
    private:
	static CRtProfiler* instance;

	CRtProfiler( size_t stop_watch_sz=0);


    public:
	static CRtProfiler* getInstance();
	// methods to access stop watches. int index => m_sw_vec, string => m_sw_map;
	// https://www.ncbi.nlm.nih.gov/IEB/ToolBox/CPP_DOC/doxyhtml/classCStopWatch.html
	// Start, Stop, AsString
	//
	void Allocate( size_t sw_ndx);
	void Allocate(std::string sw_name);
	//...
	void Start( size_t sw_ndx);
	void Start(std::string sw_name);
	void StartMT( size_t sw_ndx);
	void StartMT(std::string sw_name);
	//..
	void Stop(size_t sw_ndx);
	void Stop(std::string sw_name);
	void StopMT(size_t sw_ndx);
	void StopMT(std::string sw_name);
	//...
	void AddMarkerMT(std::string new_marker );
	//...
	void AddUserKV(const std::string &key_name, bool bool_val );
	void AddUserKVMT(const std::string &key_name, bool bool_val );
	void AddUserKV(const std::string &key_name, int int_val );
	void AddUserKVMT(const std::string &key_name, int int_val );
	void AddUserKV(const std::string  &key_name, const std::string &key_val );
	void AddUserKVMT(const std::string  &key_name, const std::string &key_val );
	//...
	std::string 	AsString (size_t sw_ndx, const CTimeFormat &fmt=kEmptyStr) ;
	std::string 	AsString (std::string sw_name, const CTimeFormat &fmt=kEmptyStr) ;
	std::string 	AsStringMT (size_t sw_ndx, const CTimeFormat &fmt=kEmptyStr) ;
	std::string 	AsStringMT (std::string sw_name, const CTimeFormat &fmt=kEmptyStr) ;
	//.... 
	bool CheckDoReport( std::string &report_name );
	bool DoReport(const CTimeFormat &fmt=kEmptyStr) ;

    private:
	CStopWatch m_w0;
	std::vector< CStopWatch > m_sw_vec;          // OBSOLETE. 
	std::map< std::string,CStopWatch > m_sw_map;
	CFastMutex m_sw_vec_mx;                      // OBSOLETE
	CFastMutex m_sw_map_mx;
	//...
	std::list< std::pair <std::string, std::string> > m_markers;
	CFastMutex m_markers_mx;
	//...
	CTimeFormat m_ctfmt;
	std::list <std::pair< std::string, std::string > > m_user_data;
	CFastMutex m_user_data_mx;
};


END_NCBI_SCOPE
#endif

