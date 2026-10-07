cmake_minimum_required(VERSION 3.20)
project(personal_loot_transaction_tests LANGUAGES CXX)
enable_testing()
add_executable(personal_loot_transaction_tests checked_transaction_test.cpp)
target_compile_features(personal_loot_transaction_tests PRIVATE cxx_std_20)
set(PERSONAL_LOOT_CORE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../.." CACHE PATH "Patched AzerothCore checkout")
target_include_directories(personal_loot_transaction_tests PRIVATE
    "${PERSONAL_LOOT_CORE_PATH}/src/server/database/Database")
add_test(NAME personal_loot_transaction_outcomes COMMAND personal_loot_transaction_tests)
