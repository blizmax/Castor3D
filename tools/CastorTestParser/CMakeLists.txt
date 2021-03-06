project( CastorTestParser )

find_package( SQLite3 QUIET )
find_package( Boost QUIET COMPONENTS date_time )

if ( SQLite3_FOUND AND Boost_FOUND )
	set( ${PROJECT_NAME}_WXWIDGET 1 )
	set( ${PROJECT_NAME}_DESCRIPTION "Scene tests parser for ${MAIN_PROJECT_NAME}" )
	set( ${PROJECT_NAME}_VERSION_MAJOR 1 )
	set( ${PROJECT_NAME}_VERSION_MINOR 0 )
	set( ${PROJECT_NAME}_VERSION_BUILD 0 )

	set( ${PROJECT_NAME}_HDR_FILES
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/CategoryPanel.hpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/CastorTestParser.hpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/LayeredPanel.hpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/MainFrame.hpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Prerequisites.hpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/TestDatabase.hpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/TestPanel.hpp
	)
	set( ${PROJECT_NAME}_SRC_FILES
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/CategoryPanel.cpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/CastorTestParser.cpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/LayeredPanel.cpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/MainFrame.cpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Prerequisites.cpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/TestDatabase.cpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/TestPanel.cpp
	)
	source_group( "Header Files"
		FILES
			${${PROJECT_NAME}_HDR_FILES}
	)
	source_group( "Source Files"
		FILES
			${${PROJECT_NAME}_SRC_FILES}
	)
	set( ${PROJECT_NAME}_FOLDER_HDR_FILES
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Database/DbConnection.hpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Database/DbDateTimeHelpers.hpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Database/DbField.hpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Database/DbFieldType.hpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Database/DbParameter.hpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Database/DbParameterBinding.hpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Database/DbParameteredObject.hpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Database/DbParameteredObject.inl
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Database/DbParameterType.hpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Database/DbPrerequisites.hpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Database/DbResult.hpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Database/DbRow.hpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Database/DbStatement.hpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Database/DbStatement.inl
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Database/DbStatementParameter.hpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Database/DbValue.hpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Database/DbValue.inl
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Database/DbValueBase.hpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Database/DbValuedObject.hpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Database/DbValuedObjectInfos.hpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Database/DbValuePolicy.hpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Database/DbValuePolicy.inl
	)
	set( ${PROJECT_NAME}_FOLDER_SRC_FILES
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Database/DbConnection.cpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Database/DbDateTimeHelpers.cpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Database/DbField.cpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Database/DbParameter.cpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Database/DbParameteredObject.cpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Database/DbResult.cpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Database/DbRow.cpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Database/DbStatement.cpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Database/DbStatementParameter.cpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Database/DbValueBase.cpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Database/DbValuedObject.cpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Database/DbValuedObjectInfos.cpp
	)
	source_group( "Header Files\\Database"
		FILES
			${${PROJECT_NAME}_FOLDER_HDR_FILES}
	)
	source_group( "Source Files\\Database"
		FILES
			${${PROJECT_NAME}_FOLDER_SRC_FILES}
	)
	set( ${PROJECT_NAME}_HDR_FILES
		${${PROJECT_NAME}_HDR_FILES}
		${${PROJECT_NAME}_FOLDER_HDR_FILES}
	)
	set( ${PROJECT_NAME}_SRC_FILES
		${${PROJECT_NAME}_SRC_FILES}
		${${PROJECT_NAME}_FOLDER_SRC_FILES}
	)
	set( ${PROJECT_NAME}_FOLDER_HDR_FILES
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Model/ListModel.hpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Model/ListStoreDerivedModel.hpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Model/TreeModel.hpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Model/TreeModelNode.hpp
	)
	set( ${PROJECT_NAME}_FOLDER_SRC_FILES
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Model/ListModel.cpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Model/ListStoreDerivedModel.cpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Model/TreeModel.cpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Model/TreeModelNode.cpp
	)
	source_group( "Header Files\\Model"
		FILES
			${${PROJECT_NAME}_FOLDER_HDR_FILES}
	)
	source_group( "Source Files\\Model"
		FILES
			${${PROJECT_NAME}_FOLDER_SRC_FILES}
	)
	set( ${PROJECT_NAME}_HDR_FILES
		${${PROJECT_NAME}_HDR_FILES}
		${${PROJECT_NAME}_FOLDER_HDR_FILES}
	)
	set( ${PROJECT_NAME}_SRC_FILES
		${${PROJECT_NAME}_SRC_FILES}
		${${PROJECT_NAME}_FOLDER_SRC_FILES}
	)
	set( ${PROJECT_NAME}_FOLDER_HDR_FILES
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Aui/AuiDockArt.hpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Aui/AuiTabArt.hpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Aui/AuiToolBarArt.hpp
	)
	set( ${PROJECT_NAME}_FOLDER_SRC_FILES
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Aui/AuiDockArt.cpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Aui/AuiTabArt.cpp
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}/Aui/AuiToolBarArt.cpp
	)
	source_group( "Header Files\\Aui"
		FILES
			${${PROJECT_NAME}_FOLDER_HDR_FILES}
	)
	source_group( "Source Files\\Aui"
		FILES
			${${PROJECT_NAME}_FOLDER_SRC_FILES}
	)
	set( ${PROJECT_NAME}_HDR_FILES
		${${PROJECT_NAME}_HDR_FILES}
		${${PROJECT_NAME}_FOLDER_HDR_FILES}
	)
	set( ${PROJECT_NAME}_SRC_FILES
		${${PROJECT_NAME}_SRC_FILES}
		${${PROJECT_NAME}_FOLDER_SRC_FILES}
	)
	add_target_min(
		${PROJECT_NAME}
		bin
		""
		""
	)
	target_include_directories( ${PROJECT_NAME} PRIVATE
		${CASTOR_SOURCE_DIR}/samples/GuiCommon/xpms
		${CASTOR_SOURCE_DIR}/tools
		${CASTOR_BINARY_DIR}/tools
		${wxWidgets_INCLUDE_DIRS}
		${GTK2_INCLUDE_DIRS}
		${Boost_INCLUDE_DIR}
	)
	target_link_libraries( ${PROJECT_NAME} PRIVATE
		castor::CastorUtils
		${wxWidgets_LIBRARIES}
		${GTK2_LIBRARIES}
		${X11_LIBRARIES}
		SQLite::SQLite3
		${Boost_LIBRARIES}
	)
	target_compile_definitions( ${PROJECT_NAME} PRIVATE
		${CastorToolsDefinitions}
		${wxWidgets_DEFINITIONS}
	)
	set_property( TARGET ${PROJECT_NAME}
		PROPERTY
			FOLDER "Tools"
	)
	install_target( ${PROJECT_NAME}
		bin
		${CASTOR_SOURCE_DIR}/tools/${PROJECT_NAME}
	)
	set( Build "yes (version ${${PROJECT_NAME}_VERSION_MAJOR}.${${PROJECT_NAME}_VERSION_MINOR}.${${PROJECT_NAME}_VERSION_BUILD})" PARENT_SCOPE )
	add_target_astyle( ${PROJECT_NAME} ".h;.hpp;.inl;.cpp" )
else ()
	if ( NOT Boost_FOUND )
		set( Build "no (missing boost)" PARENT_SCOPE )
	else ()
		set( Build "no (missing sqlite3)" PARENT_SCOPE )
	endif ()
endif ()
