/*
 * This file is part of the FRED system.
 *
 * Copyright (c) 2010-2012, University of Pittsburgh, John Grefenstette, Shawn Brown, 
 * Roni Rosenfield, Alona Fyshe, David Galloway, Nathan Stone, Jay DePasse, 
 * Anuroop Sriram, and Donald Burke
 * All rights reserved.
 *
 * Copyright (c) 2013-2019, University of Pittsburgh, John Grefenstette, Robert Frankeny,
 * David Galloway, Mary Krauland, Michael Lann, David Sinclair, and Donald Burke
 * All rights reserved.
 *
 * FRED is distributed on the condition that users fully understand and agree to all terms of the 
 * End User License Agreement.
 *
 * FRED is intended FOR NON-COMMERCIAL, EDUCATIONAL OR RESEARCH PURPOSES ONLY.
 *
 * See the file "LICENSE" for more information.
 */

//
//
// File: Regional_Layer.h
//

#ifndef _FRED_REGIONAL_LAYER_H
#define _FRED_REGIONAL_LAYER_H

#include "Abstract_Grid.h"
#include "Global.h"

#include <vector>

class Regional_Patch;
class Person;
class Place;

class Regional_Layer : public Abstract_Grid {
 public:
  Regional_Layer(fred::geo minlon, fred::geo minlat, fred::geo maxlon, fred::geo maxlat);
  ~Regional_Layer() {}

  Regional_Patch** get_neighbors(int row, int col);
  Regional_Patch* get_patch(int row, int col);
  Regional_Patch* get_patch(fred::geo lat, fred::geo lon);
  Regional_Patch* get_patch(Place* place);
  Regional_Patch* get_patch_with_global_coords(int row, int col);
  Regional_Patch* get_patch_from_id(int id);
  Regional_Patch* select_random_patch();
  void add_workplace(Place* place);
  void add_hospital(Place* place);
  Place* get_nearby_workplace(int row, int col, double x, double y, int min_staff, int max_staff, double* min_dist);
  vector<Place*> get_nearby_hospitals(int row, int col, double x, double y, int min_found);
  void set_population_size();
  void quality_control();
  void end_membership(fred::geo lat, fred::geo lon, Person* person);
  bool is_in_region(fred::geo lat, fred::geo lon) {
    return (this->get_patch(lat,lon) != NULL);
  }

 protected:
  Regional_Patch** grid;            // Rectangular array of patches
};

#endif // _FRED_REGIONAL_LAYER_H
