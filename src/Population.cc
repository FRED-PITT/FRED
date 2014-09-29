/*
 This file is part of the FRED system.

 Copyright (c) 2010-2012, University of Pittsburgh, John Grefenstette,
 Shawn Brown, Roni Rosenfield, Alona Fyshe, David Galloway, Nathan
 Stone, Jay DePasse, Anuroop Sriram, and Donald Burke.

 Licensed under the BSD 3-Clause license.  See the file "LICENSE" for
 more information.
 */

//
//
// File: Population.cc
//

#include "Population.h"
#include "Params.h"
#include "Global.h"
#include "Place_List.h"
#include "Household.h"
#include "Disease.h"
#include "Person.h"
#include "Demographics.h"
#include "Manager.h"
#include "AV_Manager.h"
#include "Vaccine_Manager.h"
#include "Age_Map.h"
#include "Random.h"
#include "Date.h"
#include "Travel.h"
#include "Utils.h"
#include "Past_Infection.h"
#include "Evolution.h"
#include "Activities.h"
#include "Behavior.h"
#include "Geo_Utils.h"
#include <unistd.h>

#if SNAPPY
#include "Compression.h"
#endif

// used for reporting
int age_count_male[Demographics::MAX_AGE + 1];
int age_count_female[Demographics::MAX_AGE + 1];
int birth_count[Demographics::MAX_AGE + 1];
int death_count_male[Demographics::MAX_AGE + 1];
int death_count_female[Demographics::MAX_AGE + 1];

char Population::pop_outfile[FRED_STRING_SIZE];
char Population::output_population_date_match[FRED_STRING_SIZE];
int Population::output_population = 0;
bool Population::is_initialized = false;
int Population::next_id = 0;

Population::Population() {

  clear_static_arrays();
  this->birthday_map.clear();
  this->pop_size = 0;
  this->disease = NULL;
  this->av_manager = NULL;
  this->vacc_manager = NULL;

  for(int i = 0; i < 367; ++i) {
    this->birthday_vecs[i].clear();
  }
}

void Population::initialize_masks() {
  // can't do this in the constructor because the Global:: variables aren't yet
  // available when the Global::Pop is defined
  this->blq.add_mask(fred::Infectious);
  this->blq.add_mask(fred::Susceptible);
  this->blq.add_mask(fred::Update_Deaths);
  this->blq.add_mask(fred::Update_Births);
  this->blq.add_mask(fred::Update_Health);
  this->blq.add_mask(fred::Travel);
}

// index and id are not the same thing!
Person * Population::get_person_by_index(int _index) {
  if(this->blq.is_valid_index(_index)) {
    return this->blq.get_item_pointer_by_index(_index);
  } else {
    return NULL;
  }
}

//Person * Population::get_person_by_id( int _id ) {
//  return blq.get_item_pointer_by_index( id_to_index[ _id ] ); 
//}

Population::~Population() {
  // free all memory
  this->pop_size = 0;
  if(this->disease != NULL)
    delete[] this->disease;
  if(this->vacc_manager != NULL)
    delete this->vacc_manager;
  if(this->av_manager != NULL)
    delete this->av_manager;
}

void Population::get_parameters() {

  // Only do this one time
  if(!Population::is_initialized) {
    Params::get_param_from_string("enable_copy_files", &this->enable_copy_files);
    Params::get_param_from_string("output_population", &Population::output_population);
    if(Population::output_population > 0) {
      Params::get_param_from_string("pop_outfile", Population::pop_outfile);
      Params::get_param_from_string("output_population_date_match",
          Population::output_population_date_match);
    }
    this->disease = new Disease[Global::Diseases];
    for(int d = 0; d < Global::Diseases; d++) {
      this->disease[d].get_parameters(d);
    }
    Population::is_initialized = true;
  }
}

/*
 * All Persons in the population must have been created using add_person
 */
Person * Population::add_person(int age, char sex, int race, int rel, Place *house, Place *school,
    Place *work, int day, bool today_is_birthday) {

  fred::Scoped_Lock lock(this->add_person_mutex);

  int id = Population::next_id++;
  int idx = this->blq.get_free_index();

  Person * person = this->blq.get_free_pointer(idx);

  // mark valid before adding person so that mask operations will be
  // available in the constructor (of Person and all ancillary objects)
  this->blq.mark_valid_by_index(idx);

  new (person) Person();

  person->setup(idx, id, age, sex, race, rel, house, school, work, day, today_is_birthday);

  //assert( id_to_index.find( id ) == id_to_index.end() );
  //id_to_index[ id ] = idx;

  assert((unsigned ) this->pop_size == blq.size() - 1);
  this->pop_size = blq.size();

  if(Global::Enable_Population_Dynamics) {
    Demographics * demographics = person->get_demographics();
    int pos = demographics->get_birth_day_of_year();
    //Check to see if the day of the year is after FEB 28
    if(pos > 59 && !Date::is_leap_year(demographics->get_birth_year())) {
      pos++;
    }
    this->birthday_vecs[pos].push_back(person);
    FRED_VERBOSE(2,
        "Adding person %d to birthday vector for day = %d.\n ... birthday_vecs[ %d ].size() = %zu\n",
        id, pos, pos, birthday_vecs[ pos ].size());
    this->birthday_map[person] = ((int)this->birthday_vecs[pos].size() - 1);
  }
  return person;
}

void Population::set_mask_by_index(fred::Pop_Masks mask, int person_index) {
  // assert that the mask has in fact been added
  this->blq.set_mask_by_index(mask, person_index);
}

void Population::clear_mask_by_index(fred::Pop_Masks mask, int person_index) {
  // assert that the mask has in fact been added
  this->blq.clear_mask_by_index(mask, person_index);
}

void Population::delete_person(Person * person) {
  FRED_VERBOSE(1, "DELETING PERSON: %d ...\n", person->get_id());

  person->terminate();
  FRED_VERBOSE(1, "DELETED PERSON: %d\n", person->get_id());

  for(int d = 0; d < Global::Diseases; d++) {
    this->disease[d].get_evolution()->terminate_person(person);
  }

  if(Global::Enable_Travel) {
    Travel::terminate_person(person);
  }

  int idx = person->get_pop_index();
  assert(get_person_by_index(idx) == person);
  // call Person's destructor directly!!!
  get_person_by_index(idx)->~Person();
  this->blq.mark_invalid_by_index(person->get_pop_index());

  this->pop_size--;
  assert((unsigned ) this->pop_size == this->blq.size());
}

void Population::prepare_to_die(int day, int person_index) {
  Person * person = get_person_by_index(person_index);
  fred::Scoped_Lock lock(this->mutex);
  // add person to daily death_list
  this->death_list.push_back(person);
  report_death(day, person);
  // you'll be stone dead in a moment...
  person->die();
  if(Global::Verbose > 1) {
    printf("popsize = %d\n", this->pop_size);
    fprintf(Global::Statusfp, "prepare to die: ");
    person->print(Global::Statusfp, 0);
    printf("popsize = %d\n", this->pop_size);
  }
}

void Population::prepare_to_give_birth(int day, int person_index) {
  Person * person = get_person_by_index(person_index);
  fred::Scoped_Lock lock(this->mutex);
  // add person to daily maternity_list
  this->maternity_list.push_back(person);
  report_birth(day, person);
  if(Global::Verbose > 1) {
    fprintf(Global::Statusfp, "prepare to give birth: ");
    person->print(Global::Statusfp, 0);
  }
}

void Population::setup() {
  FRED_STATUS(0, "setup population entered\n", "");

  for(int d = 0; d < Global::Diseases; d++) {
    this->disease[d].setup(this);
  }

  if(Global::Enable_Vaccination) {
    this->vacc_manager = new Vaccine_Manager(this);
  } else {
    this->vacc_manager = new Vaccine_Manager();
  }

  if(Global::Enable_Antivirals) {
    this->av_manager = new AV_Manager(this);
  } else {
    this->av_manager = new AV_Manager();
  }

  if(Global::Verbose > 1)
    this->av_manager->print();

  // TODO provide clear() method for bloque
  //id_to_index.clear();
  this->pop_size = 0;
  this->maternity_list.clear();
  this->death_list.clear();
  read_all_populations();
  Demographics::set_initial_popsize(this->pop_size);

  if(Global::Verbose > 0) {
    for(int d = 0; d < Global::Diseases; d++) {
      int count = 0;
      for(int i = 0; i < this->get_index_size(); i++) {
	if (get_person_by_index(i) == NULL) continue;
	Disease * dis = &this->disease[d];
	if(get_person_by_index(i)->is_immune(dis)) {
	  count++;
	}
      }
      FRED_STATUS(0, "number of residually immune people for disease %d = %d\n", d, count);
    }
  }
  this->av_manager->reset();
  this->vacc_manager->reset();

  FRED_STATUS(0, "population setup finished\n", "");
}

