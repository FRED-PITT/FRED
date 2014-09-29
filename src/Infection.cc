/*
 This file is part of the FRED system.

 Copyright (c) 2010-2012, University of Pittsburgh, John Grefenstette,
 Shawn Brown, Roni Rosenfield, Alona Fyshe, David Galloway, Nathan
 Stone, Jay DePasse, Anuroop Sriram, and Donald Burke.

 Licensed under the BSD 3-Clause license.  See the file "LICENSE" for
 more information.
 */

// Infection.cc
// ------------
// Stores the infectivity and symptomaticity trajectories that determine the transition dates for this person.
#include <stdexcept>
#include "limits.h"
#include <map>
#include <vector>
#include <float.h>
#include <string>
#include <sstream>

#include "Infection.h"
#include "Evolution.h"
#include "Transmission.h"
#include "Global.h"
#include "Person.h"
#include "Place.h"
#include "Workplace.h"
#include "Household.h"
#include "Random.h"
#include "Disease.h"
#include "Health.h"
#include "IntraHost.h"
#include "Activities.h"
#include "Place_List.h"
#include "Utils.h"

using std::out_of_range;

Infection::Infection(Disease* disease, Person* infector, Person* host, Place* place, int day) {

  // flag for health updates
  Global::Pop.set_mask_by_index(fred::Update_Health, host->get_pop_index());

  // general
  this->disease = disease;
  this->infector = infector;
  this->host = host;
  this->place = place;
  this->trajectory = NULL;
  this->infectee_count = 0;
  this->age_at_exposure = host->get_age();

  this->is_susceptible = true;
  this->trajectory_infectivity_threshold = disease->get_infectivity_threshold();
  this->trajectory_symptomaticity_threshold = disease->get_symptomaticity_threshold();

  // parameters
  this->infectivity_multp = 1.0;
  this->infectivity = 0.0;
  this->susceptibility = 0.0;
  this->symptoms = 0.0;

  // chrono
  this->susceptibility_period = 0;
  this->latent_period = 0;
  this->asymptomatic_period = 0;
  this->symptomatic_period = 0;
  this->incubation_period = 0;

  this->susceptible_date = -1;
  this->infectious_date = -1;
  this->symptomatic_date = -1;
  this->asymptomatic_date = -1;
  this->recovery_date = -1;

  this->will_be_symptomatic = false;
  this->infection_is_fatal_today = false;

  this->exposure_date = day;
  this->recovery_period = disease->get_days_recovered();

  // Determine if this infection produces an immune response
  this->immune_response = disease->gen_immunity_infection(host->get_age());
}

Infection::~Infection() {
  delete this->trajectory;
}

void Infection::determine_transition_dates() {
  // returns the first date that the agent changes state
  bool was_latent = true;
  bool was_incubating = true;

  this->latent_period = 0;
  this->asymptomatic_period = 0;
  this->symptomatic_period = 0;
  this->incubation_period = 0;

  this->infectious_date = -1;
  this->symptomatic_date = -1;
  this->asymptomatic_date = -1;
  this->recovery_date = this->exposure_date + 366000;
  if (this->trajectory == NULL)
    return;

  this->recovery_date = this->exposure_date + this->trajectory->get_duration();
  Trajectory::iterator trj_it = Trajectory::iterator(this->trajectory);
  while(trj_it.has_next()) {
    bool infective = (trj_it.next().infectivity > this->trajectory_infectivity_threshold);
    bool symptomatic = (trj_it.next().symptomaticity > this->trajectory_symptomaticity_threshold);
    bool asymptomatic = (infective && !(symptomatic));

    if(infective && was_latent) {
      this->latent_period = trj_it.get_current();
      this->infectious_date = this->exposure_date + this->latent_period; // TODO

      if(asymptomatic & was_latent) {
        this->asymptomatic_date = this->infectious_date;
      }

      was_latent = false;
    }

    if(symptomatic && was_incubating) {
      this->incubation_period = trj_it.get_current();
      this->symptomatic_date = this->exposure_date + this->incubation_period;
      was_incubating = false;
    }

    if(symptomatic) {
      this->symptomatic_period++;
    }

    if(asymptomatic) {
      this->asymptomatic_period++;
    }
  }
  // report_infection(day);
}

void Infection::chronic_update(int today) {
  int days_post_exposure = today - this->exposure_date;
  // printf("CHRONIC day %d post %d\n", today, days_post_exposure);
  if (days_post_exposure > 3 && this->host->is_infectious(disease->get_id()) == false) {
    this->host->become_infectious(this->disease);
  }
}


