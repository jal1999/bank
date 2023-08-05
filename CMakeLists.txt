cmake_minimum_required(VERSION 3.26.4)

project(BankApp 
	VERSION 1.0 
	DESCRIPTION "A banking desktop application" 
	LANGUAGES CXX)

add_executable(BankApp src/logic.cpp src/logic.hpp src/main.cpp src/logic.cpp)

target_compile_features(BankApp PRIVATE cxx_std_17)
target_compile_options(BankApp PRIVATE -Wall -Wextra -Wpedantic)

find_library(MONGO mongocxx)
find_library(BSON bsoncxx)
find_library(POCO PocoZip)
find_library(NET PocoNet)

# Telling compiler where the definitions for the included files are
target_link_libraries(BankApp PRIVATE ${MONGO} ${BSON} ${POCO} ${NET}) 

# Telling compiler where to find the declarations when we use #include
target_include_directories(BankApp PRIVATE 
	"/usr/local/include/mongocxx/v_noabi/" 
	"/usr/local/include/bsoncxx/v_noabi/"
	"/usr/local/include/wx-3.2/"
	"/usr/local/lib/wx/include/osx_cocoa-unicode-3.2/"
	"/usr/local/include/")

# Adding compiler flags
add_definitions("`wx-config --cxxflags --libs`")