# Engine source files built by both the Mac and the M33 builds (doomgeneric, minus its platform files and the X11 layer).
SRCS="dummy am_map doomdef doomstat dstrings d_event d_items d_iwad d_loop d_main d_mode d_net f_finale f_wipe g_game hu_lib hu_stuff info i_cdmus i_endoom i_joystick i_scale i_sound i_system i_timer memio m_argv m_bbox m_cheat m_config m_controls m_fixed m_menu m_misc m_random p_ceilng p_doors p_enemy p_floor p_inter p_lights p_map p_maputl p_mobj p_plats p_pspr p_saveg p_setup p_sight p_spec p_switch p_telept p_tick p_user r_bsp r_data r_draw r_main r_plane r_segs r_sky r_things sha1 sounds statdump st_lib st_stuff s_sound tables v_video wi_stuff w_checksum w_file w_main w_wad z_zone i_input i_video doomgeneric"
# The squeeze sets used in the experiment (compile flags). S0 is the platform change only (no output buffer, no screen melt).
SQ_S0="-DFOX_NOWIPE"
SQ_S1="$SQ_S0 -DFOX_CONST_TABLES"
SQ_S2="$SQ_S1 -DFOX_MAXVISPLANES=64 -DFOX_MAXOPENINGS=5120 -DFOX_MAXDRAWSEGS=128 -DFOX_MAXVISSPRITES=100"
SQ_S3="$SQ_S2 -DFOX_BACKUPTICS=16"
