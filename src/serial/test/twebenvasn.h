/***********************************************************************
*
**
*        Automatic header module from ASNTOOL
*
************************************************************************/

#ifndef _ASNTOOL_
#include <asn.h>
#endif

static char * asnfilename = "twebenvasn.h11";
static AsnType atx[93] = {
  {401, "Web-Env" ,1,0,0,0,0,0,0,0,NULL,&atx[7],&atx[1],0,&atx[3]} ,
  {0, "arguments" ,128,0,0,1,0,0,0,0,NULL,&atx[8],&atx[2],0,&atx[9]} ,
  {0, NULL,1,-1,0,0,0,0,0,0,NULL,&atx[3],NULL,0,NULL} ,
  {402, "Argument" ,1,0,0,0,0,0,0,0,NULL,&atx[7],&atx[4],0,&atx[11]} ,
  {0, "name" ,128,0,0,0,0,0,0,0,NULL,&atx[5],NULL,0,&atx[6]} ,
  {323, "VisibleString" ,0,26,0,0,0,0,0,0,NULL,NULL,NULL,0,NULL} ,
  {0, "value" ,128,1,0,0,0,0,0,0,NULL,&atx[5],NULL,0,NULL} ,
  {311, "SEQUENCE" ,0,16,0,0,0,0,0,0,NULL,NULL,NULL,0,NULL} ,
  {314, "SET OF" ,0,17,0,0,0,0,0,0,NULL,NULL,NULL,0,NULL} ,
  {0, "db-Env" ,128,1,0,1,0,0,0,0,NULL,&atx[8],&atx[10],0,&atx[31]} ,
  {0, NULL,1,-1,0,0,0,0,0,0,NULL,&atx[11],NULL,0,NULL} ,
  {403, "Db-Env" ,1,0,0,0,0,0,0,0,NULL,&atx[7],&atx[12],0,&atx[33]} ,
  {0, "name" ,128,0,0,0,0,0,0,0,NULL,&atx[5],NULL,0,&atx[13]} ,
  {0, "arguments" ,128,1,0,1,0,0,0,0,NULL,&atx[8],&atx[14],0,&atx[15]} ,
  {0, NULL,1,-1,0,0,0,0,0,0,NULL,&atx[3],NULL,0,NULL} ,
  {0, "filters" ,128,2,0,1,0,0,0,0,NULL,&atx[8],&atx[16],0,&atx[20]} ,
  {0, NULL,1,-1,0,0,0,0,0,0,NULL,&atx[17],NULL,0,NULL} ,
  {409, "Filter-Value" ,1,0,0,0,0,0,0,0,NULL,&atx[7],&atx[18],0,&atx[22]} ,
  {0, "name" ,128,0,0,0,0,0,0,0,NULL,&atx[5],NULL,0,&atx[19]} ,
  {0, "value" ,128,1,0,0,0,0,0,0,NULL,&atx[5],NULL,0,NULL} ,
  {0, "clipboard" ,128,3,0,1,0,0,0,0,NULL,&atx[8],&atx[21],0,NULL} ,
  {0, NULL,1,-1,0,0,0,0,0,0,NULL,&atx[22],NULL,0,NULL} ,
  {410, "Db-Clipboard" ,1,0,0,0,0,0,0,0,NULL,&atx[7],&atx[23],0,&atx[37]} ,
  {0, "name" ,128,0,0,0,0,0,0,0,NULL,&atx[5],NULL,0,&atx[24]} ,
  {0, "count" ,128,1,0,0,0,0,0,0,NULL,&atx[25],NULL,0,&atx[26]} ,
  {302, "INTEGER" ,0,2,0,0,0,0,0,0,NULL,NULL,NULL,0,NULL} ,
  {0, "items" ,128,2,0,0,0,0,0,0,NULL,&atx[27],NULL,0,NULL} ,
  {416, "Item-Set" ,1,0,0,0,0,0,0,0,NULL,&atx[7],&atx[28],0,&atx[40]} ,
  {0, "items" ,128,0,0,0,0,0,0,0,NULL,&atx[29],NULL,0,&atx[30]} ,
  {304, "OCTET STRING" ,0,4,0,0,0,0,0,0,NULL,NULL,NULL,0,NULL} ,
  {0, "count" ,128,1,0,0,0,0,0,0,NULL,&atx[25],NULL,0,NULL} ,
  {0, "queries" ,128,2,0,1,0,0,0,0,NULL,&atx[71],&atx[32],0,NULL} ,
  {0, NULL,1,-1,0,0,0,0,0,0,NULL,&atx[33],NULL,0,NULL} ,
  {404, "Query-History" ,1,0,0,0,0,0,0,0,NULL,&atx[7],&atx[34],0,&atx[72]} ,
  {0, "name" ,128,0,0,1,0,0,0,0,NULL,&atx[5],NULL,0,&atx[35]} ,
  {0, "seqNumber" ,128,1,0,0,0,0,0,0,NULL,&atx[25],NULL,0,&atx[36]} ,
  {0, "time" ,128,2,0,0,0,0,0,0,NULL,&atx[37],NULL,0,&atx[48]} ,
  {411, "Time" ,1,0,0,0,0,0,0,0,NULL,&atx[47],&atx[38],0,&atx[49]} ,
  {0, "unix" ,128,0,0,0,0,0,0,0,NULL,&atx[25],NULL,0,&atx[39]} ,
  {0, "full" ,128,1,0,0,0,0,0,0,NULL,&atx[40],NULL,0,NULL} ,
  {417, "Full-Time" ,1,0,0,0,0,0,0,0,NULL,&atx[7],&atx[41],0,&atx[82]} ,
  {0, "year" ,128,0,0,0,0,0,0,0,NULL,&atx[25],NULL,0,&atx[42]} ,
  {0, "month" ,128,1,0,0,0,0,0,0,NULL,&atx[25],NULL,0,&atx[43]} ,
  {0, "day" ,128,2,0,0,0,0,0,0,NULL,&atx[25],NULL,0,&atx[44]} ,
  {0, "hour" ,128,3,0,0,0,0,0,0,NULL,&atx[25],NULL,0,&atx[45]} ,
  {0, "minute" ,128,4,0,0,0,0,0,0,NULL,&atx[25],NULL,0,&atx[46]} ,
  {0, "second" ,128,5,0,0,0,0,0,0,NULL,&atx[25],NULL,0,NULL} ,
  {315, "CHOICE" ,0,-1,0,0,0,0,0,0,NULL,NULL,NULL,0,NULL} ,
  {0, "command" ,128,3,0,0,0,0,0,0,NULL,&atx[49],NULL,0,NULL} ,
  {412, "Query-Command" ,1,0,0,0,0,0,0,0,NULL,&atx[47],&atx[50],0,&atx[51]} ,
  {0, "search" ,128,0,0,0,0,0,0,0,NULL,&atx[51],NULL,0,&atx[59]} ,
  {413, "Query-Search" ,1,0,0,0,0,0,0,0,NULL,&atx[7],&atx[52],0,&atx[60]} ,
  {0, "db" ,128,0,0,0,0,0,0,0,NULL,&atx[5],NULL,0,&atx[53]} ,
  {0, "term" ,128,1,0,0,0,0,0,0,NULL,&atx[5],NULL,0,&atx[54]} ,
  {0, "field" ,128,2,0,1,0,0,0,0,NULL,&atx[5],NULL,0,&atx[55]} ,
  {0, "filters" ,128,3,0,1,0,0,0,0,NULL,&atx[8],&atx[56],0,&atx[57]} ,
  {0, NULL,1,-1,0,0,0,0,0,0,NULL,&atx[17],NULL,0,NULL} ,
  {0, "count" ,128,4,0,0,0,0,0,0,NULL,&atx[25],NULL,0,&atx[58]} ,
  {0, "flags" ,128,5,0,1,0,0,0,0,NULL,&atx[25],NULL,0,NULL} ,
  {0, "select" ,128,1,0,0,0,0,0,0,NULL,&atx[60],NULL,0,&atx[63]} ,
  {414, "Query-Select" ,1,0,0,0,0,0,0,0,NULL,&atx[7],&atx[61],0,&atx[64]} ,
  {0, "db" ,128,0,0,0,0,0,0,0,NULL,&atx[5],NULL,0,&atx[62]} ,
  {0, "items" ,128,1,0,0,0,0,0,0,NULL,&atx[27],NULL,0,NULL} ,
  {0, "related" ,128,2,0,0,0,0,0,0,NULL,&atx[64],NULL,0,NULL} ,
  {415, "Query-Related" ,1,0,0,0,0,0,0,0,NULL,&atx[7],&atx[65],0,&atx[27]} ,
  {0, "base" ,128,0,0,0,0,0,0,0,NULL,&atx[49],NULL,0,&atx[66]} ,
  {0, "relation" ,128,1,0,0,0,0,0,0,NULL,&atx[5],NULL,0,&atx[67]} ,
  {0, "db" ,128,2,0,0,0,0,0,0,NULL,&atx[5],NULL,0,&atx[68]} ,
  {0, "items" ,128,3,0,0,0,0,0,0,NULL,&atx[47],&atx[69],0,NULL} ,
  {0, "items" ,128,0,0,0,0,0,0,0,NULL,&atx[27],NULL,0,&atx[70]} ,
  {0, "itemCount" ,128,1,0,0,0,0,0,0,NULL,&atx[25],NULL,0,NULL} ,
  {312, "SEQUENCE OF" ,0,16,0,0,0,0,0,0,NULL,NULL,NULL,0,NULL} ,
  {405, "Web-Settings" ,1,0,0,0,0,0,0,0,NULL,&atx[7],&atx[73],0,&atx[77]} ,
  {0, "arguments" ,128,0,0,1,0,0,0,0,NULL,&atx[8],&atx[74],0,&atx[75]} ,
  {0, NULL,1,-1,0,0,0,0,0,0,NULL,&atx[3],NULL,0,NULL} ,
  {0, "db-Env" ,128,1,0,1,0,0,0,0,NULL,&atx[8],&atx[76],0,NULL} ,
  {0, NULL,1,-1,0,0,0,0,0,0,NULL,&atx[11],NULL,0,NULL} ,
  {406, "Web-Saved" ,1,0,0,0,0,0,0,0,NULL,&atx[7],&atx[78],0,&atx[80]} ,
  {0, "queries" ,128,0,0,1,0,0,0,0,NULL,&atx[8],&atx[79],0,&atx[87]} ,
  {0, NULL,1,-1,0,0,0,0,0,0,NULL,&atx[80],NULL,0,NULL} ,
  {407, "Named-Query" ,1,0,0,0,0,0,0,0,NULL,&atx[7],&atx[81],0,&atx[89]} ,
  {0, "name" ,128,0,0,0,0,0,0,0,NULL,&atx[82],NULL,0,&atx[85]} ,
  {418, "Name" ,1,0,0,0,0,0,0,0,NULL,&atx[7],&atx[83],0,NULL} ,
  {0, "name" ,128,0,0,0,0,0,0,0,NULL,&atx[5],NULL,0,&atx[84]} ,
  {0, "description" ,128,1,0,1,0,0,0,0,NULL,&atx[5],NULL,0,NULL} ,
  {0, "time" ,128,1,0,0,0,0,0,0,NULL,&atx[37],NULL,0,&atx[86]} ,
  {0, "command" ,128,2,0,0,0,0,0,0,NULL,&atx[49],NULL,0,NULL} ,
  {0, "item-Sets" ,128,1,0,1,0,0,0,0,NULL,&atx[8],&atx[88],0,NULL} ,
  {0, NULL,1,-1,0,0,0,0,0,0,NULL,&atx[89],NULL,0,NULL} ,
  {408, "Named-Item-Set" ,1,0,0,0,0,0,0,0,NULL,&atx[7],&atx[90],0,&atx[17]} ,
  {0, "name" ,128,0,0,0,0,0,0,0,NULL,&atx[82],NULL,0,&atx[91]} ,
  {0, "db" ,128,1,0,0,0,0,0,0,NULL,&atx[5],NULL,0,&atx[92]} ,
  {0, "item-Set" ,128,2,0,0,0,0,0,0,NULL,&atx[27],NULL,0,NULL} };

