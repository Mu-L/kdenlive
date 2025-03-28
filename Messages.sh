#!/bin/sh
# SPDX-FileCopyrightText: None
# SPDX-License-Identifier: CC0-1.0

kdenlive_subdirs="plugins renderer data src src/ui"

$EXTRACTRC --tag=name --tag=description --tag=label --tag=comment --tag=paramlistdisplay data/transitions/*.xml data/transitions/frei0r/*.xml data/effects/*.xml data/effects/frei0r/*.xml data/effects/avfilter/*.xml data/effects/ladspa/*.xml data/effects/sox/*.xml data/generators/*.xml data/kdenliveeffectscategory.rc >> rc.cpp
$EXTRACTRC `find $kdenlive_subdirs -name \*.rc -a ! -name encodingprofiles.rc -a ! -name camcorderfilters.rc -a ! -name externalproxies.rc -o -name \*.ui` >> rc.cpp

$XGETTEXT `find $kdenlive_subdirs -name \*.cpp -o -name \*.h -o -name \*.qml` *.cpp -o $podir/kdenlive.pot
rm -f rc.cpp
