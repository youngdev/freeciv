# Try to configure the Qt-client (gui-qt)

dnl FC_QT_CLIENT
dnl Test for Qt and needed libraries for gui-qt

AC_DEFUN([FC_QT_CLIENT],
[

dnl FIXME: In its current state Qt-client is never selected automatically.
dnl        When Qt-client matures to usable state enable auto-selection by
dnl        replacing below if-check with one that is currently commented out.
dnl if test "x$gui_qt" = "xyes" || test "x$client" = "xall" ||
dnl   test "x$client" = "xauto" ; then

if test "x$gui_qt" = "xyes" || test "x$client" = "xall" ; then

  if test "x$cxx_works" = "xyes" ; then

    FC_QT5

    if test x$fc_qt5_usable = xtrue ; then
      GUI_qt_CPPFLAGS=$FC_QT5_CPPFLAGS
      GUI_qt_CXXFLAGS=$FC_QT5_CXXFLAGS
      GUI_qt_LIBS=$FC_QT5_LIBS
    else
      AC_MSG_RESULT([not found])
    fi

  else
    AC_MSG_RESULT([not found])
  fi

  if test "x$fc_qt5_usable" = "xtrue" ; then
    gui_qt=yes
    if test "x$client" = "xauto" ; then
      client=yes
    fi
  elif test "x$gui_qt" = "xyes" ; then
    AC_MSG_ERROR([selected client 'qt' cannot be built])
  fi
fi
])
