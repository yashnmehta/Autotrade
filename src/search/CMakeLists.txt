# ========================================
# SEARCH LAYER - Symbol Search & Tokenizer
# ========================================

add_library(search STATIC
    SearchTokenizer.cpp

    # Headers (for AUTOMOC)
    ${CMAKE_SOURCE_DIR}/include/search/SearchTokenizer.h
)

target_link_libraries(search PUBLIC
    Qt5::Core
)
