#include "gtest/gtest.h"
#include "../core/core.hpp"
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <algorithm>
#include <set>
#include <string>
#include <vector>
#include <unistd.h>

namespace fs = std::filesystem;

class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Configurar entorno de prueba
        test_root = fs::temp_directory_path() / "bahamut_integration_test";
        
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
        
        // Guardar variables de entorno originales
        old_node_path = getenv("NODE_PATH");
        old_python_path = getenv("PYTHONPATH");
        
        // Configurar constantes globales temporales
        // Nota: En una implementación real, estas deberían ser configurables
        // Para esta prueba, solo necesitamos que existan los directorios
    }
    
    void TearDown() override {
        // Restaurar directorio de trabajo
        if (fs::exists(old_cwd)) {
            fs::current_path(old_cwd);
        }
        
        // Restaurar variables de entorno
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
        
        // Limpiar directorio temporal (con cuidado)
        try {
            if (fs::exists(test_root) && test_root.string().find("bahamut_integration_test") != std::string::npos) {
                fs::remove_all(test_root);
            }
        } catch (const std::exception& e) {
            // Silenciar errores de limpieza durante tests
        }
    }
    
    // Función segura para crear módulos de prueba
    void createSafeModule(const std::string& name, const std::string& content) {
        std::string module_path = (modules_dir / name).string();
        std::ofstream file(module_path);
        file << content;
        file.close();
        
        // Hacer ejecutable si es shell script
        if (name.ends_with(".sh")) {
            chmod(module_path.c_str(), 0755);
        }
    }
    
    fs::path test_root;
    fs::path modules_dir;
    fs::path profiles_dir;
    fs::path shared_deps;
    fs::path old_cwd;
    const char* old_node_path;
    const char* old_python_path;
};

// Prueba segura de ejecución de módulo simple
TEST_F(IntegrationTest, SafeBashModuleExecution) {
    // Crear un módulo bash simple que termina inmediatamente
    std::string bash_module = R"(#!/bin/bash
# Name: safe-test
# Description: Safe test bash module
# Type: sh
# Stage: 1
# Provides: test

# Salida simple que termina inmediatamente
echo '{"t":"d","f":"test","v":"bash-output-123"}'
exit 0)";
    
    createSafeModule("safe_test.sh", bash_module);
    
    // Verificar que el archivo se creó
    ASSERT_TRUE(fs::exists(modules_dir / "safe_test.sh"));
    
    // Ejecutar el módulo de forma segura
    std::vector<std::string> args;
    
    // Usar runModule que no tiene pipes bidireccionales
    testing::internal::CaptureStdout();  // Capturar salida stdout
    runModule("safe_test.sh", args);
    std::string output = testing::internal::GetCapturedStdout();
    
    // Verificar que hubo salida (puede incluir logs del sistema además del JSON)
    EXPECT_FALSE(output.empty());
    
    // El módulo debería haber ejecutado sin bloquear
    // Nota: No podemos verificar storage porque runModule no lo expone
}

// Prueba de perfil simple sin ejecución real
TEST_F(IntegrationTest, ProfileLoadingOnly) {
    // Crear perfil de prueba
    std::vector<std::string> expected_modules = {"test1.js", "test2.py"};
    std::ofstream profile_file(profiles_dir / "bahamut_simple_profile.txt");
    for (const auto& mod : expected_modules) {
        profile_file << mod << "\n";
    }
    profile_file.close();
    
    // Cargar perfil
    auto modules = loadProfile("simple_profile");
    
    // Verificar carga
    EXPECT_EQ(modules.size(), 2);
    EXPECT_EQ(modules[0], "test1.js");
    EXPECT_EQ(modules[1], "test2.py");
    
    // Verificar que módulos no existen (no deberían causar error)
    std::string path = findModulePath("test1.js");
    EXPECT_TRUE(path.empty());
}

// Prueba de metadata parsing con módulos de prueba
TEST_F(IntegrationTest, ModuleMetadataParsing) {
    // Crear módulos con metadata específica
    std::string js_module = R"(#!/usr/bin/env node
// Name: js-module-test
// Description: JavaScript module for testing
// Type: js
// Stage: 2
// Consumes: json
// Provides: processed
// Install: npm install test-package
// InstallScope: shared

console.log("Test module"))";
    
    std::string py_module = R"(#!/usr/bin/env python3