void Infection::update(int today) {

  // printf("INFECTION Day %d disease_name |%s|\n", today, this->disease->get_disease_name());
  if (strcmp(this->disease->get_disease_name(), "hiv") == 0) {
    chronic_update(today);
    return;
  }

  if(this->trajectory == NULL)
    return;

  int days_post_exposure = today - this->exposure_date;

  Trajectory::point trajectory_point = this->trajectory->get_data_point(days_post_exposure);
  this->infectivity = trajectory_point.infectivity;
  this->symptoms = trajectory_point.symptomaticity;
  
  if(today == get_infectious_date()) {
    this->host->become_infectious(this->disease);
  }

  if(today == get_symptomatic_date()) {
    this->host->become_symptomatic(this->disease);
  }

  if(today == get_recovery_date()) {
    this->host->recover(this->disease);
  }

  if(today == get_unsusceptible_date()) {
    this->host->become_unsusceptible(this->disease);
    this->is_susceptible = false;
  }

  if(today != get_recovery_date()) {
    vector<int> strains;
    this->trajectory->get_all_strains(strains);
  }

  // if host is symptomatic, determine if infection is fatal today.
  // if so, set flag and terminate infection update.
  if(this->disease->is_case_fatality_enabled() && is_symptomatic()) {
    int days_symptomatic = today - this->symptomatic_date;
    if(Global::Enable_Chronic_Condition) {
      if(this->disease->is_fatal(this->host, this->symptoms, days_symptomatic)) {
        set_fatal_infection();
        return;
      }
    } else {
      if(this->disease->is_fatal(this->host->get_age(), this->symptoms, days_symptomatic)) {
        set_fatal_infection();
        return;
      }
    }
  }
}

void Infection::advance_seed_infection(int days_to_advance) {
  // only valid for seed infections!
  assert(this->recovery_date != -1);
  assert(this->exposure_date != -1);
  this->exposure_date -= days_to_advance;
  determine_transition_dates();
  if(get_infectious_date() <= 0 + Global::Epidemic_offset) {
    this->host->become_infectious(this->disease);
  }
  if(get_symptomatic_date() <= 0 + Global::Epidemic_offset) {
    this->host->become_symptomatic(this->disease);
  }
  if(get_recovery_date() <= 0 + Global::Epidemic_offset) {
    this->host->recover(this->disease);
  }
  if(get_unsusceptible_date() <= 0 + Global::Epidemic_offset) {
    this->host->become_unsusceptible(this->disease);
  }
}

void Infection::modify_symptomatic_period(double multp, int today) {
  // negative multiplier
  if(multp < 0)
    throw out_of_range("cannot modify: negative multiplier");

  // after symptomatic period
  if(today >= get_recovery_date())
    throw out_of_range("cannot modify: past symptomatic period");

  // before symptomatic period
  else if(today < get_symptomatic_date()) {
    this->trajectory->modify_symp_period(this->symptomatic_date, this->symptomatic_period * multp);
    determine_transition_dates();
  }

  // during symptomatic period
  // if days_left becomes 0, we make it 1 so that update() sees the new dates
  else {
    //int days_into = today - get_symptomatic_date();
    int days_left = get_recovery_date() - today;
    days_left *= multp;
    this->trajectory->modify_symp_period(today - this->exposure_date, days_left);
    determine_transition_dates();
  }
}

void Infection::modify_asymptomatic_period(double multp, int today) {
  // negative multiplier
  if(multp < 0)
    throw out_of_range("cannot modify: negative multiplier");

  // after asymptomatic period
  if(today >= get_symptomatic_date()) {
    printf("ERROR: Person %d %d %d\n", host->get_id(), today, get_symptomatic_date());
    throw out_of_range("cannot modify: past asymptomatic period");
  } else if(today < get_infectious_date()) { // before asymptomatic period
    this->trajectory->modify_asymp_period(this->exposure_date, this->asymptomatic_period * multp,
        get_symptomatic_date());
    determine_transition_dates();
  } else { // during asymptomatic period
    //int days_into = today - get_infectious_date();
    int days_left = get_symptomatic_date() - today;
    this->trajectory->modify_asymp_period(today - this->exposure_date, days_left * multp,
        get_symptomatic_date());
    determine_transition_dates();
  }
}

void Infection::modify_infectious_period(double multp, int today) {
  if(today < get_symptomatic_date())
    modify_asymptomatic_period(multp, today);

  modify_symptomatic_period(multp, today);
}

void Infection::modify_develops_symptoms(bool symptoms, int today) {
  if((today >= get_symptomatic_date() && get_asymptomatic_date() == -1)
      || (today >= get_recovery_date()))
    throw out_of_range("cannot modify: past symptomatic period");

  if(will_be_symptomatic != symptoms) {
    this->symptomatic_period = (this->will_be_symptomatic ? this->disease->get_days_symp() : 0);
    trajectory->modify_develops_symp(get_symptomatic_date(), symptomatic_period);
    determine_transition_dates();
  }
}