Person_Init_Data Population::get_person_init_data(char * line,
						  bool is_group_quarters_population,
						  bool is_2010_ver1_format) {
  char newline[1024];
  Utils::replace_csv_missing_data(newline, line, "-1");
  Utils::Tokens tokens = Utils::split_by_delim(newline, ',', false);
  const PopFileColIndex & col = get_pop_file_col_index(is_group_quarters_population, is_2010_ver1_format);
  assert(int(tokens.size()) == col.number_of_columns);
  // initialized with default values
  Person_Init_Data pid = Person_Init_Data();
  strcpy(pid.label, tokens[col.p_id]);
  // add type indicator to label for places
  if(is_group_quarters_population) {
    pid.in_grp_qrtrs = true;
    sscanf(tokens[col.gq_type], "%c", &pid.gq_type);
    // columns present in group quarters population
    if(strcmp(tokens[col.home_id], "-1")) {
      sprintf(pid.house_label, "H%s", tokens[col.home_id]);
    }
    if(strcmp(tokens[col.workplace_id], "-1")) {
      sprintf(pid.work_label, "W%s", tokens[col.workplace_id]);
    }
    // printf("GQ person %s house %s work %s\n", pid.label, pid.house_label, pid.work_label);
  } else {
    // columns not present in group quarters population
    sscanf(tokens[col.relate], "%d", &pid.relationship);
    sscanf(tokens[col.race_str], "%d", &pid.race);
    // schools only defined for synth_people
    if(strcmp(tokens[col.school_id], "-1")) {
      sprintf(pid.school_label, "S%s", tokens[col.school_id]);
    }
    // standard formatting for house and workplace labels
    if(strcmp(tokens[col.home_id], "-1")) {
      sprintf(pid.house_label, "H%s", tokens[col.home_id]);
    }
    if(strcmp(tokens[col.workplace_id], "-1")) {
      sprintf(pid.work_label, "W%s", tokens[col.workplace_id]);
    }
  }
  // age, sex same for synth_people and synth_gq_people
  sscanf(tokens[col.age_str], "%d", &pid.age);
  pid.sex = strcmp(tokens[col.sex_str], "1") == 0 ? 'M' : 'F';
  // set pointer to primary places in init data object
  pid.house = Global::Places.get_place_from_label(pid.house_label);
  pid.work =  Global::Places.get_place_from_label(pid.work_label);
  pid.school = Global::Places.get_place_from_label(pid.school_label);
  // warn if we can't find workplace
  if(strcmp(pid.work_label, "-1") != 0 && pid.work == NULL) {
    FRED_VERBOSE(2, "WARNING: person %s -- no workplace found for label = %s\n", pid.label,
        pid.work_label);
    if(Global::Enable_Local_Workplace_Assignment) {
      pid.work = Global::Places.get_random_workplace();
      FRED_CONDITIONAL_VERBOSE(0, pid.work != NULL, "WARNING: person %s assigned to workplace %s\n",
          pid.label, pid.work->get_label());
      FRED_CONDITIONAL_VERBOSE(0, pid.work == NULL,
          "WARNING: no workplace available for person %s\n", pid.label);
    }
  }
  // warn if we can't find school.  No school for gq_people
  FRED_CONDITIONAL_VERBOSE(0, (strcmp(pid.school_label,"-1")!=0 && pid.school == NULL),
      "WARNING: person %s -- no school found for label = %s\n", pid.label, pid.school_label);

  return pid;
}

void Population::parse_lines_from_stream(std::istream & stream, bool is_group_quarters_pop) {

  // vector used for batch add of new persons
  std::vector<Person_Init_Data> pidv;
  pidv.reserve(2000000);

  // flag for 2010_ver1 format
  bool is_2010_ver1_format = false;

  while(stream.good()) {
    char line[FRED_STRING_SIZE];
    stream.getline(line, FRED_STRING_SIZE);

    // check for 2010_ver1 format
    if(strncmp(line, "sp_id", 5) == 0) {
      is_2010_ver1_format = true;
      continue;
    }

    // skip empty lines...
    if((line[0] == '\0') || strncmp(line, "p_id", 4) == 0 || strncmp(line, "sp_id", 5) == 0)
      continue;

    //printf("line: |%s|\n", line); fflush(stdout); // exit(0);
    const Person_Init_Data & pid = get_person_init_data(line,
							is_group_quarters_pop,
							is_2010_ver1_format);

    // verbose printing of all person initialization data
    if (Global::Verbose > 1) {
      FRED_VERBOSE(1, "%s\n", pid.to_string().c_str());
    }

    //skip header line
    if(strcmp(pid.label, "p_id") == 0)
      continue;
    /*
     printf("|%s %d %c %d %s %s %s %d|\n", label, age, sex, race
     house_label, work_label, school_label, relationship);
     fflush(stdout);

     if (strcmp(work_label,"-1") && strcmp(school_label,"-1")) {
     printf("STUDENT-WORKER: %s %d %c %s %s\n", label, age, sex, work_label, school_label);
     fflush(stdout);
     }
     */
    if(pid.house != NULL) {
      // create a Person_Init_Data object
      pidv.push_back(pid);
    } else {
      // we need at least a household (homeless people not yet supported), so
      // skip this person
      FRED_VERBOSE(0, "WARNING: skipping person %s -- %s %s\n", pid.label,
          "no household found for label =", pid.house_label);
    }
  } // <----- end while loop over stream

  // Iterate through vector of already parsed initialization data and
  // add to population bloque.  More efficient to do this in batches; also
  // preserves the (fine-grained) order in the population file.  Protect
  // with mutex so that we do this sequentially and avoid thrashing the 
  // scoped mutex in add_person.
  fred::Scoped_Lock lock(this->batch_add_person_mutex);
  std::vector<Person_Init_Data>::iterator it = pidv.begin();
  for(; it != pidv.end(); ++it) {
    Person_Init_Data & pid = *it;
    // here the person is actually created and added to the population
    // The person's unique id is automatically assigned
    add_person(pid.age, pid.sex, pid.race, pid.relationship, pid.house, pid.school, pid.work,
        pid.day, pid.today_is_birthday);
  }
}

void Population::split_synthetic_populations_by_deme() {
  using namespace std;
  using namespace Utils;
  const char delim = ' ';

  FRED_STATUS(0, "Validating synthetic population identifiers before reading.\n", "");
  const char * pop_dir = Global::Synthetic_population_directory;
  FRED_STATUS(0, "Using population directory: %s\n", pop_dir);
  FRED_STATUS(0, "FRED defines a \"deme\" to be a local population %s\n%s%s\n",
      "of people whose households are contained in the same bounded geographic region.",
      "No Synthetic Population ID may have more than one Deme ID, but a Deme ID may ",
      "contain many Synthetic Population IDs.");

  int param_num_demes = 1;
  Params::get_param_from_string("num_demes", &param_num_demes);
  assert(param_num_demes > 0);
  this->demes.clear();

  std::set<std::string> pop_id_set;

  for(int d = 0; d < param_num_demes; ++d) {
    // allow up to 195 (county) population ids per deme
    // TODO set this limit in param file / Global.h.
    char pop_id_string[4096];
    std::stringstream ss;
    ss << "synthetic_population_id[" << d << "]";

    if(Params::does_param_exist(ss.str())) {
      Params::get_indexed_param("synthetic_population_id", d, pop_id_string);
    } else {
      if(d == 0) {
        strcpy(pop_id_string, Global::Synthetic_population_id);
      } else {
        Utils::fred_abort("Help! %d %s %d %s %d %s\n", param_num_demes,
            "demes were requested (param_num_demes = ", param_num_demes,
            "), but indexed paramater synthetic_population_id[", d, "] was not found!");
      }
    }
    this->demes.push_back(split_by_delim(pop_id_string, delim));
    FRED_STATUS(0, "Split ID string \"%s\" into %d %s using delimiter: \'%c\'\n", pop_id_string,
        int(this->demes[d].size()), this->demes[d].size() > 1 ? "tokens" : "token", delim);
    assert(this->demes.size() > 0);
    // only allow up to 255 demes
    assert(this->demes.size() <= std::numeric_limits<unsigned char>::max());
    FRED_STATUS(0, "Deme %d comprises %d synthetic population id%s:\n", d, this->demes[d].size(),
        this->demes[d].size() > 1 ? "s" : "");
    assert(this->demes[d].size() > 0);
    for(int i = 0; i < this->demes[d].size(); ++i) {
      FRED_CONDITIONAL_WARNING(pop_id_set.find(this->demes[d][i] ) != pop_id_set.end(),
          "%s %s %s %s\n", "Population ID ", this->demes[d][i], "was specified more than once!",
          "Population IDs must be unique across all Demes!");
      assert(pop_id_set.find(this->demes[d][i]) == pop_id_set.end());
      pop_id_set.insert(this->demes[d][i]);
      FRED_STATUS(0, "--> Deme %d, Synth. Pop. ID %d: %s\n", d, i, this->demes[d][i]);
    }
  }
}

void Population::read_all_populations() {
  using namespace std;
  using namespace Utils;
  const char * pop_dir = Global::Synthetic_population_directory;
  assert(this->demes.size() > 0);
  for(int d = 0; d < this->demes.size(); ++d) {
    FRED_STATUS(0, "Loading population for Deme %d:\n", d);
    assert(this->demes[d].size() > 0);
    for(int i = 0; i < this->demes[d].size(); ++i) {
      FRED_STATUS(0, "Loading population %d for Deme %d: %s\n", i, d, this->demes[d][i]);
      // o---------------------------------------- Call read_population to actually
      // |                                         read the population files
      // V
      read_population(pop_dir, this->demes[d][i], "people");
      if(Global::Enable_Group_Quarters) {
        FRED_STATUS(0, "Loading group quarters population %d for Deme %d: %s\n", i, d,
            this->demes[ d ][ i ]);
        // o---------------------------------------- Call read_population to actually
        // |                                         read the population files
        // V
        read_population(pop_dir, this->demes[d][i], "gq_people");
      }
    }
  }
  // report on time take to read populations
  Utils::fred_print_lap_time("reading populations");

  // select adult to make health decisions
  Setup_Population_Behavior setup_population_behavior;
  this->blq.apply(setup_population_behavior);
}

void Population::Setup_Population_Behavior::operator()(Person & p) {
  p.setup_behavior();
}

