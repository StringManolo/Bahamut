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

static bool g_debugMode = false;

void setDebugMode(bool enabled) {
  g_debugMode = enabled;
}

bool isDebugEnabled() {
  return g_debugMode;
}

void DebugLog(const std::string& msg) {
  if (g_debugMode) {
    std::cout << "[DEBUG] " << msg << std::endl;
  }
}

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
  if (str.empty()) return "";

  size_t first = 0;
  while (first < str.size()) {
    unsigned char c = str[first];
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      first++;
    } else if (first + 1 < str.size() &&
        static_cast<unsigned char>(str[first]) == 0xC2 &&
        static_cast<unsigned char>(str[first + 1]) == 0xA0) {
      first += 2;
    }
    else {
      break;
    }
  }

  if (first >= str.size()) return "";

  size_t last = str.size() - 1;
  while (last > first) {
    unsigned char c = str[last];
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      last--;
    }
    else if (last >= 1 &&
        static_cast<unsigned char>(str[last - 1]) == 0xC2 &&
        static_cast<unsigned char>(str[last]) == 0xA0) {
      last -= 2;
    }
    else {
      break;
    }
  }

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
  meta.storageBehavior = "add";
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
    } else if (line.find("Storage:") != std::string::npos) {
      std::string behavior = trimString(line.substr(line.find("Storage:") + 8));
      if (behavior == "replace" || behavior == "delete") {
        meta.storageBehavior = behavior;
      } else {
        meta.storageBehavior = "add";
      }
    } else if (line.find("Args:") != std::string::npos) {
      std::string argSpec = trimString(line.substr(line.find("Args:") + 5));
      meta.argSpecs.push_back(argSpec);
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

void parseBMOPLine(const std::string& line, std::map<std::string, std::vector<DataItem>>& storage) {
  if (line.empty() || line[0] != '{') return;

  rapidjson::Document doc;
  rapidjson::ParseResult ok = doc.Parse(line.c_str());

  if (!ok) {
    rapidjson::Document doc2;
    rapidjson::ParseResult ok2 = doc2.Parse<rapidjson::kParseDefaultFlags |
      rapidjson::kParseCommentsFlag |
      rapidjson::kParseTrailingCommasFlag>(line.c_str());

    if (!ok2) {
      std::cerr << "[Warn] BMOP parse error. Offset: " << ok.Offset()
        << ", Reason: " << rapidjson::GetParseError_En(ok.Code()) << std::endl;
      return;
    }
    doc.Swap(doc2);
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

    std::string valuePreview = item.value.length() > 50 ? item.value.substr(0, 47) + "..." : item.value;
    DebugLog("parseBMOPLine] Stored: format=" + item.format + ", value=" + valuePreview);
  }
  else if (type == "batch") {
    if (doc.HasMember("f") && doc["f"].IsString()) {
      DataItem batchInfo;
      batchInfo.format = doc["f"].GetString();
      batchInfo.value = batchInfo.format;
      storage["__batch_format__"].push_back(batchInfo);
      DebugLog("parseBMOPLine] Batch START: format=" + batchInfo.format);
    }
  }
  else if (type == "batch_end") {
    DebugLog("parseBMOPLine] Batch END marker received");
  }
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

      line = trimString(line);
      if (line.empty()) continue;

      if (line[0] == '{') {
        parseBMOPLine(line, storage);

        if (line.find("\"t\":\"batch\"") != std::string::npos) {
          inBatch = true;
          if (!storage["__batch_format__"].empty()) {
            batchFormat = storage["__batch_format__"][0].format;
            storage["__batch_format__"].clear();
          }
        } else if (line.find("\"t\":\"batch_end\"") != std::string::npos) {
          inBatch = false;
          batchFormat.clear();
        }
      } else if (inBatch && !batchFormat.empty()) {
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

  DebugLog("====== START " + moduleName + " ======");
  DebugLog("Before execution - storage contents:");
  
  int total_items_before = 0;
  if (g_debugMode) {
    for (const auto& [format, items] : storage) {
      if (format == "__batch_format__") continue;
      std::cout << "[DEBUG]   " << format << ": " << items.size() << " items" << std::endl;
      total_items_before += items.size();
    }
  }
  
  DebugLog("Total items in storage: " + std::to_string(total_items_before));
  DebugLog("Module consumes format: '" + consumesFormat + "'");

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
    runner = "node";
  } else if (fullPath.ends_with(".py")) {
    runner = getPythonVersion(fullPath) + " -u";
    DebugLog("Using Python runner with -u flag: " + runner);
  } else if (fullPath.ends_with(".sh")) {
    runner = "bash";
  }

  if (runner.empty()) {
    std::cout << "[-] No runner found for module: " << moduleName << std::endl;
    return;
  }

  std::cout << "------------------------------------------" << std::endl;
  std::cout << "Running (" << meta.installScope << "): " << moduleName;
  if (!consumesFormat.empty()) {
    std::cout << " [consumes: " << consumesFormat << "]";
  }
  std::cout << std::endl;

  std::string cmd = runner + " " + fullPath;
  for (const auto& arg : args) {
    cmd += " " + arg;
  }
  DebugLog("Full command: " + cmd);

  if (!consumesFormat.empty()) {
    DebugLog("====== MODULE CONSUMES DATA ======");
    DebugLog("Setting up bidirectional pipes...");

    int stdin_pipe[2];
    int stdout_pipe[2];

    if (pipe(stdin_pipe) != 0) {
      std::cout << "[-] Failed to create stdin pipe" << std::endl;
      return;
    }
    if (pipe(stdout_pipe) != 0) {
      std::cout << "[-] Failed to create stdout pipe" << std::endl;
      close(stdin_pipe[0]);
      close(stdin_pipe[1]);
      return;
    }

    DebugLog("Pipes created successfully");
    DebugLog("Forking process...");

    pid_t pid = fork();
    if (pid == 0) {
      close(stdin_pipe[1]);
      dup2(stdin_pipe[0], STDIN_FILENO);
      close(stdin_pipe[0]);

      close(stdout_pipe[0]);
      dup2(stdout_pipe[1], STDOUT_FILENO);
      close(stdout_pipe[1]);

      std::vector<char*> exec_args;
      std::istringstream iss(cmd);
      std::string token;

      while (iss >> token) {
        char* arg = new char[token.size() + 1];
        strcpy(arg, token.c_str());
        exec_args.push_back(arg);
      }
      exec_args.push_back(nullptr);

      execvp(exec_args[0], exec_args.data());

      std::cerr << "[DEBUG] CHILD: execvp FAILED! Error: " << strerror(errno) << std::endl;

      for (auto& arg : exec_args) {
        if (arg) delete[] arg;
      }

      exit(EXIT_FAILURE);
    }
    else if (pid > 0) {
      DebugLog("PARENT PROCESS: Child PID = " + std::to_string(pid));

      close(stdin_pipe[0]);
      close(stdout_pipe[1]);

      DebugLog("PARENT: Writing data to module's stdin...");
      FILE* writePipe = fdopen(stdin_pipe[1], "w");
      if (!writePipe) {
        std::cout << "[-] Failed to open write pipe" << std::endl;
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        return;
      }

      int items_sent = 0;
      int formats_sent = 0;

      if (consumesFormat == "*") {
        DebugLog("PARENT: Sending ALL formats from storage");
        
        if (g_debugMode) {
          for (const auto& [format, items] : storage) {
            if (format == "__batch_format__") continue;
            std::cout << "[DEBUG] PARENT:   Format '" << format << "' has "
              << items.size() << " items" << std::endl;

            if (items.empty()) continue;

            formats_sent++;
            for (const auto& item : items) {
              fprintf(writePipe, "{\"t\":\"d\",\"f\":\"%s\",\"v\":\"%s\"}\n",
                  item.format.c_str(), item.value.c_str());
              items_sent++;

              if (items_sent % 1000 == 0) {
                std::cout << "[DEBUG] PARENT: Sent " << items_sent << " items so far..." << std::endl;
              }
            }
          }
        } else {
          for (const auto& [format, items] : storage) {
            if (format == "__batch_format__") continue;
            if (items.empty()) continue;

            formats_sent++;
            for (const auto& item : items) {
              fprintf(writePipe, "{\"t\":\"d\",\"f\":\"%s\",\"v\":\"%s\"}\n",
                  item.format.c_str(), item.value.c_str());
              items_sent++;
            }
          }
        }
      } else {
        DebugLog("PARENT: Sending specific format: '" + consumesFormat + "'");
        auto it = storage.find(consumesFormat);
        if (it != storage.end() && !it->second.empty()) {
          formats_sent = 1;
          for (const auto& item : it->second) {
            fprintf(writePipe, "{\"t\":\"d\",\"f\":\"%s\",\"v\":\"%s\"}\n",
                item.format.c_str(), item.value.c_str());
            items_sent++;
          }
        } else {
          DebugLog("PARENT: No data found for format '" + consumesFormat + "'");
        }
      }

      DebugLog("PARENT: Finished writing. Total: " + std::to_string(items_sent) + 
               " items from " + std::to_string(formats_sent) + " formats");

      fflush(writePipe);
      fclose(writePipe);
      DebugLog("PARENT: Write pipe closed (EOF sent to module)");

      DebugLog("PARENT: Reading module output from stdout...");

      if (!consumesFormat.empty() && consumesFormat != "*") {
        std::string currentModulePath = findModulePath(moduleName);
        if (!currentModulePath.empty()) {
          ModuleMetadata currentMeta = parseModuleMetadata(currentModulePath);
          if (currentMeta.provides == consumesFormat) {
            if (currentMeta.storageBehavior == "replace") {
              DebugLog("STORAGE BEHAVIOR: REPLACE for '" + consumesFormat + "'");
              DebugLog("  Clearing " + std::to_string(storage[consumesFormat].size()) + " existing items.");
              storage[consumesFormat].clear();

            } else if (currentMeta.storageBehavior == "delete") {
              DebugLog("STORAGE BEHAVIOR: DELETE for '" + consumesFormat + "'");
              DebugLog("  Removing key and " + std::to_string(storage[consumesFormat].size()) + " items.");
              storage.erase(consumesFormat);
            }
          }
        }
      }

      FILE* readPipe = fdopen(stdout_pipe[0], "r");
      if (!readPipe) {
        std::cout << "[-] Failed to open read pipe" << std::endl;
        close(stdout_pipe[0]);
        return;
      }

      char buffer[4096];
      std::string lineBuffer;
      std::string batchFormat;
      bool inBatch = false;
      int items_collected = 0;
      int lines_read = 0;

      while (fgets(buffer, sizeof(buffer), readPipe)) {
        lines_read++;

        std::cout << buffer;

        std::string chunk(buffer);
        lineBuffer += chunk;

        size_t pos;
        while ((pos = lineBuffer.find('\n')) != std::string::npos) {
          std::string line = lineBuffer.substr(0, pos);
          lineBuffer = lineBuffer.substr(pos + 1);

          line = trimString(line);
          if (line.empty()) continue;

          if (line[0] == '{') {
            parseBMOPLine(line, storage);

            if (!storage["__batch_format__"].empty()) {
              inBatch = true;
              batchFormat = storage["__batch_format__"][0].format;
              storage["__batch_format__"].clear();
              DebugLog("PARENT: Batch START detected. Format: " + batchFormat);
            }
            else if (line.find("batch_end") != std::string::npos) {
              inBatch = false;
              DebugLog("PARENT: Batch END detected. Total collected: " + 
                       std::to_string(storage[batchFormat].size()) + " items");
              batchFormat.clear();
            }
          }
          else if (inBatch && !batchFormat.empty()) {
            DataItem item;
            item.format = batchFormat;
            item.value = line;
            storage[batchFormat].push_back(item);
            items_collected++;

            if (g_debugMode && items_collected % 1000 == 0) {
              std::cout << "[DEBUG] PARENT: Collected " << items_collected
                << " items from batch" << std::endl;
            }
          }
        }
      }

      DebugLog("PARENT: Finished reading module output");
      DebugLog("PARENT: Lines read: " + std::to_string(lines_read));
      DebugLog("PARENT: Items collected: " + std::to_string(items_collected));

      fclose(readPipe);

      DebugLog("PARENT: Waiting for module to finish...");
      int status;
      waitpid(pid, &status, 0);

      if (WIFEXITED(status)) {
        DebugLog("PARENT: Module exited with status: " + std::to_string(WEXITSTATUS(status)));
      } else if (WIFSIGNALED(status)) {
        DebugLog("PARENT: Module terminated by signal: " + std::to_string(WTERMSIG(status)));
      }

      DebugLog("PARENT: Data sent to module: " + std::to_string(items_sent) + " items");
      DebugLog("PARENT: Data received from module: " + std::to_string(items_collected) + " items");

    } else {
      std::cout << "[-] Fork failed" << std::endl;
      close(stdin_pipe[0]);
      close(stdin_pipe[1]);
      close(stdout_pipe[0]);
      close(stdout_pipe[1]);
      return;
    }
  }
  else {
    DebugLog("====== MODULE GENERATES DATA ONLY ======");

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
      std::cout << "[-] Failed to execute module" << std::endl;
      return;
    }

    char buffer[4096];
    std::string lineBuffer;
    std::string batchFormat;
    bool inBatch = false;
    int items_collected = 0;
    int lines_read = 0;

    while (fgets(buffer, sizeof(buffer), pipe)) {
      lines_read++;

      std::cout << buffer;

      std::string chunk(buffer);
      lineBuffer += chunk;

      size_t pos;
      while ((pos = lineBuffer.find('\n')) != std::string::npos) {
        std::string line = lineBuffer.substr(0, pos);
        lineBuffer = lineBuffer.substr(pos + 1);

        line = trimString(line);
        if (line.empty()) continue;

        if (line[0] == '{') {
          parseBMOPLine(line, storage);

          if (!storage["__batch_format__"].empty()) {
            inBatch = true;
            batchFormat = storage["__batch_format__"][0].format;
            storage["__batch_format__"].clear();
            DebugLog("Batch START detected. Format: " + batchFormat);
          }
          else if (line.find("batch_end") != std::string::npos) {
            inBatch = false;
            DebugLog("Batch END detected. Total " + batchFormat + 
                     " items: " + std::to_string(storage[batchFormat].size()));
            batchFormat.clear();
          }
        }
        else if (inBatch && !batchFormat.empty()) {
          DataItem item;
          item.format = batchFormat;
          item.value = line;
          storage[batchFormat].push_back(item);
          items_collected++;

          if (g_debugMode && items_collected % 1000 == 0) {
            std::cout << "[DEBUG] Collected " << items_collected
              << " " << batchFormat << " items so far..." << std::endl;
          }
        }
        else if (!line.empty()) {
          DebugLog("Line ignored - Not JSON and not in batch: '" + line + "'");
        }
      }
    }

    int pclose_status = pclose(pipe);
    if (pclose_status != 0) {
      DebugLog("Module exited with non-zero status: " + std::to_string(pclose_status));
    }

    DebugLog("Total lines read: " + std::to_string(lines_read));
    DebugLog("Total items collected: " + std::to_string(items_collected));
  }

  DebugLog("====== END " + moduleName + " ======");
  DebugLog("After execution - storage contents:");
  
  int total_items_after = 0;
  if (g_debugMode) {
    for (const auto& [format, items] : storage) {
      if (format == "__batch_format__") continue;
      std::cout << "[DEBUG]   " << format << ": " << items.size() << " items" << std::endl;
      total_items_after += items.size();
    }
  }
  
  DebugLog("Total items in storage: " + std::to_string(total_items_after));
  DebugLog("Net change: +" + std::to_string(total_items_after - total_items_before) + " items");

  if (g_debugMode) {
    for (const auto& [format, items] : storage) {
      if (format == "__batch_format__") continue;
      if (!items.empty()) {
        std::cout << "[DEBUG] Sample of " << format << " items (first 3):" << std::endl;
        for (size_t i = 0; i < std::min(items.size(), size_t(3)); i++) {
          std::cout << "[DEBUG]   [" << i << "] " << items[i].value << std::endl;
        }
      }
    }
  }
}

