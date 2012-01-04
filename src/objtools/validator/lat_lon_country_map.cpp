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
* Author:  Colleen Bollin and Michael Kornbluh
*
* File Description:
*   Internal code for checking that stated coordinates and place names match.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include "validatorp.hpp"

#include <math.h>

#include <util/line_reader.hpp>
#include <util/util_misc.hpp>

#define NCBI_USE_ERRCODE_X   Objtools_Validator

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)


// CCountryLine
CCountryLine::CCountryLine 
(const string & country_name, double y, double min_x, double max_x, double scale)
: m_CountryName(country_name) ,
  m_Scale (scale)
{
    m_Y = x_ConvertLat(y);
    m_MinX = x_ConvertLon(min_x);
    m_MaxX = x_ConvertLon(max_x);

}


CCountryLine::~CCountryLine (void)
{
}


#define EPSILON 0.001

int CCountryLine::ConvertLat (double y, double scale) 
{

    int  val = 0;

    if (y < -90.0) {
        y = -90.0;
    }
    if (y > 90.0) {
        y = 90.0;
    }

    if (y > 0) {
        val = (int) (y * scale + EPSILON);
    } else {
        val = (int) (-(-y * scale + EPSILON));
    }

    return val;
}


int CCountryLine::x_ConvertLat (double y) 
{
    return ConvertLat(y, m_Scale);
}

int CCountryLine::ConvertLon (double x, double scale) 
{

  int  val = 0;

  if (x < -180.0) {
    x = -180.0;
  }
  if (x > 180.0) {
    x = 180.0;
  }

  if (x > 0) {
    val = (int) (x * scale + EPSILON);
  } else {
    val = (int) (-(-x * scale + EPSILON));
  }

  return val;
}


int CCountryLine::x_ConvertLon (double x) 
{
    return ConvertLon(x, m_Scale);
}


CCountryExtreme::CCountryExtreme (const string & country_name, int min_x, int min_y, int max_x, int max_y)
: m_CountryName(country_name) , m_MinX (min_x), m_MinY (min_y), m_MaxX(max_x), m_MaxY (max_y)
{
    m_Area = (1 + m_MaxY - m_MinY) * (1 + m_MaxX - m_MinX);
    size_t pos = NStr::Find(country_name, ":");
    if (pos == string::npos) {
        m_Level0 = country_name;
        m_Level1 = "";
    } else {
        m_Level0 = country_name.substr(0, pos);
        NStr::TruncateSpacesInPlace(m_Level0);
        m_Level1 = country_name.substr(pos + 1);
        NStr::TruncateSpacesInPlace(m_Level1);
    }

}


CCountryExtreme::~CCountryExtreme (void)
{

}


bool CCountryExtreme::SetMinX(int min_x) 
{ 
    if (min_x < m_MinX) { 
        m_MinX = min_x; 
        return true;
    } else { 
        return false; 
    } 
}


bool CCountryExtreme::SetMaxX(int max_x) 
{ 
    if (max_x > m_MaxX) { 
        m_MaxX = max_x; 
        return true;
    } else { 
        return false; 
    } 
}


bool CCountryExtreme::SetMinY(int min_y) 
{ 
    if (min_y < m_MinY) { 
        m_MinY = min_y; 
        return true;
    } else { 
        return false; 
    } 
}


bool CCountryExtreme::SetMaxY(int max_y) 
{ 
    if (max_y > m_MaxY) { 
        m_MaxY = max_y; 
        return true;
    } else { 
        return false; 
    } 
}


void CCountryExtreme::AddLine(const CCountryLine *line)
{
    if (line) {
        SetMinX(line->GetMinX());
        SetMaxX(line->GetMaxX());
        SetMinY(line->GetY());
        SetMaxY(line->GetY());
        m_Area += 1 + line->GetMaxX() - line->GetMinX();
    }
}


