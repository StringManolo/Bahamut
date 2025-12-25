#include "../include/simpleargumentsparser.hpp"
#include "../core/core.hpp"
#include <iostream>
#include <cstdlib>
#include <iomanip>
#include <vector>

CLI cli;
CLI moduleArgs;
bool hasModuleArgs = false;

void Verbose(std::string msg);
void Debug(std::string msg);
void Warning(std::string msg);
void Error(std::string msg);
void Exit(std::string msg);
void ShowHelp();
bool PrintLogo(const std::string& path);
void splitArguments(int argc, char* argv[], std::vector<char*>& bahamutArgs, std::vector<char*>& modArgs);
void debugModuleArgs();

int main(int argc, char* argv[]) {
  std::vector<char*> bahamutArgs;
  std::vector<char*> modArgs;
  
  splitArguments(argc, argv, bahamutArgs, modArgs);
  
  cli = parseCLI(bahamutArgs.size(), bahamutArgs.data());
  
  if (!modArgs.empty()) {
    moduleArgs = parseCLI(modArgs.size(), modArgs.data());
    hasModuleArgs = true;
  }
  
  bool verbose = cli.s["v"] || cli.c["verbose"];
  bool debug = cli.s["d"] || cli.c["debug"];

  setDebugMode(debug);

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

  if (cli.c["debug-module-args"]) {
    debugModuleArgs();
    return 0;
  }

  if (cli.o.size() > 0) {
    std::string command = cli.o[0].first;

    if (command == "list") {
      listModules();
    }
    else if (command == "run") {
      std::vector<std::string> extraArgs;
      
      if (hasModuleArgs) {
        for (size_t i = 1; i < modArgs.size(); ++i) {
          extraArgs.push_back(modArgs[i]);
        }
      }

      if (cli.c["profile"]) {
        std::string profileName = cli.c["profile"].toString();

        if (verbose) {
          Verbose("Executing profile: " + profileName);
          if (!extraArgs.empty()) {
            std::cout << cli.color["dim"]("  Module arguments: ");
            for (const auto& arg : extraArgs) {
              std::cout << arg << " ";
            }
            std::cout << std::endl;
          }
        }
        
        runModulesFromProfile(profileName, extraArgs);
      }
      else if (cli.o.size() < 2) {
        Error("Usage: run <module_name | all> [-- args...]");
      }
      else {
        std::string target = cli.o[1].first;

        if (verbose) {
          Verbose("Module: " + target);
          if (!extraArgs.empty()) {
            std::cout << cli.color["dim"]("  Module arguments: ");
            for (const auto& arg : extraArgs) {
              std::cout << arg << " ";
            }
            std::cout << std::endl;
          }
        }

        if (target == "all") {
          if (verbose) Verbose("Executing all modules by stage...");
          runModulesByStage(extraArgs);
        } else {
          runModule(target, extraArgs);
        }
      }
    }
    else if (command == "describe") {
      if (cli.o.size() > 1) {
        describeModule(cli.o[1].first);
      } else {
        Error("Usage: describe <module_name>");
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

void splitArguments(int argc, char* argv[], std::vector<char*>& bahamutArgs, std::vector<char*>& modArgs) {
  bahamutArgs.push_back(argv[0]);
  
  int separatorPos = -1;
  
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--") {
      separatorPos = i;
      break;
    }
  }
  
  if (separatorPos == -1) {
    for (int i = 1; i < argc; ++i) {
      bahamutArgs.push_back(argv[i]);
    }
  } else {
    for (int i = 1; i < separatorPos; ++i) {
      bahamutArgs.push_back(argv[i]);
    }
    
    modArgs.push_back(argv[0]);
    for (int i = separatorPos + 1; i < argc; ++i) {
      modArgs.push_back(argv[i]);
    }
  }
}

void debugModuleArgs() {
  std::cout << "\n" << cli.color["bold"]["cyan"]("MODULE ARGUMENTS DEBUG") << std::endl;
  std::cout << cli.color["white"]("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━") << "\n" << std::endl;
  
  if (!hasModuleArgs) {
    std::cout << cli.color["yellow"]("No module arguments found (no -- separator)") << std::endl;
    std::cout << std::endl;
    return;
  }
  
  std::cout << cli.color["bold"]["yellow"]("Raw CLI Object:") << std::endl;
  moduleArgs.debug();
  
  std::cout << "\n" << cli.color["bold"]["yellow"]("JSON Representation:") << std::endl;
  std::string json = moduleArgs.toJSON(2, false);
  std::cout << json << std::endl;
  
  std::cout << "\n" << cli.color["bold"]["yellow"]("Parsed Arguments:") << std::endl;
  
  if (!moduleArgs.s.empty()) {
    std::cout << cli.color["green"]("  Single-dash args (-x):") << std::endl;
    for (const auto& key : moduleArgs.s.keys()) {
      std::cout << "    -" << key << " = " << cli.color["cyan"](moduleArgs.s[key].toString()) << std::endl;
    }
  } else {
    std::cout << cli.color["dim"]("  No single-dash args") << std::endl;
  }
  
  if (!moduleArgs.c.empty()) {
    std::cout << cli.color["green"]("  Double-dash args (--xxx):") << std::endl;
    for (const auto& key : moduleArgs.c.keys()) {
      std::cout << "    --" << key << " = " << cli.color["cyan"](moduleArgs.c[key].toString()) << std::endl;
    }
  } else {
    std::cout << cli.color["dim"]("  No double-dash args") << std::endl;
  }
  
  if (!moduleArgs.o.empty()) {
    std::cout << cli.color["green"]("  Positional args:") << std::endl;
    for (const auto& item : moduleArgs.o) {
      std::cout << "    [" << item.second << "] " << cli.color["cyan"](item.first) << std::endl;
    }
  } else {
    std::cout << cli.color["dim"]("  No positional args") << std::endl;
  }
  
  std::cout << "\n" << cli.color["bold"]["yellow"]("Statistics:") << std::endl;
  std::cout << "  Total argc: " << moduleArgs.argc << std::endl;
  std::cout << "  No args: " << (moduleArgs.noArgs ? cli.color["yellow"]("true") : "false") << std::endl;
  std::cout << "  Has piped input: " << (moduleArgs.p.empty() ? "false" : cli.color["green"]("true")) << std::endl;
  
  std::cout << std::endl;
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
  std::cout << std::left << std::setw(40) << "  run <module> [-- args...]" << "Run a specific module with optional arguments" << std::endl;
  std::cout << std::left << std::setw(40) << "  run all [-- args...]" << "Run all modules by stage with global args" << std::endl;
  std::cout << std::left << std::setw(40) << "  run --profile <name> [-- args...]" << "Run modules from profile with optional args" << std::endl;
  std::cout << std::left << std::setw(40) << "  list" << "List all available modules" << std::endl;
  std::cout << std::left << std::setw(40) << "  describe <module>" << "Show module details and arguments" << std::endl;
  std::cout << std::left << std::setw(40) << "  install <module>" << "Install dependencies for a module" << std::endl;
  std::cout << std::left << std::setw(40) << "  uninstall <module>" << "Remove module-specific dependencies" << std::endl;
  std::cout << std::left << std::setw(40) << "  purge" << "Clear all shared dependencies and symlinks" << std::endl;

  std::cout << "\n" << bold["white"]("OPTIONS:") << std::endl;
  std::cout << std::left << std::setw(40) << "  -h, --help" << "Show this help" << std::endl;
  std::cout << std::left << std::setw(40) << "  -v, --verbose" << "Show more information" << std::endl;
  std::cout << std::left << std::setw(40) << "  -d, --debug" << "Show debug logs" << std::endl;
  std::cout << std::left << std::setw(40) << "  --version" << "Show version" << std::endl;
  std::cout << std::left << std::setw(40) << "  --debug-module-args" << "Debug module argument parsing" << std::endl;

  std::cout << "\n" << dim["yellow"]("Examples:") << std::endl;
  std::cout << "  " << cyan("./bahamut run checktor.js") << std::endl;
  std::cout << "  " << cyan("./bahamut run getrobotsfromurl.py -- --url example.com") << std::endl;
  std::cout << "  " << cyan("./bahamut run getrobotsfromurl.py -- -u google.com -v") << std::endl;
  std::cout << "  " << cyan("./bahamut -v run all -- --timeout 10") << std::endl;
  std::cout << "  " << cyan("./bahamut run --profile recon -- --depth 3") << std::endl;
  std::cout << "  " << cyan("./bahamut describe getrobotsfromurl.py") << std::endl;
  std::cout << "  " << cyan("./bahamut --debug-module-args run scanner.py -- --test arg") << std::endl;
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
