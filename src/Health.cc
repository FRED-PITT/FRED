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
// File: Health.cc
//
#include <new>
#include <stdexcept>

#include "Age_Map.h"
#include "Antiviral.h"
#include "AV_Health.h"
#include "AV_Manager.h"
#include "Date.h"
#include "Condition.h"
#include "Condition_List.h"
#include "Evolution.h"
#include "Infection.h"
#include "Health.h"
#include "Household.h"
#include "Manager.h"
#include "Mixing_Group.h"
#include "Past_Infection.h"
#include "Person.h"
#include "Place.h"
#include "Place_List.h"
#include "Population.h"
#include "Random.h"
#include "Utils.h"
#include "Vaccine.h"
#include "Vaccine_Dose.h"
#include "Vaccine_Health.h"
#include "Vaccine_Manager.h"

// static variables
int Health::nantivirals = -1;
Age_Map* Health::asthma_prob = NULL;
Age_Map* Health::COPD_prob = NULL;
Age_Map* Health::chronic_renal_condition_prob = NULL;
Age_Map* Health::diabetes_prob = NULL;
Age_Map* Health::heart_condition_prob = NULL;
Age_Map* Health::hypertension_prob = NULL;
Age_Map* Health::hypercholestrolemia_prob = NULL;

Age_Map* Health::asthma_hospitalization_prob_mult = NULL;
Age_Map* Health::COPD_hospitalization_prob_mult = NULL;
Age_Map* Health::chronic_renal_condition_hospitalization_prob_mult = NULL;
Age_Map* Health::diabetes_hospitalization_prob_mult = NULL;
Age_Map* Health::heart_condition_hospitalization_prob_mult = NULL;
Age_Map* Health::hypertension_hospitalization_prob_mult = NULL;
Age_Map* Health::hypercholestrolemia_hospitalization_prob_mult = NULL;

Age_Map* Health::asthma_case_fatality_prob_mult = NULL;
Age_Map* Health::COPD_case_fatality_prob_mult = NULL;
Age_Map* Health::chronic_renal_condition_case_fatality_prob_mult = NULL;
Age_Map* Health::diabetes_case_fatality_prob_mult = NULL;
Age_Map* Health::heart_condition_case_fatality_prob_mult = NULL;
Age_Map* Health::hypertension_case_fatality_prob_mult = NULL;
Age_Map* Health::hypercholestrolemia_case_fatality_prob_mult = NULL;

Age_Map* Health::pregnancy_hospitalization_prob_mult = NULL;
Age_Map* Health::pregnancy_case_fatality_prob_mult = NULL;

bool Health::is_initialized = false;

// health protective behavior parameters
int Health::Days_to_wear_face_masks = 0;
double Health::Face_mask_compliance = 0.0;
double Health::Hand_washing_compliance = 0.0;

double Health::Hh_income_susc_mod_floor = 0.0;

double Health::health_insurance_distribution[Insurance_assignment_index::UNSET];
int Health::health_insurance_cdf_size = 0;

// static method called in main (Fred.cc)

