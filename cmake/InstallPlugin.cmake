# Installs a built plugin into a plugin folder. The build runs it after building each plugin:
#
#   cmake -Dbundle=<built plugin bundle> -Dfolder=<plugin folder> -P InstallPlugin.cmake
#
# The bundle's contents are copied into a bundle of the same name in the folder, which needs the right to write to
# that bundle but not to the folder. So a system-wide folder such as /Library/Audio/Plug-Ins/VST3, which only an
# administrator can write to, can be installed into by every build once an administrator has made that bundle's
# folder and given it to the user. Until then the build says how to do that, rather than failing.

get_filename_component(name "${bundle}" NAME)
set(installed "${folder}/${name}")

if(NOT EXISTS "${installed}")
    execute_process(COMMAND mkdir -p "${installed}" RESULT_VARIABLE made ERROR_QUIET)

    if(NOT made EQUAL 0)
        message(WARNING "${name} wasn't installed, as only an administrator can add to ${folder}. "
                        "To let builds install it there, run this once:\n"
                        "  sudo mkdir \"${installed}\" && sudo chown \"$ENV{USER}\" \"${installed}\"")
        return()
    endif()
endif()

# The trailing slashes copy what's in the bundle rather than the bundle itself, and --delete removes anything an
# earlier build left that this one didn't make.
execute_process(COMMAND rsync -a --delete "${bundle}/" "${installed}/" RESULT_VARIABLE copied)

if(NOT copied EQUAL 0)
    message(WARNING "${name} couldn't be copied into ${installed}. Check that you own that folder.")
    return()
endif()

# Hosts on Apple silicon load only signed plugins, and the signature has to cover the bundle where it's installed.
execute_process(COMMAND codesign --force --sign - "${installed}" RESULT_VARIABLE signed OUTPUT_QUIET ERROR_QUIET)

if(NOT signed EQUAL 0)
    message(WARNING "${installed} was copied but couldn't be signed, so hosts may not load it.")
    return()
endif()

message(STATUS "Installed ${installed}")
