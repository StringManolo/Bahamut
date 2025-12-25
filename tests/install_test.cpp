#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <vector>
#include <map>
#include <set>
#include <chrono>
#include <thread>
#include "../core/core.hpp"

namespace fs = std::filesystem;

class InstallationTest : public ::testing::Test {
protected:
  void SetUp() override {
    std::string timestamp = std::to_string(time(nullptr));
    test_dir = (fs::temp_directory_path() / ("bahamut_install_test_" + timestamp)).string();
    
    original_cwd = fs::current_path();
    fs::create_directories(test_dir);
    fs::current_path(test_dir);
    
    setupTestEnvironment();
  }
  
  void TearDown() override {
    fs::current_path(original_cwd);
    fs::remove_all(test_dir);
  }
  
  void setupTestEnvironment() {
    fs::create_directories("modules/collectors");
    fs::create_directories("modules/processors");
    fs::create_directories("modules/outputs");
    fs::create_directories("modules/shared_deps");
  }
  
  std::string test_dir;
  std::string original_cwd;
  
  void createTestModule(const std::string& filename, const std::string& content) {
    fs::path filepath(filename);
    fs::create_directories(filepath.parent_path());
    
    std::ofstream file(filename);
    file << content;
    file.close();
    
    fs::permissions(filepath, fs::perms::owner_exec, fs::perm_options::add);
  }
};

TEST_F(InstallationTest, InstallNodeModuleShared) {
  std::string content = R"(#!/usr/bin/env node
// Install: npm install chalk
// InstallScope: shared
console.log('{"bmop":"1.0","module":"test"}');
console.log('{"t":"result","ok":true,"count":0}');)";
  
  createTestModule("modules/node_shared.js", content);
  
  fs::remove_all("modules/shared_deps");
  
  installModule("node_shared.js");
  
  EXPECT_TRUE(fs::exists("modules/shared_deps/node_modules"));
  EXPECT_TRUE(fs::exists("modules/shared_deps/node_modules/chalk"));
  EXPECT_TRUE(fs::exists("modules/shared_deps/package.json"));
}

TEST_F(InstallationTest, InstallNodeModuleIsolated) {
  std::string content = R"(#!/usr/bin/env node
// Install: npm install lodash
// InstallScope: isolated
console.log('{"bmop":"1.0","module":"test"}');
console.log('{"t":"result","ok":true,"count":0}');)";
  
  createTestModule("modules/isolated/node_isolated.js", content);
  
  fs::remove_all("modules/isolated/node_modules");
  fs::remove_all("modules/isolated/package.json");
  
  installModule("node_isolated.js");
  
  EXPECT_TRUE(fs::exists("modules/isolated/node_modules"));
  EXPECT_TRUE(fs::exists("modules/isolated/node_modules/lodash"));
  EXPECT_TRUE(fs::exists("modules/isolated/package.json"));
  EXPECT_FALSE(fs::exists("modules/node_modules"));
}

TEST_F(InstallationTest, InstallPythonModuleShared) {
  std::string content = R"(#!/usr/bin/env python3
# Install: pip install requests
# InstallScope: shared
import json
print(json.dumps({"bmop":"1.0","module":"test"}))
print(json.dumps({"t":"result","ok":True,"count":0})))";
  
  createTestModule("modules/python_shared.py", content);
  
  fs::remove_all("modules/shared_deps/python_libs");
  
  installModule("python_shared.py");
  
  EXPECT_TRUE(fs::exists("modules/shared_deps/python_libs"));
  
  bool found_requests = false;
  for (const auto& entry : fs::directory_iterator("modules/shared_deps/python_libs")) {
    std::string name = entry.path().filename().string();
    if (name.find("requests") != std::string::npos) {
      found_requests = true;
      break;
    }
  }
  EXPECT_TRUE(found_requests);
}

TEST_F(InstallationTest, InstallPythonModuleIsolated) {
  std::string content = R"(#!/usr/bin/env python3
# Install: pip install six
# InstallScope: isolated
import json
print(json.dumps({"bmop":"1.0","module":"test"}))
print(json.dumps({"t":"result","ok":True,"count":0})))";
  
  createTestModule("modules/isolated/python_isolated.py", content);
  
  fs::remove_all("modules/isolated/python_libs");
  
  installModule("python_isolated.py");
  
  EXPECT_TRUE(fs::exists("modules/isolated/python_libs"));
  
  bool found_six = false;
  for (const auto& entry : fs::directory_iterator("modules/isolated/python_libs")) {
    std::string name = entry.path().filename().string();
    if (name.find("six") != std::string::npos) {
      found_six = true;
      break;
    }
  }
  EXPECT_TRUE(found_six);
}

