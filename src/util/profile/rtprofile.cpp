#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>
#include <stdlib.h>

#include <util/profile/rtprofile.hpp>
#include <corelib/ncbi_process.hpp>

BEGIN_NCBI_SCOPE

CRtProfiler* CRtProfiler::instance = 0;

CRtProfiler* CRtProfiler::getInstance()
{
   if (instance == 0){
       instance = new CRtProfiler();
   }
   return instance;
}


CRtProfiler::CRtProfiler ( size_t stop_watch_sz )
{
   if ( stop_watch_sz ) {
       for( size_t sw_ndx = 0; sw_ndx < stop_watch_sz; ++sw_ndx ){
	   m_sw_vec.push_back( CStopWatch() );
       }
   }
}

void CRtProfiler::Allocate( size_t sw_ndx){
    if( sw_ndx  < m_sw_vec.size() ) return; // array is larger enought
    // ok. user index is larger then current size, enlarge ( ru-tim penalties )
    for( size_t sw_item_ndx = m_sw_vec.size();  sw_item_ndx < sw_ndx; ++sw_item_ndx ){
	   m_sw_vec.push_back( CStopWatch() );
    }
}
void CRtProfiler::Allocate(std::string sw_name){
    std::map< std::string,CStopWatch >::iterator it = m_sw_map.find( sw_name );
    if( it != m_sw_map.end() ) return; // CStopWach already exists wih this name
    // allocate new CStopWatch instance with this name
    m_sw_map[ sw_name ] = CStopWatch();
}
void CRtProfiler::Start( size_t sw_ndx){
    if( sw_ndx  < m_sw_vec.size() ) Allocate( sw_ndx);
    m_sw_vec[ sw_ndx ].Start();
}
void CRtProfiler::Start(std::string sw_name){ 
    std::map< std::string,CStopWatch >::iterator it = m_sw_map.find( sw_name );
    if( it == m_sw_map.end() ) Allocate( sw_name);
    m_sw_map[ sw_name ].Start();
}
void CRtProfiler::StartMT( size_t sw_ndx){
    m_sw_vec_mx.Lock();
    Start(sw_ndx);
    m_sw_vec_mx.Unlock();
}
void CRtProfiler::StartMT(std::string sw_name){
    m_sw_map_mx.Lock();
    Start(sw_name);
    m_sw_map_mx.Unlock();
}
void CRtProfiler::Stop( size_t sw_ndx){
    if( sw_ndx  > m_sw_vec.size() )  return;
    m_sw_vec[ sw_ndx ].Stop();
}
void CRtProfiler::Stop(std::string sw_name){ 
    std::map< std::string,CStopWatch >::iterator it = m_sw_map.find( sw_name );
    if( it == m_sw_map.end() ) return;
    m_sw_map[ sw_name ].Stop();
}
void CRtProfiler::StopMT(size_t sw_ndx){
    m_sw_vec_mx.Lock();
    Stop(sw_ndx);
    m_sw_vec_mx.Unlock();
}
void CRtProfiler::StopMT(std::string sw_name){
    m_sw_map_mx.Lock();
    Stop(sw_name);
    m_sw_map_mx.Unlock();
}
void CRtProfiler::AddUserKV(const std::string &key_name, bool bool_val ){
    std::string key_val = (bool_val?string("TRUE"):string("FALSE"));
    AddUserKV( key_name, key_val);
}
void CRtProfiler::AddUserKVMT(const std::string &key_name,  bool bool_val ){
    std::string key_val = (bool_val?string("TRUE"):string("FALSE"));
    m_user_data_mx.Lock();
    AddUserKV( key_name, key_val);
    m_user_data_mx.Unlock();
}
void CRtProfiler::AddUserKV(const std::string &key_name, int int_val ){
    std::string key_val = NStr::NumericToString( int_val ); 
    AddUserKV( key_name, key_val);
}
void CRtProfiler::AddUserKVMT(const std::string &key_name, int int_val ){
    std::string key_val = NStr::NumericToString( int_val ); 
    m_user_data_mx.Lock();
    AddUserKV( key_name, key_val);
    m_user_data_mx.Unlock();
}
void CRtProfiler::AddUserKVMT(const std::string &key_name, const std::string &key_val ){
    m_user_data_mx.Lock();
    AddUserKV( key_name, key_val);
    m_user_data_mx.Unlock();
}
void CRtProfiler::AddUserKV(const std::string &key_name, const std::string &key_val ){
    m_user_data.push_back( std::make_pair( key_name, key_val) );
}
std::string 	CRtProfiler::AsString (size_t sw_ndx, const CTimeFormat &fmt) {
	if( sw_ndx  > m_sw_vec.size() )  return string("BAD_INDEX:") + NStr::NumericToString( (int) sw_ndx) ;
	return m_sw_vec[ sw_ndx ].AsString (fmt);
}

