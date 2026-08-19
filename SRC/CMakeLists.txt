# ==============================================================
# CMakeLists.txt corregido (Versión Limpia)
# ==============================================================
# 1. Qt no genera el código de la macro Q_OBJECT.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)


INCLUDE_DIRECTORIES(.)
LINK_DIRECTORIES(${CMAKE_BINARY_DIR}/src)

# ==========================================================
# SOURCES POR MÓDULO
# ==========================================================

SET(SkyChartExporter_SRCS

    Core/SkyChartExporter.cpp
    Core/SkyChartExporter.hpp
    Core/SkyChartExporterOptions.hpp
    Core/SkyScene.hpp

    Extraction/SkySceneExtractor.cpp
    Extraction/SkySceneExtractor.hpp

    Render/SkyChartRenderer.cpp
    Render/SkyChartRenderer.hpp

    Export/PdfExporter.cpp
    Export/PdfExporter.hpp
    Export/SvgExporter.cpp
    Export/SvgExporter.hpp
    Export/ImageExporter.cpp
    Export/ImageExporter.hpp

    Layout/AtlasLayoutManager.cpp
    Layout/AtlasLayoutManager.hpp

    
    UI/SkyChartExporterDialog.cpp
    UI/SkyChartExporterDialog.hpp
    UI/PreviewDialog.cpp
    UI/PreviewDialog.hpp

    
)

SET(SkyChartExporter_UIS
    UI/SkyChartExporterDialog.ui
    UI/PreviewDialog.ui
)

SET(SkyChartExporter_RES
    ../SkyChartExporter.qrc
)

IF(${QT_VERSION_MAJOR} EQUAL "5")
    QT5_WRAP_UI(SkyChartExporter_UIS_H ${SkyChartExporter_UIS})
    QT5_ADD_RESOURCES(SkyChartExporter_RES_CXX ${SkyChartExporter_RES})
ELSE()
    QT_WRAP_UI(SkyChartExporter_UIS_H ${SkyChartExporter_UIS})
    QT_ADD_RESOURCES(SkyChartExporter_RES_CXX ${SkyChartExporter_RES})
ENDIF()


# ==========================================================
# PLUGIN FINAL
# ==========================================================

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    PrintSupport
    Svg
)

ADD_LIBRARY(SkyChartExporter-static STATIC
    ${SkyChartExporter_SRCS}
    ${SkyChartExporter_RES_CXX}
    ${SkyChartExporter_UIS_H}
)

TARGET_LINK_LIBRARIES(SkyChartExporter-static
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::PrintSupport
    Qt${QT_VERSION_MAJOR}::Svg
)

# ----------------------------------------------------------
# Directorios del plugin
# ----------------------------------------------------------

TARGET_INCLUDE_DIRECTORIES(SkyChartExporter-static PRIVATE

    # Plugin
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Core
    ${CMAKE_CURRENT_SOURCE_DIR}/Extraction
    ${CMAKE_CURRENT_SOURCE_DIR}/Render
    ${CMAKE_CURRENT_SOURCE_DIR}/Export
    ${CMAKE_CURRENT_SOURCE_DIR}/Layout
    ${CMAKE_CURRENT_SOURCE_DIR}/UI

    # Código principal de Stellarium
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/core/modules
    ${CMAKE_SOURCE_DIR}/src/core/planetsephems
    ${CMAKE_SOURCE_DIR}/src/gui
    ${CMAKE_SOURCE_DIR}/src/scripting

    # Archivos generados (moc/uic)
    ${CMAKE_CURRENT_BINARY_DIR}
)

SET_TARGET_PROPERTIES(SkyChartExporter-static
    PROPERTIES
        OUTPUT_NAME "SkyChartExporter"
        COMPILE_FLAGS "-DQT_STATICPLUGIN"
        FOLDER "plugins/SkyChartExporter"
)

ADD_DEPENDENCIES(AllStaticPlugins SkyChartExporter-static)

# Registrar el plugin estático en el sistema de Stellarium
TARGET_COMPILE_DEFINITIONS(SkyChartExporter-static PRIVATE
    USE_STATIC_PLUGIN_SKYCHARTEXPORTER
)
