#include "./core.hpp"
#include "../include/rapidjson/document.h"
#include "../include/rapidjson/error/en.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <cstdlib>
#include <memory>
#include <unistd.h>
#include <sys/wait.h>

namespace fs = std::filesystem;

const std::string MODULES_ROOT = "./modules";
const std::string SHARED_DEPS = "./modules/shared_deps";
const std::string PROFILES_DIR = "./profiles";

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

std::string trimString(const std::string& str) {
  size_t first = str.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return "";
  size_t last = str.find_last_not_of(" \t\r\n");
  return str.substr(first, last - first + 1);
}

ModuleMetadata parseModuleMetadata(const std::string& modulePath) {
  ModuleMetadata meta;
  meta.name = "";
  meta.description = "";
  meta.type = "";
  meta.stage = 999;
  meta.consumes = "";
  meta.provides = "";
  meta.installCmd = "";
  meta.installScope = "shared";

  std::ifstream file(modulePath);
  if (!file.is_open()) return meta;

  std::string line;
  while (std::getline(file, line)) {
    if (line.find("Name:") != std::string::npos) {
      meta.name = trimString(line.substr(line.find("Name:") + 5));
    }
    else if (line.find("Description:") != std::string::npos) {
      meta.description = trimString(line.substr(line.find("Description:") + 12));
    }
    else if (line.find("Type:") != std::string::npos) {
      meta.type = trimString(line.substr(line.find("Type:") + 5));
    }
    else if (line.find("Stage:") != std::string::npos) {
      std::string stageStr = trimString(line.substr(line.find("Stage:") + 6));
      try {
        meta.stage = std::stoi(stageStr);
      } catch (...) {}
    }
    else if (line.find("Consumes:") != std::string::npos) {
      meta.consumes = trimString(line.substr(line.find("Consumes:") + 9));
    }
    else if (line.find("Provides:") != std::string::npos) {
      meta.provides = trimString(line.substr(line.find("Provides:") + 9));
    }
    else if (line.find("Install:") != std::string::npos) {
      meta.installCmd = trimString(line.substr(line.find("Install:") + 8));
    }
    else if (line.find("InstallScope:") != std::string::npos) {
      std::string scope = trimString(line.substr(line.find("InstallScope:") + 13));
      if (scope.find("isolated") != std::string::npos) meta.installScope = "isolated";
      else if (scope.find("global") != std::string::npos) meta.installScope = "global";
      else meta.installScope = "shared";
    }
  }
  file.close();

  return meta;
}

std::string getPythonVersion(const std::string& modulePath) {
  std::ifstream file(modulePath);
  if (file.is_open()) {
    std::string line;
    std::getline(file, line);
    file.close();

    if (line.find("python3.") != std::string::npos) {
      size_t pos = line.find("python3.");
      std::string version = line.substr(pos + 6, 4);
      return "python" + version;
    }
    if (line.find("python3") != std::string::npos) {
      return "python3";
    }
    if (line.find("python2") != std::string::npos) {
      return "python2";
    }
  }
  return "python3";
}

bool ensurePipInstalled(const std::string& pythonCmd) {
  std::string testCmd = pythonCmd + " -m pip --version >/dev/null 2>&1";
  if (std::system(testCmd.c_str()) == 0) {
    return true;
  }

  std::cout << "[!] pip not found. Installing pip..." << std::endl;

  std::string installPipCmd = pythonCmd + " -m ensurepip --upgrade 2>&1";
  int result = std::system(installPipCmd.c_str());

  if (result == 0) {
    std::cout << "[+] pip installed successfully" << std::endl;
    return true;
  }

  std::string downloadPip = "curl -sS https://bootstrap.pypa.io/get-pip.py -o /tmp/get-pip.py";
  if (std::system(downloadPip.c_str()) == 0) {
    std::string installCmd = pythonCmd + " /tmp/get-pip.py --break-system-packages --no-warn-script-location 2>&1";
    result = std::system(installCmd.c_str());
    std::system("rm -f /tmp/get-pip.py");

    if (result == 0) {
      std::cout << "[+] pip installed successfully" << std::endl;
      return true;
    }
  }

  std::cout << "[-] Failed to install pip. Install it manually with:" << std::endl;
  std::cout << "    sudo apt-get install " << pythonCmd << "-pip" << std::endl;
  return false;
}

