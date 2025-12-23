#include "./core.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;
const std::string MODULES_PATH = "./modules";

std::vector<std::string> getModules() {
  std::vector<std::string> modules;
  if (!fs::exists(MODULES_PATH)) return modules;

  for (const auto& entry : fs::directory_iterator(MODULES_PATH)) {
    if (entry.is_regular_file()) {
      std::string name = entry.path().filename().string();
      if (name.find("_modules") == std::string::npos && name.find("_libs") == std::string::npos) {
        modules.push_back(name);
      }
    }
  }
  return modules;
}

void runModule(const std::string& moduleFile, const std::vector<std::string>& args) {
  std::string fullPath = MODULES_PATH + "/" + moduleFile;

  std::ifstream file(fullPath);
  if (file.is_open()) {
    std::string line;
    while (std::getline(file, line)) {
      size_t pos = line.find("Install:");
      if (pos != std::string::npos) {
        std::string dep = line.substr(pos + 8);
        dep.erase(0, dep.find_first_not_of(" \t"));

        if (moduleFile.ends_with(".js")) {
          fs::create_directories(MODULES_PATH + "/node_modules");
          std::string installCmd = "cd " + MODULES_PATH + " && " + dep + " --silent";
          std::system(installCmd.c_str());
        } else if (moduleFile.ends_with(".py")) {
          std::string pyLib = MODULES_PATH + "/python_libs";
          fs::create_directories(pyLib);
          std::string installCmd = dep + " --target=" + pyLib + " --quiet";
          std::system(installCmd.c_str());
        }
      }
    }
    file.close();
  }

  std::string nodePath = fs::absolute(MODULES_PATH + "/node_modules").string();
  std::string pythonPath = fs::absolute(MODULES_PATH + "/python_libs").string();

  setenv("NODE_PATH", nodePath.c_str(), 1);
  char* currentPyPath = std::getenv("PYTHONPATH");
  std::string newPyPath = pythonPath + (currentPyPath ? ":" + std::string(currentPyPath) : "");
  setenv("PYTHONPATH", newPyPath.c_str(), 1);

  std::string cmd;
  if (moduleFile.ends_with(".js")) cmd = "node ";
  else if (moduleFile.ends_with(".py")) cmd = "python3 ";
  else if (moduleFile.ends_with(".sh")) cmd = "bash ";

  cmd += fullPath;
  for (const auto& arg : args) cmd += " " + arg;

  std::cout << "------------------------------------------" << std::endl;
  std::cout << "Running: " << moduleFile << std::endl;

  char buffer[512];
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
  if (!pipe) {
    std::cout << "Error: Could not launch " << moduleFile << std::endl;
    return;
  }

  while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
    std::cout << "  " << buffer;
  }
}

void runModules(const std::vector<std::string>& args) {
  std::vector<std::string> modules = getModules();
  for (const auto& mod : modules) {
    runModule(mod, args);
  }
}

void listModules() {
  if (!fs::exists(MODULES_PATH)) {
    std::cout << "Modules directory not found: " << MODULES_PATH << std::endl;
    return;
  }

  for (const auto& entry : fs::directory_iterator(MODULES_PATH)) {
    if (!entry.is_regular_file()) continue;

    std::ifstream file(entry.path());
    if (!file.is_open()) continue;

    std::string line, shebang, name, desc;
    bool first = true;

    while (std::getline(file, line)) {
      if (first) {
        if (line.size() >= 2 && line.substr(0, 2) == "#!") {
          shebang = line;
        }
        first = false;
        continue;
      }

      size_t n_pos = line.find("Name:");
      size_t d_pos = line.find("Description:");

      if (name.empty() && n_pos != std::string::npos) {
        name = line.substr(n_pos + 5);
      } else if (desc.empty() && d_pos != std::string::npos) {
        desc = line.substr(d_pos + 12);
      }

      if (!name.empty() && !desc.empty()) break;
    }

    std::cout << "------------------------------------------" << std::endl;
    std::cout << "File:        " << entry.path().filename().string() << std::endl;
    std::cout << "Interpreter: " << (shebang.empty() ? "ELF/Binary" : shebang) << std::endl;
    std::cout << "Name:        " << (name.empty() ? "Unknown" : name) << std::endl;
    std::cout << "Description: " << (desc.empty() ? "None" : desc) << std::endl;
  }
}