void Population::read_population(const char * pop_dir, const char * pop_id, const char * pop_type) {

  FRED_STATUS(0, "read population entered\n");

  char cmd[1024];
  char line[1024];
  char * pop_file;
  char population_file[1024];
  char temp_file[256];
  if(getenv("SCRATCH_RAMDISK") != NULL) {
    sprintf(temp_file, "%s/temp_file-%d-%d",
	    getenv("SCRATCH_RAMDISK"),
	    (int)getpid(),
	    Global::Simulation_run_number);
  } else {
    sprintf(temp_file, "./temp_file-%d-%d",
	    (int)getpid(),
	    Global::Simulation_run_number);
  }

  bool is_group_quarters_pop = strcmp(pop_type, "gq_people") == 0 ? true : false;
  FILE *fp = NULL;

#if SNAPPY

  // try to open compressed population file
  sprintf(population_file, "%s/%s/%s_synth_%s.txt.fsz", pop_dir, pop_id, pop_id, pop_type);
  
  fp = Utils::fred_open_file(population_file);
  // fclose(fp); fp = NULL; // TEST
  if(fp != NULL) {
    fclose(fp);
    if(this->enable_copy_files) {
      sprintf(cmd, "cp %s %s", population_file, temp_file);
      printf("COPY_FILE: %s\n", cmd);
      fflush(stdout);
      system(cmd);
      pop_file = temp_file;
    } else {
      pop_file = population_file;
    }

    FRED_VERBOSE(0, "calling SnappyFileCompression on pop_file = %s\n", pop_file);
    SnappyFileCompression compressor = SnappyFileCompression(pop_file);
    compressor.init_compressed_block_reader();
    // if we have the magic, then it must be fsz block compressed
    if(compressor.check_magic_bytes()) {
      // limit to two threads to prevent locking and I/O overhead; also
      // helps to preserve population order in bloque assignment
#pragma omp parallel
      {
        std::stringstream stream;
        while(compressor.load_next_block_stream(stream)) {
          parse_lines_from_stream(stream, is_group_quarters_pop);
        }
      }
    }
    if(this->enable_copy_files) {
      unlink(temp_file);
    }
    FRED_VERBOSE(0, "finished reading compressed population, pop_size = %d\n", pop_size);
    return;
  }
#endif

  // try to read the uncompressed file
  sprintf(population_file, "%s/%s/%s_synth_%s.txt", pop_dir, pop_id, pop_id, pop_type);
  fp = Utils::fred_open_file(population_file);
  if(fp == NULL) {
    Utils::fred_abort("population_file %s not found\n", population_file);
  }
  fclose(fp);
  if(this->enable_copy_files) {
    sprintf(cmd, "cp %s %s", population_file, temp_file);
    printf("COPY_FILE: %s\n", cmd);
    fflush(stdout);
    system(cmd);
    // printf("copy finished\n"); fflush(stdout);
    pop_file = temp_file;
  } else {
    pop_file = population_file;
  }
  std::ifstream stream(pop_file, ifstream::in);
  parse_lines_from_stream(stream, is_group_quarters_pop);
  if(this->enable_copy_files) {
    unlink(temp_file);
  }
  FRED_VERBOSE(0, "finished reading uncompressed population, pop_size = %d\n", pop_size);

}

void Population::update(int day) {

  // clear lists of births and deaths
  this->death_list.clear();
  this->maternity_list.clear();

  if(Global::Enable_Population_Dynamics) {

    //Find out if we are currently in a leap year
    int year = Global::Sim_Start_Date->get_year(day);
    int day_of_year = Date::get_day_of_year(year, Global::Sim_Start_Date->get_month(day),
        Global::Sim_Start_Date->get_day_of_month(day));

    int bd_count = 0;
    size_t vec_size = 0;

    FRED_VERBOSE(1, "day_of_year = [%d]\n", day_of_year);

    bool is_leap = Date::is_leap_year(year);

    FRED_VERBOSE(2, "Day: %d, Year: %d, is_leap: %d\n", day_of_year, year, is_leap);

    // All birthdays except Feb. 29 ( unless in leap year ) 
    if(day_of_year != 60 || is_leap) {
      for(size_t p = 0; p < this->birthday_vecs[day_of_year].size(); p++) {
        this->birthday_vecs[day_of_year][p]->birthday(day);
        bd_count++;
      }
    }

    //If we are NOT in a leap year, then we need to do all of the day 60 (feb 29) birthdays on day 61
    if(!is_leap && day_of_year == 61) {
      for(size_t p = 0; p < this->birthday_vecs[60].size(); p++) {
        this->birthday_vecs[60][p]->birthday(day);
        bd_count++;
      }
    }
    FRED_VERBOSE(1, "birthday count = [%d]\n", bd_count);
  }

  if(Global::Enable_Population_Dynamics) {
    // populate the maternity list (Demographics::update_births)
    Update_Population_Births update_population_births(day);
    this->blq.parallel_masked_apply(fred::Update_Births, update_population_births);
    // add the births to the population
    size_t births = this->maternity_list.size();
    for(size_t i = 0; i < births; i++) {
      Person * mother = this->maternity_list[i];
      Person * baby = mother->give_birth(day);

      if(Global::Enable_Behaviors) {
        // turn mother into an adult decision maker, if not already
        if(mother->is_health_decision_maker() == false) {
          FRED_VERBOSE(0, "young mother %d age %d becomes baby's health decision maker on day %d\n",
              mother->get_id(), mother->get_age(), day);
          mother->become_health_decision_maker(mother);
        }
        // let mother decide health behaviors for child
        baby->set_health_decision_maker(mother);
      }

      if(this->vacc_manager->do_vaccination()) {
        FRED_DEBUG(1, "Adding %d to Vaccine Queue\n", baby->get_id());
        this->vacc_manager->add_to_queue(baby);
      }
      int age_lookup = mother->get_age();
      if(age_lookup > Demographics::MAX_AGE)
        age_lookup = Demographics::MAX_AGE;
      birth_count[age_lookup]++;
    }
    FRED_STATUS(1, "births = %d pop_size = %d \n", (int) births, this->pop_size);
  }

  if(Global::Enable_Population_Dynamics) {
    // populate the death list (Demographics::update_deaths)
    Update_Population_Deaths update_population_deaths(day);
    this->blq.parallel_masked_apply(fred::Update_Deaths, update_population_deaths);

    size_t deaths = this->death_list.size();
    remove_dead_from_population(day);
    FRED_STATUS(1, "non-disease related deaths = %d pop_size = %d\n", (int) deaths, this->pop_size);
  }

  // first update everyone's health intervention status
  if(Global::Enable_Vaccination || Global::Enable_Antivirals) {
    Update_Health_Interventions update_health_interventions(day);
    this->blq.apply(update_health_interventions);
  }

  FRED_VERBOSE(1, "population::update health  day = %d\n", day);

  // update everyone's health status
  Update_Population_Health update_population_health(day);
  this->blq.parallel_masked_apply(fred::Update_Health, update_population_health);
  // Utils::fred_print_wall_time("day %d update_health", day);

  // handle deaths from disease
  size_t deaths = this->death_list.size();
  remove_dead_from_population(day);
  FRED_STATUS(1, "disease-related deaths = %d pop_size = %d\n", (int) deaths, this->pop_size);

  FRED_VERBOSE(1, "population::update prepare activities day = %d\n", day);

  // prepare Activities at start up
  if(day == 0) {
    Prepare_Population_Activities prepare_population_activities(day);
    this->blq.apply(prepare_population_activities);
    Activities::before_run();
  }

  // update activity profiles on July 1
  if(Global::Enable_Population_Dynamics && Date::simulation_date_matches_pattern("07-01-*")) {
    FRED_VERBOSE(0, "population::update_activity_profile day = %d\n", day);
    Update_Population_Activities update_population_activities(day);
    this->blq.apply(update_population_activities);
  }

  FRED_VERBOSE(1, "population::update_travel day = %d\n", day);

  // update travel decisions
  Travel::update_travel(day);
  // Utils::fred_print_wall_time("day %d update_travel", day);

  FRED_VERBOSE(1, "population::update_behavior day = %d\n", day);

  // update decisions about behaviors
  Update_Population_Behaviors update_population_behaviors(day);
  this->blq.apply(update_population_behaviors);
  // Utils::fred_print_wall_time("day %d update_behavior", day);

  FRED_VERBOSE(1, "population::update vacc_manager day = %d\n", day);

  // distribute vaccines
  this->vacc_manager->update(day);
  // Utils::fred_print_wall_time("day %d vacc_manager", day);

  FRED_VERBOSE(1, "population::update av_manager day = %d\n", day);

  // distribute AVs
  this->av_manager->update(day);
  // Utils::fred_print_wall_time("day %d av_manager", day);

  FRED_STATUS(1, "population begin_day finished, pop_size = %d\n", this->pop_size);
}

void Population::remove_dead_from_population(int day) {
  size_t deaths = this->death_list.size();
  for(size_t i = 0; i < deaths; i++) {
    // For reporting
    int age_lookup = this->death_list[i]->get_age();
    if(age_lookup > Demographics::MAX_AGE)
      age_lookup = Demographics::MAX_AGE;
    if(this->death_list[i]->get_sex() == 'F') {
      death_count_female[age_lookup]++;
    } else {
      death_count_male[age_lookup]++;
    }

    if(this->vacc_manager->do_vaccination()) {
      FRED_DEBUG(1, "Removing %d from Vaccine Queue\n", death_list[ i ]->get_id());
      this->vacc_manager->remove_from_queue(this->death_list[i]);
    }

    // Remove the person from the birthday lists
    if(Global::Enable_Population_Dynamics) {
      map<Person *, int>::iterator itr;
      itr = this->birthday_map.find(this->death_list[i]);
      if(itr == this->birthday_map.end()) {
        FRED_VERBOSE(0, "Help! person %d deleted, but not in the birthday_map\n",
            death_list[ i ]->get_id());
      }
      assert(itr != this->birthday_map.end());
      int pos = (*itr).second;
      int day_of_year = this->death_list[i]->get_birth_day_of_year();

      // Check to see if the day of the year is after FEB 28
      // and in a leap year, if so, increment position in birthday vector
      if(day_of_year > 59 && !Date::is_leap_year(this->death_list[i]->get_birth_year()))
        day_of_year++;

      Person * last = this->birthday_vecs[day_of_year].back();
      this->birthday_map.erase(itr);
      this->birthday_map[last] = pos;

      this->birthday_vecs[day_of_year][pos] = this->birthday_vecs[day_of_year].back();
      this->birthday_vecs[day_of_year].pop_back();
    }

    // finally, 
    delete_person(this->death_list[i]);
  }
  // clear the death list
  this->death_list.clear();
}