std::string getPipCommand(const std::string& pythonCmd) {
  std::string testCmd = pythonCmd + " -m pip --version >/dev/null 2>&1";
  if (std::system(testCmd.c_str()) == 0) {
    return pythonCmd + " -m pip";
  }

  if (!ensurePipInstalled(pythonCmd)) {
    return "";
  }

  return pythonCmd + " -m pip";
}

void installModule(std::string moduleName) {
  std::string fullPath = findModulePath(moduleName);
  if (fullPath.empty()) {
    std::cout << "[-] Error: Module " << moduleName << " not found." << std::endl;
    return;
  }

  ModuleMetadata meta = parseModuleMetadata(fullPath);
  std::string moduleDir = fs::path(fullPath).parent_path().string();

  if (meta.installCmd.empty()) {
    std::cout << "[!] No installation command found for " << moduleName << std::endl;
    return;
  }

  std::string finalInstall;
  bool isPython = fullPath.ends_with(".py");
  bool isNode = fullPath.ends_with(".js");

  if (meta.installScope == "global") {
    if (isNode) {
      finalInstall = meta.installCmd + " -g";
    } else if (isPython) {
      std::string pythonCmd = getPythonVersion(fullPath);
      std::string pipCmd = getPipCommand(pythonCmd);

      if (pipCmd.empty()) {
        std::cout << "[-] Cannot install: pip not available" << std::endl;
        return;
      }

      size_t pos = meta.installCmd.find("pip install");
      if (pos != std::string::npos) {
        std::string packages = meta.installCmd.substr(pos + 12);
        finalInstall = pipCmd + " install " + packages + " --break-system-packages";
      } else {
        finalInstall = meta.installCmd + " --break-system-packages";
      }
    } else {
      finalInstall = meta.installCmd;
    }
  } else {
    std::string targetDir = (meta.installScope == "isolated") ? moduleDir : SHARED_DEPS;
    fs::create_directories(targetDir);

    if (isNode) {
      ensurePackageJson(targetDir);
      finalInstall = "cd " + targetDir + " && " + meta.installCmd + " --silent";
    } else if (isPython) {
      std::string pythonCmd = getPythonVersion(fullPath);
      std::string pipCmd = getPipCommand(pythonCmd);

      if (pipCmd.empty()) {
        std::cout << "[-] Cannot install: pip not available" << std::endl;
        return;
      }

      std::string pythonLibs = targetDir + "/python_libs";
      fs::create_directories(pythonLibs);

      size_t pos = meta.installCmd.find("pip install");
      if (pos != std::string::npos) {
        std::string packages = meta.installCmd.substr(pos + 12);
        finalInstall = pipCmd + " install " + packages + " --target=" + pythonLibs + " --no-warn-script-location --disable-pip-version-check --break-system-packages";
      } else {
        finalInstall = meta.installCmd + " --target=" + pythonLibs + " --no-warn-script-location --disable-pip-version-check --break-system-packages";
      }
    } else {
      finalInstall = "cd " + targetDir + " && " + meta.installCmd;
    }
  }

  std::cout << "[+] Installing dependencies (" << meta.installScope << ") for " << moduleName << "..." << std::endl;
  if (isPython) {
    std::string pythonCmd = getPythonVersion(fullPath);
    std::string pipCmd = getPipCommand(pythonCmd);
    if (!pipCmd.empty()) {
      std::cout << "[+] Using " << pipCmd << " for installation" << std::endl;
    }
  }

  int result = std::system(finalInstall.c_str());
  if (result != 0) {
    std::cout << "[-] Installation failed with exit code: " << result << std::endl;
  } else {
    std::cout << "[+] Installation completed successfully" << std::endl;
  }
}

void uninstallModule(std::string moduleName) {
  std::string fullPath = findModulePath(moduleName);
  if (fullPath.empty()) {
    std::cout << "[-] Error: Module " << moduleName << " not found." << std::endl;
    return;
  }

  ModuleMetadata meta = parseModuleMetadata(fullPath);
  std::string moduleDir = fs::path(fullPath).parent_path().string();

  try {
    if (meta.installScope == "isolated") {
      std::cout << "[+] Removing isolated dependencies in: " << moduleDir << std::endl;
      fs::remove_all(moduleDir + "/node_modules");
      fs::remove_all(moduleDir + "/python_libs");
      fs::remove(moduleDir + "/package.json");
    } else {
      std::cout << "[+] Removing local symlink for: " << moduleName << std::endl;
      fs::remove(moduleDir + "/node_modules");
    }
    std::cout << "[+] Done." << std::endl;
  } catch (const std::exception& e) {
    std::cout << "[-] Uninstall error: " << e.what() << std::endl;
  }
}

