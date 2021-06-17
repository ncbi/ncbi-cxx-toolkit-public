// Added by Karanjit Siyan 4/3/2004
function SymbolSearch(bookID)
{

  var f = document.forms['frmSymbolSearch'];
  var url; 
  var sel;
  for(i=0;i<6;i++) if(f.__symboloc[i].checked == true ) { sel=f.__symboloc[i].id; }

  if(sel=='pLXR') { url = "http://www.ncbi.nlm.nih.gov/IEB/ToolBox/CPP_DOC/lxr/ident?i=" + f.__symbol.value + "&d="; } else
  if(sel=='pLib') { url = "http://www.ncbi.nlm.nih.gov/IEB/ToolBox/CPP_DOC/lib_search/libsearch.cgi?public=yes&symbol=" + f.__symbol.value; } else
  if(sel=='pToolkitAll') { url = "http://www.ncbi.nlm.nih.gov/toolkitall/?term=" + f.__symbol.value; } else
  if(sel=='iLXR') { url = "http://intranet.ncbi.nlm.nih.gov/ieb/ToolBox/CPP_DOC/lxr/ident?i=" + f.__symbol.value + "&d="; } else
  if(sel=='iLib') { url = "http://www.ncbi.nlm.nih.gov/IEB/ToolBox/CPP_DOC/lib_search/libsearch.cgi?public=no&symbol=" + f.__symbol.value; } else
  if(sel=='iToolkitAll') { url = "http://test.ncbi.nlm.nih.gov/toolkitall/?term=" + f.__symbol.value; } // else
 // if(sel=='iDoxy') { url = "http://intranet.ncbi.nlm.nih.gov/ieb/ToolBox/CPP_DOC/doxyhtml/search.php?query=" + f.__symbol.value; } 

  window.location = url;
  return false;
}

 
function SymbolSearchKeyPress(bookID,e)
{
 var nav = ( navigator.appName == "Netscape" ) ? true : false;
 var msie = ( navigator.appName.indexOf("Microsoft") != -1 ) ? true : false;
 var k = 0;

 if( nav ) { k = e.which; }
 else if( msie ) { k = e.keyCode; }
 if( k==13 ) SymbolSearch(bookID);
}