void Population::Update_Population_Births::operator() (Person & p) {
  p.update_births(this->day);
}

void Population::Update_Population_Deaths::operator() (Person & p) {
  p.update_deaths(this->day);
}

void Population::Update_Health_Interventions::operator() (Person & p) {
  p.update_health_interventions(this->day);
}

void Population::Update_Population_Health::operator() (Person & p) {
  p.update_health(this->day);
}

void Population::Prepare_Population_Activities::operator()(Person & p) {
  p.prepare_activities();
}

void Population::Update_Population_Activities::operator() (Person & p) {
  p.update_activity_profile();
}

void Population::Update_Population_Behaviors::operator() (Person & p) {
  p.update_behavior(this->day);
}

void Population::report(int day) {

  if (Global::Enable_Household_Isolation) {
    // update infection counters for places
    for(int d = 0; d < Global::Diseases; d++) {
      for(int i = 0; i < this->get_index_size(); ++i) {
	Person * person = get_person_by_index(i);
	if (person == NULL) continue;
	if(person->is_infected(d)) {
	  
	  // Update the infection counters for household
	  person->update_household_counts(day, d);
	  
	  // Update the infection counters for schools
	  if(person->get_school() != NULL)
	    person->update_school_counts(day, d);
	}
      }
    }
  }

  // give out anti-virals (after today's infections)
  this->av_manager->disseminate(day);

  if(Global::Verbose > 0 && Date::simulation_date_matches_pattern("12-31-*")) {
    // print the statistics on December 31 of each year
    for(int i = 0; i < this->get_index_size(); ++i) {
      Person * person = get_person_by_index(i);
      if (person == NULL) continue;
      int age_lookup = person->get_age();
      if(age_lookup > Demographics::MAX_AGE)
        age_lookup = Demographics::MAX_AGE;
      if(person->get_sex() == 'F')
        age_count_female[age_lookup]++;
      else
        age_count_male[age_lookup]++;
    }

    for(int i = 0; i <= Demographics::MAX_AGE; ++i) {
      int count, num_deaths, num_births;
      double birthrate, deathrate;
      FRED_VERBOSE(1,"DEMOGRAPHICS Year %d TotalPop %d Age %d ",
          Global::Sim_Current_Date->get_year(), this->pop_size, i);
      count = age_count_female[i];
      num_births = birth_count[i];
      num_deaths = death_count_female[i];
      birthrate = count > 0 ? ((100.0 * num_births) / count) : 0.0;
      deathrate = count > 0 ? ((100.0 * num_deaths) / count) : 0.0;
      FRED_VERBOSE(1,
		   "count_f %d births_f %d birthrate_f %.03f deaths_f %d deathrate_f %.03f ", count,
		   num_births, birthrate, num_deaths, deathrate);
      count = age_count_male[i];
      num_deaths = death_count_male[i];
      deathrate = count ? ((100.0 * num_deaths) / count) : 0.0;
      FRED_VERBOSE(1, "count_m %d deaths_m %d deathrate_m %.03f\n", count, num_deaths,
          deathrate);
    }
    clear_static_arrays();
  }

  for(int d = 0; d < Global::Diseases; d++) {
    this->disease[d].print_stats(day);
  }

  // Write out the population if the output_population parameter is set.
  // Will write only on the first day of the simulation, on days
  // matching the date pattern in the parameter file, and the on
  // the last day of the simulation
  if(Population::output_population > 0) {
    if((day == 0)
        || (Date::simulation_date_matches_pattern(Population::output_population_date_match))) {
      this->write_population_output_file(day);
    }
  }
}

void Population::end_of_run() {
  // Write the population to the output file if the parameter is set
  // Will write only on the first day of the simulation, days matching
  // the date pattern in the parameter file, and the last day of the
  // simulation
  if(Population::output_population > 0) {
    this->write_population_output_file(Global::Days);
  }
}

Disease *Population::get_disease(int disease_id) {
  return &this->disease[disease_id];
}

void Population::quality_control() {
  if(Global::Verbose > 0) {
    fprintf(Global::Statusfp, "population quality control check\n");
    fflush(Global::Statusfp);
  }

  // check population
  for(int p = 0; p < this->get_index_size(); p++) {
    Person * person = get_person_by_index(p);
    if(person == NULL) continue;
    if(person->get_household() == NULL) {
      fprintf(Global::Statusfp, "Help! Person %d has no home.\n", person->get_id());
      person->print(Global::Statusfp, 0);
    }
  }

  if(Global::Verbose > 0) {
    int n0, n5, n18, n65;
    int count[20];
    int total = 0;
    n0 = n5 = n18 = n65 = 0;
    // age distribution
    for(int c = 0; c < 20; c++) {
      count[c] = 0;
    }
    for(int p = 0; p < this->get_index_size(); p++) {
      Person * person = get_person_by_index(p);
      if (person == NULL) continue;
      int a = person->get_age();

      if(a < 5) {
        n0++;
      } else if(a < 18) {
        n5++;
      } else if(a < 65) {
        n18++;
      } else {
        n65++;
      }
      int n = a / 5;
      if(n < 20) {
        count[n]++;
      } else {
        count[19]++;
      }
      total++;
    }
    fprintf(Global::Statusfp, "\nAge distribution: %d people\n", total);
    for(int c = 0; c < 20; c++) {
      fprintf(Global::Statusfp, "age %2d to %d: %6d (%.2f%%)\n", 5 * c, 5 * (c + 1) - 1, count[c],
          (100.0 * count[c]) / total);
    }
    fprintf(Global::Statusfp, "AGE 0-4: %d %.2f%%\n", n0, (100.0 * n0) / total);
    fprintf(Global::Statusfp, "AGE 5-17: %d %.2f%%\n", n5, (100.0 * n5) / total);
    fprintf(Global::Statusfp, "AGE 18-64: %d %.2f%%\n", n18, (100.0 * n18) / total);
    fprintf(Global::Statusfp, "AGE 65-100: %d %.2f%%\n", n65, (100.0 * n65) / total);
    fprintf(Global::Statusfp, "\n");

    // Print out At Risk distribution
    for(int d = 0; d < Global::Diseases; d++) {
      if(this->disease[d].get_at_risk()->get_num_ages() > 0) {
        Disease* dis = &this->disease[d];
        int rcount[20];
        for(int c = 0; c < 20; c++) {
          rcount[c] = 0;
        }
        for(int p = 0; p < this->get_index_size(); p++) {
	  Person * person = get_person_by_index(p);
	  if (person == NULL) continue;
          int a = person->get_age();
          int n = a / 10;
          if(person->get_health()->is_at_risk(dis) == true) {
            if(n < 20) {
              rcount[n]++;
            } else {
              rcount[19]++;
            }
          }
        }
        fprintf(Global::Statusfp, "\n Age Distribution of At Risk for Disease %d: %d people\n", d,
            total);
        for(int c = 0; c < 10; c++) {
          fprintf(Global::Statusfp, "age %2d to %2d: %6d (%.2f%%)\n", 10 * c, 10 * (c + 1) - 1,
              rcount[c], (100.0 * rcount[c]) / total);
        }
        fprintf(Global::Statusfp, "\n");
      }
    }
  }
  FRED_STATUS(0, "population quality control finished\n");
}

//Static function to clear arrays
void Population::clear_static_arrays() {
  for(int i = 0; i <= Demographics::MAX_AGE; ++i) {
    age_count_male[i] = 0;
    age_count_female[i] = 0;
    death_count_male[i] = 0;
    death_count_female[i] = 0;
    birth_count[i] = 0;
  }
}

void Population::assign_classrooms() {
  if(Global::Verbose > 0) {
    fprintf(Global::Statusfp, "assign classrooms entered\n");
    fflush(Global::Statusfp);
  }
  for(int p = 0; p < this->get_index_size(); p++) {
    Person * person = get_person_by_index(p);
    if (person == NULL) continue;
    if (person->get_school() != NULL)
      person->assign_classroom();
  }
  FRED_VERBOSE(0,"assign classrooms finished\n");
}

void Population::assign_offices() {
  if(Global::Verbose > 0) {
    fprintf(Global::Statusfp, "assign offices entered\n");
    fflush(Global::Statusfp);
  }
  for(int p = 0; p < this->get_index_size(); p++) {
    Person * person = get_person_by_index(p);
    if (person == NULL) continue;
    if (person->get_workplace() != NULL)
      person->assign_office();
  }
  FRED_VERBOSE(0,"assign offices finished\n");
}

void Population::get_network_stats(char *directory) {
  if(Global::Verbose > 0) {
    fprintf(Global::Statusfp, "get_network_stats entered\n");
    fflush(Global::Statusfp);
  }
  char filename[FRED_STRING_SIZE];
  sprintf(filename, "%s/degree.csv", directory);
  FILE *fp = fopen(filename, "w");
  fprintf(fp, "id,age,deg,h,n,s,c,w,o\n");
  for(int p = 0; p < this->get_index_size(); p++) {
    Person * person = get_person_by_index(p);
    if (person == NULL) continue;
    fprintf(fp, "%d,%d,%d,%d,%d,%d,%d,%d,%d\n", person->get_id(), person->get_age(), person->get_degree(),
        person->get_household_size(), person->get_neighborhood_size(), person->get_school_size(),
        person->get_classroom_size(), person->get_workplace_size(), person->get_office_size());
  }
  fclose(fp);
  if(Global::Verbose > 0) {
    fprintf(Global::Statusfp, "get_network_stats finished\n");
    fflush(Global::Statusfp);
  }
}