void purgeSharedDeps() {
  std::cout << "[!] Purging all shared dependencies in " << SHARED_DEPS << "..." << std::endl;
  try {
    if (fs::exists(SHARED_DEPS)) {
      fs::remove_all(SHARED_DEPS);
      std::cout << "[+] Shared directory purged." << std::endl;
    }

    for (const auto& entry : fs::recursive_directory_iterator(MODULES_ROOT)) {
      if (entry.is_directory() && entry.path().filename() == "node_modules") {
        if (fs::is_symlink(entry.path())) {
          fs::remove(entry.path());
        }
      }
    }
    std::cout << "[+] All shared symlinks cleared." << std::endl;
  } catch (const std::exception& e) {
    std::cout << "[-] Purge error: " << e.what() << std::endl;
  }
}

std::string setupNodeEnvironment(const std::string& fullPath, const std::string& scope, const std::string& moduleDir) {
  std::string sourceNodeDir;

  if (scope == "global") {
    if (fs::exists("/usr/local/lib/node_modules"))
      sourceNodeDir = "/usr/local/lib/node_modules";
    else if (fs::exists("/usr/lib/node_modules"))
      sourceNodeDir = "/usr/lib/node_modules";

    if (sourceNodeDir.empty()) {
      char buffer[128];
      std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("npm root -g", "r"), pclose);
      if (pipe && fgets(buffer, sizeof(buffer), pipe.get())) {
        sourceNodeDir = buffer;
        if (!sourceNodeDir.empty() && sourceNodeDir.back() == '\n')
          sourceNodeDir.pop_back();
      }
    }
  } else if (scope == "isolated") {
    sourceNodeDir = moduleDir + "/node_modules";
  } else {
    fs::create_directories(SHARED_DEPS);
    sourceNodeDir = SHARED_DEPS + "/node_modules";
  }

  if (scope == "shared" || scope == "global") {
    std::string localSymlink = moduleDir + "/node_modules";
    if (!fs::exists(localSymlink) && !sourceNodeDir.empty()) {
      try {
        fs::create_directory_symlink(fs::absolute(sourceNodeDir), localSymlink);
      } catch (...) {}
    }
  }

  if (!sourceNodeDir.empty()) {
    setenv("NODE_PATH", fs::absolute(sourceNodeDir).c_str(), 1);
  }

  return sourceNodeDir;
}

std::string setupPythonEnvironment(const std::string& fullPath, const std::string& scope, const std::string& moduleDir) {
  std::string pythonLibsPath;

  if (scope == "global") {
    std::string pythonCmd = getPythonVersion(fullPath);
    char buffer[256];
    std::string cmd = pythonCmd + " -c \"import site; print(site.getsitepackages()[0])\" 2>/dev/null";
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (pipe && fgets(buffer, sizeof(buffer), pipe.get())) {
      pythonLibsPath = buffer;
      if (!pythonLibsPath.empty() && pythonLibsPath.back() == '\n')
        pythonLibsPath.pop_back();
    }
  } else if (scope == "isolated") {
    pythonLibsPath = moduleDir + "/python_libs";
  } else {
    pythonLibsPath = SHARED_DEPS + "/python_libs";
  }

  if (!pythonLibsPath.empty() && fs::exists(pythonLibsPath)) {
    char* oldPy = std::getenv("PYTHONPATH");
    std::string newPy = fs::absolute(pythonLibsPath).string() +
                       (oldPy ? ":" + std::string(oldPy) : "");
    setenv("PYTHONPATH", newPy.c_str(), 1);
  }

  return pythonLibsPath;
}