bool CCountryExtreme::DoesOverlap(const CCountryExtreme* other_block) const
{
    if (!other_block) {
        return false;
    } else if (m_MaxX >= other_block->GetMinX()
        && m_MaxX <= other_block->GetMaxX()
        && m_MaxY >= other_block->GetMinY()
        && m_MinY <= other_block->GetMaxY()) {
        return true;
    } else if (other_block->GetMaxX() >= m_MinX
        && other_block->GetMaxX() <= m_MaxX
        && other_block->GetMaxY() >= m_MinY
        && other_block->GetMinY() <= m_MaxY) {
        return true;
    } else {
        return false;
    }
}


bool CCountryExtreme::PreferTo(const CCountryExtreme* other_block, const string country, const string province, const bool prefer_new) const
{
    if (!other_block) {
        return true;
    }

    // if no preferred country, these are equal
    if (NStr::IsBlank(country)) {
        return prefer_new;
    }
    
    // if match to preferred country 
    if (NStr::EqualNocase(country, m_Level0)) {
        // if best was not preferred country, take new match
        if (!NStr::EqualNocase(country, other_block->GetLevel0())) {
            return true;
        }
        // if match to preferred province
        if (!NStr::IsBlank(province) && NStr::EqualNocase(province, m_Level1)) {
            // if best was not preferred province, take new match
            if (!NStr::EqualNocase(province, other_block->GetLevel1())) {
                return true;
            }
        }
            
        // if both match province, or neither does, or no preferred province, take smallest
        return prefer_new;
    }

    // if best matches preferred country, keep
    if (NStr::EqualNocase(country, other_block->GetLevel0())) {
        return false;
    }

    // otherwise take smallest
    return prefer_new;
}


CLatLonCountryId::CLatLonCountryId(float lat, float lon)
{
    m_Lat = lat;
    m_Lon = lon;
    m_FullGuess = "";
    m_GuessCountry = "";
    m_GuessProvince = "";
    m_GuessWater = "";
    m_ClosestFull = "";
    m_ClosestCountry = "";
    m_ClosestProvince = "";
    m_ClosestWater = "";
    m_ClaimedFull = "";
    m_LandDistance = -1;
    m_WaterDistance = -1;
    m_ClaimedDistance = -1;
}

CLatLonCountryId::~CLatLonCountryId(void)
{
}


#include "lat_lon_country.inc"

static const int k_NumLatLonCountryText = sizeof (s_DefaultLatLonCountryText) / sizeof (char *);

#include "lat_lon_water.inc"

static const int k_NumLatLonWaterText = sizeof (s_DefaultLatLonWaterText) / sizeof (char *);

void CLatLonCountryMap::x_InitFromDefaultList(const char * const *list, int num)
{
      // initialize list of country lines
    m_CountryLineList.clear();
    m_Scale = 20.0;
    string current_country = "";

    for (int i = 0; i < num; i++) {
            const string& line = list[i];

        if (line.c_str()[0] == '-') {
            // skip comment
        } else if (isalpha (line.c_str()[0])) {
            current_country = line;
        } else if (isdigit (line.c_str()[0])) {
            m_Scale = NStr::StringToDouble(line);
        } else {          
            vector<string> tokens;
              NStr::Tokenize(line, "\t", tokens);
            if (tokens.size() > 3) {
                double x = NStr::StringToDouble(tokens[1]);
                for (size_t j = 2; j < tokens.size() - 1; j+=2) {
                    m_CountryLineList.push_back(new CCountryLine(current_country, x, NStr::StringToDouble(tokens[j]), NStr::StringToDouble(tokens[j + 1]), m_Scale));
                }
            }
        }
    }
}




