//: Run a second routine concurrently using 'start-running', without any
//: guarantees on how the operations in each are interleaved with each other.

:(scenario scheduler)
recipe f1 [
  start-running f2:recipe
  1:integer <- copy 3:literal
]
recipe f2 [
  2:integer <- copy 4:literal
]
+schedule: f1
+schedule: f2

//: first, add a deadline to run(routine)
//: these changes are ugly and brittle; just close your nose and get through the next few lines
:(replace "void run_current_routine()")
void run_current_routine(size_t time_slice)
:(replace "while (!Current_routine->completed())" following "void run_current_routine(size_t time_slice)")
size_t ninstrs = 0;
while (Current_routine->state == RUNNING && ninstrs < time_slice)
:(after "Running One Instruction")
ninstrs++;

//: now the rest of the scheduler is clean

:(before "struct routine")
enum routine_state {
  RUNNING,
  COMPLETED,
  // End routine States
};
:(before "End routine Fields")
enum routine_state state;
:(before "End routine Constructor")
state = RUNNING;

:(before "End Globals")
vector<routine*> Routines;
index_t Current_routine_index = 0;
size_t Scheduling_interval = 500;
:(before "End Setup")
Scheduling_interval = 500;
:(replace{} "void run(recipe_number r)")
void run(recipe_number r) {
  Routines.push_back(new routine(r));
  Current_routine_index = 0, Current_routine = Routines.at(0);
  while (!all_routines_done()) {
    skip_to_next_routine();
//?     cout << "scheduler: " << Current_routine_index << '\n'; //? 1
    assert(Current_routine);
    assert(Current_routine->state == RUNNING);
    trace("schedule") << current_recipe_name();
//?     trace("schedule") << Current_routine->id << " " << current_recipe_name(); //? 1
    run_current_routine(Scheduling_interval);
    if (Current_routine->completed())
      Current_routine->state = COMPLETED;
    // End Scheduler State Transitions
  }
//?   cout << "done with run\n"; //? 1
}

:(code)
bool all_routines_done() {
  for (index_t i = 0; i < Routines.size(); ++i) {
//?     cout << "routine " << i << ' ' << Routines.at(i)->state << '\n'; //? 1
    if (Routines.at(i)->state == RUNNING) {
      return false;
    }
  }
  return true;
}

// skip Current_routine_index past non-RUNNING routines
void skip_to_next_routine() {
  assert(!Routines.empty());
  assert(Current_routine_index < Routines.size());
  for (index_t i = (Current_routine_index+1)%Routines.size();  i != Current_routine_index;  i = (i+1)%Routines.size()) {
    if (Routines.at(i)->state == RUNNING) {
//?       cout << "switching to " << i << '\n'; //? 1
      Current_routine_index = i;
      Current_routine = Routines.at(i);
      return;
    }
  }
//?   cout << "all done\n"; //? 1
}

:(before "End Teardown")
for (index_t i = 0; i < Routines.size(); ++i)
  delete Routines.at(i);
Routines.clear();

//:: To schedule new routines to run, call 'start-running'.

//: 'start-running' will return a unique id for the routine that was created.
:(before "End routine Fields")
index_t id;
:(before "End Globals")
index_t Next_routine_id = 1;
:(before "End Setup")
Next_routine_id = 1;
:(before "End routine Constructor")
id = Next_routine_id;
Next_routine_id++;

:(before "End Primitive Recipe Declarations")
START_RUNNING,
:(before "End Primitive Recipe Numbers")
Recipe_number["start-running"] = START_RUNNING;
:(before "End Primitive Recipe Implementations")
case START_RUNNING: {
  assert(isa_literal(current_instruction().ingredients.at(0)));
  assert(!current_instruction().ingredients.at(0).initialized);
  routine* new_routine = new routine(Recipe_number[current_instruction().ingredients.at(0).name]);
  // populate ingredients
  for (index_t i = 1; i < current_instruction().ingredients.size(); ++i)
    new_routine->calls.top().ingredient_atoms.push_back(ingredients.at(i));
  Routines.push_back(new_routine);
  products.resize(1);
  products.at(0).push_back(new_routine->id);
  break;
}

:(scenario scheduler_runs_single_routine)
% Scheduling_interval = 1;
recipe f1 [
  1:integer <- copy 0:literal
  2:integer <- copy 0:literal
]
+schedule: f1
+run: instruction f1/0
+schedule: f1
+run: instruction f1/1