/*
void parseBMOPLine(const std::string& line, std::map<std::string, std::vector<DataItem>>& storage) {
  if (line.empty() || line[0] != '{') return;

  size_t tPos = line.find("\"t\":");
  if (tPos == std::string::npos) return;

  size_t tStart = line.find("\"", tPos + 4);
  size_t tEnd = line.find("\"", tStart + 1);
  if (tStart == std::string::npos || tEnd == std::string::npos) return;

  std::string type = line.substr(tStart + 1, tEnd - tStart - 1);

  if (type == "d") {
    size_t fPos = line.find("\"f\":");
    size_t vPos = line.find("\"v\":");

    if (fPos == std::string::npos || vPos == std::string::npos) return;

    size_t fStart = line.find("\"", fPos + 4);
    size_t fEnd = line.find("\"", fStart + 1);
    size_t vStart = line.find("\"", vPos + 4);
    size_t vEnd = line.find("\"", vStart + 1);

    if (fStart == std::string::npos || fEnd == std::string::npos ||
        vStart == std::string::npos || vEnd == std::string::npos) return;

    std::string format = line.substr(fStart + 1, fEnd - fStart - 1);
    std::string value = line.substr(vStart + 1, vEnd - vStart - 1);

    DataItem item;
    item.format = format;
    item.value = value;
    storage[format].push_back(item);
  }
  else if (type == "batch") {
    size_t fPos = line.find("\"f\":");
    if (fPos != std::string::npos) {
      size_t fStart = line.find("\"", fPos + 4);
      size_t fEnd = line.find("\"", fStart + 1);
      if (fStart != std::string::npos && fEnd != std::string::npos) {
        std::string format = line.substr(fStart + 1, fEnd - fStart - 1);
        storage["__batch_format__"] = {{format, format}};
      }
    }
  }
}
*/

void parseBMOPLine(const std::string& line, std::map<std::string, std::vector<DataItem>>& storage) {
  if (line.empty() || line[0] != '{') return;

  rapidjson::Document doc;
  rapidjson::ParseResult ok = doc.Parse(line.c_str());

  if (!ok) {
    std::cerr << "[Warn] BMOP parse error. Offset: " << ok.Offset()
      << ", Reason: " << rapidjson::GetParseError_En(ok.Code()) << std::endl;
    return;
  }

  if (!doc.HasMember("t") || !doc["t"].IsString()) return;

  std::string type = doc["t"].GetString();

  if (type == "d") {
    if (!doc.HasMember("f") || !doc.HasMember("v") ||
        !doc["f"].IsString() || !doc["v"].IsString()) {
      return;
    }

    DataItem item;
    item.format = doc["f"].GetString();
    item.value = doc["v"].GetString();
    storage[item.format].push_back(item);
  }
  else if (type == "batch") {
    if (doc.HasMember("f") && doc["f"].IsString()) {
      DataItem batchInfo;
      batchInfo.format = doc["f"].GetString();
      batchInfo.value = "";
      storage["__batch_format__"].push_back(batchInfo);
    }
  }
  // Ignorar "batch_end" y otros tipos no manejados
}

void collectModuleOutput(const std::string& moduleName, FILE* pipe, std::map<std::string, std::vector<DataItem>>& storage) {
  char buffer[4096];
  std::string lineBuffer;
  std::string batchFormat;
  bool inBatch = false;

  while (fgets(buffer, sizeof(buffer), pipe)) {
    std::string chunk(buffer);
    lineBuffer += chunk;

    size_t pos;
    while ((pos = lineBuffer.find('\n')) != std::string::npos) {
      std::string line = lineBuffer.substr(0, pos);
      lineBuffer = lineBuffer.substr(pos + 1);

      if (line.empty()) continue;

      if (line[0] == '{') {
        if (line.find("\"t\":\"batch\"") != std::string::npos) {
          inBatch = true;
          if (!storage["__batch_format__"].empty()) {
            batchFormat = storage["__batch_format__"][0].format;
            storage["__batch_format__"].clear();
          }
          parseBMOPLine(line, storage);
        }
        else if (line.find("\"t\":\"batch_end\"") != std::string::npos) {
          inBatch = false;
          batchFormat.clear();
        }
        else {
          parseBMOPLine(line, storage);
        }
      }
      else if (inBatch && !batchFormat.empty()) {
        DataItem item;
        item.format = batchFormat;
        item.value = line;
        storage[batchFormat].push_back(item);
      }
    }
  }
}