bool CLatLonCountryMap::x_InitFromFile(const string& filename)
{
    string fname = g_FindDataFile (filename);
    if (NStr::IsBlank (fname)) {
        return false;
    }

    CRef<ILineReader> lr = ILineReader::New (fname);
    if (lr.Empty()) {
        return false;
    } else {
        m_Scale = 20.0;
        string current_country = "";

        // make sure to clear before using.  in this outer
        // scope in the interest of speed (avoid repeated 
        // construction/destruction)
        vector<SIZE_TYPE> tab_positions;

        do {
            // const string& line = *++*lr;
            CTempString line = *++*lr;
            if (line[0] == '-') {
                // skip comment
            } else if (isalpha (line[0])) {
                current_country = line;
            } else if (isdigit (line[0])) {
                m_Scale = NStr::StringToDouble(line);
            } else {          
                // NStr::Tokenize would be much simpler, but
                // it's just too slow in this case, especially
                // in debug mode.

                // for the future, if we need even more speed,
                // it should be possible to eliminate the tab_positions
                // vector and collect tab positions on the fly without
                // any heap-allocated memory

                // find position of all tabs on this line
                tab_positions.clear();
                SIZE_TYPE tab_pos = line.find('\t');
                while( tab_pos != NPOS ) {
                    tab_positions.push_back(tab_pos);
                    tab_pos = line.find('\t', tab_pos+1);
                }
                // an imaginary sentinel tab
                tab_positions.push_back(line.length());

                const char * line_start = line.data();
                if( tab_positions.size() >= 4 ) {
                    CTempString y_str( line_start + tab_positions[0]+1, tab_positions[1] - tab_positions[0] - 1 );
                    double y = NStr::StringToDouble( y_str );

                    // convert into line list
                    for (size_t j = 1; j < tab_positions.size() - 2; j+=2) {
                        const SIZE_TYPE pos1 = tab_positions[j];
                        const SIZE_TYPE pos2 = tab_positions[j+1];
                        const SIZE_TYPE pos3 = tab_positions[j+2];
                        CTempString first_num( line_start + pos1 + 1, pos2 - pos1 - 1 );
                        CTempString second_num( line_start + pos2 + 1, pos3 - pos2 - 1 );
                        m_CountryLineList.push_back(new CCountryLine(current_country, y, NStr::StringToDouble(first_num), NStr::StringToDouble(second_num), m_Scale));
                    }
                }
            }
        } while ( !lr->AtEOF() );

        return true;
    }
}


bool CLatLonCountryMap::
        s_CompareTwoLinesByCountry(const CCountryLine* line1,
                                    const CCountryLine* line2)
{
    int cmp = NStr::CompareNocase(line1->GetCountry(), line2->GetCountry());
    if (cmp == 0) {        
        if (line1->GetY() < line2->GetY()) {
            return true;
        } else if (line1->GetY() > line2->GetY()) {
            return false;
        } else {
            if (line1->GetMinX() < line2->GetMinX()) {
                return true;
            } else {
                return false;
            }
        }
    } else if (cmp < 0) {
        return true;
    } else {
        return false;
    }
}


bool CLatLonCountryMap::
        s_CompareTwoLinesByLatLonThenCountry(const CCountryLine* line1,
                                    const CCountryLine* line2)
{
    if (line1->GetY() < line2->GetY()) {
        return true;
    } else if (line1->GetY() > line2->GetY()) {
        return false;
    } if (line1->GetMinX() < line2->GetMinX()) {
        return true;
    } else if (line1->GetMinX() > line2->GetMinX()) {
        return false;
    } else if (line1->GetMaxX() < line2->GetMaxX()) {
        return true;
    } else if (line1->GetMaxX() > line2->GetMaxX()) {
        return false;
    } else {
        int cmp = NStr::CompareNocase(line1->GetCountry(), line2->GetCountry());
        if (cmp < 0) {
            return true;
        } else {
            return false;
        }
    }
}


