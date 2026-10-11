# olagarro_embed_files(<target> [NAMESPACE <namespace>] FUNCTION <name> FILES <file>...)
#
# Builds files into <target>, such as translation catalogs, help texts or fonts: generates a source defining
# `std::vector<std::string_view> <namespace>::<name>()`, which returns each file's contents in the order given, and
# `<name>_names()`, each file's name without its folders, as "help.es.md", and adds it to the target. Editing a file
# regenerates the source on the next build.
#
# The files go in compressed with gzip, which CMake writes itself, and come back whole the first time the function is
# called, through olagarro::oui::inflate_gzip: the target links OUI. A text takes about a third of the room.
function(olagarro_embed_files target)
    cmake_parse_arguments(PARSE_ARGV 1 EMBED "" "NAMESPACE;FUNCTION" "FILES")
    set(source "#include <oui/inflate.h>\n\n#include <string>\n#include <string_view>\n#include <vector>\n\n")
    if(EMBED_NAMESPACE)
        string(APPEND source "namespace ${EMBED_NAMESPACE}\n{\n\n")
    endif()
    set(entries "")
    set(names "")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/embedded")
    set(index 0)
    foreach(file IN LISTS EMBED_FILES)
        get_filename_component(file "${file}" ABSOLUTE)
        get_filename_component(name "${file}" NAME)
        string(APPEND names "\"${name}\", ")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${file}")
        set(packed "${CMAKE_CURRENT_BINARY_DIR}/embedded/${target}_${EMBED_FUNCTION}_${index}.gz")
        file(ARCHIVE_CREATE OUTPUT "${packed}" PATHS "${file}" FORMAT raw COMPRESSION GZip COMPRESSION_LEVEL 9)
        # Bytes rather than a string literal: compilers cap the length of a literal, not of an array.
        file(READ "${packed}" bytes HEX)
        # The header's time, bytes 4 to 7, as nothing: the same file then makes the same source at every configure,
        # which nothing has to build again.
        string(SUBSTRING "${bytes}" 0 8 head)
        string(SUBSTRING "${bytes}" 16 -1 tail)
        set(bytes "${head}00000000${tail}")
        string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${bytes}")
        string(APPEND source "static const unsigned char packed_${index}[]{${bytes}};\n")
        string(APPEND entries
            "        olagarro::oui::inflate_gzip({reinterpret_cast<const char*>(packed_${index}), sizeof packed_${index}}),\n")
        math(EXPR index "${index} + 1")
    endforeach()
    string(APPEND source "\nstd::vector<std::string_view> ${EMBED_FUNCTION}();\n")
    string(APPEND source "std::vector<std::string_view> ${EMBED_FUNCTION}()\n{\n")
    string(APPEND source "    // Each file as it was, uncompressed the first time it is asked for.\n")
    string(APPEND source "    static const std::vector<std::string> files{\n${entries}    };\n")
    string(APPEND source "    return {files.begin(), files.end()};\n}\n")
    string(APPEND source "\nstd::vector<std::string_view> ${EMBED_FUNCTION}_names();\n")
    string(APPEND source "std::vector<std::string_view> ${EMBED_FUNCTION}_names()\n{\n")
    string(APPEND source "    return {${names}};\n}\n")
    if(EMBED_NAMESPACE)
        string(APPEND source "\n} // namespace ${EMBED_NAMESPACE}\n")
    endif()
    set(output "${CMAKE_CURRENT_BINARY_DIR}/${target}_${EMBED_FUNCTION}.cpp")
    file(CONFIGURE OUTPUT "${output}" CONTENT "${source}" @ONLY)
    target_sources(${target} PRIVATE "${output}")
endfunction()
