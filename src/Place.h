/*
  This file is part of the FRED system.

  Copyright (c) 2010-2015, University of Pittsburgh, John Grefenstette,
  Shawn Brown, Roni Rosenfield, Alona Fyshe, David Galloway, Nathan
  Stone, Jay DePasse, Anuroop Sriram, and Donald Burke.

  Licensed under the BSD 3-Clause license.  See the file "LICENSE" for
  more information.
*/

//
//
// File: Place.h
//

#ifndef _FRED_PLACE_H
#define _FRED_PLACE_H

#include <vector>
using namespace std;

#include "Global.h"
#include "Geo.h"

class Neighborhood_Patch;
class Person;

typedef std::vector<Person*> person_vec_t;

class Place {

public:
  
  // place type codes
  static char HOUSEHOLD;
  static char NEIGHBORHOOD;
  static char SCHOOL;
  static char CLASSROOM;
  static char WORKPLACE;
  static char OFFICE;
  static char HOSPITAL;
  static char COMMUNITY;

  virtual ~Place() {}

  /**
   *  Sets the id, label, logitude, latitude of this Place
   *  Allocates disease-related memory for this place
   *
   *  @param lab this Place's label
   *  @param lon this Place's longitude
   *  @param lat this Place's latitude
   */
  void setup(const char* lab, fred::geo lon, fred::geo lat);

  virtual void print(int disease_id);

  void reset_place_state(int disease_id) {
    this->infectious_bitset.reset(disease_id);
  }

  virtual void prepare();

  // enroll/unenroll:
  virtual int enroll(Person* per);
  virtual void unenroll(int pos);

  // daily update
  virtual void update(int day);

  void reset_visualization_data(int day);

  void record_infectious_days(int day);

  // infectious people
  void clear_infectious_people(int disease_id) {
    this->infectious_people[disease_id].clear();
  }

  void add_infectious_person(int disease_id, Person * person);

  int get_number_of_infectious_people(int disease_id) {
    return this->infectious_people[disease_id].size();
  }

  bool has_infectious_people(int disease_id) {
    return this->infectious_people[disease_id].size() > 0;
  }

  bool is_infectious(int disease_id) {
    return infectious_people[disease_id].size() > 0;
  }
  
  bool is_infectious() {
    return this->infectious_bitset.any();
  }
  
  bool is_human_infectious(int disease_id) {
    return this->human_infectious_bitset.test(disease_id);
  }  

  void set_human_infectious (int disease_id) {
    if(!(this->human_infectious_bitset.test(disease_id))) {
      this->human_infectious_bitset.set(disease_id);
    }
  }
    
  void reset_human_infectious() {
    this->human_infectious_bitset.reset();
  }
    
  void set_exposed(int disease_id) {
    this->exposed_bitset.set(disease_id);
  }
  
  void reset_exposed() {
    this->exposed_bitset.reset();
  }

  bool is_exposed(int disease_id) {
    return this->exposed_bitset.test(disease_id);
  }

  void set_recovered(int disease_id) {
    this->recovered_bitset.set(disease_id);
  }
  
  void reset_recovered() {
    this->recovered_bitset.reset();
  }
  
  bool is_recovered(int disease_id) {
    return this->recovered_bitset.test(disease_id);
  }


  void print_infectious(int disease_id);

  // disease transmission

  std::vector<Person*> get_potential_infectors(int disease_id) {
    return this->infectious_people[disease_id];
  }

  std::vector<Person*> get_potential_infectees(int disease_id) {
    return this->enrollees;
  }

  // access methods:
  int get_adults();
  int get_children();
  virtual bool is_open(int day) {
    return true;
  }

  /**
   * Get the age group for a person given a particular disease_id.
   *
   * @param disease_id an integer representation of the disease
   * @param per a pointer to a Person object
   * @return the age group for the given person for the given diease
   */
  virtual int get_group(int disease_id, Person * per) = 0;

