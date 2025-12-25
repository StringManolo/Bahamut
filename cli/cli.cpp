#include "../include/simpleargumentsparser.hpp"
#include "../core/core.hpp"
#include <iostream>
#include <cstdlib>
#include <iomanip>

CLI cli;

void Verbose(std::string msg);
void Debug(std::string msg);
void Warning(std::string msg);
void Error(std::string msg);
void Exit(std::string msg);
void ShowHelp();
bool PrintLogo(const std::string& path);

int main(int argc, char* argv[]) {
  cli = parseCLI(argc, argv);
  bool verbose = cli.s["v"] || cli.c["verbose"];
  bool debug = cli.s["d"] || cli.c["debug"];

  if (cli.c["version"]) {
    PrintLogo("repoAssets/bahamut_landscape.png");
    std::cout << std::endl;
    std::cout << cli.color["bold"]["red"]("Bahamut V1.0.0") << std::endl;
    return 0;
  }

  if (cli.s["h"] || cli.c["help"] || cli.noArgs) {
    ShowHelp();
    return 0;
  }

  if (verbose) Verbose("Verbose mode enabled");
  if (debug) Debug("Debug mode enabled");

  if (cli.o.size() > 0) {
    std::string command = cli.o[0].first;

    if (command == "list") {
      listModules();
    }
    else if (command == "run") {
      if (cli.c["profile"]) {
        std::string profileName = cli.c["profile"].toString();
        std::vector<std::string> extraArgs;

        for (size_t i = 1; i < cli.o.size(); ++i) {
          extraArgs.push_back(cli.o[i].first);
        }

        if (verbose) Verbose("Executing profile: " + profileName);
        runModulesFromProfile(profileName, extraArgs);
      }
      else if (cli.o.size() < 2) {
        Error("Usage: run <module_name | all> [args...] or run --profile <name>");
      }
      else {
        std::string target = cli.o[1].first;
        std::vector<std::string> extraArgs;

        for (size_t i = 2; i < cli.o.size(); ++i) {
          extraArgs.push_back(cli.o[i].first);
        }

        if (target == "all") {
          if (verbose) Verbose("Executing all modules by stage...");
          runModulesByStage(extraArgs);
        } else {
          runModule(target, extraArgs);
        }
      }
    }
    else if (command == "install") {
      if (cli.o.size() > 1) {
        installModule(cli.o[1].first);
      } else {
        Error("Usage: install <module_name>");
      }
    }
    else if (command == "uninstall") {
      if (cli.o.size() > 1) {
        uninstallModule(cli.o[1].first);
      } else {
        Error("Usage: uninstall <module_name>");
      }
    }
    else if (command == "purge") {
      purgeSharedDeps();
    }
    else {
      Error("Unknown command: " + command);
    }
  }

  return 0;
}

void ShowHelp() {
  auto bold = cli.color["bold"];
  auto cyan = cli.color["cyan"];
  auto dim = cli.color["dim"];

  std::cout << "\n" << bold["red"]("BAHAMUT") << " - Modular Hacking Orchestrator\n" << std::endl;

  PrintLogo("repoAssets/bahamut_landscape.png -s 80x27");
  std::cout << std::endl;
  std::cout << bold["white"]("USAGE:") << std::endl;
  std::cout << "  ./bahamut [command] [arguments]\n" << std::endl;

  std::cout << bold["white"]("COMMANDS:") << std::endl;
  std::cout << std::left << std::setw(30) << "  run <module>" << "Run a specific module" << std::endl;
  std::cout << std::left << std::setw(30) << "  run all" << "Run all modules by stage" << std::endl;
  std::cout << std::left << std::setw(30) << "  run --profile <name>" << "Run modules from profile" << std::endl;
  std::cout << std::left << std::setw(30) << "  list" << "List all available modules" << std::endl;
  std::cout << std::left << std::setw(30) << "  install <module>" << "Install dependencies for a module" << std::endl;
  std::cout << std::left << std::setw(30) << "  uninstall <module>" << "Remove module-specific dependencies" << std::endl;
  std::cout << std::left << std::setw(30) << "  purge" << "Clear all shared dependencies and symlinks" << std::endl;

  std::cout << "\n" << bold["white"]("OPTIONS:") << std::endl;
  std::cout << std::left << std::setw(30) << "  -h, --help" << "Show this help" << std::endl;
  std::cout << std::left << std::setw(30) << "  -v, --verbose" << "Show more information" << std::endl;
  std::cout << std::left << std::setw(30) << "  -d, --debug" << "Show debug logs" << std::endl;
  std::cout << std::left << std::setw(30) << "  --version" << "Show version" << std::endl;

  std::cout << "\n" << dim["yellow"]("Examples:") << std::endl;
  std::cout << "  " << cyan("./bahamut run checktor.js") << std::endl;
  std::cout << "  " << cyan("./bahamut run all") << std::endl;
  std::cout << "  " << cyan("./bahamut run --profile recon") << std::endl;
  std::cout << std::endl;
}

void Verbose(std::string msg) {
  std::cout << cli.color["green"]("[+] " + msg) << std::endl;
}

void Debug(std::string msg) {
  std::cout << cli.color["blue"]("[DEBUG] " + msg) << std::endl;
}

void Warning(std::string msg) {
  std::cout << cli.color["yellow"]("[!] " + msg) << std::endl;
}

void Error(std::string msg) {
  std::cout << cli.color["red"]("[-] Error: " + msg) << std::endl;
  std::exit(1);
}

void Exit(std::string msg) {
  std::cout << msg << std::endl;
  std::exit(0);
}

bool PrintLogo(const std::string& path) {
  if (system("command -v chafa > /dev/null 2>&1") == 0) {
    std::string command = "chafa " + path + " 2>/dev/null";
    system(command.c_str());
    return true;
  }
  return false;
}
