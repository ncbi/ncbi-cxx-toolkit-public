<!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML//EN">
<html>
  <head>
    <title>car.cpp</title>
  </head>

  <body BGCOLOR="#FFFFFF">
  <pre><font color = "red">// File name: car.cpp
// Description: Implement the CCarAttr class
<font color = "green">
#include "car.hpp"
<font color = blue>
BEGIN_NCBI_SCOPE
</font></font>

/////////////////////////////////////////////////////////////////////////////
//  CCarAttr::
</font>
set&lt;string> CCarAttr::sm_Features;
set&lt;string> CCarAttr::sm_Colors;


CCarAttr::CCarAttr(void)
{
<font color=red>    // make sure there is only one instance of this class</font>
    if ( !sm_Features.empty() ) {
        _TROUBLE;
        return;
    }

    // initialize static data
    sm_Features.insert("Air conditioning");
    sm_Features.insert("CD Player");
    sm_Features.insert("Four door");
    sm_Features.insert("Power windows");
    sm_Features.insert("Sunroof");

    sm_Colors.insert("Black");
    sm_Colors.insert("Navy");
    sm_Colors.insert("Silver");
    sm_Colors.insert("Tan");
    sm_Colors.insert("White");
}
<font color=red>
// dummy instance of CCarAttr -- to provide initialization of
// CCarAttr::sm_Features and CCarAttr::sm_Colors </font>
static CCarAttr s_InitCarAttr;
<font color=blue>
END_NCBI_SCOPE
