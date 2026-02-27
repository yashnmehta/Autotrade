# ========================================
# REPOSITORY LAYER - Master Contract Data Access
# ========================================

add_library(repository STATIC
    MasterFileParser.cpp
    NSEFORepository.cpp
    NSEFORepositoryPreSorted.cpp
    NSECMRepository.cpp
    BSEFORepository.cpp
    BSECMRepository.cpp
    RepositoryManager.cpp

    # Headers (for AUTOMOC)
    ${CMAKE_SOURCE_DIR}/include/repository/ScripMaster.h
    ${CMAKE_SOURCE_DIR}/include/repository/ContractData.h
    ${CMAKE_SOURCE_DIR}/include/repository/MasterFileParser.h
    ${CMAKE_SOURCE_DIR}/include/repository/NSEFORepository.h
    ${CMAKE_SOURCE_DIR}/include/repository/NSEFORepositoryPreSorted.h
    ${CMAKE_SOURCE_DIR}/include/repository/NSECMRepository.h
    ${CMAKE_SOURCE_DIR}/include/repository/BSEFORepository.h
    ${CMAKE_SOURCE_DIR}/include/repository/BSECMRepository.h
    ${CMAKE_SOURCE_DIR}/include/repository/RepositoryManager.h
)

target_link_libraries(repository PUBLIC
    Qt5::Core
)