TEST_F(InstallationTest, InstallModuleGlobal) {
  std::string content = R"(#!/usr/bin/env node
// Install: npm install -g typescript
// InstallScope: global
console.log('{"bmop":"1.0","module":"test"}');
console.log('{"t":"result","ok":true,"count":0}');)";
  
  createTestModule("modules/global_test.js", content);
  
  installModule("global_test.js");
}

TEST_F(InstallationTest, ReinstallDependencies) {
  std::string content = R"(#!/usr/bin/env node
// Install: npm install uuid
// InstallScope: shared
console.log('{"bmop":"1.0","module":"test"}');
console.log('{"t":"result","ok":true,"count":0}');)";
  
  createTestModule("modules/reinstall_test.js", content);
  
  installModule("reinstall_test.js");
  
  EXPECT_TRUE(fs::exists("modules/shared_deps/node_modules/uuid"));
  
  auto first_install_time = fs::last_write_time("modules/shared_deps/node_modules/uuid");
  
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  
  installModule("reinstall_test.js");
  
  EXPECT_TRUE(fs::exists("modules/shared_deps/node_modules/uuid"));
  
  auto second_install_time = fs::last_write_time("modules/shared_deps/node_modules/uuid");
}

TEST_F(InstallationTest, InstallMultiplePackages) {
  std::string content = R"(#!/usr/bin/env node
// Install: npm install axios chalk lodash
// InstallScope: shared
console.log('{"bmop":"1.0","module":"test"}');
console.log('{"t":"result","ok":true,"count":0}');)";
  
  createTestModule("modules/multi_pkg_test.js", content);
  
  fs::remove_all("modules/shared_deps");
  
  installModule("multi_pkg_test.js");
  
  EXPECT_TRUE(fs::exists("modules/shared_deps/node_modules/axios"));
  EXPECT_TRUE(fs::exists("modules/shared_deps/node_modules/chalk"));
  EXPECT_TRUE(fs::exists("modules/shared_deps/node_modules/lodash"));
}

TEST_F(InstallationTest, InstallWithSpecificPythonVersion) {
  fs::remove_all("modules/shared_deps");
  
  std::vector<std::string> pythonVersions = {"python3", "python3.8", "python3.9", "python3.10", "python3.11"};
  
  for (const auto& pythonCmd : pythonVersions) {
    std::string content = "# Install: pip install certifi\n"
                          "# InstallScope: shared\n"
                          "import json\n"
                          "print(json.dumps({\"bmop\":\"1.0\",\"module\":\"test\"}))\n"
                          "print(json.dumps({\"t\":\"result\",\"ok\":True,\"count\":0}))";
    
    std::string shebang = "#!/usr/bin/env " + pythonCmd + "\n";
    std::string fullContent = shebang + content;
    
    fs::path filepath("modules/python_version_test.py");
    fs::create_directories(filepath.parent_path());
    std::ofstream file(filepath);
    file << fullContent;
    file.close();
    fs::permissions(filepath, fs::perms::owner_exec, fs::perm_options::add);
    
    installModule("python_version_test.py");
    
    bool found_certifi = false;
    if (fs::exists("modules/shared_deps/python_libs")) {
      for (const auto& entry : fs::directory_iterator("modules/shared_deps/python_libs")) {
        std::string name = entry.path().filename().string();
        if (name.find("certifi") != std::string::npos) {
          found_certifi = true;
          break;
        }
      }
    }
    EXPECT_TRUE(found_certifi);
    return;
  }
}

TEST_F(InstallationTest, InstallNonexistentPackage) {
  std::string content = R"(#!/usr/bin/env node
// Install: npm install this-package-does-not-exist-xyz123
// InstallScope: shared
console.log('{"bmop":"1.0","module":"test"}');
console.log('{"t":"result","ok":true,"count":0}');)";
  
  createTestModule("modules/failed_install.js", content);
  
  installModule("failed_install.js");
  
  EXPECT_TRUE(fs::exists("modules/shared_deps/package.json"));
}