void Health::initialize_static_variables() {
  //Setup the static variables if they aren't already initialized
  if(!Health::is_initialized) {

    Params::get_param_from_string("days_to_wear_face_masks", &(Health::Days_to_wear_face_masks));
    Params::get_param_from_string("face_mask_compliance", &(Health::Face_mask_compliance));
    Params::get_param_from_string("hand_washing_compliance", &(Health::Hand_washing_compliance));

    int temp_int = 0;

    if(Global::Enable_hh_income_based_susc_mod) {
      Params::get_param_from_string("hh_income_susc_mod_floor", &(Health::Hh_income_susc_mod_floor));
    }

    if(Global::Enable_Chronic_Condition) {
      Health::asthma_prob = new Age_Map("Asthma Probability");
      Health::asthma_prob->read_from_input("asthma_prob");
      Health::asthma_hospitalization_prob_mult = new Age_Map("Asthma Hospitalization Probability Mult");
      Health::asthma_hospitalization_prob_mult->read_from_input("asthma_hospitalization_prob_mult");
      Health::asthma_case_fatality_prob_mult = new Age_Map("Asthma Case Fatality Probability Mult");
      Health::asthma_case_fatality_prob_mult->read_from_input("asthma_case_fatality_prob_mult");

      Health::COPD_prob = new Age_Map("COPD Probability");
      Health::COPD_prob->read_from_input("COPD_prob");
      Health::COPD_hospitalization_prob_mult = new Age_Map("COPD Hospitalization Probability Mult");
      Health::COPD_hospitalization_prob_mult->read_from_input("COPD_hospitalization_prob_mult");
      Health::COPD_case_fatality_prob_mult = new Age_Map("COPD Case Fatality Probability Mult");
      Health::COPD_case_fatality_prob_mult->read_from_input("COPD_case_fatality_prob_mult");

      Health::chronic_renal_condition_prob = new Age_Map("Chronic Renal Condition Probability");
      Health::chronic_renal_condition_prob->read_from_input("chronic_renal_condition_prob");
      Health::chronic_renal_condition_hospitalization_prob_mult = new Age_Map("Chronic Renal Condition Hospitalization Probability Mult");
      Health::chronic_renal_condition_hospitalization_prob_mult->read_from_input("chronic_renal_condition_hospitalization_prob_mult");
      Health::chronic_renal_condition_case_fatality_prob_mult = new Age_Map("Chronic Renal Condition Case Fatality Probability Mult");
      Health::chronic_renal_condition_case_fatality_prob_mult->read_from_input("chronic_renal_condition_case_fatality_prob_mult");

      Health::diabetes_prob = new Age_Map("Diabetes Probability");
      Health::diabetes_prob->read_from_input("diabetes_prob");
      Health::diabetes_hospitalization_prob_mult = new Age_Map("Diabetes Hospitalization Probability Mult");
      Health::diabetes_hospitalization_prob_mult->read_from_input("diabetes_hospitalization_prob_mult");
      Health::diabetes_case_fatality_prob_mult = new Age_Map("Diabetes Case Fatality Probability Mult");
      Health::diabetes_case_fatality_prob_mult->read_from_input("diabetes_case_fatality_prob_mult");

      Health::heart_condition_prob = new Age_Map("Heart Condition Probability");
      Health::heart_condition_prob->read_from_input("heart_condition_prob");
      Health::heart_condition_hospitalization_prob_mult = new Age_Map("Heart Condition Hospitalization Probability Mult");
      Health::heart_condition_hospitalization_prob_mult->read_from_input("heart_condition_hospitalization_prob_mult");
      Health::heart_condition_case_fatality_prob_mult = new Age_Map("Heart Condition Case Fatality Probability Mult");
      Health::heart_condition_case_fatality_prob_mult->read_from_input("heart_condition_case_fatality_prob_mult");

      Health::hypertension_prob = new Age_Map("Hypertension Probability");
      Health::hypertension_prob->read_from_input("hypertension_prob");
      Health::hypertension_hospitalization_prob_mult = new Age_Map("Hypertension Hospitalization Probability Mult");
      Health::hypertension_hospitalization_prob_mult->read_from_input("hypertension_hospitalization_prob_mult");
      Health::hypertension_case_fatality_prob_mult = new Age_Map("Hypertension Case Fatality Probability Mult");
      Health::hypertension_case_fatality_prob_mult->read_from_input("hypertension_case_fatality_prob_mult");

      Health::hypercholestrolemia_prob = new Age_Map("Hypercholestrolemia Probability");
      Health::hypercholestrolemia_prob->read_from_input("hypercholestrolemia_prob");
      Health::hypercholestrolemia_hospitalization_prob_mult = new Age_Map("Hypercholestrolemia Hospitalization Probability Mult");
      Health::hypercholestrolemia_hospitalization_prob_mult->read_from_input("hypercholestrolemia_hospitalization_prob_mult");
      Health::hypercholestrolemia_case_fatality_prob_mult = new Age_Map("Hypercholestrolemia Case Fatality Probability Mult");
      Health::hypercholestrolemia_case_fatality_prob_mult->read_from_input("hypercholestrolemia_case_fatality_prob_mult");

      Health::pregnancy_hospitalization_prob_mult = new Age_Map("Pregnancy Hospitalization Probability Mult");
      Health::pregnancy_hospitalization_prob_mult->read_from_input("pregnancy_hospitalization_prob_mult");
      Health::pregnancy_case_fatality_prob_mult = new Age_Map("Pregnancy Case Fatality Probability Mult");
      Health::pregnancy_case_fatality_prob_mult->read_from_input("pregnancy_case_fatality_prob_mult");
    }

    if(Global::Enable_Health_Insurance) {

      Health::health_insurance_cdf_size = Params::get_param_vector((char*)"health_insurance_distribution", Health::health_insurance_distribution);

      // convert to cdf
      double stotal = 0;
      for(int i = 0; i < Health::health_insurance_cdf_size; ++i) {
        stotal += Health::health_insurance_distribution[i];
      }
      if(stotal != 100.0 && stotal != 1.0) {
        Utils::fred_abort("Bad distribution health_insurance_distribution params_str\nMust sum to 1.0 or 100.0\n");
      }
      double cumm = 0.0;
      for(int i = 0; i < Health::health_insurance_cdf_size; ++i) {
        Health::health_insurance_distribution[i] /= stotal;
        Health::health_insurance_distribution[i] += cumm;
        cumm = Health::health_insurance_distribution[i];
      }
    }

    Health::is_initialized = true;
  }
}

Insurance_assignment_index::e Health::get_health_insurance_from_distribution() {
  if(Global::Enable_Health_Insurance && Health::is_initialized) {
    int i = Random::draw_from_distribution(Health::health_insurance_cdf_size, Health::health_insurance_distribution);
    return Health::get_insurance_type_from_int(i);
  } else {
    return Insurance_assignment_index::UNSET;
  }
}

double Health::get_chronic_condition_case_fatality_prob_mult(double real_age, Chronic_condition_index::e cond_idx) {
  if(Global::Enable_Chronic_Condition && Health::is_initialized) {
    assert(cond_idx >= Chronic_condition_index::ASTHMA);
    assert(cond_idx < Chronic_condition_index::CHRONIC_MEDICAL_CONDITIONS);
    switch(cond_idx) {
    case Chronic_condition_index::ASTHMA:
      return Health::asthma_case_fatality_prob_mult->find_value(real_age);
    case Chronic_condition_index::COPD:
      return Health::COPD_case_fatality_prob_mult->find_value(real_age);
    case Chronic_condition_index::CHRONIC_RENAL_CONDITION:
      return Health::chronic_renal_condition_case_fatality_prob_mult->find_value(real_age);
    case Chronic_condition_index::DIABETES:
      return Health::diabetes_case_fatality_prob_mult->find_value(real_age);
    case Chronic_condition_index::HEART_CONDITION:
      return Health::heart_condition_case_fatality_prob_mult->find_value(real_age);
    case Chronic_condition_index::HYPERTENSION:
      return Health::hypertension_case_fatality_prob_mult->find_value(real_age);
    case Chronic_condition_index::HYPERCHOLESTROLEMIA:
      return Health::hypercholestrolemia_case_fatality_prob_mult->find_value(real_age);
    default:
      return 1.0;
    }
  }
  return 1.0;
}