void pipeDataToModule(FILE* pipe, const std::map<std::string, std::vector<DataItem>>& storage, const std::string& consumesFormat) {
  if (consumesFormat == "*") {
    for (const auto& [format, items] : storage) {
      if (format == "__batch_format__") continue;
      for (const auto& item : items) {
        fprintf(pipe, "{\"t\":\"d\",\"f\":\"%s\",\"v\":\"%s\"}\n", 
                item.format.c_str(), item.value.c_str());
        fflush(pipe);
      }
    }
  } else {
    auto it = storage.find(consumesFormat);
    if (it != storage.end()) {
      for (const auto& item : it->second) {
        fprintf(pipe, "{\"t\":\"d\",\"f\":\"%s\",\"v\":\"%s\"}\n", 
                item.format.c_str(), item.value.c_str());
        fflush(pipe);
      }
    }
  }
}

void runModuleWithPipe(const std::string& moduleName, const std::vector<std::string>& args, 
                       std::map<std::string, std::vector<DataItem>>& storage,
                       const std::string& consumesFormat) {
  std::string fullPath = findModulePath(moduleName);
  if (fullPath.empty()) {
    std::cout << "[-] Error: Module " << moduleName << " not found." << std::endl;
    return;
  }

  ModuleMetadata meta = parseModuleMetadata(fullPath);
  std::string moduleDir = fs::path(fullPath).parent_path().string();

  if (fullPath.ends_with(".js")) {
    if (!meta.installCmd.empty() && meta.installScope != "global") {
      std::string nodeDir = setupNodeEnvironment(fullPath, meta.installScope, moduleDir);
      if (!fs::exists(nodeDir)) {
        std::cout << "[!] Dependencies not found. Installing..." << std::endl;
        installModule(moduleName);
        setupNodeEnvironment(fullPath, meta.installScope, moduleDir);
      }
    } else {
      setupNodeEnvironment(fullPath, meta.installScope, moduleDir);
    }
  }
  else if (fullPath.ends_with(".py")) {
    if (!meta.installCmd.empty() && meta.installScope != "global") {
      std::string pythonLibs = setupPythonEnvironment(fullPath, meta.installScope, moduleDir);
      if (!fs::exists(pythonLibs)) {
        std::cout << "[!] Dependencies not found. Installing..." << std::endl;
        installModule(moduleName);
        setupPythonEnvironment(fullPath, meta.installScope, moduleDir);
      }
    } else {
      setupPythonEnvironment(fullPath, meta.installScope, moduleDir);
    }
  }

  std::string runner;
  if (fullPath.ends_with(".js")) {
    runner = "node ";
  } else if (fullPath.ends_with(".py")) {
    runner = getPythonVersion(fullPath) + " ";
  } else if (fullPath.ends_with(".sh")) {
    runner = "bash ";
  }

  if (runner.empty()) return;

  std::string cmd = runner + fullPath;
  for (const auto& arg : args) {
    cmd += " " + arg;
  }

  std::cout << "------------------------------------------" << std::endl;
  std::cout << "Running (" << meta.installScope << "): " << moduleName;
  if (!consumesFormat.empty()) {
    std::cout << " [consumes: " << consumesFormat << "]";
  }
  std::cout << std::endl;

  if (!consumesFormat.empty()) {
    FILE* writeP = popen(cmd.c_str(), "w");
    if (!writeP) {
      std::cout << "[-] Failed to execute module" << std::endl;
      return;
    }

    pipeDataToModule(writeP, storage, consumesFormat);
    pclose(writeP);

    FILE* readP = popen(cmd.c_str(), "r");
    if (readP) {
      collectModuleOutput(moduleName, readP, storage);
      pclose(readP);
    }
  } else {
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
      std::cout << "[-] Failed to execute module" << std::endl;
      return;
    }

    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe)) {
      std::cout << buffer;
    }

    pclose(pipe);

    FILE* collectPipe = popen(cmd.c_str(), "r");
    if (collectPipe) {
      collectModuleOutput(moduleName, collectPipe, storage);
      pclose(collectPipe);
    }
  }
}

void runModule(const std::string& moduleName, const std::vector<std::string>& args) {
  std::map<std::string, std::vector<DataItem>> dummyStorage;
  runModuleWithPipe(moduleName, args, dummyStorage, "");
}

