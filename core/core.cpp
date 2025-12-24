#include "./core.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <vector>
#include <set>

namespace fs = std::filesystem;
const std::string MODULES_ROOT = "./modules";
const std::string SHARED_DEPS = "./modules/shared_deps";

void ensurePackageJson(const std::string& path) {
  std::string pjson = path + "/package.json";
  if (!fs::exists(pjson)) {
    std::ofstream file(pjson);
    file << "{\n  \"name\": \"bahamut-module\",\n  \"version\": \"1.0.0\",\n  \"type\": \"module\"\n}";
    file.close();
  }
}

std::string findModulePath(const std::string& moduleName) {
  if (!fs::exists(MODULES_ROOT)) return "";
  for (const auto& entry : fs::recursive_directory_iterator(MODULES_ROOT)) {
    if (entry.is_regular_file() && entry.path().filename() == moduleName) {
      return entry.path().string();
    }
  }
  return "";
}

std::vector<std::string> getModules() {
  std::vector<std::string> modules;
  if (!fs::exists(MODULES_ROOT)) return modules;

  std::set<std::string> allowedExtensions = {".js", ".py", ".sh"};

  for (const auto& entry : fs::recursive_directory_iterator(MODULES_ROOT)) {
    if (entry.is_regular_file()) {
      std::string path = entry.path().string();
      std::string ext = entry.path().extension().string();

      if (path.find("node_modules") != std::string::npos || 
          path.find("python_libs") != std::string::npos ||
          path.find("shared_deps") != std::string::npos) {
        continue;
      }

      if (allowedExtensions.count(ext)) {
        modules.push_back(entry.path().filename().string());
      }
    }
  }
  return modules;
}

void runModule(const std::string& moduleName, const std::vector<std::string>& args) {
  std::string fullPath = findModulePath(moduleName);
  if (fullPath.empty()) {
    std::cout << "Error: Module " << moduleName << " not found." << std::endl;
    return;
  }

  std::string moduleDir = fs::path(fullPath).parent_path().string();
  std::string scope = "shared";
  std::string installCmd = "";

  std::ifstream file(fullPath);
  if (file.is_open()) {
    std::string line;
    while (std::getline(file, line)) {
      if (line.find("InstallScope:") != std::string::npos) {
        if (line.find("isolated") != std::string::npos) scope = "isolated";
        else if (line.find("global") != std::string::npos) scope = "global";
      }
      if (line.find("Install:") != std::string::npos) {
        installCmd = line.substr(line.find("Install:") + 8);
        installCmd.erase(0, installCmd.find_first_not_of(" \t"));
      }
    }
    file.close();
  }

  std::string sourceNodeDir, targetDir;
  if (scope == "isolated") {
    targetDir = moduleDir;
    sourceNodeDir = moduleDir + "/node_modules";
  } else if (scope == "shared") {
    fs::create_directories(SHARED_DEPS);
    targetDir = SHARED_DEPS;
    sourceNodeDir = SHARED_DEPS + "/node_modules";
  }

  if (!installCmd.empty() && scope != "global") {
    if (!fs::exists(sourceNodeDir)) {
      fs::create_directories(targetDir);
      if (fullPath.ends_with(".js")) ensurePackageJson(targetDir);
      
      std::string finalInstall = "cd " + targetDir + " && " + installCmd + " --silent";
      if (fullPath.ends_with(".py")) finalInstall += " --target=./python_libs";
      
      std::cout << "Installing dependencies (" << scope << ")..." << std::endl;
      std::system(finalInstall.c_str());
    }
  }

  if (fullPath.ends_with(".js") && scope == "shared") {
    std::string localSymlink = moduleDir + "/node_modules";
    if (!fs::exists(localSymlink)) {
      try {
        fs::create_directory_symlink(fs::absolute(sourceNodeDir), localSymlink);
      } catch (...) {}
    }
  }

  if (scope != "global") {
    setenv("NODE_PATH", fs::absolute(sourceNodeDir).c_str(), 1);
    char* oldPy = std::getenv("PYTHONPATH");
    std::string pythonPath = (scope == "isolated" ? moduleDir : SHARED_DEPS) + "/python_libs";
    std::string newPy = fs::absolute(pythonPath).string() + (oldPy ? ":" + std::string(oldPy) : "");
    setenv("PYTHONPATH", newPy.c_str(), 1);
  }

  std::string runner = "";
  if (fullPath.ends_with(".js")) runner = "node ";
  else if (fullPath.ends_with(".py")) runner = "python3 ";
  else if (fullPath.ends_with(".sh")) runner = "bash ";

  if (runner == "") return;

  std::string cmd = runner + fullPath;
  for (const auto& arg : args) cmd += " " + arg;

  std::cout << "------------------------------------------" << std::endl;
  std::cout << "Running (" << scope << "): " << moduleName << std::endl;

  char buffer[512];
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
  while (pipe && fgets(buffer, sizeof(buffer), pipe.get())) std::cout << "  " << buffer;
}

void runModules(const std::vector<std::string>& args) {
  std::vector<std::string> modules = getModules();
  int count = 0;
  for (const auto& mod : modules) {
    runModule(mod, args);
    count++;
  }
  std::cout << "------------------------------------------" << std::endl;
  std::cout << "Execution finished. Total modules: " << count << std::endl;
}

void listModules() {
  std::vector<std::string> modules = getModules();
  for (const auto& modName : modules) {
    std::string path = findModulePath(modName);
    std::ifstream file(path);
    std::string line, name, desc;
    while (std::getline(file, line)) {
      if (line.find("Name:") != std::string::npos) name = line.substr(line.find("Name:") + 5);
      if (line.find("Description:") != std::string::npos) desc = line.substr(line.find("Description:") + 12);
    }
    std::cout << "------------------------------------------" << std::endl;
    std::cout << "Module: " << modName << "\nName:   " << name << "\nDesc:   " << desc << std::endl;
  }
}
