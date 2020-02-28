cmake_minimum_required(VERSION 3.1.0)
set(32BLIT_PATH "../" CACHE PATH "Path to 32blit.cmake")
project(doom)

include(${32BLIT_PATH}/32blit.cmake)

set(DOOM_SOURCES
    src/chocdoom/dummy.c
    src/chocdoom/am_map.c
    src/chocdoom/doomdef.c
    src/chocdoom/doomstat.c
    src/chocdoom/dstrings.c
    src/chocdoom/d_event.c
    src/chocdoom/d_items.c
    src/chocdoom/d_iwad.c
    src/chocdoom/d_loop.c
    src/chocdoom/d_main.c
    src/chocdoom/d_mode.c
    src/chocdoom/d_net.c
    src/chocdoom/f_finale.c
    src/chocdoom/f_wipe.c
    src/chocdoom/g_game.c
    src/chocdoom/hu_lib.c
    src/chocdoom/hu_stuff.c
    src/chocdoom/info.c
    src/chocdoom/i_cdmus.c
    src/chocdoom/i_endoom.c
    src/chocdoom/i_joystick.c
    src/chocdoom/i_main.c
    src/chocdoom/i_scale.c
    src/chocdoom/i_sound.c
    src/chocdoom/i_system.c
    src/chocdoom/i_timer.c
    src/chocdoom/i_video.c
    src/chocdoom/memio.c
    src/chocdoom/m_argv.c
    src/chocdoom/m_bbox.c
    src/chocdoom/m_cheat.c
    src/chocdoom/m_config.c
    src/chocdoom/m_controls.c
    src/chocdoom/m_fixed.c
    src/chocdoom/m_menu.c
    src/chocdoom/m_misc.c
    src/chocdoom/m_random.c
    src/chocdoom/p_ceilng.c
    src/chocdoom/p_doors.c
    src/chocdoom/p_enemy.c
    src/chocdoom/p_floor.c
    src/chocdoom/p_inter.c
    src/chocdoom/p_lights.c
    src/chocdoom/p_map.c
    src/chocdoom/p_maputl.c
    src/chocdoom/p_mobj.c
    src/chocdoom/p_plats.c
    src/chocdoom/p_pspr.c
    src/chocdoom/p_saveg.c
    src/chocdoom/p_setup.c
    src/chocdoom/p_sight.c
    src/chocdoom/p_spec.c
    src/chocdoom/p_switch.c
    src/chocdoom/p_telept.c
    src/chocdoom/p_tick.c
    src/chocdoom/p_user.c
    src/chocdoom/r_bsp.c
    src/chocdoom/r_data.c
    src/chocdoom/r_draw.c
    src/chocdoom/r_main.c
    src/chocdoom/r_plane.c
    src/chocdoom/r_segs.c
    src/chocdoom/r_sky.c
    src/chocdoom/r_things.c
    src/chocdoom/sha1.c
    src/chocdoom/sounds.c
    src/chocdoom/statdump.c
    src/chocdoom/st_lib.c
    src/chocdoom/st_stuff.c
    src/chocdoom/s_sound.c
    src/chocdoom/tables.c
    src/chocdoom/v_video.c
    src/chocdoom/wi_stuff.c
    src/chocdoom/w_checksum.c
    src/chocdoom/w_file.c
    src/chocdoom/w_file_stdc.c
    src/chocdoom/w_main.c
    src/chocdoom/w_wad.c
    src/chocdoom/z_zone.c
)

set(SOURCES
    src/blit/main.cpp
    src/blit/sound.cpp
)

# the 32blit apis are c++, compile everything as c++ so I don't have to write a bunch of wrappers...
set_source_files_properties(${DOOM_SOURCES} PROPERTIES
    LANGUAGE CXX
    COMPILE_FLAGS "-fpermissive -Wno-write-strings -Wno-literal-suffix"
)

blit_executable(doom ${SOURCES} ${DOOM_SOURCES})
blit_assets_yaml(doom assets.yml)
target_include_directories(doom PRIVATE src/blit)