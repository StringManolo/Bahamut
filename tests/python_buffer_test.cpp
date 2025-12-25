#include "gtest/gtest.h"
#include "../core/core.hpp"
#include <fstream>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

class PythonBufferTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir = fs::temp_directory_path() / "python_buffer_test";
        modules_dir = test_dir / "modules";
        
        fs::create_directories(modules_dir);
        original_cwd = fs::current_path();
        fs::current_path(test_dir);
    }
    
    void TearDown() override {
        fs::current_path(original_cwd);
        fs::remove_all(test_dir);
    }
    
    void writeModule(const std::string& name, const std::string& content) {
        std::string path = (modules_dir / name).string();
        std::ofstream file(path);
        file << content;
        file.close();
        chmod(path.c_str(), 0755);
    }
    
    fs::path test_dir;
    fs::path modules_dir;
    fs::path original_cwd;
};

TEST_F(PythonBufferTest, PythonModuleWithoutBuffering) {
    // Crear un módulo Python que simula getbugbountydomains.py
    std::string python_code = R"(#!/usr/bin/env python3
import sys
import time

print('{"t":"batch","f":"domain"}')
sys.stdout.flush()

# Generar algunos dominios
for i in range(10):
    print(f"example{i}.com")
    sys.stdout.flush()  # IMPORTANTE: flush después de cada línea
    time.sleep(0.001)   # Pequeña pausa

print('{"t":"batch_end"}')
sys.stdout.flush()
print('{"t":"result","ok":true,"count":10}')
sys.stdout.flush()
)";
    
    writeModule("test_python.py", python_code);
    
    // Ejecutar el módulo
    std::map<std::string, std::vector<DataItem>> storage;
    std::vector<std::string> args;
    
    testing::internal::CaptureStdout();
    runModuleWithPipe("test_python.py", args, storage, "");
    std::string output = testing::internal::GetCapturedStdout();
    
    std::cout << "\n=== Python Module Test Output ===" << std::endl;
    std::cout << "Storage size: " << storage.size() << std::endl;
    
    bool has_domains = storage.find("domain") != storage.end();
    if (has_domains) {
        std::cout << "Domain items: " << storage["domain"].size() << std::endl;
        for (size_t i = 0; i < std::min(storage["domain"].size(), size_t(3)); i++) {
            std::cout << "  - " << storage["domain"][i].value << std::endl;
        }
    } else {
        std::cout << "ERROR: No 'domain' items in storage!" << std::endl;
    }
    
    EXPECT_TRUE(has_domains);
    if (has_domains) {
        EXPECT_EQ(storage["domain"].size(), 10);
    }
}

TEST_F(PythonBufferTest, TestActualGetBugBountyDomains) {
    // Este test intenta ejecutar el módulo real
    // Solo funciona si el módulo existe en el sistema real
    
    std::string real_path = findModulePath("getbugbountydomains.py");
    
    if (!real_path.empty()) {
        std::cout << "\n=== Testing real getbugbountydomains.py ===" << std::endl;
        std::cout << "Found at: " << real_path << std::endl;
        
        // Leer primeras líneas para ver el formato
        std::ifstream file(real_path);
        std::string line;
        int lines = 0;
        std::cout << "First 10 lines:" << std::endl;
        while (std::getline(file, line) && lines < 10) {
            std::cout << "  " << line << std::endl;
            lines++;
        }
        
        // Verificar si usa flush
        file.clear();
        file.seekg(0);
        std::string content((std::istreambuf_iterator<char>(file)), 
                           std::istreambuf_iterator<char>());
        
        bool has_flush = content.find("flush()") != std::string::npos ||
                         content.find("sys.stdout.flush") != std::string::npos;
        
        std::cout << "Uses flush: " << (has_flush ? "YES" : "NO") << std::endl;
        
        if (!has_flush) {
            std::cout << "\n⚠️  ADVERTENCIA: getbugbountydomains.py NO hace flush!" << std::endl;
            std::cout << "   Esto causa el problema de buffering." << std::endl;
            std::cout << "   Solución: Añadir -u al comando Python" << std::endl;
        }
        
    } else {
        std::cout << "getbugbountydomains.py not found in test environment" << std::endl;
    }
    
    // No fallar si no encuentra el módulo
    SUCCEED();
}

/*
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}*/
