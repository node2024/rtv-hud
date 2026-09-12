cmake_minimum_required(VERSION 3.16)
project(rtv_hud LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
if(MSVC)
  add_compile_options(/UNDEBUG /W4 /utf-8)
else()
  add_compile_options(-UNDEBUG -Wall -Wextra)
endif()
enable_testing()
add_executable(vote_test tests/vote_test.cpp)
target_include_directories(vote_test PRIVATE src)
add_test(NAME vote COMMAND vote_test)
add_executable(browser_native_test tests/browser_native_test.cpp)
target_include_directories(browser_native_test PRIVATE src)
add_test(NAME browser_native COMMAND browser_native_test)
add_executable(native_commands_test tests/native_commands_test.cpp)
target_include_directories(native_commands_test PRIVATE src)
add_test(NAME native_commands COMMAND native_commands_test)
add_executable(personal_huds_test tests/personal_huds_test.cpp)
target_include_directories(personal_huds_test PRIVATE src)
add_test(NAME personal_huds COMMAND personal_huds_test)
add_executable(map_end_test tests/map_end_test.cpp)
target_include_directories(map_end_test PRIVATE src)
add_test(NAME map_end COMMAND map_end_test)
add_executable(browser_all_test tests/browser_all_test.cpp)
target_include_directories(browser_all_test PRIVATE src)
add_test(NAME browser_all COMMAND browser_all_test ${CMAKE_CURRENT_SOURCE_DIR}/deploy/addons/rtv_hud/maplist.txt)
add_executable(browser_ui_test tests/browser_ui_test.cpp)
target_include_directories(browser_ui_test PRIVATE src)
add_test(NAME browser_ui COMMAND browser_ui_test)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME browser_layout COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/browser_layout_test.py)
set(SDK "" CACHE PATH "Pinned hl2sdk-cs2 checkout")
set(MMS "" CACHE PATH "Pinned metamod-source checkout")
if(SDK AND MMS)
  if(NOT EXISTS "${MMS}/third_party/khook/include/khook.hpp")
    message(FATAL_ERROR "KHook headers missing: use the pinned Metamod checkout and initialize third_party/khook")
  endif()
  execute_process(COMMAND ${SDK}/devtools/bin/linux/protoc
    --proto_path=${SDK}/common --proto_path=${SDK}/game/shared --proto_path=${SDK}/thirdparty/protobuf-3.21.8/src
    --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
    ${SDK}/game/shared/usermessages.proto ${SDK}/common/networkbasetypes.proto ${SDK}/common/valveextensions.proto
    RESULT_VARIABLE chat_proto_result)
  if(NOT chat_proto_result EQUAL 0)
    message(FATAL_ERROR "Chat protoc failed: ${chat_proto_result}")
  endif()
  execute_process(COMMAND ${SDK}/devtools/bin/linux/protoc
    --proto_path=${SDK}/common --proto_path=${SDK}/thirdparty/protobuf-3.21.8/src
    --cpp_out=${CMAKE_CURRENT_BINARY_DIR} ${SDK}/common/network_connection.proto
    RESULT_VARIABLE proto_result)
  if(NOT proto_result EQUAL 0)
    message(FATAL_ERROR "SDK protoc failed: ${proto_result}")
  endif()
  add_library(rtv_hud SHARED src/plugin.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/network_connection.pb.cc
    ${CMAKE_CURRENT_BINARY_DIR}/usermessages.pb.cc
    ${CMAKE_CURRENT_BINARY_DIR}/networkbasetypes.pb.cc
    ${CMAKE_CURRENT_BINARY_DIR}/valveextensions.pb.cc
    ${SDK}/entity2/entityidentity.cpp ${SDK}/entity2/entitysystem.cpp
    ${SDK}/entity2/entitykeyvalues.cpp ${SDK}/tier1/keyvalues3.cpp
    ${SDK}/tier1/convar.cpp)
  target_compile_definitions(rtv_hud PRIVATE LINUX POSIX _LINUX GNUC COMPILER_GCC PLATFORM_64BITS
    _GLIBCXX_USE_CXX11_ABI=0 META_IS_SOURCE2 stricmp=strcasecmp strnicmp=strncasecmp)
  target_compile_options(rtv_hud PRIVATE -fPIC -fvisibility=hidden -Wno-invalid-offsetof)
  target_include_directories(rtv_hud PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${MMS}/core ${MMS}/third_party/khook/include
    ${SDK}/public ${SDK}/public/tier0 ${SDK}/public/tier1 ${SDK}/public/entity2
    ${SDK}/public/interfaces ${SDK}/game/shared ${SDK}/common ${SDK}/public/mathlib
    ${SDK}/thirdparty/protobuf-3.21.8/src)
  target_link_libraries(rtv_hud PRIVATE ${SDK}/lib/linux64/interfaces.a ${SDK}/lib/linux64/libtier0.so
    ${SDK}/lib/linux64/release/libprotobuf.a dl pthread)
  target_link_options(rtv_hud PRIVATE -Wl,--exclude-libs,libprotobuf.a)
  target_link_options(rtv_hud PRIVATE -Wl,-z,defs -Wl,-Bsymbolic)
  target_compile_options(rtv_hud PRIVATE -fno-gnu-unique)
  set_target_properties(rtv_hud PROPERTIES PREFIX "" BUILD_WITH_INSTALL_RPATH TRUE INSTALL_RPATH "$ORIGIN/../../../../../bin/linuxsteamrt64")
endif()
