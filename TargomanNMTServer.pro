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
    src/server.cpp \
    src/main.cpp
# +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-#
CONFIG(OldMarian) {
message("////////////////////////////////////////////////////")
message("//           With old marian nmt                  //")
message("////////////////////////////////////////////////////")
LIBS += \
    -L/usr/local/cuda/lib64\
    -L/opt/intel/mkl/lib/intel64_lin\
    -Llibmarian/lib\
    -lmarian
CONFIG(NoCUDA){
}else{
LIBS += \
    -lmarian_cuda\
    -lcudart\
    -lcudnn\
    -lcublas\
    -lcusparse\
    -lcurand
}
LIBS += \
    -levent\
    -lmkl_intel_lp64\
    -lmkl_rt\
    -lmkl_core\
    -lmkl_sequential\
    -lboost_iostreams\
    -lboost_timer\
    -lz\
    -lrt
} else {
message("////////////////////////////////////////////////////")
message("//           With new marian nmt                  //")
message("////////////////////////////////////////////////////")
LIBS += \
    -L/usr/local/cuda/lib64\
    -L/opt/intel/mkl/lib/intel64_lin\
    -Llibmarian/lib\
    -lmarian
CONFIG(NoCUDA){
}else{
LIBS += \
    -lmarian_cuda\
    -lcudart\
    -lcudnn\
    -lcublas\
    -lcusparse\
    -lcurand
}
LIBS += \
    -levent\
    -lfbgemm\
    -lasmjit\
    -lclog\
    -lcpuinfo_internals\
    -lcpuinfo\
    -lintgemm\
    -lsentencepiece\
    -lsentencepiece_train\
    -lmkl_intel_lp64\
    -lmkl_rt\
    -lmkl_core\
    -lmkl_sequential\
    -lrt    
}
# +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-#
INCLUDEPATH += libmarian/include libmarian/include/marian libmarian/include/marian/3rd_party
# +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-#
QMAKE_CXXFLAGS += -Wno-unknown-pragmas
