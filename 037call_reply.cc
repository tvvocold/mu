//: Calls can also generate products, using 'reply'.

:(scenario reply)
recipe main [
  1:integer, 2:integer <- f 34:literal
]
recipe f [
  12:integer <- next-ingredient
  13:integer <- add 1:literal, 12:integer
  reply 12:integer, 13:integer
]
+run: instruction main/0
+mem: storing 34 in location 1
+mem: storing 35 in location 2

:(before "End Primitive Recipe Declarations")
REPLY,
:(before "End Primitive Recipe Numbers")
Recipe_number["reply"] = REPLY;
:(before "End Primitive Recipe Implementations")
case REPLY: {
  const instruction& reply_inst = current_instruction();  // save pointer into recipe before pop
  Current_routine->calls.pop();
  // just in case 'main' returns a value, drop it for now
  if (Current_routine->calls.empty()) goto stop_running_current_routine;
  const instruction& caller_instruction = current_instruction();
  // make reply results available to caller
  copy(ingredients.begin(), ingredients.end(), inserter(products, products.begin()));
  // check that any reply ingredients with /same-as-ingredient connect up
  // the corresponding ingredient and product in the caller.
  for (index_t i = 0; i < caller_instruction.products.size(); ++i) {
    trace("run") << "result " << i << " is " << to_string(ingredients.at(i));
    if (has_property(reply_inst.ingredients.at(i), "same-as-ingredient")) {
      vector<string> tmp = property(reply_inst.ingredients.at(i), "same-as-ingredient");
      assert(tmp.size() == 1);
      long long int ingredient_index = to_int(tmp.at(0));
      if (caller_instruction.products.at(i).value != caller_instruction.ingredients.at(ingredient_index).value)
        raise << "'same-as-ingredient' result " << caller_instruction.products.at(i).value << " must be location " << caller_instruction.ingredients.at(ingredient_index).value << '\n';
    }
  }
  // refresh instruction_counter to caller's step_index
  instruction_counter = current_step_index();
  break;
}

//: Products can include containers and exclusive containers, addresses and arrays.
:(scenario reply_container)
recipe main [
  3:point <- f 2:literal
]
recipe f [
  12:integer <- next-ingredient
  13:integer <- copy 35:literal
  reply 12:point
]
+run: instruction main/0
+run: result 0 is [2, 35]
+mem: storing 2 in location 3
+mem: storing 35 in location 4

//: In mu we'd like to assume that any instruction doesn't modify its
//: ingredients unless they're also products. The /same-as-ingredient inside
//: the recipe's 'reply' will help catch accidental misuse of such
//: 'ingredient-results' (sometimes called in-out parameters in other languages).
:(scenario reply_same_as_ingredient)
% Hide_warnings = true;
recipe main [
  1:integer <- copy 0:literal
  2:integer <- test1 1:integer  # call with different ingredient and product
]
recipe test1 [
  10:address:integer <- next-ingredient
  reply 10:address:integer/same-as-ingredient:0
]
+warn: 'same-as-ingredient' result 2 must be location 1

:(code)
string to_string(const vector<long long int>& in) {
  if (in.empty()) return "[]";
  ostringstream out;
  if (in.size() == 1) {
    out << in.at(0);
    return out.str();
  }
  out << "[";
  for (index_t i = 0; i < in.size(); ++i) {
    if (i > 0) out << ", ";
    out << in.at(i);
  }
  out << "]";
  return out.str();
}