void Population::report_birth(int day, Person *per) const {
  if(Global::Birthfp == NULL)
    return;
  fprintf(Global::Birthfp, "day %d mother %d age %d\n", day, per->get_id(), per->get_age());
  fflush(Global::Birthfp);
}

void Population::report_death(int day, Person *per) const {
  if(Global::Deathfp == NULL)
    return;
  fprintf(Global::Deathfp, "day %d person %d age %d\n", day, per->get_id(), per->get_age());
  fflush(Global::Deathfp);
}

void Population::report_mean_hh_income_per_school() {

  typedef std::map<string, int> LabelMapT;

  LabelMapT * school_label_enrollment_map = new LabelMapT();
  LabelMapT * school_label_hh_income_map = new LabelMapT();

  for(int p = 0; p < this->get_index_size(); p++) {
    Person * person = get_person_by_index(p);
    if(person == NULL) continue;
    if(person->get_school() == NULL) {
      continue;
    } else {
      string school_label(person->get_school()->get_label());
      LabelMapT::const_iterator got = school_label_enrollment_map->find(school_label);
      //Try to find the school label
      if(got == school_label_enrollment_map->end()) {
        //Add the school to the map
        school_label_enrollment_map->insert(std::pair<std::string, int>(school_label, 1));
        Household * student_hh = (Household *)person->get_household();
        assert(student_hh != NULL);
        std::pair<std::string, int> my_insert(school_label, student_hh->get_household_income());
        school_label_hh_income_map->insert(my_insert);
      } else {
        //Update the values
        school_label_enrollment_map->at(school_label) += 1;
        Household * student_hh = (Household *)person->get_household();
        assert(student_hh != NULL);
        school_label_hh_income_map->at(school_label) += student_hh->get_household_income();
      }
    }
  }

  FRED_STATUS(0, "MEAN HOUSEHOLD INCOME STATS PER SCHOOL SUMMARY:\n");
  for(LabelMapT::iterator itr = school_label_enrollment_map->begin();
      itr != school_label_enrollment_map->end(); ++itr) {
    double enrollmen_tot = (double)itr->second;
    LabelMapT::const_iterator got = school_label_hh_income_map->find(itr->first);
    double hh_income_tot = (double)got->second;
    FRED_STATUS(0, "MEAN_HH_INCOME: %s %.2f\n", itr->first.c_str(),
        (hh_income_tot / enrollmen_tot));
  }

}

void Population::report_mean_hh_size_per_school() {

  typedef std::map<string, int> LabelMapT;

  LabelMapT * school_label_enrollment_map = new LabelMapT();
  LabelMapT * school_label_hh_size_map = new LabelMapT();

  for(int p = 0; p < this->get_index_size(); p++) {
    Person * person = get_person_by_index(p);
    if(person == NULL) continue;
    if(person->get_school() == NULL) {
      continue;
    } else {
      string school_label(person->get_school()->get_label());
      LabelMapT::const_iterator got = school_label_enrollment_map->find(school_label);
      //Try to find the school label
      if(got == school_label_enrollment_map->end()) {
        //Add the school to the map
        school_label_enrollment_map->insert(std::pair<std::string, int>(school_label, 1));
        Household * student_hh = (Household *)person->get_household();
        assert(student_hh != NULL);
        std::pair<std::string, int> my_insert(school_label, student_hh->get_size());
        school_label_hh_size_map->insert(my_insert);
      } else {
        //Update the values
        school_label_enrollment_map->at(school_label) += 1;
        Household * student_hh = (Household *)person->get_household();
        assert(student_hh != NULL);
        school_label_hh_size_map->at(school_label) += student_hh->get_size();
      }
    }
  }

  FRED_STATUS(0, "MEAN HOUSEHOLD SIZE STATS PER SCHOOL SUMMARY:\n");
  for(LabelMapT::iterator itr = school_label_enrollment_map->begin();
      itr != school_label_enrollment_map->end(); ++itr) {
    double enrollmen_tot = (double)itr->second;
    LabelMapT::const_iterator got = school_label_hh_size_map->find(itr->first);
    double hh_size_tot = (double)got->second;
    FRED_STATUS(0, "MEAN_HH_SIZE: %s %.2f\n", itr->first.c_str(), (hh_size_tot / enrollmen_tot));
  }

}

void Population::report_mean_hh_distance_from_school() {

  typedef std::map<string, int> LabelMapT;
  typedef std::map<string, double> LabelMapDist;

  LabelMapT * school_label_enrollment_map = new LabelMapT();
  LabelMapDist * school_label_hh_distance_map = new LabelMapDist();

  for(int p = 0; p < this->get_index_size(); p++) {
    Person * person = get_person_by_index(p);
    if(person == NULL) continue;
    if(person->get_school() == NULL) {
      continue;
    } else {
      string school_label(person->get_school()->get_label());
      LabelMapT::const_iterator got = school_label_enrollment_map->find(school_label);
      //Try to find the school label
      if(got == school_label_enrollment_map->end()) {
        //Add the school to the map
        school_label_enrollment_map->insert(std::pair<std::string, int>(school_label, 1));
        Household * student_hh = (Household *)person->get_household();
        assert(student_hh != NULL);
        double distance = Geo_Utils::haversine_distance(person->get_school()->get_longitude(),
            person->get_school()->get_latitude(), student_hh->get_longitude(),
            student_hh->get_latitude());
        std::pair<std::string, int> my_insert(school_label, distance);
        school_label_hh_distance_map->insert(my_insert);
      } else {
        //Update the values
        school_label_enrollment_map->at(school_label) += 1;
        Household * student_hh = (Household *)person->get_household();
        assert(student_hh != NULL);
        double distance = Geo_Utils::haversine_distance(person->get_school()->get_longitude(),
            person->get_school()->get_latitude(), student_hh->get_longitude(),
            student_hh->get_latitude());
        school_label_hh_distance_map->at(school_label) += distance;
      }
    }
  }

  FRED_STATUS(0, "MEAN HOUSEHOLD DISTANCE STATS PER SCHOOL SUMMARY:\n");
  for(LabelMapT::iterator itr = school_label_enrollment_map->begin();
      itr != school_label_enrollment_map->end(); ++itr) {
    double enrollmen_tot = (double)itr->second;
    LabelMapDist::const_iterator got = school_label_hh_distance_map->find(itr->first);
    double hh_distance_tot = got->second;
    FRED_STATUS(0, "MEAN_HH_DISTANCE: %s %.2f\n", itr->first.c_str(),
        (hh_distance_tot / enrollmen_tot));
  }
}

void Population::report_mean_hh_stats_per_income_category() {

  //First sort households into sets based on their income level
  std::set<Household *> household_sets[Household_income_level_code::UNCLASSIFIED + 1];

  for(int p = 0; p < this->get_index_size(); p++) {
    Person * person = get_person_by_index(p);
    if(person == NULL) continue;
    if(person->get_household() == NULL) {
      if(Global::Enable_Hospitals && person->is_hospitalized() && person->get_permanent_household() != NULL) {
        int income_level = ((Household *)person->get_permanent_household())->get_household_income_code();
        household_sets[income_level].insert((Household *)person->get_permanent_household());
        Global::Income_Category_Tracker->add_index(income_level);
      } else {
        continue;
      }
    } else {
      int income_level = ((Household *)person->get_household())->get_household_income_code();
      household_sets[income_level].insert((Household *)person->get_household());
      Global::Income_Category_Tracker->add_index(income_level);
    }

    if(person->get_household() == NULL) {
      continue;
    } else {
      int income_level = ((Household *)person->get_household())->get_household_income_code();
      household_sets[income_level].insert((Household *)person->get_household());
    }
  }

  for(int i = Household_income_level_code::CAT_I; i < Household_income_level_code::UNCLASSIFIED; i++) {
    int count_hh = (int)household_sets[i].size();
    float hh_income = 0.0;
    int count_people = 0;
    int count_children = 0;

    Global::Income_Category_Tracker->set_index_key_pair(i, "number_of_households", count_hh);
    Global::Income_Category_Tracker->set_index_key_pair(i, "number_of_people", (int)0);
    Global::Income_Category_Tracker->set_index_key_pair(i, "number_of_children", (int)0);
    Global::Income_Category_Tracker->set_index_key_pair(i, "number_of_school_children", (int)0);
    Global::Income_Category_Tracker->set_index_key_pair(i, "number_of_workers", (int)0);
    Global::Income_Category_Tracker->set_index_key_pair(i, "number_of_gq_households", (int)0);
    Global::Income_Category_Tracker->set_index_key_pair(i, "number_of_gq_people", (int)0);
    Global::Income_Category_Tracker->set_index_key_pair(i, "number_of_gq_children", (int)0);
    Global::Income_Category_Tracker->set_index_key_pair(i, "number_of_gq_school_children", (int)0);
    Global::Income_Category_Tracker->set_index_key_pair(i, "number_of_gq_workers", (int)0);

    for(std::set<Household *>::iterator itr = household_sets[i].begin();
        itr !=household_sets[i].end(); ++itr) {
      count_people += (*itr)->get_size();
      count_children += (*itr)->get_children();

      if((*itr)->is_group_quarters()) {
        Global::Income_Category_Tracker->increment_index_key_pair(i, "number_of_gq_households", (int)1);
        Global::Income_Category_Tracker->increment_index_key_pair(i, "number_of_gq_people", (*itr)->get_size());
        Global::Income_Category_Tracker->increment_index_key_pair(i, "number_of_gq_children", (*itr)->get_children());
        Global::Income_Category_Tracker->increment_index_key_pair(i, "number_of_people", (*itr)->get_size());
        Global::Income_Category_Tracker->increment_index_key_pair(i, "number_of_children", (*itr)->get_children());
      } else {
        Global::Income_Category_Tracker->increment_index_key_pair(i, "number_of_people", (*itr)->get_size());
        Global::Income_Category_Tracker->increment_index_key_pair(i, "number_of_children", (*itr)->get_children());
      }

      hh_income += (float)(*itr)->get_household_income();
      std::vector<Person *> inhab_vec = (*itr)->get_inhabitants();
      for(std::vector<Person *>::iterator inner_itr = inhab_vec.begin();
          inner_itr != inhab_vec.end(); ++inner_itr) {
        if((*inner_itr)->is_child()) {
          if((*inner_itr)->get_school() != NULL) {
            if((*itr)->is_group_quarters()) {
              Global::Income_Category_Tracker->increment_index_key_pair(i, "number_of_gq_school_children", (int)1);
              Global::Income_Category_Tracker->increment_index_key_pair(i, "number_of_school_children", (int)1);
            } else {
              Global::Income_Category_Tracker->increment_index_key_pair(i, "number_of_school_children", (int)1);
            }
          }
        }
        if((*inner_itr)->get_workplace() != NULL) {
          if((*itr)->is_group_quarters()) {
            Global::Income_Category_Tracker->increment_index_key_pair(i, "number_of_gq_workers", (int)1);
            Global::Income_Category_Tracker->increment_index_key_pair(i, "number_of_workers", (int)1);
          } else {
            Global::Income_Category_Tracker->increment_index_key_pair(i, "number_of_workers", (int)1);
          }
        }
      }
    }

    if(count_hh > 0) {
      Global::Income_Category_Tracker->set_index_key_pair(i, "mean_household_income", (hh_income / (double)count_hh));
    } else {
      Global::Income_Category_Tracker->set_index_key_pair(i, "mean_household_income", (double)0.0);
    }

    //Store size info for later usage
    Household::count_inhabitants_by_household_income_level_map[i] = count_people;
    Household::count_children_by_household_income_level_map[i] = count_children;
  }
}