  /**
   * Get the transmission probability for a given diease between two Person objects.
   *
   * @param disease_id an integer representation of the disease
   * @param i a pointer to a Person object
   * @param s a pointer to a Person object
   * @return the probability that there will be a transmission of disease_id from i to s
   */
  virtual double get_transmission_prob(int disease_id, Person * i, Person * s) = 0;

  virtual double get_contacts_per_day(int disease_id) = 0; // access functions

  double get_contact_rate(int day, int disease_id);

  int get_contact_count(Person* infector, int disease_id, int day, double contact_rate);

  /**
   * Determine if the place should be open. It is dependent on the disease_id and simulation day.
   *
   * @param day the simulation day
   * @param disease_id an integer representation of the disease
   * @return <code>true</code> if the place should be open; <code>false</code> if not
   */
  virtual bool should_be_open(int day, int disease_id) = 0;

  /**
   * Get the id.
   * @return the id
   */
  int get_id() {
    return this->id;
  }

  /**
   * Get the label.
   *
   * @return the label
   */
  char* get_label() {
    return this->label;
  }

  /**
   * Get the type (H)OME, (W)ORK, (S)CHOOL, (C)OMMUNITY).
   *
   * @return the type
   */
  char get_type() {
    return this->type;
  }

  // test place types
  bool is_household() {
    return this->type == Place::HOUSEHOLD;
  }
  
  bool is_neighborhood() {
    return this->type == Place::NEIGHBORHOOD;
  }
  
  bool is_school() {
    return this->type == Place::SCHOOL;
  }
  
  bool is_classroom() {
    return this->type == Place::CLASSROOM;
  }
  
  bool is_workplace(){
    return this->type == Place::WORKPLACE;
  }
  
  bool is_office() {
    return this->type == Place::OFFICE;
  }
  
  bool is_hospital() {
    return this->type == Place::HOSPITAL;
  }
  
  bool is_community() {
    return this->type == Place::COMMUNITY;
  }

  // test place subtypes
  bool is_college(){
    return this->subtype == fred::PLACE_SUBTYPE_COLLEGE;
  }
  
  bool is_prison(){
    return this->subtype == fred::PLACE_SUBTYPE_PRISON;
  }
  
  bool is_nursing_home(){
    return this->subtype == fred::PLACE_SUBTYPE_NURSING_HOME;
  }
  
  bool is_military_base(){
    return this->subtype == fred::PLACE_SUBTYPE_MILITARY_BASE;
  }
    
  bool is_healthcare_clinic(){
    return this->subtype == fred::PLACE_SUBTYPE_HEALTHCARE_CLINIC;
  }

  bool is_mobile_healthcare_clinic(){
    return this->subtype == fred::PLACE_SUBTYPE_MOBILE_HEALTHCARE_CLINIC;
  }
    
  bool is_group_quarters() {
    return (is_college() || is_prison() || is_military_base() || is_nursing_home());
  }

  // test for household types
  bool is_college_dorm(){
    return is_household() && is_college();
  }
  
  bool is_prison_cell(){
    return  is_household() && is_prison();
  }
  
  bool is_military_barracks() {
    return is_household() && is_military_base();
  }

  /**
   * Get the latitude.
   *
   * @return the latitude
   */
  fred::geo get_latitude() {
    return this->latitude;
  }

  /**
   * Get the longitude.
   *
   * @return the longitude
   */
  fred::geo get_longitude() {
    return this->longitude;
  }

  double get_distance(Place *place) {
    double x1 = this->get_x();
    double y1 = this->get_y();
    double x2 = place->get_x();
    double y2 = place->get_y();
    double distance = sqrt((x1-x2)*(x1-x2) + (y1-y2)*(y1-y2));
    return distance;
  }

  /**
   * Get the count of agents in this place.
   *
   * @return the count of agents
   */
  int get_size() {
    return this->enrollees.size();
  }

  virtual int get_container_size() {
    return get_size();
  }

  int get_orig_size() {
    return this->N_orig;
  }

  /**
   * Get the simulation day (an integer value of days from the start of the simulation) when the place will close.
   *
   * @return the close_date
   */
  int get_close_date() {
    return this->close_date;
  }

