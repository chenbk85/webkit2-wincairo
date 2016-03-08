file(MAKE_DIRECTORY ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR})
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/Source/JavaScriptCore/runtime)

file(MAKE_DIRECTORY ${DERIVED_SOURCES_WEBINSPECTORUI_DIR})
file(MAKE_DIRECTORY ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/Protocol)
file(MAKE_DIRECTORY ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/UserInterface/Protocol)

if (ENABLE_WEBCORE)
    file(MAKE_DIRECTORY ${DERIVED_SOURCES_WEBCORE_DIR})
endif ()

if (ENABLE_WEBKIT2)
    file(MAKE_DIRECTORY ${DERIVED_SOURCES_WEBKIT2_DIR})
endif ()