:(scenario scheduler_interleaves_routines)
% Scheduling_interval = 1;
recipe f1 [
  start-running f2:recipe
  1:integer <- copy 0:literal
  2:integer <- copy 0:literal
]
recipe f2 [
  3:integer <- copy 4:literal
  4:integer <- copy 4:literal
]
+schedule: f1
+run: instruction f1/0
+schedule: f2
+run: instruction f2/0
+schedule: f1
+run: instruction f1/1
+schedule: f2
+run: instruction f2/1
+schedule: f1
+run: instruction f1/2

:(scenario start_running_takes_args)
recipe f1 [
  start-running f2:recipe, 3:literal
]
recipe f2 [
  1:integer <- next-ingredient
  2:integer <- add 1:integer, 1:literal
]
+mem: storing 4 in location 2

:(scenario start_running_returns_routine_id)
recipe f1 [
  1:integer <- start-running f2:recipe
]
recipe f2 [
  12:integer <- copy 44:literal
]
+mem: storing 2 in location 1

:(scenario scheduler_skips_completed_routines)
# this scenario will require some careful setup in escaped C++
# (straining our tangle capabilities to near-breaking point)
% recipe_number f1 = load("recipe f1 [\n1:integer <- copy 0:literal\n]").front();
% recipe_number f2 = load("recipe f2 [\n2:integer <- copy 0:literal\n]").front();
% Routines.push_back(new routine(f1));  // f1 meant to run
% Routines.push_back(new routine(f2));
% Routines.back()->state = COMPLETED;  // f2 not meant to run
#? % Trace_stream->dump_layer = "all";
# must have at least one routine without escaping
recipe f3 [
  3:integer <- copy 0:literal
]
# by interleaving '+' lines with '-' lines, we allow f1 and f3 to run in any order
+schedule: f1
+mem: storing 0 in location 1
-schedule: f2
-mem: storing 0 in location 2
+schedule: f3
+mem: storing 0 in location 3

:(scenario scheduler_starts_at_middle_of_routines)
% Routines.push_back(new routine(COPY));
% Routines.back()->state = COMPLETED;
recipe f1 [
  1:integer <- copy 0:literal
  2:integer <- copy 0:literal
]
+schedule: f1
-run: idle

//:: 'routine-state' can tell if a given routine id is running

:(scenario routine_state_test)
% Scheduling_interval = 2;
recipe f1 [
  1:integer/child-id <- start-running f2:recipe
  12:integer <- copy 0:literal  # race condition since we don't care about location 12
  # thanks to Scheduling_interval, f2's one instruction runs in between here and completes
  2:integer/state <- routine-state 1:integer/child-id
]
recipe f2 [
  12:integer <- copy 0:literal
  # trying to run a second instruction marks routine as completed
]
# recipe f2 should be in state COMPLETED
+mem: storing 1 in location 2

:(before "End Primitive Recipe Declarations")
ROUTINE_STATE,
:(before "End Primitive Recipe Numbers")
Recipe_number["routine-state"] = ROUTINE_STATE;
:(before "End Primitive Recipe Implementations")
case ROUTINE_STATE: {
  assert(ingredients.at(0).size() == 1);  // routine id must be scalar
  index_t id = ingredients.at(0).at(0);
  long long int result = -1;
  for (index_t i = 0; i < Routines.size(); ++i) {
    if (Routines.at(i)->id == id) {
      result = Routines.at(i)->state;
      break;
    }
  }
  products.resize(1);
  products.at(0).push_back(result);
  break;
}

:(before "End Primitive Recipe Declarations")
RESTART,
:(before "End Primitive Recipe Numbers")
Recipe_number["restart"] = RESTART;
:(before "End Primitive Recipe Implementations")
case RESTART: {
  assert(ingredients.at(0).size() == 1);  // routine id must be scalar
  index_t id = ingredients.at(0).at(0);
  for (index_t i = 0; i < Routines.size(); ++i) {
    if (Routines.at(i)->id == id) {
      Routines.at(i)->state = RUNNING;
      break;
    }
  }
  break;
}

:(before "End Primitive Recipe Declarations")
_DUMP_ROUTINES,
:(before "End Primitive Recipe Numbers")
Recipe_number["$dump-routines"] = _DUMP_ROUTINES;
:(before "End Primitive Recipe Implementations")
case _DUMP_ROUTINES: {
  for (index_t i = 0; i < Routines.size(); ++i) {
    cerr << Routines.at(i)->id << ": " << Routines.at(i)->state << '\n';
  }
  break;
}
