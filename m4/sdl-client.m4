# Try to configure the SDL client (gui-sdl)

dnl FC_SDL_CLIENT
dnl Test for SDL and needed libraries for gui-sdl

AC_DEFUN([FC_SDL_CLIENT],
[
  if test "x$gui_sdl" = "xyes" || test "x$client" = "xall" ||
     test "x$client" = "xauto" ; then
    if test "x$SDL_mixer" = "xsdl2" ; then
      if test "x$gui_sdl" = "xyes"; then
        AC_MSG_ERROR([specified client 'sdl' not configurable (cannot use SDL2_mixer with it))])
      fi
      sdl_found=no
    else
      AM_PATH_SDL([1.1.4], [sdl_found="yes"], [sdl_found="no"])
    fi
    if test "$sdl_found" = yes; then
      GUI_sdl_CFLAGS="$SDL_CFLAGS"
      GUI_sdl_LIBS="$SDL_LIBS"
      FC_SDL_PROJECT([SDL_image], [IMG_Load], [SDL/SDL_image.h])
      if test "x$sdl_h_found" = "xyes" ; then
        FC_SDL_PROJECT([SDL_gfx], [rotozoomSurface], [SDL/SDL_rotozoom.h])
      else
        missing_project="SDL_image"
      fi
      if test "x$sdl_h_found" = "xyes" ; then
        FC_SDL_PROJECT([SDL_ttf], [TTF_OpenFont], [SDL/SDL_ttf.h])
      elif test "x$missing_project" = "x" ; then
        missing_project="SDL_gfx"
      fi
      if test "x$sdl_h_found" = "xyes" ; then
        AC_CHECK_FT2([2.1.3], [freetype_found="yes"],[freetype_found="no"])
        if test "$freetype_found" = yes; then
	  GUI_sdl_CFLAGS="$GUI_sdl_CFLAGS $FT2_CFLAGS"
	  GUI_sdl_LIBS="$GUI_sdl_LIBS $FT2_LIBS"
          found_sdl_client=yes
        elif test "x$gui_sdl" = "xyes"; then
          AC_MSG_ERROR([specified client 'sdl' not configurable (FreeType2 >= 2.1.3 is needed (www.freetype.org))])
        fi
      elif test "x$gui_sdl" = "xyes"; then
        if test "x$missing_project" = "x" ; then
          missing_project="SDL_ttf"
        fi
        if test "x$sdl_lib_found" = "xyes" ; then
          missing_type="-devel"
        else
          missing_type=""
        fi
        AC_MSG_ERROR([specified client 'sdl' not configurable (${missing_project}${missing_type} is needed (www.libsdl.org))])
      fi
    fi

    if test "$found_sdl_client" = yes; then
      gui_sdl=yes
      if test "x$client" = "xauto" ; then
        client=yes
      fi

      dnl Check for libiconv (which is usually included in glibc, but may
      dnl be distributed separately).
      AM_ICONV
      AM_LIBCHARSET
      AM_LANGINFO_CODESET
      GUI_sdl_LIBS="$LIBICONV $GUI_sdl_LIBS"

      dnl Check for some other libraries - needed under BeOS for instance.
      dnl These should perhaps be checked for in all cases?
      AC_CHECK_LIB(socket, connect, GUI_sdl_LIBS="-lsocket $GUI_sdl_LIBS")
      AC_CHECK_LIB(bind, gethostbyaddr, GUI_sdl_LIBS="-lbind $GUI_sdl_LIBS")

    elif test "x$gui_sdl" = "xyes"; then
      AC_MSG_ERROR([specified client 'sdl' not configurable (SDL >= 1.1.4 is needed (www.libsdl.org))])
    fi
  fi
])

AC_DEFUN([FC_SDL_PROJECT],
[
  ac_save_CPPFLAGS="$CPPFLAGS"
  ac_save_CFLAGS="$CFLAGS"
  ac_save_LIBS="$LIBS"
  CPPFLAGS="$CPPFLAGS $SDL_CFLAGS"
  CFLAGS="$CFLAGS $SDL_CFLAGS"
  LIBS="$LIBS $SDL_LIBS"
  AC_CHECK_LIB([$1], [$2],
               [sdl_lib_found="yes"], [sdl_lib_found="no"
sdl_h_found="no"])
  if test "x$sdl_lib_found" = "xyes" ; then
    AC_CHECK_HEADER([$3],
                    [sdl_h_found="yes"
GUI_sdl_LIBS="${GUI_sdl_LIBS} -l$1"], [sdl_h_found="no"])
  fi
  CPPFLAGS="$ac_save_CPPFLAGS"
  CFLAGS="$ac_save_CFLAGS"
  LIBS="$ac_save_LIBS"
])