CLatLonCountryMap::CLatLonCountryMap (bool is_water) 
{
    // initialize list of country lines
    m_CountryLineList.clear();

    if (is_water) {
        if (!x_InitFromFile("lat_lon_water.txt")) {
            x_InitFromDefaultList(s_DefaultLatLonWaterText, k_NumLatLonWaterText);
        }
    } else {
        if (!x_InitFromFile("lat_lon_country.txt")) {
            x_InitFromDefaultList(s_DefaultLatLonCountryText, k_NumLatLonCountryText);
        }
    }
    sort (m_CountryLineList.begin(), m_CountryLineList.end(), s_CompareTwoLinesByCountry);

    // set up extremes index and copy into LatLon index
    m_CountryExtremes.clear();
    m_LatLonSortedList.clear();
      size_t i, ext = 0;

    for (i = 0; i < m_CountryLineList.size(); i++) {
        if (ext > 0 && NStr::Equal(m_CountryLineList[i]->GetCountry(), m_CountryExtremes[ext - 1]->GetCountry())) {
            m_CountryExtremes[ext - 1]->AddLine(m_CountryLineList[i]);
        } else {
            m_CountryExtremes.push_back(new CCountryExtreme(m_CountryLineList[i]->GetCountry(),
                                                m_CountryLineList[i]->GetMinX(), 
                                                m_CountryLineList[i]->GetY(),
                                                m_CountryLineList[i]->GetMaxX(),
                                                m_CountryLineList[i]->GetY()));
            ext++;
        }
        m_LatLonSortedList.push_back(m_CountryLineList[i]);
        m_CountryLineList[i]->SetBlock(m_CountryExtremes[ext - 1]);
    }
    sort (m_LatLonSortedList.begin(), m_LatLonSortedList.end(), s_CompareTwoLinesByLatLonThenCountry);

}


CLatLonCountryMap::~CLatLonCountryMap (void)
{
      size_t i;

    for (i = 0; i < m_CountryLineList.size(); i++) {
        delete (m_CountryLineList[i]);
    }
    m_CountryLineList.clear();

    for (i = 0; i < m_CountryExtremes.size(); i++) {
        delete (m_CountryExtremes[i]);
    }
    m_CountryExtremes.clear();
    // note - do not delete items in m_LatLonSortedList, they are pointing to the same objects as m_CountryLineList
    m_LatLonSortedList.clear();
}


bool CLatLonCountryMap::IsCountryInLatLon(const string& country, double lat,
                                          double lon)
{
    int x = CCountryLine::ConvertLon(lon, m_Scale);
    int y = CCountryLine::ConvertLat(lat, m_Scale);

    size_t L, R, mid;

    L = 0;
    R = m_CountryLineList.size() - 1;
    mid = 0;

    while (L < R) {
        mid = (L + R) / 2;
        int cmp = NStr::Compare(m_CountryLineList[mid]->GetCountry(), country);
        if (cmp < 0) {
            L = mid + 1;
        } else if (cmp > 0) {
            R = mid;
        } else {
            while (mid > 0 
                   && NStr::Compare(m_CountryLineList[mid - 1]->GetCountry(), country) == 0
                   && m_CountryLineList[mid - 1]->GetY() >= y) {
                mid--;
            }
            L = mid;
            R = mid;
        }
    }

    while (R < m_CountryLineList.size() 
           && NStr::EqualNocase(country, m_CountryLineList[R]->GetCountry())
           && m_CountryLineList[R]->GetY() < y) {
        R++;
    }

    while (R < m_CountryLineList.size() 
           && NStr::EqualNocase(country, m_CountryLineList[R]->GetCountry())
           && m_CountryLineList[R]->GetY() == y
           && m_CountryLineList[R]->GetMaxX() < x) {
        R++;
    }
    if (R < m_CountryLineList.size() 
           && NStr::EqualNocase(country, m_CountryLineList[R]->GetCountry())
           && m_CountryLineList[R]->GetY() == y
           && m_CountryLineList[R]->GetMinX() <= x 
           && m_CountryLineList[R]->GetMaxX() >= x) {
        return true;
    } else {
        return false;
    }    
}


