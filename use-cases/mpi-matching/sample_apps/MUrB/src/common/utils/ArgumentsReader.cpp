/*!
 * \file    ArgumentsReader.cpp
 * \brief   Command line arguments management class.
 * \author  A. Cassagne
 * \date    2014
 *
 * \section LICENSE
 * This file is under MIT license (https://opensource.org/licenses/MIT).
 *
 * \section DESCRIPTION
 * This is the traditional entry file for the code execution.
 */
#include <cassert>
#include <iostream>

#include "ArgumentsReader.h"

using namespace std;

Arguments_reader::Arguments_reader(int argc, char **argv) : m_argv(argc) {
  assert(argc > 0);

  this->m_program_name = argv[0];

  for (unsigned short i = 0; i < argc; ++i)
    this->m_argv[i] = argv[i];
}

Arguments_reader::~Arguments_reader() {}

bool Arguments_reader::parse_arguments(map<string, string> requireArgs,
                                       map<string, string> facultativeArgs) {
  // assert(requireArgs.size() > 0); // useless, it is possible to have no
  // require arguments
  unsigned short int nReqArg = 0;

  this->clear_arguments();

  this->m_require_args = requireArgs;
  this->m_facultative_args = facultativeArgs;

  for (unsigned short i = 0; i < this->m_argv.size(); ++i) {
    if (this->sub_parse_arguments(this->m_require_args, i))
      nReqArg++;
    this->sub_parse_arguments(this->m_facultative_args, i);
  }

  return nReqArg >= requireArgs.size();
}

bool Arguments_reader::sub_parse_arguments(map<string, string> args,
                                           unsigned short posArg) {
  assert(posArg < this->m_argv.size());

  bool isFound = false;

  map<string, string>::iterator it;
  for (it = args.begin(); it != args.end(); ++it) {
    string curArg = "-" + it->first;
    if (curArg == this->m_argv[posArg]) {
      if (it->second != "") {
        if (posArg != (this->m_argv.size() - 1)) {
          this->m_args[it->first] = this->m_argv[posArg + 1];
          isFound = true;
        }
      } else {
        this->m_args[it->first] = "";
        isFound = true;
      }
    }
  }

  return isFound;
}

bool Arguments_reader::exist_argument(std::string tag) {
  return (this->m_args.find(tag) != this->m_args.end());
}

string Arguments_reader::get_argument(string tag) { return this->m_args[tag]; }

bool Arguments_reader::parse_doc_args(
    std::map<std::string, std::string> docArgs) {
  bool reVal = true;

  if (docArgs.empty())
    reVal = false;

  map<string, string>::iterator it;
  for (it = this->m_require_args.begin(); it != this->m_require_args.end();
       ++it)
    if (!(docArgs.find(it->first) != docArgs.end()))
      reVal = false;
    else
      this->m_doc_args[it->first] = docArgs[it->first];

  for (it = this->m_facultative_args.begin();
       it != this->m_facultative_args.end(); ++it)
    if (!(docArgs.find(it->first) != docArgs.end()))
      reVal = false;
    else
      this->m_doc_args[it->first] = docArgs[it->first];

  return reVal;
}

void Arguments_reader::print_usage() {
  cout << "Usage: " << this->m_program_name;

  map<string, string>::iterator it;
  for (it = this->m_require_args.begin(); it != this->m_require_args.end();
       ++it)
    if (it->second != "")
      cout << " -" << it->first << " " << it->second;
    else
      cout << " -" << it->first;

  for (it = this->m_facultative_args.begin();
       it != this->m_facultative_args.end(); ++it)
    if (it->second != "")
      cout << " [-" << it->first << " " << it->second << "]";
    else
      cout << " [-" << it->first << "]";

  cout << "\n";

  if (!this->m_doc_args.empty()) {
    cout << "\n";
    for (it = this->m_require_args.begin(); it != this->m_require_args.end();
         ++it)
      if (this->m_doc_args.find(it->first) != this->m_doc_args.end())
        cout << "\t-" << it->first << "\t\t"
             << this->m_doc_args.find(it->first)->second << "\n";

    for (it = this->m_facultative_args.begin();
         it != this->m_facultative_args.end(); ++it)
      if (this->m_doc_args.find(it->first) != this->m_doc_args.end())
        cout << "\t-" << it->first << "\t\t"
             << this->m_doc_args.find(it->first)->second << "\n";
  }
}

void Arguments_reader::clear_arguments() {
  this->m_require_args.clear();
  this->m_facultative_args.clear();
  this->m_args.clear();
  this->m_doc_args.clear();
}