double Health::get_chronic_condition_hospitalization_prob_mult(double real_age, Chronic_condition_index::e cond_idx) {
  if(Global::Enable_Chronic_Condition && Health::is_initialized) {
    assert(cond_idx >= Chronic_condition_index::ASTHMA);
    assert(cond_idx < Chronic_condition_index::CHRONIC_MEDICAL_CONDITIONS);
    switch(cond_idx) {
    case Chronic_condition_index::ASTHMA:
      return Health::asthma_hospitalization_prob_mult->find_value(real_age);
    case Chronic_condition_index::COPD:
      return Health::COPD_hospitalization_prob_mult->find_value(real_age);
    case Chronic_condition_index::CHRONIC_RENAL_CONDITION:
      return Health::chronic_renal_condition_hospitalization_prob_mult->find_value(real_age);
    case Chronic_condition_index::DIABETES:
      return Health::diabetes_hospitalization_prob_mult->find_value(real_age);
    case Chronic_condition_index::HEART_CONDITION:
      return Health::heart_condition_hospitalization_prob_mult->find_value(real_age);
    case Chronic_condition_index::HYPERTENSION:
      return Health::hypertension_hospitalization_prob_mult->find_value(real_age);
    case Chronic_condition_index::HYPERCHOLESTROLEMIA:
      return Health::hypercholestrolemia_hospitalization_prob_mult->find_value(real_age);
    default:
      return 1.0;
    }
  }
  return 1.0;
}

Health::Health() {
  this->myself = NULL;
  this->past_infections = NULL;
  this->alive = true;
  this->av_health = NULL;
  this->checked_for_av = NULL;
  this->vaccine_health = NULL;
  this->has_face_mask_behavior = false;
  this->wears_face_mask_today = false;
  this->days_wearing_face_mask = 0;
  this->washes_hands = false;
  this->days_symptomatic = 0;
  this->previous_infection_serotype = 0;
  this->insurance_type = Insurance_assignment_index::UNSET;
  this->infection = NULL;
  this->immunity_end_date = NULL;
  this->infectee_count = NULL;
  this->susceptibility_multp = NULL;
  this->exposure_date = NULL;
  this->infector_id = NULL;
  this->infected_in_mixing_group = NULL;
  this->health_condition = NULL;
  this->health_state.clear();
}

void Health::setup(Person* self) {
  this->myself = self;
  FRED_VERBOSE(1, "Health::setup for person %d\n", myself->get_id());
  this->alive = true;
  this->intervention_flags = intervention_flags_type();
  // infection pointers stored in statically allocated array (length of which
  // is determined by static constant Global::MAX_NUM_CONDITIONS)
  this->susceptible = fred::condition_bitset();
  this->infectious = fred::condition_bitset();
  this->symptomatic = fred::condition_bitset();
  this->recovered = fred::condition_bitset();
  this->immunity = fred::condition_bitset();

  // Determines if the agent is at risk
  this->at_risk = fred::condition_bitset();
  
  // Determine if the agent washes hands
  this->washes_hands = false;
  if(Health::Hand_washing_compliance > 0.0) {
    this->washes_hands = (Random::draw_random() < Health::Hand_washing_compliance);
  }

  // Determine if the agent will wear a face mask if sick
  this->has_face_mask_behavior = false;
  this->wears_face_mask_today = false;
  this->days_wearing_face_mask = 0;
  if(Health::Face_mask_compliance > 0.0) {
    if(Random::draw_random()<Health::Face_mask_compliance) {
      this->has_face_mask_behavior = true;
    }
    // printf("FACEMASK: has_face_mask_behavior = %d\n", this->has_face_mask_behavior?1:0);
  }

  this->case_fatality = fred::condition_bitset();
  int conditions = Global::Conditions.get_number_of_conditions();
  FRED_VERBOSE(1, "Health::setup conditions %d\n", conditions);
  this->infection = new Infection* [conditions];
  this->susceptibility_multp = new double [conditions];
  this->infectee_count = new int [conditions];
  this->exposure_date = new int [conditions];
  this->infector_id = new int [conditions];
  this->infected_in_mixing_group = new Mixing_Group*[conditions];
  this->immunity_end_date = new int [conditions];
  this->past_infections = new past_infections_type [conditions];
  this->health_condition = new health_condition_t [conditions];

  for(int condition_id = 0; condition_id < conditions; ++condition_id) {
    this->recovered.reset(condition_id);
    this->susceptible.reset(condition_id);
    this->case_fatality.reset(condition_id);
    this->infection[condition_id] = NULL;
    this->susceptibility_multp[condition_id] = 1.0;
    this->infectee_count[condition_id] = 0;
    this->exposure_date[condition_id] = -1;
    this->infector_id[condition_id] = -1;
    this->infected_in_mixing_group[condition_id] = NULL;
    this->immunity_end_date[condition_id] = -1;
    this->past_infections[condition_id].clear();
    this->health_condition[condition_id].state = -1;
    this->health_condition[condition_id].last_transition_day = -1;
    this->health_condition[condition_id].next_state = -1;
    this->health_condition[condition_id].next_transition_day = -1;

    Condition* condition = Global::Conditions.get_condition(condition_id);
    if (condition->assume_susceptible()) {
      become_susceptible(condition_id);
    }

    if(condition->get_at_risk() != NULL && !condition->get_at_risk()->is_empty()) {
      double at_risk_prob = condition->get_at_risk()->find_value(myself->get_real_age());
      if(Random::draw_random() < at_risk_prob) { // Now a probability <=1.0
        declare_at_risk(condition);
      }
    }
  }

  this->days_symptomatic = 0;
  this->vaccine_health = NULL;
  this->av_health = NULL;
  this->checked_for_av = NULL;
  this->previous_infection_serotype = -1;

  if(Health::nantivirals == -1) {
    Params::get_param_from_string("number_antivirals", &Health::nantivirals);
  }

  if(Global::Enable_Chronic_Condition && Health::is_initialized) {
    double prob = 0.0;
    prob = Health::asthma_prob->find_value(myself->get_real_age());
    set_is_asthmatic((Random::draw_random() < prob));

    prob = Health::COPD_prob->find_value(myself->get_real_age());
    set_has_COPD((Random::draw_random() < prob));

    prob = Health::chronic_renal_condition_prob->find_value(myself->get_real_age());
    set_has_chronic_renal_condition((Random::draw_random() < prob));

    prob = Health::diabetes_prob->find_value(myself->get_real_age());
    set_is_diabetic((Random::draw_random() < prob));

    prob = Health::heart_condition_prob->find_value(myself->get_real_age());
    set_has_heart_condition((Random::draw_random() < prob));

    prob = Health::hypertension_prob->find_value(myself->get_real_age());
    set_has_hypertension((Random::draw_random() < prob));

    prob = Health::hypercholestrolemia_prob->find_value(myself->get_real_age());
    set_has_hypercholestrolemia((Random::draw_random() < prob));
  }
}

