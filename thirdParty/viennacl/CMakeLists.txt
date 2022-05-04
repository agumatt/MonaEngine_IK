file(GLOB_RECURSE viennaFiles "*.hpp" "*.cpp")
add_library(viennacl STATIC ${viennaFiles})
target_include_directories(viennacl PRIVATE ${THIRD_PARTY_INCLUDE_DIRECTORIES})