void runModule(const std::string& moduleName, const std::vector<std::string>& args) {
  std::map<std::string, std::vector<DataItem>> dummyStorage;
  runModuleWithPipe(moduleName, args, dummyStorage, "");
}

std::vector<ProfileModule> loadProfile(const std::string& profileName) {
  std::vector<ProfileModule> modules;

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

    ProfileModule module;

    size_t firstSpace = trimmed.find(' ');

    if (firstSpace == std::string::npos) {
      module.moduleName = trimmed;
    } else {
      module.moduleName = trimmed.substr(0, firstSpace);

      std::string argsStr = trimmed.substr(firstSpace + 1);
      argsStr = trimString(argsStr);

      if (!argsStr.empty()) {
        bool inQuotes = false;
        char quoteChar = '\0';
        std::string currentArg;

        for (size_t i = 0; i < argsStr.size(); ++i) {
          char c = argsStr[i];

          if ((c == '"' || c == '\'') && !inQuotes) {
            inQuotes = true;
            quoteChar = c;
            currentArg += c;
          } else if (c == quoteChar && inQuotes) {
            inQuotes = false;
            quoteChar = '\0';
            currentArg += c;
          } else if (c == ' ' && !inQuotes) {
            if (!currentArg.empty()) {
              module.args.push_back(currentArg);
              currentArg.clear();
            }
          } else {
            currentArg += c;
          }
        }

        if (!currentArg.empty()) {
          module.args.push_back(currentArg);
        }
      }
    }

    modules.push_back(module);
  }

  file.close();
  return modules;
}

