#include "./core.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <set>
#include <cstdlib>
#include <memory>

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

  if (installCmd.empty()) {
    std::cout << "[!] No installation command found for " << moduleName << std::endl;
    return;
  }

  std::string finalInstall;
  bool isPython = fullPath.ends_with(".py");
  bool isNode = fullPath.ends_with(".js");

  if (scope == "global") {
    if (isNode) {
      finalInstall = installCmd + " -g";
    } else if (isPython) {
      std::string pythonCmd = getPythonVersion(fullPath);
      std::string pipCmd = getPipCommand(pythonCmd);
      
      if (pipCmd.empty()) {
        std::cout << "[-] Cannot install: pip not available" << std::endl;
        return;
      }
      
      size_t pos = installCmd.find("pip install");
      if (pos != std::string::npos) {
        std::string packages = installCmd.substr(pos + 12);
        finalInstall = pipCmd + " install " + packages + " --break-system-packages";
      } else {
        finalInstall = installCmd + " --break-system-packages";
      }
    } else {
      finalInstall = installCmd;
    }
  } else {
    std::string targetDir = (scope == "isolated") ? moduleDir : SHARED_DEPS;
    fs::create_directories(targetDir);

    if (isNode) {
      ensurePackageJson(targetDir);
      finalInstall = "cd " + targetDir + " && " + installCmd + " --silent";
    } else if (isPython) {
      std::string pythonCmd = getPythonVersion(fullPath);
      std::string pipCmd = getPipCommand(pythonCmd);
      
      if (pipCmd.empty()) {
        std::cout << "[-] Cannot install: pip not available" << std::endl;
        return;
      }
      
      std::string pythonLibs = targetDir + "/python_libs";
      fs::create_directories(pythonLibs);
      
      size_t pos = installCmd.find("pip install");
      if (pos != std::string::npos) {
        std::string packages = installCmd.substr(pos + 12);
        finalInstall = pipCmd + " install " + packages + " --target=" + pythonLibs + " --no-warn-script-location --disable-pip-version-check --break-system-packages";
      } else {
        finalInstall = installCmd + " --target=" + pythonLibs + " --no-warn-script-location --disable-pip-version-check --break-system-packages";
      }
    } else {
      finalInstall = "cd " + targetDir + " && " + installCmd;
    }
  }

  std::cout << "[+] Installing dependencies (" << scope << ") for " << moduleName << "..." << std::endl;
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

  std::string moduleDir = fs::path(fullPath).parent_path().string();
  std::string scope = "shared";

  std::ifstream file(fullPath);
  if (file.is_open()) {
    std::string line;
    while (std::getline(file, line)) {
      if (line.find("InstallScope:") != std::string::npos) {
        if (line.find("isolated") != std::string::npos) scope = "isolated";
      }
    }
    file.close();
  }

  try {
    if (scope == "isolated") {
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

void runModule(const std::string& moduleName, const std::vector<std::string>& args) {
  std::string fullPath = findModulePath(moduleName);
  if (fullPath.empty()) {
    std::cout << "[-] Error: Module " << moduleName << " not found." << std::endl;
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

  std::string sourceNodeDir;
  if (fullPath.ends_with(".js")) {
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

    if (!installCmd.empty() && scope != "global") {
      if (!fs::exists(sourceNodeDir)) {
        std::cout << "[!] Dependencies not found. Installing..." << std::endl;
        installModule(moduleName);
      }
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
  }

  if (fullPath.ends_with(".py")) {
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

    if (!installCmd.empty() && scope != "global") {
      if (!fs::exists(pythonLibsPath)) {
        std::cout << "[!] Dependencies not found. Installing..." << std::endl;
        installModule(moduleName);
      }
    }

    if (!pythonLibsPath.empty() && fs::exists(pythonLibsPath)) {
      char* oldPy = std::getenv("PYTHONPATH");
      std::string newPy = fs::absolute(pythonLibsPath).string() + 
                         (oldPy ? ":" + std::string(oldPy) : "");
      setenv("PYTHONPATH", newPy.c_str(), 1);
    }
  }

  std::string runner = "";
  if (fullPath.ends_with(".js")) {
    runner = "node ";
  } else if (fullPath.ends_with(".py")) {
    runner = getPythonVersion(fullPath) + " ";
  } else if (fullPath.ends_with(".sh")) {
    runner = "bash ";
  }

  if (runner == "") return;

  std::string cmd = runner + fullPath;
  for (const auto& arg : args) 
    cmd += " " + arg;

  std::cout << "------------------------------------------" << std::endl;
  std::cout << "Running (" << scope << "): " << moduleName << std::endl;

  char buffer[512];
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
  while (pipe && fgets(buffer, sizeof(buffer), pipe.get())) 
    std::cout << buffer;
}

void runModules(const std::vector<std::string>& args) {
  std::vector<std::string> modules = getModules();
  int count = 0;
  
  std::cout << "[+] Executing all modules in sequence..." << std::endl;
  for (const auto& mod : modules) {
    runModule(mod, args);
    count++;
  }
  
  std::cout << "------------------------------------------" << std::endl;
  std::cout << "[+] Execution finished. Total modules: " << count << std::endl;
}

void listModules() {
  std::vector<std::string> modules = getModules();
  
  for (const auto& modName : modules) {
    std::string path = findModulePath(modName);
    std::ifstream file(path);
    std::string line, name, desc;
    
    while (std::getline(file, line)) {
      if (line.find("Name:") != std::string::npos) 
        name = line.substr(line.find("Name:") + 5);
      if (line.find("Description:") != std::string::npos) 
        desc = line.substr(line.find("Description:") + 12);
    }
    
    std::cout << "------------------------------------------" << std::endl;
    std::cout << "Module: " << modName 
              << "\nName: " << (name.empty() ? "N/A" : name)
              << "\nDesc: " << (desc.empty() ? "N/A" : desc) << std::endl;
  }
}
