# Declare files.
set(SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/animation/WidgetAnimationManager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/animation/WidgetAnimator.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/resources/ResourceInitialization.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/style/eventFilters/ComboboxItemViewFilter.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/style/eventFilters/CommandLinkButtonEventFilter.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/style/eventFilters/LineEditButtonEventFilter.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/style/eventFilters/LineEditMenuEventFilter.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/style/eventFilters/MenuEventFilter.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/style/eventFilters/MouseWheelBlockerEventFilter.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/style/eventFilters/TabBarEventFilter.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/style/eventFilters/TextEditEventFilter.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/style/eventFilters/WidgetWithFocusFrameEventFilter.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/style/Delegates.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/style/EventFilters.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/style/EventFilters.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/style/QlementineStyle.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/style/Theme.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/style/ThemeManager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/tools/ThemeEditor.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/BadgeUtils.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/ColorUtils.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/FontUtils.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/GeometryUtils.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/IconUtils.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/ImageUtils.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/LayoutUtils.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/MenuUtils.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/PrimitiveUtils.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/RadiusesF.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/StateUtils.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/StyleUtils.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/WidgetUtils.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/AboutDialog.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/AbstractItemListWidget.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/Action.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/ActionButton.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/ColorButton.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/ColorEditor.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/CommandLinkButton.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/Expander.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/IconWidget.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/Label.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/LineEdit.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/LoadingSpinner.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/Menu.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/NavigationBar.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/NotificationBadge.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/PlainTextEdit.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/Popover.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/PopoverButton.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/RoundedFocusFrame.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/SegmentedControl.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/StatusBadgeWidget.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/Switch.cpp
)

set(HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/animation/WidgetAnimation.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/animation/WidgetAnimationManager.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/animation/WidgetAnimator.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/Common.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/resources/ResourceInitialization.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/style/Delegates.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/style/QlementineStyle.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/style/QlementineStyleOption.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/style/Theme.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/style/ThemeManager.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/tools/ThemeEditor.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/utils/BadgeUtils.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/utils/BlurUtils.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/utils/ColorUtils.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/utils/FontUtils.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/utils/GeometryUtils.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/utils/IconUtils.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/utils/ImageUtils.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/utils/LayoutUtils.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/utils/MenuUtils.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/utils/PrimitiveUtils.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/utils/RadiusesF.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/utils/StateUtils.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/utils/StyleUtils.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/utils/WidgetUtils.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/widgets/AboutDialog.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/widgets/AbstractItemListWidget.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/widgets/Action.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/widgets/ActionButton.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/widgets/ColorButton.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/widgets/ColorEditor.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/widgets/CommandLinkButton.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/widgets/Expander.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/widgets/IconWidget.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/widgets/Label.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/widgets/LineEdit.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/widgets/LoadingSpinner.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/widgets/Menu.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/widgets/NavigationBar.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/widgets/NotificationBadge.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/widgets/PlainTextEdit.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/widgets/Popover.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/widgets/PopoverButton.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/widgets/RoundedFocusFrame.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/widgets/SegmentedControl.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/widgets/StatusBadgeWidget.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qlementine/widgets/Switch.hpp
)

set(RESOURCES
  resources/qlementine.qrc
  resources/qlementine_font_inter.qrc
  resources/qlementine_font_roboto.qrc
)

# Create target.
qt_add_library(${PROJECT_NAME} STATIC
  ${HEADERS}
  ${SOURCES}
  ${RESOURCES}
)
include(CMakePackageConfigHelpers)

target_include_directories(${PROJECT_NAME}
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

target_link_libraries(${PROJECT_NAME}
  PUBLIC
    Qt::Core
    Qt::Widgets
    Qt::Svg
)

set_target_properties(${PROJECT_NAME}
  PROPERTIES
    OUTPUT_NAME ${PROJECT_NAME}
    PROJECT_LABEL ${PROJECT_NAME}
    FOLDER lib
    SOVERSION ${PROJECT_VERSION_MAJOR}
    VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    DEBUG_POSTFIX _debug
    CMAKE_AUTORCC ON
    CMAKE_AUTOMOC ON
    CMAKE_AUTOUIC ON
)

target_compile_options(${PROJECT_NAME}
  PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/MP /WX /W4>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Werror>
)

# Create source groups.
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}
  FILES
    ${HEADERS}
    ${SOURCES}
)

# Select correct startup project in Visual Studio.
if(WIN32)
  set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
endif()

# Install target
configure_package_config_file("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.cmake.in"
  "${CMAKE_BINARY_DIR}/cmake/${PROJECT_NAME}Config.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")

write_basic_package_version_file("${CMAKE_BINARY_DIR}/cmake/${PROJECT_NAME}ConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY AnyNewerVersion)

install(TARGETS ${PROJECT_NAME}
  EXPORT "${PROJECT_NAME}Targets"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

install(EXPORT "${PROJECT_NAME}Targets"
  FILE "${PROJECT_NAME}Targets.cmake"
  NAMESPACE ${PROJECT_NAME}::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")

install(FILES
  "${CMAKE_BINARY_DIR}/cmake/${PROJECT_NAME}Config.cmake"
  "${CMAKE_BINARY_DIR}/cmake/${PROJECT_NAME}ConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")

export(EXPORT "${PROJECT_NAME}Targets"
  FILE "${CMAKE_BINARY_DIR}/cmake/${PROJECT_NAME}Targets.cmake"
  NAMESPACE ${PROJECT_NAME}::)