  /**
   * Get the simulation day (an integer value of days from the start of the simulation) when the place will open.
   *
   * @return the open_date
   */
  int get_open_date() {
    return this->open_date;
  }

  /**
   * Set the type.
   *
   * @param t the new type
   */
  void set_type(char t) {
    this->type = t;
  }

  /**
   * Set the latitude.
   *
   * @param x the new latitude
   */
  void set_latitude(double x) {
    this->latitude = x;
  }

  /**
   * Set the longitude.
   *
   * @param x the new longitude
   */
  void set_longitude(double x) {
    this->longitude = x;
  }

  /**
   * Set the simulation day (an integer value of days from the start of the simulation) when the place will close.
   *
   * @param day the simulation day when the place will close
   */
  void set_close_date(int day) {
    this->close_date = day;
  }

  /**
   * Set the simulation day (an integer value of days from the start of the simulation) when the place will open.
   *
   * @param day the simulation day when the place will open
   */
  void set_open_date(int day) {
    this->open_date = day;
  }

  /**
   * Get the patch where this place is.
   *
   * @return a pointer to the patch where this place is
   */
  Neighborhood_Patch* get_patch() {
    return this->patch;
  }

  /**
   * Set the patch where this place will be.
   *
   * @param p the new patch
   */
  void set_patch(Neighborhood_Patch* p) {
    this->patch = p;
  }
  
  int get_output_count(int disease_id, int output_code);

  void turn_workers_into_teachers(Place* school);
  void reassign_workers(Place* place);

  double get_x() {
    return Geo::get_x(this->longitude);
  }

  double get_y() {
    return Geo::get_y(this->latitude);
  }

  void add_new_infection(int disease_id) {
    this->new_infections[disease_id]++;
    this->total_infections[disease_id]++;
  }
  
  void add_current_infection(int disease_id) {
    this->current_infections[disease_id]++;
  }
  
  void add_new_symptomatic_infection(int disease_id) {
    this->new_symptomatic_infections[disease_id]++;
    this->total_symptomatic_infections[disease_id]++;
  }
  
  void add_current_symptomatic_infection(int disease_id) {
    this->current_symptomatic_infections[disease_id]++;
  }
  
  int get_new_infections(int disease_id) {
    return this->new_infections[disease_id];
  }

  int get_current_infections(int disease_id) {
    return this->current_infections[disease_id];
  }

  int get_total_infections(int disease_id) {
    return this->total_infections[disease_id];
  }

  int get_new_symptomatic_infections(int disease_id) {
    return this->new_symptomatic_infections[disease_id];
  }

  int get_current_symptomatic_infections(int disease_id) {
    return this->current_symptomatic_infections[disease_id];
  }

  int get_total_symptomatic_infections(int disease_id) {
    return this->total_symptomatic_infections[disease_id];
  }

  int get_current_infectious_visitors(int disease_id) {
    return this->current_infectious_visitors[disease_id];
  }

  int get_current_symptomatic_visitors(int disease_id) {
    return this->current_symptomatic_visitors[disease_id];
  }

  /**
   * Get the number of cases of a given disease for day.
   *
   * @param disease_id an integer representation of the disease
   * @return the count of cases for a given diease
   */
  int get_current_cases(int disease_id) {
    return get_current_symptomatic_visitors(disease_id);
  }

  /**
   * Get the number of deaths from a given disease for a day.
   * The member variable deaths gets reset when <code>update()</code> is called, which for now is on a daily basis.
   *
   * @param disease_id an integer representation of the disease
   * @return the count of deaths for a given disease
   */
  int get_new_deaths(int disease_id) {
    return 0 /* new_deaths[disease_id] */;
  }

  int get_recovereds(int disease_id);

  /**
   * Get the number of cases of a given disease for the simulation thus far.
   *
   * @param disease_id an integer representation of the disease
   * @return the count of cases for a given disease
   */
  int get_total_cases(int disease_id) {
    return this->total_symptomatic_infections[disease_id];
  }

  /**
   * Get the number of deaths from a given disease for the simulation thus far.
   *
   * @param disease_id an integer representation of the disease
   * @return the count of deaths for a given disease
   */
  int get_total_deaths(int disease_id) {
    return 0 /* total_deaths[disease_id] */;
  }

