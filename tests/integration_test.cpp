#include "gtest/gtest.h"
#include "../core/core.hpp"
#include <fstream>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Configurar entorno de prueba más completo
        test_root = fs::temp_directory_path() / "bahamut_integration";
        
        // Crear estructura completa
        modules_dir = test_root / "modules";
        profiles_dir = test_root / "profiles";
        shared_deps = test_root / "modules" / "shared_deps";
        
        fs::create_directories(modules_dir);
        fs::create_directories(profiles_dir);
        fs::create_directories(shared_deps);
        
        // Cambiar al directorio de prueba
        old_cwd = fs::current_path();
        fs::current_path(test_root);
        
        // Configurar variables de entorno de prueba
        old_node_path = getenv("NODE_PATH");
        old_python_path = getenv("PYTHONPATH");
    }
    
    void TearDown() override {
        // Restaurar entorno
        fs::current_path(old_cwd);
        
        if (old_node_path) {
            setenv("NODE_PATH", old_node_path, 1);
        } else {
            unsetenv("NODE_PATH");
        }
        
        if (old_python_path) {
            setenv("PYTHONPATH", old_python_path, 1);
        } else {
            unsetenv("PYTHONPATH");
        }
        
        // Limpiar
        fs::remove_all(test_root);
    }
    
    fs::path test_root;
    fs::path modules_dir;
    fs::path profiles_dir;
    fs::path shared_deps;
    fs::path old_cwd;
    const char* old_node_path;
    const char* old_python_path;
};

TEST_F(IntegrationTest, ModuleDependencyChain) {
    // Crear una cadena de módulos que se pasan datos
    std::string producer = R"(#!/usr/bin/env python3
# Name: producer
# Description: Produces test data
# Type: py
# Stage: 1
# Provides: numbers

import json
import sys

# Salida en formato BMOP
for i in range(3):
    data = {"t": "d", "f": "numbers", "v": str(i)}
    print(json.dumps(data))
    sys.stdout.flush())";
    
    std::string consumer = R"(#!/usr/bin/env python3
# Name: consumer
# Description: Consumes and processes data
# Type: py
# Stage: 2
# Consumes: numbers
# Provides: squares

import json
import sys

# Leer datos de entrada
for line in sys.stdin:
    try:
        data = json.loads(line.strip())
        if data.get("f") == "numbers":
            num = int(data.get("v", 0))
            squared = num * num
            result = {"t": "d", "f": "squares", "v": str(squared)}
            print(json.dumps(result))
            sys.stdout.flush()
    except:
        pass)";
    
    // Escribir módulos
    std::ofstream(modules_dir / "producer.py") << producer;
    std::ofstream(modules_dir / "consumer.py") << consumer;
    
    // Hacer ejecutables
    chmod((modules_dir / "producer.py").c_str(), 0755);
    chmod((modules_dir / "consumer.py").c_str(), 0755);
    
    // Crear perfil
    std::ofstream(profiles_dir / "bahamut_chain.txt") << "producer.py\nconsumer.py";
    
    // Ejecutar perfil
    // Nota: Esta es una prueba de integración real que requiere Python3
    // Puede que necesites desactivarla en CI sin Python
    if (system("python3 --version") == 0) {
        // Ejecutar los módulos
        std::vector<std::string> args;
        std::map<std::string, std::vector<DataItem>> storage;
        
        // Ejecutar productor
        runModuleWithPipe("producer.py", args, storage, "");
        
        // Verificar que el productor generó datos
        EXPECT_GE(storage["numbers"].size(), 1);
        
        // Ejecutar consumidor
        runModuleWithPipe("consumer.py", args, storage, "numbers");
        
        // Verificar que el consumidor procesó los datos
        EXPECT_GE(storage["squares"].size(), 1);
    } else {
        GTEST_SKIP() << "Python3 no está disponible para esta prueba";
    }
}

TEST_F(IntegrationTest, CrossLanguageCommunication) {
    // Probar comunicación entre módulos de diferentes lenguajes
    std::string js_module = R"(#!/usr/bin/env node
// Name: js-generator
// Description: JavaScript data generator
// Type: js
// Stage: 1
// Provides: jsondata

console.log(JSON.stringify({t: "d", f: "jsondata", v: "{\"from\":\"javascript\"}"}));)";
    
    std::string py_module = R"(#!/usr/bin/env python3
# Name: py-processor
# Description: Python data processor
# Type: py
# Stage: 2
# Consumes: jsondata
# Provides: processed

import json
import sys

for line in sys.stdin:
    try:
        data = json.loads(line.strip())
        if data.get("f") == "jsondata":
            original = json.loads(data.get("v", "{}"))
            original["processed_by"] = "python"
            result = {"t": "d", "f": "processed", "v": json.dumps(original)}
            print(json.dumps(result))
    except:
        pass)";
    
    std::ofstream(modules_dir / "generator.js") << js_module;
    std::ofstream(modules_dir / "processor.py") << py_module;
    
    chmod((modules_dir / "generator.js").c_str(), 0755);
    chmod((modules_dir / "processor.py").c_str(), 0755);
    
    // Verificar que Node y Python están disponibles
    bool node_available = system("node --version >/dev/null 2>&1") == 0;
    bool python_available = system("python3 --version >/dev/null 2>&1") == 0;
    
    if (node_available && python_available) {
        std::vector<std::string> args;
        std::map<std::string, std::vector<DataItem>> storage;
        
        runModuleWithPipe("generator.js", args, storage, "");
        runModuleWithPipe("processor.py", args, storage, "jsondata");
        
        // Verificar que hubo comunicación
        EXPECT_FALSE(storage["jsondata"].empty());
        EXPECT_FALSE(storage["processed"].empty());
    } else {
        GTEST_SKIP() << "Node.js o Python3 no están disponibles";
    }
}
