<!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML//EN">
<html>
  <head>
    <title>car.hpp</title>
  </head>

  <body BGCOLOR="#FFFFFF">
  <pre><font color = "red">// File name: car.hpp
// Description: Define the CCar and CCarAttr classes
<font color = "green">
#ifndef CAR_HPP
#define CAR_HPP

#include &lt;coreilib/ncbistd.hpp>
#include &lt;set>
<font color = "blue">
BEGIN_NCBI_SCOPE
</font></font>
//////////////////////
//  CCar 
</font>
class CCar
{
public:
    CCar(unsigned base_price = 10000)  { m_Price = base_price; }

    bool HasFeature(const string& feature_name) const
        { return m_Features.find(feature_name) != m_Features.end(); }
    void AddFeature(const string& feature_name)
        { m_Features.insert(feature_name); }

    void   SetColor(const string& color_name)  { m_Color = color_name; }
    string GetColor(void) const                { return m_Color; }

    const set&lt;string>& GetFeatures() const { return m_Features; }
    unsigned GetPrice(void) const
        { return m_Price + 1000 * m_Features.size(); }
  
private:
    set&lt;string>  m_Features;
    string       m_Color;
    unsigned     m_Price;
};


<font color=red>
//////////////////////
//  CCarAttr -- use a dummy all-static class to store available car attributes
</font>
class CCarAttr {
public:
    CCarAttr(void);
    static const set&lt;string>& GetFeatures(void) { return sm_Features; }
    static const set&lt;string>& GetColors  (void) { return sm_Colors; }
private:
    static set&lt;string> sm_Features;
    static set&lt;string> sm_Colors;
};

<font color = blue>
END_NCBI_SCOPE
<font color = green>
#endif  /* CAR__HPP */