void Population::report_mean_hh_stats_per_census_tract() {

  //First sort households into sets based on their census tract and income category
  map<int, std::set<Household *> > household_sets;

  for(int p = 0; p < this->get_index_size(); p++) {
    Person * person = get_person_by_index(p);
    if(person == NULL) {
      continue;
    }

    long int census_tract;
    if(person->get_household() == NULL) {
      if(Global::Enable_Hospitals && person->is_hospitalized() && person->get_permanent_household() != NULL) {
        int census_tract_index = ((Household *)person->get_household())->get_census_tract_index();
        census_tract = Global::Places.get_census_tract_with_index(census_tract_index);
        Global::Tract_Tracker->add_index(census_tract);
        household_sets[census_tract].insert((Household *)person->get_permanent_household());
        Household::census_tract_set.insert(census_tract);
      } else {
        continue;
      }
    } else {
      int census_tract_index = ((Household *)person->get_household())->get_census_tract_index();
      census_tract = Global::Places.get_census_tract_with_index(census_tract_index);
      Global::Tract_Tracker->add_index(census_tract);
      household_sets[census_tract].insert((Household *)person->get_household());
      Household::census_tract_set.insert(census_tract);
    }
  }

  for(std::set<long int>::iterator census_tract_itr = Household::census_tract_set.begin();
      census_tract_itr != Household::census_tract_set.end(); ++census_tract_itr) {

    int count_people_per_census_tract = 0;
    int count_children_per_census_tract = 0;
    int count_hh_per_census_tract = (int)household_sets[*census_tract_itr].size();
    float hh_income_per_census_tract = 0.0;

    Global::Tract_Tracker->set_index_key_pair(*census_tract_itr, "number_of_households", count_hh_per_census_tract);
    Global::Tract_Tracker->set_index_key_pair(*census_tract_itr, "number_of_people", (int)0);
    Global::Tract_Tracker->set_index_key_pair(*census_tract_itr, "number_of_children", (int)0);
    Global::Tract_Tracker->set_index_key_pair(*census_tract_itr, "number_of_school_children", (int)0);
    Global::Tract_Tracker->set_index_key_pair(*census_tract_itr, "number_of_workers", (int)0);
    Global::Tract_Tracker->set_index_key_pair(*census_tract_itr, "number_of_gq_households", (int)0);
    Global::Tract_Tracker->set_index_key_pair(*census_tract_itr, "number_of_gq_people", (int)0);
    Global::Tract_Tracker->set_index_key_pair(*census_tract_itr, "number_of_gq_children", (int)0);
    Global::Tract_Tracker->set_index_key_pair(*census_tract_itr, "number_of_gq_school_children", (int)0);
    Global::Tract_Tracker->set_index_key_pair(*census_tract_itr, "number_of_gq_workers", (int)0);
    
    for(std::set<Household *>::iterator itr = household_sets[*census_tract_itr].begin();
        itr != household_sets[*census_tract_itr].end(); ++itr) {

      count_people_per_census_tract += (*itr)->get_size();
      count_children_per_census_tract += (*itr)->get_children();
      
      if((*itr)->is_group_quarters()) {
        Global::Tract_Tracker->increment_index_key_pair(*census_tract_itr, "number_of_gq_households", (int)1);
        Global::Tract_Tracker->increment_index_key_pair(*census_tract_itr, "number_of_gq_people", (*itr)->get_size());
        Global::Tract_Tracker->increment_index_key_pair(*census_tract_itr, "number_of_gq_children", (*itr)->get_children());
        Global::Tract_Tracker->increment_index_key_pair(*census_tract_itr, "number_of_people", (*itr)->get_size());
        Global::Tract_Tracker->increment_index_key_pair(*census_tract_itr, "number_of_children", (*itr)->get_children());
      } else {
        Global::Tract_Tracker->increment_index_key_pair(*census_tract_itr, "number_of_people", (*itr)->get_size());
        Global::Tract_Tracker->increment_index_key_pair(*census_tract_itr, "number_of_children", (*itr)->get_children());
      }
      
      hh_income_per_census_tract += (float)(*itr)->get_household_income();
      std::vector<Person *> inhab_vec = (*itr)->get_inhabitants();
      for(std::vector<Person *>::iterator inner_itr = inhab_vec.begin();
          inner_itr != inhab_vec.end(); ++inner_itr) {
        if((*inner_itr)->is_child()) {
          if((*inner_itr)->get_school() != NULL) {
            if((*itr)->is_group_quarters()) {
              Global::Tract_Tracker->increment_index_key_pair(*census_tract_itr, "number_of_gq_school_children", (int)1);
              Global::Tract_Tracker->increment_index_key_pair(*census_tract_itr, "number_of_school_children", (int)1);
            } else {
              Global::Tract_Tracker->increment_index_key_pair(*census_tract_itr, "number_of_school_children", (int)1);
            }
          }
        }
        if((*inner_itr)->get_workplace() != NULL) {
          if((*itr)->is_group_quarters()) {
            Global::Tract_Tracker->increment_index_key_pair(*census_tract_itr, "number_of_gq_workers", (int)1);
            Global::Tract_Tracker->increment_index_key_pair(*census_tract_itr, "number_of_workers", (int)1);
          } else {
            Global::Tract_Tracker->increment_index_key_pair(*census_tract_itr, "number_of_workers", (int)1);
          }
        }
      }
    }

    if(count_hh_per_census_tract > 0) {
      Global::Tract_Tracker->set_index_key_pair(*census_tract_itr, "mean_household_income", (hh_income_per_census_tract / (double)count_hh_per_census_tract));
    } else {
      Global::Tract_Tracker->set_index_key_pair(*census_tract_itr, "mean_household_income", (double)0.0);
    }

    //Store size info for later usage
    Household::count_inhabitants_by_census_tract_map[*census_tract_itr] = count_people_per_census_tract;
    Household::count_children_by_census_tract_map[*census_tract_itr] = count_children_per_census_tract;
  }
}

