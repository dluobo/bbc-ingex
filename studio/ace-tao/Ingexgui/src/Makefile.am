# these are the headers for your project
noinst_HEADERS = 
	comms.h connection_matrix.h controller.h \
	dialogues.h dragbuttonlist.h frame_info.h help.h ingexgui.h media_control.h \
	media_player.h media_sink.h media_sink_frame.h media_source.h player.h \
	progress_bar_input.h progress_bar_input_connect.h recordbutton.h recordergroup.h ticktree.h \
	timepos.h

#########################################################################
# APPLICATION SECTION
#########################################################################
# this is the program that gets installed.  it's name is used for all
# of the other Makefile.am variables
bin_PROGRAMS = ingexgui

# the application source, library search path, and link libraries
ingexgui_SOURCES = IngexPlayer.h LocalIngexPlayer.h activity.xpm blank.xpm \
	comms.cpp concerned.xpm controller.cpp dialogues.cpp dragbuttonlist.cpp \
	exclamation.xpm ffx16.xpm ffx2.xpm ffx32.xpm ffx4.xpm ffx64.xpm ffx8.xpm frx16.xpm \
	frx2.xpm frx32.xpm frx4.xpm frx64.xpm frx8.xpm happy.xpm help.cpp ingexgui.cpp \
	ingexgui.xpm keyboard_input.h keyboard_input_connect.h noactivity.xpm \
	on_screen_display.h paused.xpm play.xpm play_backwards.xpm player.cpp question.xpm \
	record.xpm recordbutton.cpp recordergroup.cpp small_disabled.xpm small_enabled.xpm \
	small_exclamation.xpm small_partial_enabled.xpm small_partial_rec.xpm small_question.xpm \
	small_rec.xpm stop.xpm ticktree.cpp timepos.cpp x11_common.h
ingexgui_LDFLAGS = $(WX_LIBS) 
