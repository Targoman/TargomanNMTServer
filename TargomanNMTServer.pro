################################################################################
#   TargomanNMTServer
#
#   Copyright(c) 2022 by Targoman Intelligent Processing <http://tip.co.ir>
#
#   Redistribution and use in source and binary forms are allowed under the
#   terms of BSD License 2.0.
################################################################################
include($$QBUILD_PATH/templates/appConfigs.pri)

# +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-#
HEADERS =
# +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-#
SOURCES = \
    src/bpe.cpp \
    src/gason.cpp \
    src/server.cpp \
    src/main.cpp
# +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-#
LIBS += \
    -Llibmarian/lib\
    -lmarian\
    -levent\
    -lfbgemm\
    -lasmjit\
    -lclog\
    -lcpuinfo_internals\
    -lcpuinfo\
    -lintgemm\
    -lsentencepiece\
    -lsentencepiece_train\
    -lrt
# +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-#
INCLUDEPATH += libmarian/include libmarian/include/marian libmarian/include/marian/3rd_party
# +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-#
QMAKE_CXXFLAGS += -Wno-unknown-pragmas