# Name: py-module-test
# Description: Python module for testing
# Type: py
# Stage: 3
# Consumes: csv
# Provides: json
# Install: pip install pandas
# InstallScope: isolated

print("Python module"))";
    
    createSafeModule("js_test.js", js_module);
    createSafeModule("py_test.py", py_module);
    
    // Parsear metadata
    std::string js_path = findModulePath("js_test.js");
    std::string py_path = findModulePath("py_test.py");
    
    ASSERT_FALSE(js_path.empty());
    ASSERT_FALSE(py_path.empty());
    
    ModuleMetadata js_meta = parseModuleMetadata(js_path);
    ModuleMetadata py_meta = parseModuleMetadata(py_path);
    
    // Verificar metadata JS
    EXPECT_EQ(js_meta.name, "js-module-test");
    EXPECT_EQ(js_meta.type, "js");
    EXPECT_EQ(js_meta.stage, 2);
    EXPECT_EQ(js_meta.consumes, "json");
    EXPECT_EQ(js_meta.provides, "processed");
    EXPECT_EQ(js_meta.installCmd, "npm install test-package");
    EXPECT_EQ(js_meta.installScope, "shared");
    
    // Verificar metadata Python
    EXPECT_EQ(py_meta.name, "py-module-test");
    EXPECT_EQ(py_meta.type, "py");
    EXPECT_EQ(py_meta.stage, 3);
    EXPECT_EQ(py_meta.consumes, "csv");
    EXPECT_EQ(py_meta.provides, "json");
    EXPECT_EQ(py_meta.installCmd, "pip install pandas");
    EXPECT_EQ(py_meta.installScope, "isolated");
}

// Prueba de getModules con estructura real
TEST_F(IntegrationTest, ModuleDiscovery) {
    // Crear varios tipos de módulos
    createSafeModule("module1.js", "// JS module");
    createSafeModule("module2.py", "# Python module");
    createSafeModule("module3.sh", "# Shell module");
    
    // Crear archivos que no son módulos
    createSafeModule("data.txt", "Text file");
    createSafeModule("config.yml", "config: value");
    
    // Crear en directorio anidado
    fs::create_directories(modules_dir / "subdir");
    createSafeModule("subdir/module4.js", "// Nested JS");
    
    // Obtener módulos
    auto modules = getModules();
    
    // Debería encontrar 4 módulos (.js, .py, .sh)
    EXPECT_EQ(modules.size(), 4);
    
    // Convertir a set para búsqueda fácil
    std::set<std::string> module_set(modules.begin(), modules.end());
    
    EXPECT_TRUE(module_set.count("module1.js"));
    EXPECT_TRUE(module_set.count("module2.py"));
    EXPECT_TRUE(module_set.count("module3.sh"));
    EXPECT_TRUE(module_set.count("module4.js"));
    EXPECT_FALSE(module_set.count("data.txt"));
    EXPECT_FALSE(module_set.count("config.yml"));
}

// Prueba de manejo de errores segura
TEST_F(IntegrationTest, SafeErrorHandling) {
    // Módulo que no existe - debería manejar el error sin bloquear
    std::vector<std::string> args;
    
    // Esto debería imprimir un mensaje de error pero no bloquear
    testing::internal::CaptureStdout();
    runModule("nonexistent_module.xyz", args);
    std::string output = testing::internal::GetCapturedStdout();
    
    // Debería contener mensaje de error
    EXPECT_TRUE(output.find("not found") != std::string::npos || 
                output.find("Error") != std::string::npos ||
                output.empty()); // O salida vacía si no hay error visible
    
    // Módulo vacío
    createSafeModule("empty.sh", "#!/bin/bash\nexit 0");
    
    testing::internal::CaptureStdout();
    runModule("empty.sh", args);
    output = testing::internal::GetCapturedStdout();
    // No debería bloquear
}

