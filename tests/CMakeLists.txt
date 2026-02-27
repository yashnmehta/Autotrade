# ========================================
# UNIT TESTS
# ========================================

enable_testing()
find_package(Qt5 COMPONENTS Test REQUIRED)

# SearchTokenizer Unit Test
add_executable(test_search_tokenizer
    test_search_tokenizer_simple.cpp
    ${CMAKE_SOURCE_DIR}/src/search/SearchTokenizer.cpp
)

target_include_directories(test_search_tokenizer PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/search
)

target_link_libraries(test_search_tokenizer
    Qt5::Core
    Qt5::Test
)

set_target_properties(test_search_tokenizer PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

if(MSVC)
    target_compile_options(test_search_tokenizer PRIVATE /W1 /FS /MP)
endif()

add_test(NAME SearchTokenizerTest COMMAND test_search_tokenizer)

message(STATUS "Unit tests enabled - test_search_tokenizer added")