  /**
   * Get the number of cases of a given disease for the simulation thus far divided by the
   * number of agents in this place.
   *
   * @param disease_id an integer representation of the disease
   * @return the count of rate of cases per people for a given disease
   */
  double get_incidence_rate(int disease_id) {
    return (double)this->total_symptomatic_infections[disease_id] / (double)get_size();
  }
  
  /**
   * Get the clincal attack rate = 100 * number of cases thus far divided by the
   * number of agents in this place.
   *
   * @param disease_id an integer representation of the disease
   * @return the count of rate of cases per people for a given disease
   */
  double get_symptomatic_attack_rate(int disease_id) {
    return (100.0 * this->total_symptomatic_infections[disease_id]) / (double) get_size();
  }
  
  /**
   * Get the attack rate = 100 * number of infections thus far divided by the
   * number of agents in this place.
   *
   * @param disease_id an integer representation of the disease
   * @return the count of rate of cases per people for a given disease
   */
  double get_attack_rate(int disease_id) {
    int n = get_size();
    return(n>0 ? (100.0 * this->total_infections[disease_id]) / (double)n : 0.0);
  }

  int get_first_day_infectious() {
    return this->first_day_infectious;
  }

  int get_last_day_infectious() {
    return this->last_day_infectious;
  }

  person_vec_t * get_enrollees() {
    return &(this->enrollees);
  }

  person_vec_t * get_infectious_people(int  disease_id) {
    return &(this->infectious_people[disease_id]);
  }

  Person* get_enrollee(int i) {
    return this->enrollees[i];
  }

  void set_index(int _index) {
    this->index = _index;
  }
  
  int get_index() {
    return this->index;
  }

  void set_subtype(fred::place_subtype _subtype) {
    this->subtype = _subtype;
  }
  
  fred::place_subtype get_subtype() {
    return this->subtype;
  }
  
  int get_staff_size() {
    return this->staff_size;
  }
  
  void set_staff_size(int _staff_size) {
    this->staff_size = _staff_size;
  }

  int get_household_fips() {
    return this->household_fips;
  }

  void set_household_fips(int input_fips) {
    this->household_fips = input_fips;
  }
  
  void set_county_index(int _county_index) {
    this->county_index = _county_index;
  }

  int get_county_index() {
    return this->county_index;
  }

  void set_census_tract_index(int _census_tract_index) {
    this->census_tract_index = _census_tract_index;
  }

  int get_census_tract_index() {
    return this->census_tract_index;
  }
  
  static char* get_place_label(Place* p);

  // vector transmission methods

  void setup_vector_model();

  void mark_vectors_as_not_infected_today() {
    this->vectors_have_been_infected_today = false;
  }

  void mark_vectors_as_infected_today() {
    this->vectors_have_been_infected_today = true;
  }

  bool have_vectors_been_infected_today() {
    return this->vectors_have_been_infected_today;
  }

  int get_vector_population_size() {
    return this->vector_disease_data->N_vectors;
  }
    
  int get_susceptible_vectors() {
    return this->vector_disease_data->S_vectors;
  }

  int get_infected_vectors(int disease_id) {
    return this->vector_disease_data->E_vectors[disease_id] +
      this->vector_disease_data->I_vectors[disease_id];
  }

  int get_infectious_vectors(int disease_id) {
    return this->vector_disease_data->I_vectors[disease_id];
  }
    
  void expose_vectors(int disease_id, int exposed_vectors) {
    this->vector_disease_data->E_vectors[disease_id] += exposed_vectors;
    this->vector_disease_data->S_vectors -= exposed_vectors;
  }

  double get_seeds(int dis, int day);

  vector_disease_data_t get_vector_disease_data () {
    assert(vector_disease_data != NULL);
    return (*vector_disease_data);
  }
  
  void update_vector_population(int day);
 