Health::~Health() {
  for(size_t i = 0; i < Global::Conditions.get_number_of_conditions(); ++i) {
    if(this->infection[i] != NULL) {
      delete this->infection[i];
    }
  }

  if(this->vaccine_health) {
    for(unsigned int i = 0; i < this->vaccine_health->size(); ++i) {
      delete (*this->vaccine_health)[i];
    }
    this->vaccine_health->clear();
    delete this->vaccine_health;
  }

  if(this->av_health) {
    for(unsigned int i = 0; i < this->av_health->size(); ++i) {
      delete (*this->av_health)[i];
    }
    this->av_health->clear();
    delete this->av_health;
  }

  if(this->checked_for_av) {
    delete this->checked_for_av;
  }
}

void Health::become_susceptible(int condition_id) {
  if(this->susceptible.test(condition_id)) {
    FRED_CONDITIONAL_STATUS(0, Global::Enable_Health_Records,
			    "HEALTH RECORD: %s person %d is already SUSCEPTIBLE for %s\n",
			    Date::get_date_string().c_str(),
			    myself->get_id(),
			    Global::Conditions.get_condition_name(condition_id).c_str());
    return;
  }
  assert(this->infection[condition_id] == NULL);
  this->susceptibility_multp[condition_id] = 1.0;
  this->susceptible.set(condition_id);
  assert(is_susceptible(condition_id));
  this->recovered.reset(condition_id);
  FRED_CONDITIONAL_STATUS(0, Global::Enable_Health_Records,
			  "HEALTH RECORD: %s person %d is SUSCEPTIBLE for %s\n",
			  Date::get_date_string().c_str(),
			  myself->get_id(),
			  Global::Conditions.get_condition_name(condition_id).c_str());
}

void Health::become_susceptible_by_vaccine_waning(int condition_id) {
  if(this->susceptible.test(condition_id)) {
    return;
  }
  if(this->infection[condition_id] == NULL) {
    // not already infected
    this->susceptibility_multp[condition_id] = 1.0;
    this->susceptible.set(condition_id);
    FRED_CONDITIONAL_STATUS(0, Global::Enable_Health_Records,
			    "HEALTH RECORD: %s person %d is SUSCEPTIBLE for %s\n",
			    Date::get_date_string().c_str(),
			    myself->get_id(),
			    Global::Conditions.get_condition_name(condition_id).c_str());
  } else {
    FRED_CONDITIONAL_STATUS(0, Global::Enable_Health_Records,
			    "HEALTH RECORD: %s person %d had no vaccine waning because was already infected with %s\n",
			    Date::get_date_string().c_str(),
			    myself->get_id(),
			    Global::Conditions.get_condition_name(condition_id).c_str());
  }
}

void Health::become_exposed(int condition_id, Person* infector, Mixing_Group* mixing_group, int day) {

   FRED_VERBOSE(1, "become_exposed: person %d is exposed to condition %d day %d\n",
		            myself->get_id(), condition_id, day);

  if(this->infection[condition_id] != NULL) {
    Utils::fred_abort("DOUBLE EXPOSURE: person %d dis_id %d day %d\n", myself->get_id(), condition_id, day);
  }

  if(Global::Verbose > 0) {
    if(mixing_group == NULL) {
      FRED_CONDITIONAL_STATUS(0, Global::Enable_Health_Records,
			      "HEALTH RECORD: %s person %d is an IMPORTED EXPOSURE to %s\n",
			      Date::get_date_string().c_str(),
			      myself->get_id(),
			      Global::Conditions.get_condition_name(condition_id).c_str());
    } else {
      FRED_CONDITIONAL_STATUS(0, Global::Enable_Health_Records,
			      "HEALTH RECORD: %s person %d is EXPOSED to %s\n",
			      Date::get_date_string().c_str(),
			      myself->get_id(),
			      Global::Conditions.get_condition_name(condition_id).c_str());
    }
  }

  this->infectious.reset(condition_id);
  this->symptomatic.reset(condition_id);
  Condition* condition = Global::Conditions.get_condition(condition_id);
  this->infection[condition_id] = Infection::get_new_infection(condition, infector, myself, mixing_group, day);
  FRED_VERBOSE(1, "setup infection: person %d dis_id %d day %d\n", myself->get_id(), condition_id, day);
  this->infection[condition_id]->setup();
  this->infection[condition_id]->report_infection(day);
  become_unsusceptible(condition);
  this->immunity_end_date[condition_id] = -1;
  if(myself->get_household() != NULL) {
    myself->get_household()->set_exposed(condition_id);
    myself->set_exposed_household(myself->get_household()->get_index());
  }
  if(infector != NULL) {
    this->infector_id[condition_id] = infector->get_id();
  }
  this->exposure_date[condition_id] = day;
  this->infected_in_mixing_group[condition_id] = mixing_group;

  if(Global::Enable_Transmission_Network) {
    FRED_VERBOSE(1, "Joining transmission network: %d\n", myself->get_id());
    myself->join_network(Global::Transmission_Network);
  }

  if(Global::Enable_Vector_Transmission && Global::Conditions.get_number_of_conditions() > 1) {
    // special check for multi-serotype dengue:
    if(this->previous_infection_serotype == -1) {
      // remember this infection's serotype
      this->previous_infection_serotype = condition_id;
      // after the first infection, become immune to other two serotypes.
      for(int sero = 0; sero < Global::Conditions.get_number_of_conditions(); ++sero) {
        // if (sero == previous_infection_serotype) continue;
        if(sero == condition_id) {
          continue;
        }
        FRED_STATUS(1, "DENGUE: person %d now immune to serotype %d\n",
		    myself->get_id(), sero);
        become_unsusceptible(Global::Conditions.get_condition(sero));
      }
    } else {
      // after the second infection, become immune to other two serotypes.
      for(int sero = 0; sero < Global::Conditions.get_number_of_conditions(); ++sero) {
        if(sero == this->previous_infection_serotype) {
          continue;
        }
        if(sero == condition_id) {
          continue;
        }
        FRED_STATUS(1, "DENGUE: person %d now immune to serotype %d\n",
		    myself->get_id(), sero);
        become_unsusceptible(Global::Conditions.get_condition(sero));
      }
    }
  }
}