std::vector<std::string> loadProfile(const std::string& profileName) {
  std::vector<std::string> modules;
  
  std::string profilePath = PROFILES_DIR + "/bahamut_" + profileName + ".txt";
  
  if (!fs::exists(profilePath)) {
    std::cout << "[-] Profile not found: " << profileName << std::endl;
    std::cout << "    Looking for: " << profilePath << std::endl;
    return modules;
  }

  std::ifstream file(profilePath);
  if (!file.is_open()) {
    std::cout << "[-] Failed to open profile: " << profileName << std::endl;
    return modules;
  }

  std::string line;
  while (std::getline(file, line)) {
    std::string trimmed = trimString(line);
    
    if (trimmed.empty() || trimmed[0] == '#') {
      continue;
    }

    modules.push_back(trimmed);
  }

  file.close();
  return modules;
}

void runModulesFromProfile(const std::string& profileName, const std::vector<std::string>& args) {
  std::vector<std::string> modules = loadProfile(profileName);
  
  if (modules.empty()) {
    std::cout << "[-] No modules found in profile or profile doesn't exist" << std::endl;
    return;
  }

  std::cout << "[+] Executing profile: " << profileName << std::endl;
  std::cout << "[+] Total modules: " << modules.size() << std::endl;

  std::map<std::string, std::vector<DataItem>> storage;
  int count = 0;

  for (const auto& moduleName : modules) {
    std::string fullPath = findModulePath(moduleName);
    if (fullPath.empty()) {
      std::cout << "[-] Module not found: " << moduleName << std::endl;
      continue;
    }

    ModuleMetadata meta = parseModuleMetadata(fullPath);
    runModuleWithPipe(moduleName, args, storage, meta.consumes);
    count++;
  }

  std::cout << "------------------------------------------" << std::endl;
  std::cout << "[+] Profile execution finished. Modules executed: " << count << std::endl;
}

void runModulesByStage(const std::vector<std::string>& args) {
  std::vector<std::string> allModules = getModules();
  
  if (allModules.empty()) {
    std::cout << "[-] No modules found" << std::endl;
    return;
  }

  std::map<int, std::vector<std::pair<std::string, ModuleMetadata>>> stageModules;

  for (const auto& moduleName : allModules) {
    std::string fullPath = findModulePath(moduleName);
    if (fullPath.empty()) continue;

    ModuleMetadata meta = parseModuleMetadata(fullPath);
    stageModules[meta.stage].push_back({moduleName, meta});
  }

  std::cout << "[+] Executing modules by stage..." << std::endl;
  
  std::map<std::string, std::vector<DataItem>> storage;
  int totalCount = 0;

  for (auto& [stage, modules] : stageModules) {
    if (modules.empty()) continue;

    std::cout << "------------------------------------------" << std::endl;
    std::cout << "[+] Stage " << stage << ": " << modules.size() << " modules" << std::endl;

    for (const auto& [moduleName, meta] : modules) {
      runModuleWithPipe(moduleName, args, storage, meta.consumes);
      totalCount++;
    }
  }

  std::cout << "------------------------------------------" << std::endl;
  std::cout << "[+] All stages completed. Total modules: " << totalCount << std::endl;
  
  std::cout << "[+] Storage summary:" << std::endl;
  for (const auto& [format, items] : storage) {
    if (format == "__batch_format__") continue;
    std::cout << "    " << format << ": " << items.size() << " items" << std::endl;
  }
}

void runModules(const std::vector<std::string>& args) {
  runModulesByStage(args);
}

void listModules() {
  std::vector<std::string> modules = getModules();

  for (const auto& modName : modules) {
    std::string path = findModulePath(modName);
    ModuleMetadata meta = parseModuleMetadata(path);

    std::cout << "------------------------------------------" << std::endl;
    std::cout << "Module: " << modName << std::endl;
    std::cout << "Name: " << (meta.name.empty() ? "N/A" : meta.name) << std::endl;
    std::cout << "Desc: " << (meta.description.empty() ? "N/A" : meta.description) << std::endl;
    
    if (!meta.type.empty()) {
      std::cout << "Type: " << meta.type << std::endl;
    }
    if (meta.stage != 999) {
      std::cout << "Stage: " << meta.stage << std::endl;
    }
    if (!meta.consumes.empty()) {
      std::cout << "Consumes: " << meta.consumes << std::endl;
    }
    if (!meta.provides.empty()) {
      std::cout << "Provides: " << meta.provides << std::endl;
    }
  }
}