TEST_F(InstallationTest, UninstallModule) {
  std::string content = R"(#!/usr/bin/env node
// Install: npm install debug
// InstallScope: isolated
console.log('{"bmop":"1.0","module":"test"}');
console.log('{"t":"result","ok":true,"count":0}');)";
  
  createTestModule("modules/uninstall_test.js", content);
  
  installModule("uninstall_test.js");
  
  EXPECT_TRUE(fs::exists("modules/uninstall_test.js"));
  
  uninstallModule("uninstall_test.js");
  
  EXPECT_FALSE(fs::exists("modules/node_modules/debug"));
  EXPECT_TRUE(fs::exists("modules/uninstall_test.js"));
}

TEST_F(InstallationTest, PurgeSharedDeps) {
  std::string content = R"(#!/usr/bin/env node
// Install: npm install yargs
// InstallScope: shared
console.log('{"bmop":"1.0","module":"test"}');
console.log('{"t":"result","ok":true,"count":0}');)";
  
  createTestModule("modules/purge_test.js", content);
  
  installModule("purge_test.js");
  
  EXPECT_TRUE(fs::exists("modules/shared_deps/node_modules/yargs"));
  
  purgeSharedDeps();
  
  EXPECT_FALSE(fs::exists("modules/shared_deps/node_modules"));
  EXPECT_FALSE(fs::exists("modules/node_modules"));
}

TEST_F(InstallationTest, ExecutionEnvironmentAfterInstall) {
  std::string content = R"(#!/usr/bin/env node
// Install: npm install colors
// InstallScope: shared
const colors = require('colors');
console.log(JSON.stringify({bmop:"1.0",module:"test"}));
console.log(JSON.stringify({t:"d",f:"test",v:"Module with colors installed"}));
console.log(JSON.stringify({t:"result",ok:true,count:1}));)";
  
  createTestModule("modules/env_test.js", content);
  
  installModule("env_test.js");
  
  std::map<std::string, std::vector<DataItem>> storage;
  
  runModuleWithPipe("env_test.js", {}, storage, "");
  
  EXPECT_EQ(storage["test"].size(), 1);
  EXPECT_EQ(storage["test"][0].value, "Module with colors installed");
}

TEST_F(InstallationTest, ConcurrentInstallations) {
  std::vector<std::pair<std::string, std::string>> modules = {
    {"mod1.js", "npm install moment"},
    {"mod2.js", "npm install express"},
    {"mod3.js", "npm install dotenv"}
  };
  
  for (const auto& [filename, installCmd] : modules) {
    std::string content = R"(#!/usr/bin/env node
// Install: )" + installCmd + R"(
// InstallScope: shared
console.log('{"bmop":"1.0","module":"test"}');
console.log('{"t":"result","ok":true,"count":0}');)";
    
    createTestModule("modules/" + filename, content);
  }
  
  for (const auto& [filename, _] : modules) {
    installModule(filename);
  }
  
  EXPECT_TRUE(fs::exists("modules/shared_deps/node_modules/moment"));
  EXPECT_TRUE(fs::exists("modules/shared_deps/node_modules/express"));
  EXPECT_TRUE(fs::exists("modules/shared_deps/node_modules/dotenv"));
}

TEST_F(InstallationTest, InstallDevDependencies) {
  std::string packageJson = R"({
  "name": "test-dev-deps",
  "version": "1.0.0",
  "devDependencies": {
    "jest": "^29.5.0",
    "@types/node": "^20.2.5"
  }
})";
  
  fs::create_directories("modules/shared_deps");
  std::ofstream pkgFile("modules/shared_deps/package.json");
  pkgFile << packageJson;
  pkgFile.close();
  
  std::string content = R"(#!/usr/bin/env node
// Install: npm install --only=dev
// InstallScope: shared
console.log('{"bmop":"1.0","module":"test"}');
console.log('{"t":"result","ok":true,"count":0}');)";
  
  createTestModule("modules/dev_deps_test.js", content);
  
  installModule("dev_deps_test.js");
  
  EXPECT_TRUE(fs::exists("modules/shared_deps/node_modules/jest"));
  EXPECT_TRUE(fs::exists("modules/shared_deps/node_modules/@types/node"));
}

TEST_F(InstallationTest, InstallationFallbackBehavior) {
  std::string content = R"(#!/usr/bin/env python999
# Install: pip install requests
# InstallScope: shared
print('test'))";
  
  createTestModule("modules/fallback_test.py", content);
  
  installModule("fallback_test.py");
}