void Health::become_unsusceptible(int condition_id) {
  this->susceptible.reset(condition_id);
  FRED_CONDITIONAL_STATUS(0, Global::Enable_Health_Records,
			  "HEALTH RECORD: %s person %d is UNSUSCEPTIBLE for %s\n",
			  Date::get_date_string().c_str(),
			  myself->get_id(),
			  Global::Conditions.get_condition_name(condition_id).c_str());
}

void Health::become_unsusceptible(Condition* condition) {
  int condition_id = condition->get_id();
  become_unsusceptible(condition_id);
}

void Health::become_infectious(Condition* condition) {
  int condition_id = condition->get_id();
  assert(this->infection[condition_id] != NULL);
  this->infectious.set(condition_id);
  int household_index = myself->get_exposed_household_index();
  Household* h = Global::Places.get_household_ptr(household_index);
  assert(h != NULL);
  h->set_human_infectious(condition_id);
  FRED_CONDITIONAL_STATUS(0, Global::Enable_Health_Records,
			  "HEALTH RECORD: %s person %d is INFECTIOUS for %s\n",
			  Date::get_date_string().c_str(),
			  myself->get_id(),
			  Global::Conditions.get_condition_name(condition_id).c_str());
}

void Health::become_noninfectious(Condition* condition) {
  int condition_id = condition->get_id();
  assert(this->infection[condition_id] != NULL);
  this->infectious.reset(condition_id);
  FRED_CONDITIONAL_STATUS(0, Global::Enable_Health_Records,
			  "HEALTH RECORD: %s person %d is NONINFECTIOUS for %s\n",
			  Date::get_date_string().c_str(),
			  myself->get_id(),
			  Global::Conditions.get_condition_name(condition_id).c_str());
}

void Health::become_symptomatic(Condition* condition) {
  int condition_id = condition->get_id();
  if(this->infection[condition_id] == NULL) {
    FRED_STATUS(1, "Help: becoming symptomatic with no infection: person %d, condition_id %d\n", myself->get_id(), condition_id);
  }
  assert(this->infection[condition_id] != NULL);
  if(this->symptomatic.test(condition_id)) {
    FRED_CONDITIONAL_STATUS(0, Global::Enable_Health_Records,
			    "HEALTH RECORD: %s person %d is ALREADY SYMPTOMATIC for %s\n",
			    Date::get_date_string().c_str(),
			    myself->get_id(),
			    Global::Conditions.get_condition_name(condition_id).c_str());
    return;
  }
  this->symptomatic.set(condition_id);
  FRED_CONDITIONAL_STATUS(0, Global::Enable_Health_Records,
			  "HEALTH RECORD: %s person %d is SYMPTOMATIC for %s\n",
			  Date::get_date_string().c_str(),
			  myself->get_id(),
			  Global::Conditions.get_condition_name(condition_id).c_str());
}

void Health::resolve_symptoms(Condition* condition) {
  int condition_id = condition->get_id();
  // assert(this->infection[condition_id] != NULL);
  if(this->symptomatic.test(condition_id)) {
    this->symptomatic.reset(condition_id);
  }
  FRED_CONDITIONAL_STATUS(0, Global::Enable_Health_Records,
			  "HEALTH RECORD: %s person %d RESOLVES SYMPTOMS for %s\n",
			  Date::get_date_string().c_str(),
			  myself->get_id(),
			  Global::Conditions.get_condition_name(condition_id).c_str());
}


void Health::recover(Condition* condition, int day) {
  int condition_id = condition->get_id();
  assert(this->infection[condition_id] != NULL);
  FRED_CONDITIONAL_STATUS(0, Global::Enable_Health_Records,
			   "HEALTH RECORD: %s person %d is RECOVERED from %s\n",
			   Date::get_date_string().c_str(),
			  myself->get_id(),
			  Global::Conditions.get_condition_name(condition_id).c_str());
  this->recovered.set(condition_id);
  int household_index = myself->get_exposed_household_index();
  Household* h = Global::Places.get_household_ptr(household_index);
  h->set_recovered(condition_id);
  h->reset_human_infectious();
  myself->reset_neighborhood();

  this->immunity_end_date[condition_id] = this->infection[condition_id]->get_immunity_end_date();
  if (this->immunity_end_date[condition_id] > -1) {
    this->immunity_end_date[condition_id] += day;
  }
  become_removed(condition_id, day);
}

