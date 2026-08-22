find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CRANKL_GUI_SOURCES
    app/main.cpp
    core/ArchiveAdapter.cpp
    core/CliCommands.cpp
    core/JobManager.cpp
    widgets/MainWindow.cpp
    widgets/NavigationList.cpp
    widgets/PlaceholderPage.cpp
    widgets/ArchiveHealthHeader.cpp
    widgets/HeatmapWidget.cpp
    widgets/MetricsPanel.cpp
    widgets/ThreeColumnPage.cpp
    widgets/SlotBrowser.cpp
    widgets/ResultSummaryPanel.cpp
    widgets/HomePage.cpp
    widgets/InspectPage.cpp
    widgets/ComparePage.cpp
    widgets/NewJobDialog.cpp
    widgets/JobsDrawerPage.cpp
    resources/gui.qrc
)

add_executable(crankl-gui ${CRANKL_GUI_SOURCES})
target_include_directories(crankl-gui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(crankl-gui PRIVATE crankl Qt6::Widgets)
crankl_set_warnings(crankl-gui)
target_compile_definitions(crankl-gui PRIVATE QT_NO_KEYWORDS)