void Infection::print() const {
  printf("INF: Infection of disease type: %i in person %i "
      "periods:  latent %i, asymp: %i, symp: %i recovery: %i "
      "dates: exposed: %i, infectious: %i, symptomatic: %i, recovered: %i susceptible: %i "
      "will have symp? %i, suscept: %.3f infectivity: %.3f "
      "infectivity_multp: %.3f symptms: %.3f\n", this->disease->get_id(), this->host->get_id(),
      this->latent_period, this->asymptomatic_period, this->symptomatic_period,
      this->recovery_period, this->exposure_date, get_infectious_date(), get_symptomatic_date(),
      get_recovery_date(), get_susceptible_date(), this->will_be_symptomatic, this->susceptibility,
      this->infectivity, this->infectivity_multp, this->symptoms);
}

void Infection::transmit(Person *infectee, Transmission & transmission) {
  int day = transmission.get_exposure_date() - this->exposure_date;
  Transmission::Loads * loads = this->trajectory->getInoculum(day);
  transmission.set_initial_loads(loads);
  infectee->become_exposed(this->disease, transmission);
}

void Infection::setTrajectory(Trajectory * _trajectory) {
  this->trajectory = _trajectory;
  determine_transition_dates();
}

void Infection::report_infection(int day) const {
  if(Global::Infectionfp == NULL)
    return;

  int place_id = (this->place == NULL ? -1 : this->place->get_id());
  char place_type = (this->place == NULL ? 'X' : this->place->get_type());
  char place_subtype = 'X';
  if(this->place != NULL && this->place->is_group_quarters()) {
    if(place->is_college()) {
      place_subtype = 'D';
    }
    if(place->is_prison()) {
      place_subtype = 'J';
    }
    if(place->is_nursing_home()) {
      place_subtype = 'L';
    }
    if(place->is_military_base()) {
      place_subtype = 'B';
    }
  }
  int place_size = (this->place == NULL ? -1 : this->place->get_size());
  if(place_type == 'O' || place_type == 'C') {
    Place *container = this->place->get_container();
    place_size = container->get_size();
  }

  std::stringstream infStrS;
  infStrS.precision(3);
  infStrS << fixed << "day " << day << " dis " << disease->get_id() << " host " << host->get_id()
  << " age " << host->get_real_age() << " sick_leave " << host->is_sick_leave_available()
  << " infector " << (this->infector == NULL ? -1 : this->infector->get_id()) << " inf_age "
  << (this->infector == NULL ? -1 : this->infector->get_real_age()) << " inf_sympt "
  << (this->infector == NULL ? -1 : this->infector->is_symptomatic()) << " inf_sick_leave "
  << (this->infector == NULL ? -1 : this->infector->is_sick_leave_available())
  << " at " << place_type << " place " <<  place_id <<" subtype " << place_subtype
  << " size " << place_size << " is_teacher " << (int) host->is_teacher();

  if(Global::Track_infection_events > 1) {
    if(place_type != 'X'  && infector != NULL) {
      double host_x = host->get_x();
      double host_y = host->get_y();
      double infector_x = this->infector->get_x();
      double infector_y = this->infector->get_y();
      double distance = sqrt((host_x-infector_x)*(host_x-infector_x) +
			     (host_y-infector_y)*(host_y-infector_y));
      infStrS << " dist " << distance;
    } else {
      infStrS << " dist -1 ";
    }
    
    //Add Census Tract information. If there was no infector, censustract is -1
    if(this->infector == NULL) {
      infStrS << " census_tract -1";
    } else {
      Household* hh = (Household*)this->infector->get_household();
      if(hh == NULL) {
        hh = (Household*)this->infector->get_permanent_household();
      }
      int census_tract_index = (hh == NULL ? -1 : hh->get_census_tract_index());
      long int census_tract = (census_tract_index == -1 ? -1 : Global::Places.get_census_tract_with_index(census_tract_index));
      infStrS << " census_tract " << census_tract;
    }
    
    infStrS << "| PERIODS  latent " << this->latent_period << " asymp " << this->asymptomatic_period
	    << " symp " << this->symptomatic_period << " recovery " << this->recovery_period;
  }

  if(Global::Track_infection_events > 2)
    infStrS << "| DATES exp " << this->exposure_date << " inf " << get_infectious_date() << " symp "
        << get_symptomatic_date() << " rec " << get_recovery_date() << " sus "
        << get_susceptible_date();

  if(Global::Track_infection_events > 3)
    infStrS << "| will_be_symp? " << this->will_be_symptomatic << " sucs " << this->susceptibility
        << " infect " << this->infectivity << " inf_multp " << infectivity_multp << " sympts "
        << this->symptoms;

  infStrS << "\n";

  fprintf(Global::Infectionfp, "%s", infStrS.str().c_str());
  // flush performed at the end of every day so that it doesn't gum up multithreading
  //fflush(Global::Infectionfp);
}

int Infection::get_num_past_infections() {
  return this->host->get_num_past_infections(this->disease->get_id());
}

Past_Infection *Infection::get_past_infection(int i) {
  return this->host->get_past_infection(this->disease->get_id(), i);
}
