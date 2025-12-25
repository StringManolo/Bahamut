#ifndef CORE_HPP
#define CORE_HPP

#include <string>
#include <vector>
#include <map>

struct DataItem {
  std::string format;
  std::string value;
};

struct ModuleMetadata {
  std::string name;
  std::string description;
  std::string type;
  int stage;
  std::string consumes;
  std::string provides;
  std::string storageBehavior;
  std::string installCmd;
  std::string installScope;
  std::vector<std::string> argSpecs;
};

void installModule(std::string moduleName);
void uninstallModule(std::string moduleName);
void purgeSharedDeps();
void describeModule(const std::string& moduleName); 
void runModule(const std::string& moduleName, const std::vector<std::string>& args);
void runModules(const std::vector<std::string>& args);
void runModulesFromProfile(const std::string& profileName, const std::vector<std::string>& args);
void runModulesByStage(const std::vector<std::string>& args);
void listModules();

void parseBMOPLine(const std::string& line, std::map<std::string, std::vector<DataItem>>& storage);
void collectModuleOutput(const std::string& moduleName, FILE* pipe, std::map<std::string, std::vector<DataItem>>& storage);
std::string trimString(const std::string& str);
void pipeDataToModule(FILE* pipe, const std::map<std::string, std::vector<DataItem>>& storage, const std::string& consumesFormat);

ModuleMetadata parseModuleMetadata(const std::string& modulePath);
void ensurePackageJson(const std::string& path);
std::vector<std::string> getModules();
std::string findModulePath(const std::string& moduleName);
std::string getPythonVersion(const std::string& modulePath);
std::vector<std::string> loadProfile(const std::string& profileName);
void runModuleWithPipe(const std::string& moduleName, const std::vector<std::string>& args, std::map<std::string, std::vector<DataItem>>& storage, const std::string& consumesFormat);
std::string setupNodeEnvironment(const std::string& fullPath, const std::string& scope, const std::string& moduleDir);
std::string setupPythonEnvironment(const std::string& fullPath, const std::string& scope, const std::string& moduleDir);

#endif