static AsnModule ampx[1] = {
  { "NCBI-Env" , "twebenvasn.h11",&atx[0],NULL,NULL,0,0} };

static AsnValxNodePtr avn = NULL;
static AsnTypePtr at = atx;
static AsnModulePtr amp = ampx;



/**************************************************
*
*    Defines for Module NCBI-Env
*
**************************************************/

#define WEB_ENV &at[0]
#define WEB_ENV_arguments &at[1]
#define WEB_ENV_arguments_E &at[2]
#define WEB_ENV_db_Env &at[9]
#define WEB_ENV_db_Env_E &at[10]
#define WEB_ENV_queries &at[31]
#define WEB_ENV_queries_E &at[32]

#define ARGUMENT &at[3]
#define ARGUMENT_name &at[4]
#define ARGUMENT_value &at[6]

#define DB_ENV &at[11]
#define DB_ENV_name &at[12]
#define DB_ENV_arguments &at[13]
#define DB_ENV_arguments_E &at[14]
#define DB_ENV_filters &at[15]
#define DB_ENV_filters_E &at[16]
#define DB_ENV_clipboard &at[20]
#define DB_ENV_clipboard_E &at[21]

#define QUERY_HISTORY &at[33]
#define QUERY_HISTORY_name &at[34]
#define QUERY_HISTORY_seqNumber &at[35]
#define QUERY_HISTORY_time &at[36]
#define QUERY_HISTORY_command &at[48]