const CCountryExtreme *
CLatLonCountryMap::x_FindCountryExtreme(const string& country)
{
    int L, R, mid;

    if (NStr::IsBlank (country)) return NULL;

    L = 0;
    R = m_CountryExtremes.size() - 1;
    mid = 0;

    while (L < R) {
        mid = (L + R) / 2;
        if (NStr::CompareNocase(m_CountryExtremes[mid]->GetCountry(), country) < 0) {
            L = mid + 1;
        } else {
            R = mid;
        }
    }
    if (!NStr::EqualNocase(m_CountryExtremes[R]->GetCountry(), country)) {
        return NULL;
    } else {
        return m_CountryExtremes[R];
    }
}


bool CLatLonCountryMap::HaveLatLonForRegion(const string& region)
{
    if (x_FindCountryExtreme(region) == NULL) {
        return false;
    } else {
        return true;
    }
}


int CLatLonCountryMap::x_GetLatStartIndex (int y)
{
    int L, R, mid;

    L = 0;
    R = m_LatLonSortedList.size() - 1;
    mid = 0;

    while (L < R) {
        mid = (L + R) / 2;
        if (m_LatLonSortedList[mid]->GetY() < y) {
            L = mid + 1;
        } else if (m_LatLonSortedList[mid]->GetY() > y) {
            R = mid;
        } else {
            while (mid > 0 && m_LatLonSortedList[mid - 1]->GetY() == y) {
                mid--;
            }
            L = mid;
            R = mid;
        }
    }
    return R;
}


const CCountryExtreme *
CLatLonCountryMap::GuessRegionForLatLon(double lat, double lon,
                                        const string& country,
                                        const string& province)
{
    int x = CCountryLine::ConvertLon(lon, m_Scale);
    int y = CCountryLine::ConvertLon(lat, m_Scale);

    size_t R = x_GetLatStartIndex(y);

    const CCountryExtreme *best = NULL;

    while (R < m_LatLonSortedList.size() && m_LatLonSortedList[R]->GetY() == y) {
            if (m_LatLonSortedList[R]->GetMinX() <= x 
            && m_LatLonSortedList[R]->GetMaxX() >= x) {
            const CCountryExtreme *other = m_LatLonSortedList[R]->GetBlock();
            if (best == NULL) {
                best = other;
            } else if (!best->PreferTo(other, country, province, (bool)(best->GetArea() <= other->GetArea()))) {
                best = other;
            }
             }
        R++;
      }
      return best;
}


//Distance on a spherical surface calculation adapted from
//http://www.linuxjournal.com/magazine/
//work-shell-calculating-distance-between-two-latitudelongitude-points

#define EARTH_RADIUS 6371.0 /* average radius of non-spherical earth in kilometers */
#define CONST_PI 3.14159265359

static double DegreesToRadians (
  double degrees
)

{
  return (degrees * (CONST_PI / 180.0));
}

static double DistanceOnGlobe (
  double latA,
  double lonA,
  double latB,
  double lonB
)

{
  double lat1, lon1, lat2, lon2;
  double dLat, dLon, a, c;

  lat1 = DegreesToRadians (latA);
  lon1 = DegreesToRadians (lonA);
  lat2 = DegreesToRadians (latB);
  lon2 = DegreesToRadians (lonB);

  dLat = lat2 - lat1;
  dLon = lon2 - lon1;

   a = sin (dLat / 2) * sin (dLat / 2) +
       cos (lat1) * cos (lat2) * sin (dLon / 2) * sin (dLon / 2);
   c = 2 * atan2 (sqrt (a), sqrt (1 - a));

  return (double) (EARTH_RADIUS * c);
}


double ErrorDistance (
  double latA,
  double lonA,
  double scale)
{
  double lat1, lon1, lat2, lon2;
  double dLat, dLon, a, c;

  lat1 = DegreesToRadians (latA);
  lon1 = DegreesToRadians (lonA);
  lat2 = DegreesToRadians (latA + (1.0 / scale));
  lon2 = DegreesToRadians (lonA + (1.0 / scale));

  dLat = lat2 - lat1;
  dLon = lon2 - lon1;

   a = sin (dLat / 2) * sin (dLat / 2) +
       cos (lat1) * cos (lat2) * sin (dLon / 2) * sin (dLon / 2);
   c = 2 * atan2 (sqrt (a), sqrt (1 - a));

  return (double) (EARTH_RADIUS * c);
  
}