void Health::become_removed(int condition_id, int day) {
  terminate_infection(condition_id, day);
  this->susceptible.reset(condition_id);
  this->infectious.reset(condition_id);
  this->symptomatic.reset(condition_id);
  FRED_CONDITIONAL_STATUS(0, Global::Enable_Health_Records,
			   "HEALTH RECORD: %s person %d is REMOVED for %s\n",
			   Date::get_date_string().c_str(),
			  myself->get_id(),
			  Global::Conditions.get_condition_name(condition_id).c_str());
}

void Health::become_immune(Condition* condition) {
  int condition_id = condition->get_id();
  condition->become_immune(myself, this->susceptible.test(condition_id),
      this->infectious.test(condition_id), this->symptomatic.test(condition_id));
  this->immunity.set(condition_id);
  this->susceptible.reset(condition_id);
  this->infectious.reset(condition_id);
  this->symptomatic.reset(condition_id);
  FRED_CONDITIONAL_STATUS(0, Global::Enable_Health_Records,
			  "HEALTH RECORD: %s person %d is IMMUNE for %s\n",
			  Date::get_date_string().c_str(),
			  myself->get_id(),
			  Global::Conditions.get_condition_name(condition_id).c_str());
}


void Health::become_case_fatality(int condition_id, int day) {
  FRED_VERBOSE(0, "CONDITION %d is FATAL: day %d person %d\n", condition_id, day, myself->get_id());
  this->case_fatality.set(condition_id);
  FRED_CONDITIONAL_STATUS(0, Global::Enable_Health_Records,
			   "HEALTH RECORD: %s person %d is CASE_FATALITY for %s\n",
			   Date::get_date_string().c_str(),
			  myself->get_id(),
			  Global::Conditions.get_condition_name(condition_id).c_str());
  become_removed(condition_id, day);

  // update household counts
  Mixing_Group* hh = myself->get_household();
  if(hh == NULL) {
    if(Global::Enable_Hospitals && myself->is_hospitalized() && myself->get_permanent_household() != NULL) {
      hh = myself->get_permanent_household();
    }
  }
  if(hh != NULL) {
    hh->increment_case_fatalities(day, condition_id);
  }


  // queue removal from population
  Global::Pop.prepare_to_die(day, myself);
}

void Health::update_infection(int day, int condition_id) {

  if(this->has_face_mask_behavior) {
    update_face_mask_decision(day);
  }
  
  if(this->infection[condition_id] == NULL) {
    return;
  }
  
  FRED_VERBOSE(1, "update_infection %d on day %d person %d\n", condition_id, day, myself->get_id());
  this->infection[condition_id]->update(day);
  
  // update days_symptomatic if needed
  if(this->is_symptomatic(condition_id)) {
    int days_symp_so_far = (day - this->get_symptoms_start_date(condition_id));
    if(days_symp_so_far > this->days_symptomatic) {
      this->days_symptomatic = days_symp_so_far;
    }
  }

  // case_fatality?
  if(this->infection[condition_id]->is_fatal(day)) {
    become_case_fatality(condition_id, day);
  }
  
  FRED_VERBOSE(1,"update_infection %d FINISHED on day %d person %d\n",
	       condition_id, day, myself->get_id());

} // end Health::update_infection //


void Health::update_face_mask_decision(int day) {
  // printf("update_face_mask_decision entered on day %d for person %d\n", day, myself->get_id());

  // should we start use face mask?
  if(this->is_symptomatic(day) && this->days_wearing_face_mask == 0) {
    FRED_VERBOSE(1, "FACEMASK: person %d starts wearing face mask on day %d\n", myself->get_id(), day);
    this->start_wearing_face_mask();
  }

  // should we stop using face mask?
  if(this->is_wearing_face_mask()) {
    if (this->is_symptomatic(day) && this->days_wearing_face_mask < Health::Days_to_wear_face_masks) {
      this->days_wearing_face_mask++;
    } else {
      FRED_VERBOSE(1, "FACEMASK: person %d stops wearing face mask on day %d\n", myself->get_id(), day);
      this->stop_wearing_face_mask();
    }
  }
}

void Health::update_interventions(int day) {
  // if deceased, health status should have been cleared during population
  // update (by calling Person->die(), then Health->die(), which will reset (bool) alive
  if(!(this->alive)) {
    return;
  }
  if(this->intervention_flags.any()) {
    // update vaccine status
    if(this->intervention_flags[Intervention_flag::TAKES_VACCINE]) {
      int size = (int)(this->vaccine_health->size());
      for(int i = 0; i < size; ++i) {
        (*this->vaccine_health)[i]->update(day, myself->get_real_age());
      }
    }
    // update antiviral status
    if(this->intervention_flags[Intervention_flag::TAKES_AV]) {
      for(av_health_itr i = this->av_health->begin(); i != this->av_health->end(); ++i) {
        (*i)->update(day);
      }
    }
  }
} // end Health::update_interventions

void Health::declare_at_risk(Condition* condition) {
  int condition_id = condition->get_id();
  this->at_risk.set(condition_id);
}

void Health::advance_seed_infection(int condition_id, int days_to_advance) {
  assert(this->infection[condition_id] != NULL);
  this->infection[condition_id]->advance_seed_infection(days_to_advance);
}

int Health::get_exposure_date(int condition_id) const {
  return this->exposure_date[condition_id];
}

int Health::get_infectious_start_date(int condition_id) const {
  if(this->infection[condition_id] == NULL) {
    return -1;
  } else {
    return this->infection[condition_id]->get_infectious_start_date();
  }
}

