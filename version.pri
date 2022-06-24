################################################################################
#   TargomanNMTServer
#
#   Copyright(c) 2022 by Targoman Intelligent Processing <http://tip.co.ir>
#
#   Redistribution and use in source and binary forms are allowed under the 
#   terms of BSD License 2.0. See LICENSE file
################################################################################
ProjectName="TargomanNMTServer"
VERSION=0.0.1

#+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-++-+-+-
LIBS += \

#+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-++-+-+-
QT-= \
    core 

QT -= \
    gui

#+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-++-+-+-
# Qt5.5.1 on OSX needs both c++11 and c++14!! the c++14 is not enough
QMAKE_CXXFLAGS += -std=c++11 -std=c++14
CONFIGS += c++11 c++14

#+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-++-+-+-
#following definitions are used on all Targoman projects
#DEFINES += TARGOMAN_DEBUG_PROCESS_LINE=1
#DEFINES += TARGOMAN_SHOW_WARNING=1
#DEFINES += TARGOMAN_SHOW_INFO=1
#DEFINES += TARGOMAN_SHOW_HAPPY=1
#DEFINES += TARGOMAN_SHOW_NORMAL=1
