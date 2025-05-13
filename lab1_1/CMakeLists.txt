cmake_minimum_required(VERSION 3.22.1)
project(DB_CP)

set(CMAKE_CXX_STANDARD 17)

find_package(Crow CONFIG REQUIRED)
find_package(asio CONFIG REQUIRED)
find_package(libpqxx CONFIG REQUIRED)

add_executable(start main.cpp functions.hpp middlewares.hpp)
add_executable(generate data_base_generator.cpp)
add_executable(index_test test_indexes.cpp)

target_link_libraries(start PRIVATE Crow::Crow asio::asio libpqxx::pqxx)
target_link_libraries(generate PRIVATE libpqxx::pqxx)
target_link_libraries(index_test PRIVATE libpqxx::pqxx)