# mbedTLS 4.x from the third-party/mbedtls submodule.
# Its generated sources are vendored in third-party/mbedtls-gen, so no Python or Perl is needed;
# refresh them with scripts/mbedtls_gen.py after updating the submodule.

set(UCXAPP_MBEDTLS_DIR ${CMAKE_CURRENT_LIST_DIR}/../third-party/mbedtls)
set(_ucx_mbedtls_gen ${CMAKE_CURRENT_LIST_DIR}/third-party/mbedtls-gen)
set(_ucx_mbedtls_bin ${CMAKE_BINARY_DIR}/mbedtls)

file(COPY ${_ucx_mbedtls_gen}/library/ DESTINATION ${_ucx_mbedtls_bin}/library)
file(COPY ${_ucx_mbedtls_gen}/tf-psa-crypto/core/ DESTINATION ${_ucx_mbedtls_bin}/tf-psa-crypto/core)

foreach(_opt GEN_FILES ENABLE_PROGRAMS ENABLE_TESTING MBEDTLS_FATAL_WARNINGS TF_PSA_CRYPTO_FATAL_WARNINGS)
  set(${_opt} OFF CACHE BOOL "" FORCE)
endforeach()
set(TF_PSA_CRYPTO_USER_CONFIG_FILE ${CMAKE_CURRENT_LIST_DIR}/mbedtls_user_config.h CACHE FILEPATH "" FORCE)

# mbedTLS declares cmake 3.5: allow LTO on its targets and lift its minimum to silence the deprecation notice.
set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)
set(CMAKE_POLICY_VERSION_MINIMUM 3.10)
add_subdirectory(${UCXAPP_MBEDTLS_DIR} ${_ucx_mbedtls_bin} EXCLUDE_FROM_ALL)
unset(CMAKE_POLICY_VERSION_MINIMUM)

set(UCXAPP_MBEDTLS_LIBS mbedtls mbedx509 tfpsacrypto)