#define WEB_SETTINGS &at[72]
#define WEB_SETTINGS_arguments &at[73]
#define WEB_SETTINGS_arguments_E &at[74]
#define WEB_SETTINGS_db_Env &at[75]
#define WEB_SETTINGS_db_Env_E &at[76]

#define WEB_SAVED &at[77]
#define WEB_SAVED_queries &at[78]
#define WEB_SAVED_queries_E &at[79]
#define WEB_SAVED_item_Sets &at[87]
#define WEB_SAVED_item_Sets_E &at[88]

#define NAMED_QUERY &at[80]
#define NAMED_QUERY_name &at[81]
#define NAMED_QUERY_time &at[85]
#define NAMED_QUERY_command &at[86]

#define NAMED_ITEM_SET &at[89]
#define NAMED_ITEM_SET_name &at[90]
#define NAMED_ITEM_SET_db &at[91]
#define NAMED_ITEM_SET_item_Set &at[92]

#define FILTER_VALUE &at[17]
#define FILTER_VALUE_name &at[18]
#define FILTER_VALUE_value &at[19]

#define DB_CLIPBOARD &at[22]
#define DB_CLIPBOARD_name &at[23]
#define DB_CLIPBOARD_count &at[24]
#define DB_CLIPBOARD_items &at[26]

