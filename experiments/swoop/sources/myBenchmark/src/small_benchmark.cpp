/** # Copyright (C) Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 *
 * # This is a small example benchmark */

#include <cstdlib>
#include <iostream>
#include <vector>

using namespace std;

/** Person struct holds 3 attributes which all null initialized: ID, attr, contactPerson.
 * ID is of type unsigned int, unique identifier
 * attr is of type int, descriptor for some feature
 * contactPerson is a pointer of struct Person.
 */
struct Person{
  unsigned int ID;
  int attr;
  Person* contactPerson;
  Person(): ID(0),attr(0), contactPerson(NULL){} //initial null
};


int main(int argc, char* argv[]){
  int vecSize, seed;
  
  //if no argument is given, default setting is used
  if(argc == 1){
    vecSize = 100000;
    seed = 0;
  }
  else if(argc == 2){
    vecSize = atoi(argv[1]);
    seed = time(NULL);
    cout << "default random with time..." << endl;
  }
  else{
    vecSize = atoi(argv[1]);
    seed = atoi(argv[2]);
  }
  srand(seed);
  std::vector<Person> record;

  //create Person vector, and set IDs
  for(int i = 0; i < vecSize; i++){
    Person p;
    p.ID = i+1;
    //p.attr = rand()%(vecSize);
    record.push_back(p);
  }

  //assign contactPerson randomly but not oneself
  for (int i = 0; i < vecSize; ++i){
    unsigned int rd = rand() % vecSize;
    if (rd == (unsigned int)i){
      rd += 2;
      rd = rd%vecSize;
    }
    record[i].contactPerson = &record[rd];
    

  }
  //Person's attr is changed to 1 if the contactPerson of this Person's contactPerson is this Person oneself.
  //loop with 5 indirections
#pragma clang loop vectorize_width(1337)
  for(int i = 0; i < vecSize; ++i){
    if ((record[(record[i].contactPerson->ID)-1].contactPerson->ID)-1 == i){
      record[i].attr = 1;
      record[(record[i].contactPerson->ID)-1].attr = 1;
    }
  }

  //print information on all Person in vector
  for(int i = 0; i < vecSize; i++){
    cout << "Person " << i << ": ID=" << record[i].ID << ", ";
    cout << "ID=" << record[i].contactPerson->ID << ", ";
    cout << "attr=" << record[i].attr << endl;
  }
  //  std::cout << std::endl;
  return 0;
}
