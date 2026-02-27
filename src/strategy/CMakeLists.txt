# ========================================
# STRATEGY LAYER - Template Builder + Runtime + Manager + Persistence
# ========================================

add_library(strategy_engine STATIC
    # Runtime: execution engines, live trading logic
    runtime/StrategyBase.cpp
    runtime/StrategyFactory.cpp
    runtime/TemplateStrategy.cpp
    runtime/LiveFormulaContext.cpp
    runtime/FormulaEngine.cpp
    runtime/IndicatorEngine.cpp
    runtime/OrderExecutionEngine.cpp
    # runtime/OptionsExecutionEngine.cpp  # POC — disabled

    # Builder: template authoring UI
    builder/StrategyTemplateBuilderDialog.cpp
    builder/ConditionBuilderWidget.cpp
    builder/IndicatorRowWidget.cpp
    builder/IndicatorCatalog.cpp
    builder/IndicatorPickerDialog.cpp
    builder/ParamEditorDialog.cpp
    builder/StrategyDeployDialog.cpp
    builder/SymbolBindingWidget.cpp
    builder/TemplateManagerDialog.cpp

    # Manager: deployed instances dashboard
    manager/StrategyManagerWindow.cpp
    manager/CreateStrategyDialog.cpp
    manager/ModifyParametersDialog.cpp
    manager/StrategyTableModel.cpp
    manager/StrategyFilterProxyModel.cpp
    manager/StrategyService.cpp

    # Persistence: data access layer
    persistence/StrategyTemplateRepository.cpp
    persistence/StrategyRepository.cpp

    # Headers (for AUTOMOC)
    # Model headers (header-only)
    ${CMAKE_SOURCE_DIR}/include/strategy/model/StrategyTemplate.h
    ${CMAKE_SOURCE_DIR}/include/strategy/model/ConditionNode.h
    ${CMAKE_SOURCE_DIR}/include/strategy/model/StrategyInstance.h

    # Runtime headers
    ${CMAKE_SOURCE_DIR}/include/strategy/runtime/StrategyBase.h
    ${CMAKE_SOURCE_DIR}/include/strategy/runtime/StrategyFactory.h
    ${CMAKE_SOURCE_DIR}/include/strategy/runtime/TemplateStrategy.h
    ${CMAKE_SOURCE_DIR}/include/strategy/runtime/LiveFormulaContext.h
    ${CMAKE_SOURCE_DIR}/include/strategy/runtime/FormulaEngine.h
    ${CMAKE_SOURCE_DIR}/include/strategy/runtime/IndicatorEngine.h
    ${CMAKE_SOURCE_DIR}/include/strategy/runtime/OrderExecutionEngine.h
    # ${CMAKE_SOURCE_DIR}/include/strategy/runtime/OptionsExecutionEngine.h  # POC — disabled

    # Builder headers
    ${CMAKE_SOURCE_DIR}/include/strategy/builder/StrategyTemplateBuilderDialog.h
    ${CMAKE_SOURCE_DIR}/include/strategy/builder/ConditionBuilderWidget.h
    ${CMAKE_SOURCE_DIR}/include/strategy/builder/IndicatorRowWidget.h
    ${CMAKE_SOURCE_DIR}/include/strategy/builder/IndicatorCatalog.h
    ${CMAKE_SOURCE_DIR}/include/strategy/builder/IndicatorPickerDialog.h
    ${CMAKE_SOURCE_DIR}/include/strategy/builder/ParamEditorDialog.h
    ${CMAKE_SOURCE_DIR}/include/strategy/builder/StrategyDeployDialog.h
    ${CMAKE_SOURCE_DIR}/include/strategy/builder/SymbolBindingWidget.h
    ${CMAKE_SOURCE_DIR}/include/strategy/builder/TemplateManagerDialog.h

    # Manager headers
    ${CMAKE_SOURCE_DIR}/include/strategy/manager/StrategyManagerWindow.h
    ${CMAKE_SOURCE_DIR}/include/strategy/manager/CreateStrategyDialog.h
    ${CMAKE_SOURCE_DIR}/include/strategy/manager/ModifyParametersDialog.h
    ${CMAKE_SOURCE_DIR}/include/strategy/manager/StrategyTableModel.h
    ${CMAKE_SOURCE_DIR}/include/strategy/manager/StrategyFilterProxyModel.h
    ${CMAKE_SOURCE_DIR}/include/strategy/manager/StrategyService.h

    # Persistence headers
    ${CMAKE_SOURCE_DIR}/include/strategy/persistence/StrategyTemplateRepository.h
    ${CMAKE_SOURCE_DIR}/include/strategy/persistence/StrategyRepository.h
)

target_link_libraries(strategy_engine PUBLIC
    Qt5::Widgets
    Qt5::Sql
    Qt5::UiTools
)