#define TIME &at[37]
#define TIME_unix &at[38]
#define TIME_full &at[39]

#define QUERY_COMMAND &at[49]
#define QUERY_COMMAND_search &at[50]
#define QUERY_COMMAND_select &at[59]
#define QUERY_COMMAND_related &at[63]

#define QUERY_SEARCH &at[51]
#define QUERY_SEARCH_db &at[52]
#define QUERY_SEARCH_term &at[53]
#define QUERY_SEARCH_field &at[54]
#define QUERY_SEARCH_filters &at[55]
#define QUERY_SEARCH_filters_E &at[56]
#define QUERY_SEARCH_count &at[57]
#define QUERY_SEARCH_flags &at[58]

#define QUERY_SELECT &at[60]
#define QUERY_SELECT_db &at[61]
#define QUERY_SELECT_items &at[62]

#define QUERY_RELATED &at[64]
#define QUERY_RELATED_base &at[65]
#define QUERY_RELATED_relation &at[66]
#define QUERY_RELATED_db &at[67]
#define QUERY_RELATED_items &at[68]
#define QUERY_RELATED_items_items &at[69]
#define QUERY_RELATED_items_itemCount &at[70]

#define ITEM_SET &at[27]
#define ITEM_SET_items &at[28]
#define ITEM_SET_count &at[30]

#define FULL_TIME &at[40]
#define FULL_TIME_year &at[41]
#define FULL_TIME_month &at[42]
#define FULL_TIME_day &at[43]
#define FULL_TIME_hour &at[44]
#define FULL_TIME_minute &at[45]
#define FULL_TIME_second &at[46]

#define NAME &at[82]
#define NAME_name &at[83]
#define NAME_description &at[84]
