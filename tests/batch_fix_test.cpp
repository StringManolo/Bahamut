#include "gtest/gtest.h"
#include "../core/core.hpp"
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

class BatchFixTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir = fs::temp_directory_path() / "bahamut_batch_fix";
        modules_dir = test_dir / "modules";
        
        fs::create_directories(modules_dir);
        original_cwd = fs::current_path();
        fs::current_path(test_dir);
    }
    
    void TearDown() override {
        fs::current_path(original_cwd);
        fs::remove_all(test_dir);
    }
    

    fs::path test_dir;
    fs::path modules_dir;
    fs::path original_cwd;
};



// Test CRÍTICO: Verificar que collectModuleOutput guarda datos de batch
TEST_F(BatchFixTest, CollectModuleOutputSavesBatchData) {
    std::map<std::string, std::vector<DataItem>> storage;
    
    // Crear archivo con batch de datos
    std::string temp_file = (test_dir / "test_batch.txt").string();
    std::ofstream file(temp_file);
    
    // Datos EXACTAMENTE como los produce getbugbountydomains.py
    file << R"({"t":"batch","f":"domain"})" << "\n";
    file << "example1.com\n";
    file << "example2.com\n";
    file << "example3.com\n";
    file << R"({"t":"batch_end"})" << "\n";
    file << R"({"t":"result","ok":true,"count":3})" << "\n";
    
    file.close();
    
    FILE* pipe = fopen(temp_file.c_str(), "r");
    ASSERT_NE(pipe, nullptr);
    
    collectModuleOutput("test", pipe, storage);
    fclose(pipe);
    
    // DEBUG: Mostrar qué hay en storage
    std::cout << "\n=== DEBUG Batch Data Storage ===" << std::endl;
    std::cout << "Total storage entries: " << storage.size() << std::endl;
    
    for (const auto& [format, items] : storage) {
        std::cout << "Format: '" << format << "' -> " << items.size() << " items" << std::endl;
        for (size_t i = 0; i < std::min(items.size(), size_t(5)); i++) {
            std::cout << "  [" << i << "]: " << items[i].value << std::endl;
        }
    }
    
    // VERIFICACIÓN CRÍTICA: ¿Se guardaron los dominios?
    EXPECT_EQ(storage["domain"].size(), 3) 
        << "¡collectModuleOutput NO está guardando los datos del batch!";
    
    if (storage["domain"].size() >= 3) {
        EXPECT_EQ(storage["domain"][0].value, "example1.com");
        EXPECT_EQ(storage["domain"][1].value, "example2.com");
        EXPECT_EQ(storage["domain"][2].value, "example3.com");
    }
    
    std::remove(temp_file.c_str());
}

// Test: Simulación completa del problema real
TEST_F(BatchFixTest, FullReconWorkflow) {
    // 1. Crear módulo que produce batch (como getbugbountydomains.py)
    std::string batch_producer = R"(#!/bin/bash
echo '{"t":"batch","f":"domain"}'
echo "hackerone.com"
echo "bugcrowd.com" 
echo "intigriti.com"
echo '{"t":"batch_end"}'
echo '{"t":"result","ok":true,"count":3}'
)";
    
    std::string producer_path = (modules_dir / "batch_producer.sh").string();
    std::ofstream prod_file(producer_path);
    prod_file << batch_producer;
    prod_file.close();
    chmod(producer_path.c_str(), 0755);
    
    // 2. Ejecutar productor y capturar datos
    std::map<std::string, std::vector<DataItem>> storage;
    std::vector<std::string> args;
    
    testing::internal::CaptureStdout();
    runModuleWithPipe("batch_producer.sh", args, storage, "");
    std::string output = testing::internal::GetCapturedStdout();
    
    std::cout << "\n=== Producer Output ===" << std::endl;
    std::cout << output << std::endl;
    
    std::cout << "\n=== Storage After Producer ===" << std::endl;
    std::cout << "domain items: " << storage["domain"].size() << std::endl;
    for (const auto& item : storage["domain"]) {
        std::cout << "  - " << item.value << std::endl;
    }
    
    // VERIFICACIÓN: Los dominios deben estar en storage
    ASSERT_EQ(storage["domain"].size(), 3) 
        << "Los dominios del batch deberían estar en storage";
    
    // 3. Crear consumidor (como cleanwildcards.js)
    std::string batch_consumer = R"(#!/bin/bash
echo '{"bmop":"1.0","module":"test-consumer","pid":$$}'

count=0
while IFS= read -r line; do
    if [[ -n "$line" ]]; then
        ((count++))
        echo "{\"t\":\"log\",\"l\":\"debug\",\"m\":\"Received: $line\"}"
    fi
done

echo "{\"t\":\"log\",\"l\":\"info\",\"m\":\"Total received: $count\"}"
if [[ $count -eq 0 ]]; then
    echo '{"t":"log","l":"error","m":"PROBLEMA: No recibió datos!"}'
fi
echo "{\"t\":\"result\",\"ok\":true,\"count\":$count}"
)";
    
    std::string consumer_path = (modules_dir / "batch_consumer.sh").string();
    std::ofstream cons_file(consumer_path);
    cons_file << batch_consumer;
    cons_file.close();
    chmod(consumer_path.c_str(), 0755);
    
    // 4. Ejecutar consumidor con los datos
    testing::internal::CaptureStdout();
    runModuleWithPipe("batch_consumer.sh", args, storage, "domain");
    std::string consumer_output = testing::internal::GetCapturedStdout();
    
    std::cout << "\n=== Consumer Output ===" << std::endl;
    std::cout << consumer_output << std::endl;
    
    // VERIFICACIÓN FINAL: El consumidor debe recibir 3 items
    EXPECT_TRUE(consumer_output.find("Total received: 3") != std::string::npos)
        << "Consumer debería haber recibido 3 items, pero recibió: " 
        << (consumer_output.find("Total received:") != std::string::npos ? 
            "ver output" : "Nada");
    
    EXPECT_TRUE(consumer_output.find("PROBLEMA: No recibió datos!") == std::string::npos)
        << "¡El consumidor no recibió ningún dato!";
}

// Test: Verificar parseBMOPLine para batch
TEST_F(BatchFixTest, ParseBMOPLineBatch) {
    std::map<std::string, std::vector<DataItem>> storage;
    
    // Probar línea de inicio de batch
    parseBMOPLine(R"({"t":"batch","f":"domain"})", storage);
    
    std::cout << "\n=== After parseBMOPLine batch start ===" << std::endl;
    std::cout << "__batch_format__ items: " << storage["__batch_format__"].size() << std::endl;
    if (!storage["__batch_format__"].empty()) {
        std::cout << "Batch format: " << storage["__batch_format__"][0].format << std::endl;
        std::cout << "Batch value: " << storage["__batch_format__"][0].value << std::endl;
    }
    
    EXPECT_EQ(storage["__batch_format__"].size(), 1);
    EXPECT_EQ(storage["__batch_format__"][0].format, "domain");
    
    // Probar línea de datos regular
    storage.clear();
    parseBMOPLine(R"({"t":"d","f":"json","v":"test"})", storage);
    EXPECT_EQ(storage["json"].size(), 1);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    std::cout << "\n🔧 EJECUTANDO TESTS DE CORRECCIÓN DE BATCH" << std::endl;
    std::cout << "========================================" << std::endl;
    return RUN_ALL_TESTS();
}