std::string 	CRtProfiler::AsString (std::string sw_name, const CTimeFormat &fmt) {
    std::map< std::string,CStopWatch >::const_iterator it = m_sw_map.find( sw_name );
    if( it == m_sw_map.end() ) return string("BAD_NAME:") + sw_name;
    return m_sw_map[ sw_name ].AsString (fmt);
}
std::string 	CRtProfiler::AsStringMT (size_t sw_ndx, const CTimeFormat &fmt){
    string ret_str;
    m_sw_vec_mx.Lock();
    ret_str = AsString(sw_ndx,fmt);
    m_sw_vec_mx.Unlock();
    return ret_str;
}
std::string 	CRtProfiler::AsStringMT (std::string sw_name, const CTimeFormat &fmt){
    string ret_str;
    m_sw_map_mx.Lock();
    ret_str = AsString(sw_name,fmt);
    m_sw_map_mx.Unlock();
    return ret_str;
}

void CRtProfiler::AddMarkerMT (std::string new_marker){
    CTimeFormat ctfmt("Y-M-DTh:m:g"); 
    CTime time_now( CTime::eCurrent  );

    std::string new_time_str = time_now.AsString( ctfmt );

    m_markers_mx.Lock();
    m_markers.push_back(  std::make_pair ( new_marker, new_time_str  )  );
    m_markers_mx.Unlock();
    return;
}
//
// environment variable BLASTAPI_PROFILE_LOG:
// 1) not set - no report
// 2) false   - no report
// 3) true    - do report, report name  name: blastapi_profile.csv
// 4) any string not 'false' - do report, report name == environment variable
bool CRtProfiler::CheckDoReport( std::string &report_name ) {
    string profile_log = "blastapi_profile.csv";
    char *env_ptr = getenv("BLASTAPI_PROFILE_LOG");
    if( env_ptr == NULL )  return false; // no loging #1
    if( !NStr::CompareNocase( string(env_ptr),"false") )  return false; // no loging #2
    report_name = profile_log; // default name
    if( NStr::CompareNocase( string(env_ptr),"true") )  {
        profile_log = string( env_ptr ) ;  // custom name #4 
    }
    return true;
}
bool CRtProfiler::DoReport(const CTimeFormat &fmt) {
    std::string report_name;
    if( !CheckDoReport( report_name ) ) return false;
    std::ofstream log_ofs;

    // REPORT #1 =======================================================================
    if( !m_user_data.empty() ) {
      log_ofs.open (report_name.c_str(), std::ofstream::out | std::ofstream::app);
      if (log_ofs.fail()) {
	cerr << "Can't log profile data to the \""<< report_name<<"\" errno: "
	     << errno << endl;
	return false; // 
      }
      //................................
      std::map< std::string,CStopWatch >::iterator item_itr;
      log_ofs <<"PID, "<< CCurrentProcess::GetPid() <<", ";
      // add user data if any
      std::list< std::pair< std::string, std::string >>::iterator user_it;
      for( user_it=m_user_data.begin(); user_it!=m_user_data.end(); user_it++){
	string key_val = user_it->second;
	if( key_val.empty() ) key_val = "NOT_SET";
	log_ofs << user_it->first  << " ," << key_val << ", ";
      }

      for( item_itr=m_sw_map.begin(); item_itr!=m_sw_map.end(); item_itr++) {
	log_ofs << item_itr->first
	        << ", " 
	        << item_itr->second.AsString (fmt)
		<< ", "; 
      }
      log_ofs << "\n"; 
      //................................
      log_ofs.close();
    }
    // REPORT #2 =======================================================================
    if( !m_markers.empty() ) {
      string pid_str = NStr::NumericToString( CCurrentProcess::GetPid() );
      report_name = report_name + string(".") + pid_str;
      log_ofs.open (report_name.c_str(), std::ofstream::out);
      if (log_ofs.fail()) {
	cerr << "Can't log profile data to the \""<< report_name<<"\" errno: "
	     << errno << endl;
	return false; // 
      }
      log_ofs << "#PID: "<<pid_str << endl;
      std::list< std::pair <std::string, std::string> >::iterator it2;
      for( it2 = m_markers.begin(); it2 != m_markers.end(); it2 ++ ) {
	log_ofs << it2->first << "," << it2->second << endl;
      }
      log_ofs << "\n"; 
      log_ofs.close();
    }

    return true;
}
/*
 * string batch_num_str = NStr::NumericToString( batch_ndx );
 *
 *
 *
 */
END_NCBI_SCOPE
/* @} */