void Population::report_mean_hh_stats_per_income_category_per_census_tract() {
  //First sort households into sets based on their census tract and income category
  map<int, map<int, std::set<Household *> > > household_sets;

  for(int p = 0; p < this->get_index_size(); p++) {
    Person * person = get_person_by_index(p);
    if(person == NULL) {
      continue;
    }
    long int census_tract;
    if(person->get_household() == NULL) {
      if(Global::Enable_Hospitals && person->is_hospitalized() && person->get_permanent_household() != NULL) {
        int census_tract_index = ((Household *)person->get_household())->get_census_tract_index();
        census_tract = Global::Places.get_census_tract_with_index(census_tract_index);
        Global::Tract_Tracker->add_index(census_tract);
        int income_level = ((Household *)person->get_permanent_household())->get_household_income_code();
        Global::Income_Category_Tracker->add_index(income_level);
        household_sets[census_tract][income_level].insert((Household *)person->get_permanent_household());
        Household::census_tract_set.insert(census_tract);
      } else {
        continue;
      }
    } else {
      int census_tract_index = ((Household *)person->get_household())->get_census_tract_index();
      census_tract = Global::Places.get_census_tract_with_index(census_tract_index);
      Global::Tract_Tracker->add_index(census_tract);
      int income_level = ((Household *)person->get_household())->get_household_income_code();
      Global::Income_Category_Tracker->add_index(income_level);
      household_sets[census_tract][income_level].insert((Household *)person->get_household());
      Household::census_tract_set.insert(census_tract);
    }
  }

  int count_hh_per_income_cat[Household_income_level_code::UNCLASSIFIED];
  float hh_income_per_income_cat[Household_income_level_code::UNCLASSIFIED];

  //Initialize the Income_Category_Tracker keys
  for(int i = Household_income_level_code::CAT_I; i < Household_income_level_code::UNCLASSIFIED; ++i) {
    Global::Income_Category_Tracker->set_index_key_pair(i, "number_of_households", (int)0);
    Global::Income_Category_Tracker->set_index_key_pair(i, "number_of_people", (int)0);
    Global::Income_Category_Tracker->set_index_key_pair(i, "number_of_children", (int)0);
    Global::Income_Category_Tracker->set_index_key_pair(i, "number_of_school_children", (int)0);
    Global::Income_Category_Tracker->set_index_key_pair(i, "number_of_workers", (int)0);
    Global::Income_Category_Tracker->set_index_key_pair(i, "number_of_gq_households", (int)0);
    Global::Income_Category_Tracker->set_index_key_pair(i, "number_of_gq_people", (int)0);
    Global::Income_Category_Tracker->set_index_key_pair(i, "number_of_gq_children", (int)0);
    Global::Income_Category_Tracker->set_index_key_pair(i, "number_of_gq_school_children", (int)0);
    Global::Income_Category_Tracker->set_index_key_pair(i, "number_of_gq_workers", (int)0);
    hh_income_per_income_cat[i] = 0.0;
    count_hh_per_income_cat[i] = 0;
  }

  for(std::set<long int>::iterator census_tract_itr = Household::census_tract_set.begin();
      census_tract_itr != Household::census_tract_set.end(); ++census_tract_itr) {
    int count_people_per_census_tract = 0;
    int count_children_per_census_tract = 0;
    int count_hh_per_census_tract = 0;
    int count_hh_per_income_cat_per_census_tract = 0;
    float hh_income_per_census_tract = 0.0;

    Global::Tract_Tracker->set_index_key_pair(*census_tract_itr, "number_of_people", (int)0);
    Global::Tract_Tracker->set_index_key_pair(*census_tract_itr, "number_of_children", (int)0);
    Global::Tract_Tracker->set_index_key_pair(*census_tract_itr, "number_of_school_children", (int)0);
    Global::Tract_Tracker->set_index_key_pair(*census_tract_itr, "number_of_workers", (int)0);
    Global::Tract_Tracker->set_index_key_pair(*census_tract_itr, "number_of_gq_households", (int)0);
    Global::Tract_Tracker->set_index_key_pair(*census_tract_itr, "number_of_gq_people", (int)0);
    Global::Tract_Tracker->set_index_key_pair(*census_tract_itr, "number_of_gq_children", (int)0);
    Global::Tract_Tracker->set_index_key_pair(*census_tract_itr, "number_of_gq_school_children", (int)0);
    Global::Tract_Tracker->set_index_key_pair(*census_tract_itr, "number_of_gq_workers", (int)0);

    for(int i = Household_income_level_code::CAT_I; i < Household_income_level_code::UNCLASSIFIED; ++i) {
      count_hh_per_income_cat_per_census_tract += (int)household_sets[*census_tract_itr][i].size();
      count_hh_per_income_cat[i] += (int)household_sets[*census_tract_itr][i].size();
      count_hh_per_census_tract += (int)household_sets[*census_tract_itr][i].size();
      int count_people_per_income_cat = 0;
      int count_children_per_income_cat = 0;
      char buffer [50];

      //First increment the Income Category Tracker Household key (not census tract stratified)
      Global::Income_Category_Tracker->increment_index_key_pair(i, "number_of_households", (int)household_sets[*census_tract_itr][i].size());

      //Per income category
      sprintf(buffer, "%s_number_of_households", Household::household_income_level_lookup(i));
      Global::Tract_Tracker->set_index_key_pair(*census_tract_itr, buffer, (int)household_sets[*census_tract_itr][i].size());
      sprintf(buffer, "%s_number_of_people", Household::household_income_level_lookup(i));
      Global::Tract_Tracker->set_index_key_pair(*census_tract_itr, buffer, (int)0);
      sprintf(buffer, "%s_number_of_children", Household::household_income_level_lookup(i));
      Global::Tract_Tracker->set_index_key_pair(*census_tract_itr, buffer, (int)0);
      sprintf(buffer, "%s_number_of_school_children", Household::household_income_level_lookup(i));
      Global::Tract_Tracker->set_index_key_pair(*census_tract_itr, buffer, (int)0);
      sprintf(buffer, "%s_number_of_workers", Household::household_income_level_lookup(i));
      Global::Tract_Tracker->set_index_key_pair(*census_tract_itr, buffer, (int)0);
      sprintf(buffer, "%s_number_of_gq_people", Household::household_income_level_lookup(i));
      Global::Tract_Tracker->set_index_key_pair(*census_tract_itr, buffer, (int)0);
      sprintf(buffer, "%s_number_of_gq_children", Household::household_income_level_lookup(i));
      Global::Tract_Tracker->set_index_key_pair(*census_tract_itr, buffer, (int)0);
      sprintf(buffer, "%s_number_of_gq_school_children", Household::household_income_level_lookup(i));
      Global::Tract_Tracker->set_index_key_pair(*census_tract_itr, buffer, (int)0);
      sprintf(buffer, "%s_number_of_gq_workers", Household::household_income_level_lookup(i));
      Global::Tract_Tracker->set_index_key_pair(*census_tract_itr, buffer, (int)0);

      for(std::set<Household *>::iterator itr = household_sets[*census_tract_itr][i].begin();
        itr != household_sets[*census_tract_itr][i].end(); ++itr) {

        count_people_per_income_cat += (*itr)->get_size();
        count_people_per_census_tract += (*itr)->get_size();
        //Store size info for later usage
        Household::count_inhabitants_by_household_income_level_map[i] += (*itr)->get_size();
        Household::count_children_by_household_income_level_map[i] += (*itr)->get_children();
        count_children_per_income_cat += (*itr)->get_children();
        count_children_per_census_tract += (*itr)->get_children();
        hh_income_per_income_cat[i] += (float)(*itr)->get_household_income();
        hh_income_per_census_tract += (float)(*itr)->get_household_income();

        //First, increment the Income Category Tracker Household key (not census tract stratified)
        if((*itr)->is_group_quarters()) {
          Global::Income_Category_Tracker->increment_index_key_pair(i, "number_of_gq_households", (int)1);
          Global::Income_Category_Tracker->increment_index_key_pair(i, "number_of_gq_people", (*itr)->get_size());
          Global::Income_Category_Tracker->increment_index_key_pair(i, "number_of_gq_children", (*itr)->get_children());
          Global::Income_Category_Tracker->increment_index_key_pair(i, "number_of_people", (*itr)->get_size());
          Global::Income_Category_Tracker->increment_index_key_pair(i, "number_of_children", (*itr)->get_children());
        } else {
          Global::Income_Category_Tracker->increment_index_key_pair(i, "number_of_people", (*itr)->get_size());
          Global::Income_Category_Tracker->increment_index_key_pair(i, "number_of_children", (*itr)->get_children());
        }

        //Next, increment the Tract tracker keys
        if((*itr)->is_group_quarters()) {
          Global::Tract_Tracker->increment_index_key_pair(*census_tract_itr, "number_of_gq_households", (int)1);
          Global::Tract_Tracker->increment_index_key_pair(*census_tract_itr, "number_of_gq_people", (*itr)->get_size());
          Global::Tract_Tracker->increment_index_key_pair(*census_tract_itr, "number_of_gq_children", (*itr)->get_children());
          sprintf(buffer, "%s_number_of_gq_people", Household::household_income_level_lookup(i));
          Global::Tract_Tracker->increment_index_key_pair(*census_tract_itr, buffer, (*itr)->get_size());
          sprintf(buffer, "%s_number_of_gq_children", Household::household_income_level_lookup(i));
          Global::Tract_Tracker->increment_index_key_pair(*census_tract_itr, buffer, (*itr)->get_children());
          //Don't forget the total counts
          Global::Tract_Tracker->increment_index_key_pair(*census_tract_itr, "number_of_people", (*itr)->get_size());
          Global::Tract_Tracker->increment_index_key_pair(*census_tract_itr, "number_of_children", (*itr)->get_children());
          sprintf(buffer, "%s_number_of_people", Household::household_income_level_lookup(i));
          Global::Tract_Tracker->increment_index_key_pair(*census_tract_itr, buffer, (*itr)->get_size());
          sprintf(buffer, "%s_number_of_children", Household::household_income_level_lookup(i));
          Global::Tract_Tracker->increment_index_key_pair(*census_tract_itr, buffer, (*itr)->get_children());
        } else {
          Global::Tract_Tracker->increment_index_key_pair(*census_tract_itr, "number_of_people", (*itr)->get_size());
          Global::Tract_Tracker->increment_index_key_pair(*census_tract_itr, "number_of_children", (*itr)->get_children());
          sprintf(buffer, "%s_number_of_people", Household::household_income_level_lookup(i));
          Global::Tract_Tracker->increment_index_key_pair(*census_tract_itr, buffer, (*itr)->get_size());
          sprintf(buffer, "%s_number_of_children", Household::household_income_level_lookup(i));
          Global::Tract_Tracker->increment_index_key_pair(*census_tract_itr, buffer, (*itr)->get_children());
        }

        std::vector<Person *> inhab_vec = (*itr)->get_inhabitants();
        for(std::vector<Person *>::iterator inner_itr = inhab_vec.begin();
            inner_itr != inhab_vec.end(); ++inner_itr) {
          if((*inner_itr)->is_child()) {
            if((*inner_itr)->get_school() != NULL) {
              if((*itr)->is_group_quarters()) {
                //First, increment the Income Category Tracker Household key (not census tract stratified)
                Global::Income_Category_Tracker->increment_index_key_pair(i, "number_of_gq_school_children", (int)1);
                //Next, increment the Tract tracker keys
                Global::Tract_Tracker->increment_index_key_pair(*census_tract_itr, "number_of_gq_school_children", (int)1);
                sprintf(buffer, "%s_number_of_gq_school_children", Household::household_income_level_lookup(i));
                Global::Tract_Tracker->increment_index_key_pair(*census_tract_itr, buffer, (int)1);
                //Don't forget the total counts
                //First, increment the Income Category Tracker Household key (not census tract stratified)
                Global::Income_Category_Tracker->increment_index_key_pair(i, "number_of_school_children", (int)1);
                //Next, increment the Tract tracker keys
                Global::Tract_Tracker->increment_index_key_pair(*census_tract_itr, "number_of_school_children", (int)1);
                sprintf(buffer, "%s_number_of_school_children", Household::household_income_level_lookup(i));
                Global::Tract_Tracker->increment_index_key_pair(*census_tract_itr, buffer, (int)1);
              } else {
                //First, increment the Income Category Tracker Household key (not census tract stratified)
                Global::Income_Category_Tracker->increment_index_key_pair(i, "number_of_school_children", (int)1);
                //Next, increment the Tract tracker keys
                Global::Tract_Tracker->increment_index_key_pair(*census_tract_itr, "number_of_school_children", (int)1);
                sprintf(buffer, "%s_number_of_school_children", Household::household_income_level_lookup(i));
                Global::Tract_Tracker->increment_index_key_pair(*census_tract_itr, buffer, (int)1);
              }
            }
          }
          if((*inner_itr)->get_workplace() != NULL) {
            if((*itr)->is_group_quarters()) {
              //First, increment the Income Category Tracker Household key (not census tract stratified)
              Global::Income_Category_Tracker->increment_index_key_pair(i, "number_of_gq_workers", (int)1);
              //Next, increment the Tract tracker keys
              Global::Tract_Tracker->increment_index_key_pair(*census_tract_itr, "number_of_gq_workers", (int)1);
              sprintf(buffer, "%s_number_of_gq_workers", Household::household_income_level_lookup(i));
              Global::Tract_Tracker->increment_index_key_pair(*census_tract_itr, buffer, (int)1);
              //Don't forget the total counts
              //First, increment the Income Category Tracker Household key (not census tract stratified)
              Global::Income_Category_Tracker->increment_index_key_pair(i, "number_of_workers", (int)1);
              //Next, increment the Tract tracker keys
              Global::Tract_Tracker->increment_index_key_pair(*census_tract_itr, "number_of_workers", (int)1);
              sprintf(buffer, "%s_number_of_workers", Household::household_income_level_lookup(i));
              Global::Tract_Tracker->increment_index_key_pair(*census_tract_itr, buffer, (int)1);
            } else {
              //First, increment the Income Category Tracker Household key (not census tract stratified)
              Global::Income_Category_Tracker->increment_index_key_pair(i, "number_of_workers", (int)1);
              //Next, increment the Tract tracker keys
              Global::Tract_Tracker->increment_index_key_pair(*census_tract_itr, "number_of_workers", (int)1);
              sprintf(buffer, "%s_number_of_workers", Household::household_income_level_lookup(i));
              Global::Tract_Tracker->increment_index_key_pair(*census_tract_itr, buffer, (int)1);
            }
          }
        }
      }
    }

    Global::Tract_Tracker->set_index_key_pair(*census_tract_itr, "number_of_households", count_hh_per_census_tract);

    if(count_hh_per_census_tract > 0) {
      Global::Tract_Tracker->set_index_key_pair(*census_tract_itr, "mean_household_income", (hh_income_per_census_tract / (double)count_hh_per_census_tract));
    } else {
      Global::Tract_Tracker->set_index_key_pair(*census_tract_itr, "mean_household_income", (double)0.0);
    }

    //Store size info for later usage
    Household::count_inhabitants_by_census_tract_map[*census_tract_itr] = count_people_per_census_tract;
    Household::count_children_by_census_tract_map[*census_tract_itr] = count_children_per_census_tract;
  }

  for(int i = Household_income_level_code::CAT_I; i < Household_income_level_code::UNCLASSIFIED; ++i) {
    if(count_hh_per_income_cat > 0) {
      Global::Income_Category_Tracker->set_index_key_pair(i, "mean_household_income", (hh_income_per_income_cat[i] / (double)count_hh_per_income_cat[i]));
    } else {
      Global::Income_Category_Tracker->set_index_key_pair(i, "mean_household_income", (double)0.0);
    }
  }
}