int Health::get_infectious_end_date(int condition_id) const {
  if(this->infection[condition_id] == NULL) {
    return -1;
  } else {
    return this->infection[condition_id]->get_infectious_end_date();
  }
}

int Health::get_symptoms_start_date(int condition_id) const {
  if(this->infection[condition_id] == NULL) {
    return -1;
  } else {
    return this->infection[condition_id]->get_symptoms_start_date();
  }
}

int Health::get_symptoms_end_date(int condition_id) const {
  if(this->infection[condition_id] == NULL) {
    return -1;
  } else {
    return this->infection[condition_id]->get_symptoms_end_date();
  }
}

int Health::get_immunity_end_date(int condition_id) const {
  return this->immunity_end_date[condition_id];
}

bool Health::is_recovered(int condition_id) {
  return this->recovered.test(condition_id);
}


int Health::get_infector_id(int condition_id) const {
  return infector_id[condition_id];
}

Person* Health::get_infector(int condition_id) const {
  if(this->infection[condition_id] == NULL) {
    return NULL;
  } else {
    return this->infection[condition_id]->get_infector();
  }
}

Mixing_Group* Health::get_infected_mixing_group(int condition_id) const {
  return this->infected_in_mixing_group[condition_id];
}

int Health::get_infected_mixing_group_id(int condition_id) const {
  Mixing_Group* mixing_group = get_infected_mixing_group(condition_id);
  if(mixing_group == NULL) {
    return -1;
  } else {
    return mixing_group->get_id();
  }
}

char Health::get_infected_mixing_group_type(int condition_id) const {
  Mixing_Group* mixing_group = get_infected_mixing_group(condition_id);
  if(mixing_group == NULL) {
    return 'X';
  } else {
    return mixing_group->get_type();
  }
}

char dummy_label[8];
char* Health::get_infected_mixing_group_label(int condition_id) const {
  if(this->infection[condition_id] == NULL) {
    strcpy(dummy_label, "-");
    return dummy_label;
  }
  Mixing_Group* mixing_group = get_infected_mixing_group(condition_id);
  if(mixing_group == NULL) {
    strcpy(dummy_label, "X");
    return dummy_label;
  } else {
    return mixing_group->get_label();
  }
}

int Health::get_infectees(int condition_id) const {
  return this->infectee_count[condition_id];
}

double Health::get_susceptibility(int condition_id) const {
  double suscep_multp = this->susceptibility_multp[condition_id];

  if(this->infection[condition_id] == NULL) {
    return suscep_multp;
  } else {
    return this->infection[condition_id]->get_susceptibility() * suscep_multp;
  }
}

double Health::get_infectivity(int condition_id, int day) const {
  if(this->infection[condition_id] == NULL) {
    return 0.0;
  } else {
    return this->infection[condition_id]->get_infectivity(day);
  }
}

double Health::get_symptoms(int condition_id, int day) const {

  if(this->infection[condition_id] == NULL) {
    return 0.0;
  } else {
    return this->infection[condition_id]->get_symptoms(day);
  }
}

//Modify Operators
double Health::get_transmission_modifier_due_to_hygiene(int condition_id) {
  Condition* condition = Global::Conditions.get_condition(condition_id);
  if(this->is_wearing_face_mask() && this->is_washing_hands()) {
    return (1.0 - condition->get_face_mask_plus_hand_washing_transmission_efficacy());
  }
  if(this->is_wearing_face_mask()) {
    return (1.0 - condition->get_face_mask_transmission_efficacy());
  }
  if(this->is_washing_hands()) {
    return (1.0 - condition->get_hand_washing_transmission_efficacy());
  }
  return 1.0;
}

double Health::get_susceptibility_modifier_due_to_hygiene(int condition_id) {
  Condition* condition = Global::Conditions.get_condition(condition_id);
  /*
    if (this->is_wearing_face_mask() && this->is_washing_hands()) {
    return (1.0 - condition->get_face_mask_plus_hand_washing_susceptibility_efficacy());
    }
    if (this->is_wearing_face_mask()) {
    return (1.0 - condition->get_face_mask_susceptibility_efficacy());
    }
  */
  if(this->is_washing_hands()) {
    return (1.0 - condition->get_hand_washing_susceptibility_efficacy());
  }
  return 1.0;
}

double Health::get_susceptibility_modifier_due_to_household_income(int hh_income) {

  if(Global::Enable_hh_income_based_susc_mod) {
    if(hh_income >= Household::get_min_hh_income_90_pct()) {
      return Health::Hh_income_susc_mod_floor;
    } else {
      double rise = 1.0 - Health::Hh_income_susc_mod_floor;
      double run = static_cast<double>(Household::get_min_hh_income() - Household::get_min_hh_income_90_pct());
      double m = rise / run;

      // Equation of line is y - y1 = m(x - x1)
      // y = m*x - m*x1 + y1
      double x = static_cast<double>(hh_income);
      return m * x - m * Household::get_min_hh_income() + 1.0;
    }
  } else {
    return 1.0;
  }
}

void Health::modify_susceptibility(int condition_id, double multp) {
  this->susceptibility_multp[condition_id] *= multp;
}

void Health::modify_infectivity(int condition_id, double multp) {
  if(this->infection[condition_id] != NULL) {
    this->infection[condition_id]->modify_infectivity(multp);
  }
}

void Health::modify_infectious_period(int condition_id, double multp, int cur_day) {
  if(this->infection[condition_id] != NULL) {
    this->infection[condition_id]->modify_infectious_period(multp, cur_day);
  }
}

void Health::modify_asymptomatic_period(int condition_id, double multp, int cur_day) {
  if(this->infection[condition_id] != NULL) {
    this->infection[condition_id]->modify_asymptomatic_period(multp, cur_day);
  }
}

