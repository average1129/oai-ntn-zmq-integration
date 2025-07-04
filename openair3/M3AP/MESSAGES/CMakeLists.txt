set(M3AP_VERSION 14 0 0)
make_version(M3AP_cc ${M3AP_VERSION})
string(REPLACE ";" "." M3AP_RELEASE "${M3AP_VERSION}")

if(M3AP_RELEASE VERSION_EQUAL "8.9.0")
  include(ASN1/m3ap-8.9.0.cmake)
elseif(M3AP_RELEASE VERSION_EQUAL "11.9.0")
  include(ASN1/m3ap-11.9.0.cmake)
elseif(M3AP_RELEASE VERSION_EQUAL "12.9.0")
  include(ASN1/m3ap-12.9.0.cmake)
elseif(M3AP_RELEASE VERSION_EQUAL "14.0.0")
  include(ASN1/m3ap-14.0.0.cmake)
elseif(M3AP_RELEASE VERSION_EQUAL "15.1.0")
  include(ASN1/m3ap-15.1.0.cmake)
else()
  message(FATAL_ERROR "unknown M3AP_RELEASE ${M3AP_RELEASE}")
endif()

run_asn1c(${CMAKE_CURRENT_SOURCE_DIR}/${M3AP_GRAMMAR}
          "M3AP_"
          OUTPUT ${m3ap_source} ${m3ap_headers}
          OPTIONS -gen-APER -no-gen-BER -no-gen-JER -no-gen-OER -gen-UPER -fcompound-names -no-gen-example -fno-include-deps
          COMMENT "M3AP source files"
)

add_library(asn1_m3ap ${m3ap_source})
target_include_directories(asn1_m3ap PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_options(asn1_m3ap PRIVATE -DASN_DISABLE_OER_SUPPORT -w)