void Population::print_age_distribution(char * dir, char * date_string, int run) {
  FILE *fp;
  int count[Demographics::MAX_AGE + 1];
  double pct[Demographics::MAX_AGE + 1];
  char filename[FRED_STRING_SIZE];
  sprintf(filename, "%s/age_dist_%s.%02d", dir, date_string, run);
  printf("print_age_dist entered, filename = %s\n", filename);
  fflush(stdout);
  for(int i = 0; i < 21; i++) {
    count[i] = 0;
  }
  for(int p = 0; p < this->get_index_size(); p++) {
    Person * person = get_person_by_index(p);
    if(person == NULL) continue;
    int age = person->get_age();
    if(0 <= age && age <= Demographics::MAX_AGE)
      count[age]++;
    if(age > Demographics::MAX_AGE)
      count[Demographics::MAX_AGE]++;
    assert(age >= 0);
  }
  fp = fopen(filename, "w");
  for(int i = 0; i < 21; i++) {
    pct[i] = 100.0 * count[i] / this->pop_size;
    fprintf(fp, "%d  %d %f\n", i * 5, count[i], pct[i]);
  }
  fclose(fp);
}

Person * Population::select_random_person() {
  int i = IRAND(0, get_index_size() - 1);
  while(get_person_by_index(i) == NULL) {
    i = IRAND(0, get_index_size() - 1);
  }
  return get_person_by_index(i);
}

Person * Population::select_random_person_by_age(int min_age, int max_age) {
  Person * person;
  int age;
  if (max_age < min_age) return NULL;
  if (Demographics::MAX_AGE < min_age) return NULL;
  while (1) {
    person = select_random_person();
    age = person->get_age();
    if (min_age <= age && age <= max_age) 
      return person;
  }
}

void Population::write_population_output_file(int day) {

  //Loop over the whole population and write the output of each Person's to_string to the file
  char population_output_file[FRED_STRING_SIZE];
  sprintf(population_output_file, "%s/%s_%s.txt", Global::Output_directory, Population::pop_outfile,
      (char *)Global::Sim_Current_Date->get_YYYYMMDD().c_str());
  FILE *fp = fopen(population_output_file, "w");
  if(fp == NULL) {
    Utils::fred_abort("Help! population_output_file %s not found\n", population_output_file);
  }

  //  fprintf(fp, "Population for day %d\n", day);
  //  fprintf(fp, "------------------------------------------------------------------\n");
  for(int p = 0; p < this->blq.get_index_size(); ++p) {
    Person * person = get_person_by_index(p);
    if(person == NULL) continue;
    fprintf(fp, "%s\n", person->to_string().c_str());
  }
  fflush(fp);
  fclose(fp);
}

void Population::get_age_distribution(int * count_males_by_age, int * count_females_by_age) {
  for (int i = 0; i <= Demographics::MAX_AGE; i++) {
    count_males_by_age[i] = 0;
    count_females_by_age[i] = 0;
  }
  for(int p = 0; p < this->get_index_size(); ++p) {
    Person * person = get_person_by_index(p);
    if(person == NULL) continue;
    int age = person->get_age();
    if (age > Demographics::MAX_AGE) age = Demographics::MAX_AGE;
    if(person->get_sex() == 'F') {
      count_females_by_age[age]++;
    } else {
      count_males_by_age[age]++;
    }
  }
}

void Population::update_infectious_people(int day) {
  FRED_STATUS(1, "update_infectious_people entered\n", "");

  update_infectious_activities update_functor(day);
  parallel_masked_apply(fred::Infectious, update_functor);

  FRED_STATUS(1, "update_infectious_people finished\n", "");
}

void Population::update_infectious_activities::operator()(Person & person) {
  person.get_activities()->update_activities_of_infectious_person(&person, this->day);
}

void Population::add_susceptibles_to_infectious_places(int day) {
  /*  FRED_STATUS(1, "add_susceptibles_to_infectious_places entered\n");

  update_susceptible_activities update_functor(day);
  parallel_masked_apply(fred::Susceptible, update_functor);

  FRED_STATUS(1, "add_susceptibles_to_infectious_places finished\n");
  */
}

/*
void Population::update_susceptible_activities::operator()(Person & person) {
  person.get_activities()->update_susceptible_activities(&person, this->day);
}
*/

void Population::add_visitors_to_infectious_places(int day) {
  // NOTE: use this idiom to loop through pop.
  // Note that pop_size is the number of valid indexes, NOT the size of blq.
  for(int p = 0; p < this->get_index_size(); ++p) {
    Person * person = get_person_by_index(p);
    if(person != NULL) {
      person->get_activities()->add_visitor_to_infectious_places(person,day);
    }
  }
}


void Population::update_traveling_people(int day) {
  FRED_STATUS(1, "update_traveling_people entered\n", "");

  update_activities_while_traveling update_functor(day);
  parallel_masked_apply(fred::Travel, update_functor);

  FRED_STATUS(1, "update_traveling_people finished\n", "");
}

void Population::update_activities_while_traveling::operator()(Person & person) {
  person.get_activities()->update_activities_while_traveling(&person, this->day);
}