  bool get_vector_control_status() {
    return vector_control_status;
  }
  void set_vector_control(){
    vector_control_status = true;
  }
  void stop_vector_control(){
      vector_control_status = false;
  }

protected:
  char label[32];         // external id
  char type;              // HOME, WORK, SCHOOL, COMMUNITY, etc;
  fred::place_subtype subtype;
  char worker_profile;
  int id;                 // place id
  fred::geo latitude;     // geo location
  fred::geo longitude;    // geo location
  int close_date;         // this place will be closed during:
  int open_date;          //   [close_date, open_date)
  int N_orig;		  // orig number of potential visitors
  double intimacy;	  // prob of intimate contact
  static double** prob_contact;
  int index;		  // index for households
  int staff_size;			// outside workers in this place

  int household_fips;
  int county_index;
  int census_tract_index;
  
  Neighborhood_Patch* patch;       // geo patch for this place
  
  // lists of people
  person_vec_t * infectious_people;
  person_vec_t  enrollees;

  // epidemic counters
  int * new_infections;				// new infections today
  int * current_infections;	      // current active infections today
  int * total_infections;	       // total infections over all time
  int * new_symptomatic_infections;	   // new sympt infections today
  int * current_symptomatic_infections; // current active sympt infections
  int * total_symptomatic_infections; // total sympt infections over all time

  // these counts refer to today's visitors:
  int * current_infectious_visitors;  // total infectious visitors today
  int * current_symptomatic_visitors;	 // total sympt infections today

  int first_day_infectious;
  int last_day_infectious;

  // track whether or not place is infectious with each disease
  fred::disease_bitset infectious_bitset; 
  fred::disease_bitset human_infectious_bitset;
  fred::disease_bitset recovered_bitset; 
  fred::disease_bitset exposed_bitset; 

  // Place_List, Neighborhood_Layer and Neighborhood_Patch are friends so that they can access
  // the Place Allocator.  
  friend class Place_List;
  friend class Neighborhood_Layer;
  friend class Neighborhood_Patch;

  // friend Place_List can assign id
  void set_id(int _id) {
    this->id = _id;
  }

  void add_infectious_visitor(int disease_id, Person * person) { 
    this->infectious_people[disease_id].push_back(person);
    this->current_infectious_visitors[disease_id]++;
  }
  
  void add_symptomatic_visitor(int disease_id) {
    this->current_symptomatic_visitors[disease_id]++;
  }
  
  // optional data for vector transmission model
  vector_disease_data_t * vector_disease_data;
  bool vectors_have_been_infected_today;
  bool vector_control_status;

  // Place Allocator reserves chunks of memory and hands out pointers for use
  // with placement new
  template<typename Place_Type>
  struct Allocator {
    Place_Type* allocation_array;
    int current_allocation_size;
    int current_allocation_index;
    int number_of_contiguous_blocks_allocated;
    int remaining_allocations;
    int allocations_made;

    Allocator() {
      remaining_allocations = 0;
      number_of_contiguous_blocks_allocated = 0;
      allocations_made = 0;
      current_allocation_index = 0;
      current_allocation_size = 0;
      allocation_array = NULL;
    }

    bool reserve( int n = 1 ) {
      if(remaining_allocations == 0) {
        current_allocation_size = n;
        allocation_array = new Place_Type[n];
        remaining_allocations = n; 
        current_allocation_index = 0;
        ++(number_of_contiguous_blocks_allocated);
        allocations_made += n;
        return true;
      }
      return false;
    }

    Place_Type* get_free() {
      if(remaining_allocations == 0) {
        reserve();
      }
      Place_Type* place_pointer = allocation_array + current_allocation_index;
      --(remaining_allocations);
      ++(current_allocation_index);
      return place_pointer;
    }

    int get_number_of_remaining_allocations() {
      return remaining_allocations;
    }

    int get_number_of_contiguous_blocks_allocated() {
      return number_of_contiguous_blocks_allocated;
    }

    int get_number_of_allocations_made() {
      return allocations_made;
    }

    Place_Type* get_base_pointer() {
      return allocation_array;
    }

    int size() {
      return allocations_made;
    }
  }; // end Place Allocator

};


#endif // _FRED_PLACE_H