void Health::modify_symptomatic_period(int condition_id, double multp, int cur_day) {
  if(this->infection[condition_id] != NULL) {
    this->infection[condition_id]->modify_symptomatic_period(multp, cur_day);
  }
}

void Health::modify_develops_symptoms(int condition_id, bool symptoms, int cur_day) {
  if(this->infection[condition_id] != NULL
     && ((this->infection[condition_id]->is_infectious(cur_day)
	        && !this->infection[condition_id]->is_symptomatic(cur_day))
	        || !this->infection[condition_id]->is_infectious(cur_day))) {

    this->infection[condition_id]->modify_develops_symptoms(symptoms, cur_day);
    this->symptomatic.set(condition_id);
  }
}

//Medication operators
void Health::take_vaccine(Vaccine* vaccine, int day, Vaccine_Manager* vm) {
  // Compliance will be somewhere else
  double real_age = myself->get_real_age();
  // Is this our first dose?
  Vaccine_Health * vaccine_health_for_dose = NULL;

  if(this->vaccine_health == NULL) {
    this->vaccine_health = new vaccine_health_type();
  }

  for(unsigned int ivh = 0; ivh < this->vaccine_health->size(); ++ivh) {
    if((*this->vaccine_health)[ivh]->get_vaccine() == vaccine) {
      vaccine_health_for_dose = (*this->vaccine_health)[ivh];
    }
  }

  if(vaccine_health_for_dose == NULL) { // This is our first dose of this vaccine
    this->vaccine_health->push_back(new Vaccine_Health(day, vaccine, real_age, myself, vm));
    this->intervention_flags[Intervention_flag::TAKES_VACCINE] = true;
  } else { // Already have a dose, need to take the next dose
    vaccine_health_for_dose->update_for_next_dose(day, real_age);
  }

  if(Global::VaccineTracefp != NULL) {
    fprintf(Global::VaccineTracefp, " id %7d vaccid %3d", myself->get_id(),
	    (*this->vaccine_health)[this->vaccine_health->size() - 1]->get_vaccine()->get_ID());
    (*this->vaccine_health)[this->vaccine_health->size() - 1]->printTrace();
    fprintf(Global::VaccineTracefp, "\n");
  }

  return;
}

void Health::take(Antiviral* av, int day) {
  if(this->checked_for_av == NULL) {
    this->checked_for_av = new checked_for_av_type();
    this->checked_for_av->assign(nantivirals, false);
  }
  if(this->av_health == NULL) {
    this->av_health = new av_health_type();
  }
  this->av_health->push_back(new AV_Health(day, av, this));
  this->intervention_flags[Intervention_flag::TAKES_AV] = true;
  return;
}

bool Health::is_on_av_for_condition(int day, int d) const {
  for(unsigned int iav = 0; iav < this->av_health->size(); ++iav) {
    if((*this->av_health)[iav]->get_condition() == d
       && (*this->av_health)[iav]->is_on_av(day)) {
      return true;
    }
  }
  return false;
}

int Health::get_av_start_day(int i) const {
  assert(this->av_health != NULL);
  return (*this->av_health)[i]->get_av_start_day();
}

void Health::infect(Person* infectee, int condition_id, Mixing_Group* mixing_group, int day) {
  infectee->become_exposed(condition_id, myself, mixing_group, day);

#pragma omp atomic
  ++(this->infectee_count[condition_id]);
  
  int exp_day = this->get_exposure_date(condition_id);
  assert(0 <= exp_day);
  Condition* condition = Global::Conditions.get_condition(condition_id);
  condition->increment_cohort_infectee_count(exp_day);

  FRED_STATUS(1, "person %d infected person %d infectees = %d\n",
	      myself->get_id(), infectee->get_id(), infectee_count[condition_id]);

  if(Global::Enable_Transmission_Network) {
    FRED_VERBOSE(1, "Creating link in transmission network: %d -> %d\n", myself->get_id(), infectee->get_id());
    myself->create_network_link_to(infectee, Global::Transmission_Network);
  }
}

void Health::update_mixing_group_counts(int day, int condition_id, Mixing_Group* mixing_group) {
  // this is only called for people with an active infection
  assert(is_infected(condition_id));

  // mixing group must exist to update
  if(mixing_group == NULL) {
    return;
  }

  // update infection counters
  if(is_newly_infected(day, condition_id)) {
    mixing_group->increment_new_infections(day, condition_id);
  }
  mixing_group->increment_current_infections(day, condition_id);

  // update symptomatic infection counters
  if(is_symptomatic(condition_id)) {
    if(is_newly_symptomatic(day, condition_id)) {
      mixing_group->increment_new_symptomatic_infections(day, condition_id);
    }
    mixing_group->increment_current_symptomatic_infections(day, condition_id);
  }
}

void Health::terminate_infection(int condition_id, int day) {
  if(this->infection[condition_id] != NULL) {
    // delete the infection object
    delete this->infection[condition_id];
    this->infection[condition_id] = NULL;
  }
}

void Health::terminate(int day) {
  for(int condition_id = 0; condition_id < Global::Conditions.get_number_of_conditions(); ++condition_id) {
    if(this->infection[condition_id] != NULL) {
      become_removed(condition_id, day);
    }
    if(this->health_condition[condition_id].state >-1) {
      Global::Conditions.get_condition(condition_id)->terminate_person(myself, day);;
    }
  }
  this->alive = false;
}

void Health::update_health_conditions(int day) {
  for(int condition_id = 0; condition_id < Global::Conditions.get_number_of_conditions(); ++condition_id) {
    if(this->health_condition[condition_id].state > -1) {
      Global::Conditions.get_condition(condition_id)->get_epidemic()->transition_person(this->myself, day, this->health_condition[condition_id].state);
    }    
  }  
}