const CCountryExtreme * CLatLonCountryMap::FindClosestToLatLon(double lat,
                                                               double lon,
                                                               double range,
                                                               double &distance)
{
    int x = CCountryLine::ConvertLon(lon, m_Scale);
    int y = CCountryLine::ConvertLon(lat, m_Scale);

    int maxDelta = (int) (range * m_Scale + EPSILON);
    int min_y = y - maxDelta;
    int max_y = y + maxDelta;
    int min_x = x - maxDelta;
    int max_x = x + maxDelta;

    // binary search to lowest lat
    size_t R = x_GetLatStartIndex(min_y);

    double closest = 0.0;
    CCountryExtreme *rval = NULL;

    while (R < m_LatLonSortedList.size() && m_LatLonSortedList[R]->GetY() <= max_y) {
        if (m_LatLonSortedList[R]->GetMaxX() < min_x || m_LatLonSortedList[R]->GetMinX() > max_x) {
            // out of range, don't bother calculating distance
        } else {
            double end;
            if (x < m_LatLonSortedList[R]->GetMinX()) {
                end = m_LatLonSortedList[R]->GetMinLon();
            } else if (x > m_LatLonSortedList[R]->GetMaxX()) {
                end = m_LatLonSortedList[R]->GetMaxLon();
            } else {
                end = lon;
            }
            double dist = DistanceOnGlobe (lat, lon, m_LatLonSortedList[R]->GetLat(), end);
            if (rval == NULL || closest > dist 
                || (closest == dist 
                    && (rval->GetArea() > m_LatLonSortedList[R]->GetBlock()->GetArea()
                        || (rval->GetArea() == m_LatLonSortedList[R]->GetBlock()->GetArea()
                            && NStr::IsBlank(rval->GetLevel1())
                            && !NStr::IsBlank(m_LatLonSortedList[R]->GetBlock()->GetLevel1()))))) {
                rval = m_LatLonSortedList[R]->GetBlock();
                closest = dist;
            }
        }
        R++;
    }
    distance = closest;
    return rval;
}


bool CLatLonCountryMap::IsClosestToLatLon(const string& comp_country,
                                          double lat, double lon,
                                          double range, double &distance)
{
    int x = CCountryLine::ConvertLon(lon, m_Scale);
    int y = CCountryLine::ConvertLon(lat, m_Scale);

    int maxDelta = (int) (range * m_Scale + EPSILON);
    int min_y = y - maxDelta;
    int max_y = y + maxDelta;
    int min_x = x - maxDelta;
    int max_x = x + maxDelta;

    // binary search to lowest lat
    size_t R = x_GetLatStartIndex(min_y);

    string country = "";
    double closest = 0.0;
    int smallest_area = -1;

    while (R < m_LatLonSortedList.size() && m_LatLonSortedList[R]->GetY() <= max_y) {
        if (m_LatLonSortedList[R]->GetMaxX() < min_x || m_LatLonSortedList[R]->GetMinX() > max_x) {
            // out of range, don't bother calculating distance
        } else {
            double end;
            if (x < m_LatLonSortedList[R]->GetMinX()) {
                end = m_LatLonSortedList[R]->GetMinLon();
            } else {
                end = m_LatLonSortedList[R]->GetMaxLon();
            }
            double dist = DistanceOnGlobe (lat, lon, m_LatLonSortedList[R]->GetLat(), end);
            if (NStr::IsBlank (country) || closest > dist) {
                country = m_LatLonSortedList[R]->GetCountry();
                closest = dist;
                const CCountryExtreme * ext = x_FindCountryExtreme(country);
                if (ext) {
                    smallest_area = ext->GetArea();
                }
            } else if (closest == dist) {
                // if the distances are the same, prefer the input country, otherwise prefer the smaller region
                if (NStr::Equal(country, comp_country)) {
                    // keep country we're searching for
                } else if (!NStr::Equal(m_LatLonSortedList[R]->GetCountry(), country)) {
                    const CCountryExtreme * ext = x_FindCountryExtreme(m_LatLonSortedList[R]->GetCountry());
                    if (ext 
                        && (ext->GetArea() < smallest_area 
                            || NStr::Equal(m_LatLonSortedList[R]->GetCountry(), comp_country))) {
                        country = m_LatLonSortedList[R]->GetCountry();
                        smallest_area = ext->GetArea();
                    }
                }
            }
        }
        R++;
    }
    distance = closest;
    return NStr::Equal(country, comp_country);
}


