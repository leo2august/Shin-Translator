add_executable(Textractor WIN32 
	main.cpp
	exception.cpp
	mainwindow.cpp
	extenwindow.cpp
	attachprocessdialog.cpp
	lunabridge.cpp
	japaneselearning.cpp
	gameprofiles.cpp
	ocrengine.cpp
	Textractor.rc
	VNTranslator.ico
)
# Pastikan .rc dikompilasi ulang bila ikon (.ico) berubah - CMake tidak mendeteksi
# dependensi ini otomatis, jadi kita nyatakan eksplisit.
set_source_files_properties(Textractor.rc PROPERTIES
	OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/VNTranslator.ico")
target_precompile_headers(Textractor REUSE_FROM pch)
# ocrengine.cpp memakai C++/WinRT (template berat) -> jangan pakai PCH bersama.
set_source_files_properties(ocrengine.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
target_link_libraries(Textractor host Qt5::Widgets Qt5::WinExtras shell32 winhttp windowsapp)

if (NOT EXISTS ${CMAKE_FINAL_OUTPUT_DIRECTORY}/Qt5Core.dll AND NOT EXISTS ${CMAKE_FINAL_OUTPUT_DIRECTORY}/Qt5Cored.dll)
	add_custom_command(TARGET Textractor
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_CURRENT_BINARY_DIR}/windeployqt"
		COMMAND set PATH=%PATH%$<SEMICOLON>${qt5_install_prefix}/bin
		COMMAND Qt5::windeployqt --dir ${CMAKE_FINAL_OUTPUT_DIRECTORY} "${CMAKE_FINAL_OUTPUT_DIRECTORY}/Textractor.exe"
	)
endif()
