# ========================================
# UNIT TESTS
# ========================================

enable_testing()
find_package(Qt5 COMPONENTS Test REQUIRED)

# ────────────────────────────────────────
# SearchTokenizer Unit Test
# ────────────────────────────────────────
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
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

if(MSVC)
    target_compile_options(test_search_tokenizer PRIVATE /W1 /FS /MP)
endif()

add_test(NAME SearchTokenizerTest COMMAND test_search_tokenizer)

# ────────────────────────────────────────
# FormulaEngine Unit Test
# Tests the recursive-descent parser, evaluator, built-in functions,
# market data functions (via mock context), and introspection.
# ────────────────────────────────────────
add_executable(test_formula_engine
    test_formula_engine.cpp
    ${CMAKE_SOURCE_DIR}/src/strategy/runtime/FormulaEngine.cpp
)

target_include_directories(test_formula_engine PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_formula_engine
    Qt5::Core
)

set_target_properties(test_formula_engine PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

if(MSVC)
    target_compile_options(test_formula_engine PRIVATE /W1 /FS /MP)
endif()

add_test(NAME FormulaEngineTest COMMAND test_formula_engine)

# ────────────────────────────────────────
# Condition Evaluation Unit Test
# Tests ConditionNode evaluation with all operand types,
# branch logic (AND/OR), crossover detection, and edge cases.
# Uses a mock evaluator that mirrors TemplateStrategy logic.
# ────────────────────────────────────────
add_executable(test_condition_evaluation
    test_condition_evaluation.cpp
    ${CMAKE_SOURCE_DIR}/src/strategy/runtime/FormulaEngine.cpp
)

target_include_directories(test_condition_evaluation PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_condition_evaluation
    Qt5::Core
)

set_target_properties(test_condition_evaluation PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

if(MSVC)
    target_compile_options(test_condition_evaluation PRIVATE /W1 /FS /MP)
endif()

add_test(NAME ConditionEvaluationTest COMMAND test_condition_evaluation)

# ────────────────────────────────────────
# Greeks & IV Calculator Unit Test
# Tests Black-Scholes Greeks (call/put, ATM/ITM/OTM, expired, zero vol),
# Put-Call parity, Newton-Raphson IV solver, Brent's fallback,
# IV round-trip, and edge cases.
# ────────────────────────────────────────
add_executable(test_greeks_iv
    test_greeks_iv.cpp
    ${CMAKE_SOURCE_DIR}/src/quant/Greeks.cpp
    ${CMAKE_SOURCE_DIR}/src/quant/IVCalculator.cpp
)

target_include_directories(test_greeks_iv PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_greeks_iv
    Qt5::Core
)

set_target_properties(test_greeks_iv PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

if(MSVC)
    target_compile_options(test_greeks_iv PRIVATE /W1 /FS /MP)
endif()

add_test(NAME GreeksIVTest COMMAND test_greeks_iv)

# ────────────────────────────────────────
# TradingDataService Unit Test
# Tests thread-safe CRUD, event-driven upsert,
# signal emission, and concurrent read/write safety.
# ────────────────────────────────────────
add_executable(test_trading_data_service
    test_trading_data_service.cpp
    ${CMAKE_SOURCE_DIR}/src/services/TradingDataService.cpp
    ${CMAKE_SOURCE_DIR}/include/services/TradingDataService.h
)

target_include_directories(test_trading_data_service PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_trading_data_service
    Qt5::Core
    Qt5::Test
)

set_target_properties(test_trading_data_service PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

if(MSVC)
    target_compile_options(test_trading_data_service PRIVATE /W1 /FS /MP)
endif()

add_test(NAME TradingDataServiceTest COMMAND test_trading_data_service)

# ────────────────────────────────────────
# MarketWatchModel Unit Test
# Tests add/remove/duplicate/blank rows, price updates,
# tick direction, column profiles, signals, and find/lookup.
# Includes a stub GreeksCalculationService singleton.
# ────────────────────────────────────────
add_executable(test_market_watch_model
    test_market_watch_model.cpp
    ${CMAKE_SOURCE_DIR}/src/models/MarketWatchModel.cpp
    ${CMAKE_SOURCE_DIR}/src/models/MarketWatchColumnProfile.cpp
    ${CMAKE_SOURCE_DIR}/include/services/GreeksCalculationService.h
    ${CMAKE_SOURCE_DIR}/include/models/qt/MarketWatchModel.h
)

target_include_directories(test_market_watch_model PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_market_watch_model
    Qt5::Core
    Qt5::Test
    Qt5::Gui
)

set_target_properties(test_market_watch_model PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

if(MSVC)
    target_compile_options(test_market_watch_model PRIVATE /W1 /FS /MP)
endif()

add_test(NAME MarketWatchModelTest COMMAND test_market_watch_model)

# ────────────────────────────────────────
# ServiceRegistry Unit Test
# Tests the DI container: register/resolve, owned/unowned,
# LIFO destruction, named services, interface resolution.
# Header-only — no source files needed.
# ────────────────────────────────────────
add_executable(test_service_registry
    test_service_registry.cpp
)

target_include_directories(test_service_registry PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_service_registry
    Qt5::Core
    Qt5::Test
)

set_target_properties(test_service_registry PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

if(MSVC)
    target_compile_options(test_service_registry PRIVATE /W1 /FS /MP)
endif()

add_test(NAME ServiceRegistryTest COMMAND test_service_registry)

# ────────────────────────────────────────
# Summary
# ────────────────────────────────────────
message(STATUS "Unit tests enabled:")
message(STATUS "  - test_search_tokenizer")
message(STATUS "  - test_formula_engine")
message(STATUS "  - test_condition_evaluation")
message(STATUS "  - test_greeks_iv")
message(STATUS "  - test_trading_data_service")
message(STATUS "  - test_market_watch_model")
message(STATUS "  - test_service_registry")