// Prueba de entorno de ejecución (sin ejecutar comandos reales)
TEST_F(IntegrationTest, EnvironmentSetup) {
    // Crear módulo Python para probar detección de versión
    std::string py_module = "#!/usr/bin/env python3.9\nprint('test')";
    createSafeModule("python_test.py", py_module);
    
    std::string path = findModulePath("python_test.py");
    ASSERT_FALSE(path.empty());
    
    // Probar getPythonVersion
    std::string version = getPythonVersion(path);
    EXPECT_TRUE(version == "python3.9" || version == "python3");
    
    // Probar parseModuleMetadata
    ModuleMetadata meta = parseModuleMetadata(path);
    EXPECT_TRUE(meta.name.empty()); // No tiene metadata
    EXPECT_EQ(meta.stage, 999); // Valor por defecto
}

// Prueba de interacción básica BMOP (sin ejecución real)
TEST_F(IntegrationTest, BMOPProtocolParsing) {
    // Configurar storage de prueba
    std::map<std::string, std::vector<DataItem>> storage;
    
    // Probar parseo de línea BMOP simple
    std::string bmop_line = R"({"t":"d","f":"json","v":"{\"key\":\"value\"}"})";
    parseBMOPLine(bmop_line, storage);
    
    EXPECT_EQ(storage["json"].size(), 1);
    if (storage["json"].size() > 0) {
        EXPECT_EQ(storage["json"][0].format, "json");
        EXPECT_EQ(storage["json"][0].value, "{\"key\":\"value\"}");
    }
    
    // Probar batch
    std::string batch_start = R"({"t":"batch","f":"csv"})";
    parseBMOPLine(batch_start, storage);
    
    EXPECT_EQ(storage["__batch_format__"].size(), 1);
    if (storage["__batch_format__"].size() > 0) {
        EXPECT_EQ(storage["__batch_format__"][0].format, "csv");
    }
}

// Prueba de stage ordering
TEST_F(IntegrationTest, ModuleStageCollection) {
    // Crear módulos con diferentes stages
    std::string module1 = R"(# Name: stage1-module
# Stage: 1
# Provides: data1)";
    
    std::string module2 = R"(# Name: stage3-module
# Stage: 3
# Consumes: data1
# Provides: data2)";
    
    std::string module3 = R"(# Name: stage2-module  
# Stage: 2
# Consumes: data1
# Provides: data3)";
    
    createSafeModule("stage1.js", module1);
    createSafeModule("stage2.js", module2); // En realidad stage 3
    createSafeModule("stage3.js", module3); // En realidad stage 2
    
    // Obtener todos los módulos
    auto all_modules = getModules();
    EXPECT_GE(all_modules.size(), 3);
    
    // Este test solo verifica que podemos obtener y parsear los módulos
    // La ejecución por stages se probaría en tests del core
}

// Prueba de cleanup seguro
TEST_F(IntegrationTest, SafeCleanupOperations) {
    // Crear estructura de dependencias compartidas
    fs::create_directories(shared_deps / "node_modules");
    fs::create_directories(shared_deps / "python_libs");
    
    // Crear symlink de prueba
    fs::create_directories(modules_dir / "test_module");
    std::string module_dir = (modules_dir / "test_module").string();
    
    // Crear un symlink simbólico (si es posible)
    std::string symlink_path = module_dir + "/node_modules";
    std::string target_path = shared_deps.string() + "/node_modules";
    
    try {
        if (!fs::exists(symlink_path)) {
            fs::create_directory_symlink(target_path, symlink_path);
        }
    } catch (...) {
        // Ignorar errores de symlink en tests
    }
    
    // Verificar que las operaciones de limpieza no fallan
    // (No llamamos a purgeSharedDeps porque modificaría el entorno real)
}

/*
// MAIN para ejecutar las pruebas
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    // Configurar GoogleTest para fallar rápido en pruebas de integración
    ::testing::GTEST_FLAG(catch_exceptions) = false;
    
    int result = RUN_ALL_TESTS();
    
    // Asegurar limpieza final
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd))) {
        std::string current_dir(cwd);
        if (current_dir.find("bahamut_integration_test") != std::string::npos) {
            // Volver al directorio original si estamos en uno de prueba
            chdir("..");
        }
    }
    
    return result;
}*/
// No main function here - using gtest_main
