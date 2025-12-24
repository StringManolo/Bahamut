#ifndef CORE_HPP
#define CORE_HPP

#include <string>
#include <vector>

void hijackModules();
void listModules();
void installModule(std::string moduleFile);
void uninstallModule(std::string moduleName);
void purgeSharedDeps();
void runModule(const std::string& moduleFile, const std::vector<std::string>& args);
void runModules(const std::vector<std::string>& args);
std::vector<std::string> getModules();

#endif
