# Chronometre les vrais compilateurs (premier g++ jusqu'a la fin du lien),
# pas un cmake --build deja a jour qui rend la main en une seconde.
#
# Appeler morf_enable_compile_recording() AVANT add_library / add_executable.
# Puis morf_record_target_build(<cible> <nom-projet>) sur le binaire de service.

function(morf_enable_compile_recording)
    if(DEFINED CMAKE_CXX_COMPILER_LAUNCHER AND CMAKE_CXX_COMPILER_LAUNCHER)
        return()
    endif()
    find_program(_MORF_PY NAMES python3 python)
    if(NOT _MORF_PY)
        return()
    endif()
    set(_script "${CMAKE_CURRENT_LIST_DIR}/record_compile.py")
    if(NOT EXISTS "${_script}")
        return()
    endif()
    set(_stamp "${CMAKE_BINARY_DIR}/.morf-compile-t0")
    # LIST : CMake prefixe chaque commande de compilation avec python + script + tampon.
    set(CMAKE_CXX_COMPILER_LAUNCHER "${_MORF_PY}" "${_script}" "${_stamp}" PARENT_SCOPE)
    set(CMAKE_C_COMPILER_LAUNCHER "${_MORF_PY}" "${_script}" "${_stamp}" PARENT_SCOPE)
    set(MORF_COMPILE_STAMP "${_stamp}" PARENT_SCOPE)
    set(MORF_COMPILE_PY "${_MORF_PY}" PARENT_SCOPE)
    set(MORF_COMPILE_SCRIPT "${_script}" PARENT_SCOPE)
endfunction()

function(morf_record_target_build tgt project_name)
    if(NOT TARGET "${tgt}")
        return()
    endif()
    if(NOT MORF_COMPILE_PY OR NOT MORF_COMPILE_SCRIPT OR NOT MORF_COMPILE_STAMP)
        return()
    endif()
    add_custom_command(TARGET "${tgt}" POST_BUILD
        COMMAND "${MORF_COMPILE_PY}" "${MORF_COMPILE_SCRIPT}"
            --finish
            --stamp "${MORF_COMPILE_STAMP}"
            --project "${project_name}"
            --preset "${CMAKE_GENERATOR}"
            --repo "${CMAKE_SOURCE_DIR}"
        COMMENT "Signaler la duree de compile a morfAnalytics (best-effort)"
    )
endfunction()