const CCountryExtreme * CLatLonCountryMap::IsNearLatLon(double lat, double lon,
                                                        double range,
                                                        double &distance,
                                                        const string& country,
                                                        const string& province)
{
    int x = CCountryLine::ConvertLon(lon, m_Scale);
    int y = CCountryLine::ConvertLat(lat, m_Scale);
    double closest = -1.0;
    int maxDelta = (int) (range * m_Scale + EPSILON);
    int min_y = y - maxDelta;
    int max_y = y + maxDelta;
    int min_x = x - maxDelta;
    int max_x = x + maxDelta;
    CCountryExtreme *ext = NULL;

    // binary search to lowest lat
    size_t R = x_GetLatStartIndex(min_y);

    while (R < m_LatLonSortedList.size() && m_LatLonSortedList[R]->GetY() <= max_y) {
        if (m_LatLonSortedList[R]->GetMaxX() < min_x || m_LatLonSortedList[R]->GetMinX() > max_x) {
            // out of range, don't bother calculating distance
        } else if (!NStr::EqualNocase(m_LatLonSortedList[R]->GetBlock()->GetLevel0(), country)) {
            // wrong country, skip
        } else if (!NStr::IsBlank(province) && !NStr::EqualNocase(m_LatLonSortedList[R]->GetBlock()->GetLevel1(), province)) {
            // wrong province, skip
        } else {
            double end;
            if (x < m_LatLonSortedList[R]->GetMinX()) {
                end = m_LatLonSortedList[R]->GetMinLon();
            } else if (x > m_LatLonSortedList[R]->GetMaxX()) {
                end = m_LatLonSortedList[R]->GetMaxLon();
            } else {
                end = lon;
            }
            double dist = DistanceOnGlobe (lat, lon, m_LatLonSortedList[R]->GetLat(), end);
            if (closest < 0.0 ||  closest > dist) { 
                closest = dist;
                ext = m_LatLonSortedList[R]->GetBlock();
            }
        }
        R++;
    }
    distance = closest;
    return ext;
}





bool CLatLonCountryMap::DoCountryBoxesOverlap(const string& country1,
                                              const string& country2)
{
    if (NStr::IsBlank (country1) || NStr::IsBlank(country2)) return false;

    const CCountryExtreme *ext1 = x_FindCountryExtreme (country1);
    if (!ext1) {
        return false;
    }
    const CCountryExtreme *ext2 = x_FindCountryExtreme (country2);
    if (!ext2) {
        return false;
    }


    return ext1->DoesOverlap(ext2);
}


int CLatLonCountryMap::AdjustAndRoundDistance (double distance, double scale)

{
  if (scale < 1.1) {
    distance += 111.19;
  } else if (scale > 19.5 && scale < 20.5) {
    distance += 5.56;
  } else if (scale > 99.5 && scale < 100.5) {
    distance += 1.11;
  }

  return (int) (distance + 0.5);
}


int CLatLonCountryMap::AdjustAndRoundDistance (double distance)

{
  return AdjustAndRoundDistance (distance, m_Scale);
}

END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
