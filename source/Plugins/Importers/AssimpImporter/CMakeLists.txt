set( FOLDER_NAME AssimpImporter )
project( castor3d${FOLDER_NAME} )

if ( VCPKG_TOOLCHAIN )
	option( CASTOR_USE_SYSTEM_ASSIMP "Use system Assimp instead of vcpkg's" OFF )

	if ( CASTOR_USE_SYSTEM_ASSIMP )
		set( CMAKE_MODULE_PATH
			${CMAKE_CURRENT_SOURCE_DIR}/CMake
			${CMAKE_CURRENT_SOURCE_DIR}/CMake/Modules
			${CMAKE_CURRENT_SOURCE_DIR}/CMake/Modules/Traditional
			${CMAKE_CURRENT_SOURCE_DIR}/CMake/Toolchains
		)
	endif ()
endif ()

if ( VCPKG_TOOLCHAIN AND NOT CASTOR_USE_SYSTEM_ASSIMP )
	find_package( irrlicht CONFIG QUIET )
	find_package( polyclipping CONFIG QUIET )
	find_package( minizip CONFIG QUIET )
	find_package( kubazip CONFIG QUIET )
	find_package( poly2tri CONFIG QUIET )
	find_package( utf8cpp CONFIG QUIET )
	find_package( assimp CONFIG QUIET )
	set( assimp_LIBRARIES assimp::assimp )
	if ( TARGET assimp::assimp )
		set( assimp_FOUND ON )
	endif ()
else ()
	find_package( assimp )
endif ()

if( assimp_FOUND )
	set( Castor3DBinsDependencies 
		${Castor3DBinsDependencies}
		castor::${FOLDER_NAME}
		PARENT_SCOPE
	)

	set( ${PROJECT_NAME}_DESCRIPTION "Assimp powered importer" )
	set( ${PROJECT_NAME}_VERSION_MAJOR 0 )
	set( ${PROJECT_NAME}_VERSION_MINOR 5 )
	set( ${PROJECT_NAME}_VERSION_BUILD 0 )

	set( ${PROJECT_NAME}_HDR_FILES
		${CASTOR_SOURCE_DIR}/source/Plugins/Importers/${FOLDER_NAME}/AssimpImporter.hpp
	)
	set( ${PROJECT_NAME}_SRC_FILES
		${CASTOR_SOURCE_DIR}/source/Plugins/Importers/${FOLDER_NAME}/AssimpImporter.cpp
		${CASTOR_SOURCE_DIR}/source/Plugins/Importers/${FOLDER_NAME}/AssimpImporterPlugin.cpp
	)
	source_group( "Header Files"
		FILES
			${${PROJECT_NAME}_HDR_FILES}
	)
	source_group( "Source Files"
		FILES
			${${PROJECT_NAME}_SRC_FILES}
	)
	add_target_min(
		${PROJECT_NAME}
		plugin
		""
		""
	)
	add_library( castor::${FOLDER_NAME}
		ALIAS
		${PROJECT_NAME}
	)
	target_include_directories( ${PROJECT_NAME}
		PRIVATE
			${ImporterIncludes}
			${assimp_INCLUDE_DIR}
	)
	target_link_libraries( ${PROJECT_NAME}
		PUBLIC
			castor::Castor3D
		PRIVATE
			${assimp_LIBRARIES}
	)
	install_target( ${PROJECT_NAME}
		plugin
		${CASTOR_SOURCE_DIR}/source/Plugins/Dividers/${FOLDER_NAME}
	)
	set_property( TARGET ${PROJECT_NAME}
		PROPERTY
			FOLDER "Plugins/Importers"
	)
	set( CASTOR_INSTEXP_LIST
		${CASTOR_INSTEXP_LIST}
		${PROJECT_NAME}
		PARENT_SCOPE
	)
	set( Build "yes (version ${${PROJECT_NAME}_VERSION_MAJOR}.${${PROJECT_NAME}_VERSION_MINOR}.${${PROJECT_NAME}_VERSION_BUILD})" PARENT_SCOPE )
	add_target_astyle( ${PROJECT_NAME} ".h;.hpp;.inl;.cpp" )

	if ( WIN32 )
		if ( NOT VCPKG_TOOLCHAIN )
			# Preparing ASSIMP dll installation
			copy_dll( ${PROJECT_NAME} "${assimp_LIBRARY_DEBUG}" "${assimp_LIBRARY_RELEASE}" )
		endif ()
	elseif ( {${CASTOR_PACKAGE_DEB} )
		set( CPACK_DEBIAN_PACKAGE_DEPENDS "libassimp3-dev, ${CPACK_DEBIAN_PACKAGE_DEPENDS}" )
	endif ()
else()
	set( Build "no (ASSIMP library not found)" PARENT_SCOPE )
endif()
