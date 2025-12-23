#include "../include/simpleargumentsparser.hpp"
#include "../core/core.hpp"
#include <iostream>
#include <cstdlib>

CLI cli;

int main(int argc, char* argv[]) {
  cli = parseCLI(argc, argv);
  bool verbose = false;
  bool debug = false;
  // bool log = false; // this should be a module
  
  if (cli.c["version"]) {
    std::cout << cli.color["dim"]("Debug mode enabled") << std::endl;
    return 0;
  }

  if (cli.s["h"] || cli.c["help"] || cli.noArgs) {
    std::cout << cli.color["bold"]["cyan"]("Bahamut Usage: blabla") << std::endl;
    return 0;
  }

  if (cli.s["v"] || cli.c["verbose"]) {
    std::cout << cli.color["dim"]["green"]("Verbose mode enabled") << std::endl;
    verbose = true;
  }

  if (cli.s["d"] || cli.c["debug"]) {
    std::cout << cli.color["dim"]["blue"]("Debug mode enabled") << std::endl;
    debug = true;
  }


  return 0;
}

void Verbose(std::string msg) {
  std::cout << cli.color["green"](msg) << std::endl;
}

void Debug(std::string msg) {
  std::cout << cli.color["blue"](msg) << std::endl;
}

void Warning(std::string msg) {
  std::cout << cli.color["yellow"](msg) << std::endl;
}

void Error(std::string msg) {
  std::cout << cli.color["red"](msg) << std::endl;
  std::exit(0);
}

void Exit(std::string msg) {
  std::cout << msg << std::endl;
  std::exit(0);
}