void runModulesFromProfile(const std::string& profileName, const std::vector<std::string>& globalArgs) {
  std::vector<ProfileModule> modules = loadProfile(profileName);

  if (modules.empty()) {
    std::cout << "[-] No modules found in profile or profile doesn't exist" << std::endl;
    return;
  }

  std::cout << "[+] Executing profile: " << profileName << std::endl;
  std::cout << "[+] Total modules: " << modules.size() << std::endl;

  std::map<std::string, std::vector<DataItem>> storage;
  int count = 0;

  for (const auto& profileModule : modules) {
    std::string fullPath = findModulePath(profileModule.moduleName);
    if (fullPath.empty()) {
      std::cout << "[-] Module not found: " << profileModule.moduleName << std::endl;
      continue;
    }

    ModuleMetadata meta = parseModuleMetadata(fullPath);
    
    std::vector<std::string> combinedArgs;
    
    for (const auto& arg : profileModule.args) {
      combinedArgs.push_back(arg);
    }
    
    for (const auto& arg : globalArgs) {
      combinedArgs.push_back(arg);
    }
    
    if (!profileModule.args.empty()) {
      std::cout << "[+] Module args: ";
      for (const auto& arg : profileModule.args) {
        std::cout << arg << " ";
      }
      std::cout << std::endl;
    }
    
    runModuleWithPipe(profileModule.moduleName, combinedArgs, storage, meta.consumes);
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

void describeModule(const std::string& moduleName) {
  std::string fullPath = findModulePath(moduleName);
  if (fullPath.empty()) {
    std::cout << "[-] Module not found: " << moduleName << std::endl;
    return;
  }

  ModuleMetadata meta = parseModuleMetadata(fullPath);

  std::cout << "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
  std::cout << "MODULE: " << moduleName << std::endl;
  std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n" << std::endl;

  std::cout << "Name:        " << (meta.name.empty() ? "N/A" : meta.name) << std::endl;
  std::cout << "Description: " << (meta.description.empty() ? "N/A" : meta.description) << std::endl;

  if (!meta.type.empty()) {
    std::cout << "Type:        " << meta.type << std::endl;
  }
  if (meta.stage != 999) {
    std::cout << "Stage:       " << meta.stage << std::endl;
  }
  if (!meta.consumes.empty()) {
    std::cout << "Consumes:    " << meta.consumes << std::endl;
  }
  if (!meta.provides.empty()) {
    std::cout << "Provides:    " << meta.provides << std::endl;
  }
  if (!meta.installCmd.empty()) {
    std::cout << "Install:     " << meta.installCmd << std::endl;
  }
  if (!meta.installScope.empty()) {
    std::cout << "InstallScope: " << meta.installScope << std::endl;
  }

  if (!meta.argSpecs.empty()) {
    std::cout << "\nARGUMENTS:" << std::endl;
    for (const auto& spec : meta.argSpecs) {
      std::cout << "  " << spec << std::endl;
    }
  } else {
    std::cout << "\n(No arguments defined)" << std::endl;
  }

  std::cout << "\nUSAGE:" << std::endl;
  std::cout << "  ./bahamut run " << moduleName;
  if (!meta.argSpecs.empty()) {
    std::cout << " -- [arguments]";
  }
  std::cout << "\n" << std::endl;

  if (!meta.argSpecs.empty()) {
    std::cout << "EXAMPLES:" << std::endl;
    std::cout << "  ./bahamut run " << moduleName << " -- --help" << std::endl;
    std::cout << std::endl;
  }
}
