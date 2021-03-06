#
# This file is intended to be included by subdirectory CMakeLists.txt files.
# It provides handly shader compilation, and asset-copying macro.
#

find_program(Vulkan_GLSLANG_VALIDATOR
   NAMES glslangValidator 
   HINTS ENV VULKAN_SDK 
   PATH_SUFFIXES bin
)


# initialize the variables defining output directories
#
# Sets the following variables:
#
# - :cmake:data:`CMAKE_ARCHIVE_OUTPUT_DIRECTORY`
# - :cmake:data:`CMAKE_LIBRARY_OUTPUT_DIRECTORY`
# - :cmake:data:`CMAKE_RUNTIME_OUTPUT_DIRECTORY`
#
# plus the per-config variants, ``*_$<CONFIG>``
#
# @public
#
macro(init_output_directories)
  # Directory for output files
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/lib 
    CACHE PATH "Output directory for static libraries.")

  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/lib
    CACHE PATH "Output directory for shared libraries.")

  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/bin
    CACHE PATH "Output directory for executables and DLL's.")

  foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
    string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
    set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/bin" CACHE PATH "" FORCE)
    set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/lib" CACHE PATH "" FORCE)
    set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/lib" CACHE PATH "" FORCE)
  endforeach()
endmacro()


# Shader compilation
macro(compile_shaders shader_src_files shader_header_files dir_name compiled_shaders)
   set(${compiled_shaders})
   set(${compiled_shaders} PARENT_SCOPE)
   foreach(shader ${${shader_src_files}})
      message("${target_name} SHADER: ${shader}")
      get_filename_component(file_name ${shader} NAME)
      get_filename_component(full_path ${shader} ABSOLUTE)
      if(IS_ABSOLUTE ${dir_name})
         set(output_dir ${dir_name})
      else()
         set(output_dir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${dir_name})
      endif()
      set(output_file ${output_dir}/${file_name}.spv)
      set(${compiled_shaders} ${${compiled_shaders}} ${output_file})
      set(${compiled_shaders} ${${compiled_shaders}} PARENT_SCOPE)
      set_source_files_properties(${shader} PROPERTIES HEADER_FILE_ONLY TRUE)
      if (WIN32)
         add_custom_command(
            OUTPUT ${output_file}
            COMMAND ${Vulkan_GLSLANG_VALIDATOR} --target-env vulkan1.2 ${full_path} -o ${output_file}
            DEPENDS ${full_path}
            DEPENDS ${${shader_header_files}}  # this makes all shaders depend on all shader_header_files... but I cant think of a better way
         )
      else()
         add_custom_command(
            OUTPUT ${output_file}
            COMMAND mkdir --parents ${output_dir} && ${Vulkan_GLSLANG_VALIDATOR} --target-env vulkan1.2 ${full_path} -o ${output_file}
            DEPENDS ${full_path}
         )
      endif()
   endforeach()
endmacro()


macro(copy_assets asset_files dir_name copied_files)
   set(${copied_files})
   set(${copied_files} PARENT_SCOPE)
   foreach(asset ${${asset_files}})
      message("${target_name} ASSET: ${asset}")
      get_filename_component(file_name ${asset} NAME)
      get_filename_component(full_path ${asset} ABSOLUTE)
      if(IS_ABSOLUTE ${dir_name})
         set(output_dir ${dir_name})
      else()
         set(output_dir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${dir_name})
      endif()
      set(output_file ${output_dir}/${file_name})
      set(${copied_files} ${${copied_files}} ${output_file})
      set(${copied_files} ${${copied_files}} PARENT_SCOPE)
      set_source_files_properties(${asset} PROPERTIES HEADER_FILE_ONLY TRUE)
      set_source_files_properties(${output_file} PROPERTIES HEADER_FILE_ONLY TRUE)
      if (WIN32)
         add_custom_command(
            OUTPUT ${output_file}
            COMMAND xcopy \"${full_path}\" \"${output_file}*\" /Y /Q /F
            DEPENDS ${full_path}
         )
      else()
         add_custom_command(
            OUTPUT ${output_file}
            COMMAND mkdir --parents ${output_dir} && cp --force ${full_path} ${output_file}
            DEPENDS ${full_path}
         )
      endif()
   endforeach()
endmacro()
