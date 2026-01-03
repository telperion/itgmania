list(APPEND SMDATA_SLAP_SRC
            "SlapFFT.cpp"
            "Slap.cpp"
            "SlapAnalysis.cpp"
            "SlapVisualInterfaces.cpp")

list(APPEND SMDATA_SLAP_HPP
            "SlapFFT.h"
            "Slap.h"
            "SlapAnalysis.h"
            "SlapVisualInterfaces.h")

source_group("Slap"
             FILES
             ${SMDATA_SLAP_SRC}
             ${SMDATA_SLAP_HPP})
