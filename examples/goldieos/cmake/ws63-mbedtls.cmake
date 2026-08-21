include_guard(GLOBAL)

include(CMakeParseArguments)

# Build the WS63-specific mbedTLS 3.1.0 library and HiSilicon hardening
# adaptation as a regular CMake target. The caller supplies the GoldieOS
# source root and the shared SDK include root so this module has no dependency
# on the directory from which it is included.
function(goldieos_add_ws63_mbedtls)
    set(_options)
    set(_one_value_args
        TARGET
        SOURCE_DIR
        SDK_INCLUDE_DIR
        PLATFORM_TARGET
    )
    cmake_parse_arguments(WS63_MBEDTLS
        "${_options}" "${_one_value_args}" "" ${ARGN})

    foreach(_required_arg IN ITEMS
            TARGET SOURCE_DIR SDK_INCLUDE_DIR PLATFORM_TARGET)
        if(NOT WS63_MBEDTLS_${_required_arg})
            message(FATAL_ERROR
                "goldieos_add_ws63_mbedtls requires ${_required_arg}")
        endif()
    endforeach()
    if(WS63_MBEDTLS_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "goldieos_add_ws63_mbedtls received unknown arguments: "
            "${WS63_MBEDTLS_UNPARSED_ARGUMENTS}")
    endif()
    if(TARGET "${WS63_MBEDTLS_TARGET}")
        message(FATAL_ERROR
            "Target already exists: ${WS63_MBEDTLS_TARGET}")
    endif()
    if(NOT TARGET "${WS63_MBEDTLS_PLATFORM_TARGET}")
        message(FATAL_ERROR
            "WS63 platform target does not exist: "
            "${WS63_MBEDTLS_PLATFORM_TARGET}")
    endif()

    set(_source_dir "${WS63_MBEDTLS_SOURCE_DIR}")
    set(_mbedtls_dir "${_source_dir}/third_party/mbedtls-ws63")
    if(NOT EXISTS "${_mbedtls_dir}/library/ssl_tls.c")
        message(FATAL_ERROR
            "WS63 mbedTLS source not found: ${_mbedtls_dir}")
    endif()

    set(_sources
        # mbedTLS core / crypto / X.509 / TLS
        "${_mbedtls_dir}/library/aes.c"
        "${_mbedtls_dir}/library/aesni.c"
        "${_mbedtls_dir}/library/aria.c"
        "${_mbedtls_dir}/library/asn1parse.c"
        "${_mbedtls_dir}/library/asn1write.c"
        "${_mbedtls_dir}/library/base64.c"
        "${_mbedtls_dir}/library/bignum.c"
        "${_mbedtls_dir}/library/camellia.c"
        "${_mbedtls_dir}/library/ccm.c"
        "${_mbedtls_dir}/library/chacha20.c"
        "${_mbedtls_dir}/library/chachapoly.c"
        "${_mbedtls_dir}/library/cipher.c"
        "${_mbedtls_dir}/library/cipher_wrap.c"
        "${_mbedtls_dir}/library/cmac.c"
        "${_mbedtls_dir}/library/constant_time.c"
        "${_mbedtls_dir}/library/ctr_drbg.c"
        "${_mbedtls_dir}/library/debug.c"
        "${_mbedtls_dir}/library/des.c"
        "${_mbedtls_dir}/library/dhm.c"
        "${_mbedtls_dir}/library/ecdh.c"
        "${_mbedtls_dir}/library/ecdsa.c"
        "${_mbedtls_dir}/library/ecjpake.c"
        "${_mbedtls_dir}/library/ecp.c"
        "${_mbedtls_dir}/library/ecp_curves.c"
        "${_mbedtls_dir}/library/entropy.c"
        "${_mbedtls_dir}/library/error.c"
        "${_mbedtls_dir}/library/gcm.c"
        "${_mbedtls_dir}/library/hmac_drbg.c"
        "${_mbedtls_dir}/library/hkdf.c"
        "${_mbedtls_dir}/library/md.c"
        "${_mbedtls_dir}/library/md5.c"
        "${_mbedtls_dir}/library/memory_buffer_alloc.c"
        "${_mbedtls_dir}/library/mps_reader.c"
        "${_mbedtls_dir}/library/mps_trace.c"
        "${_mbedtls_dir}/library/net_sockets.c"
        "${_mbedtls_dir}/library/nist_kw.c"
        "${_mbedtls_dir}/library/oid.c"
        "${_mbedtls_dir}/library/padlock.c"
        "${_mbedtls_dir}/library/pem.c"
        "${_mbedtls_dir}/library/pk.c"
        "${_mbedtls_dir}/library/pk_wrap.c"
        "${_mbedtls_dir}/library/pkcs5.c"
        "${_mbedtls_dir}/library/pkcs12.c"
        "${_mbedtls_dir}/library/pkparse.c"
        "${_mbedtls_dir}/library/pkwrite.c"
        "${_mbedtls_dir}/library/platform.c"
        "${_mbedtls_dir}/library/platform_util.c"
        "${_mbedtls_dir}/library/poly1305.c"
        "${_mbedtls_dir}/library/ripemd160.c"
        "${_mbedtls_dir}/library/rsa.c"
        "${_mbedtls_dir}/library/rsa_alt_helpers.c"
        "${_mbedtls_dir}/library/sha1.c"
        "${_mbedtls_dir}/library/sha256.c"
        "${_mbedtls_dir}/library/sha512.c"
        "${_mbedtls_dir}/library/ssl_cache.c"
        "${_mbedtls_dir}/library/ssl_ciphersuites.c"
        "${_mbedtls_dir}/library/ssl_cli.c"
        "${_mbedtls_dir}/library/ssl_cookie.c"
        "${_mbedtls_dir}/library/ssl_debug_helpers_generated.c"
        "${_mbedtls_dir}/library/ssl_msg.c"
        "${_mbedtls_dir}/library/ssl_srv.c"
        "${_mbedtls_dir}/library/ssl_ticket.c"
        "${_mbedtls_dir}/library/ssl_tls.c"
        "${_mbedtls_dir}/library/ssl_tls13_client.c"
        "${_mbedtls_dir}/library/ssl_tls13_generic.c"
        "${_mbedtls_dir}/library/ssl_tls13_server.c"
        "${_mbedtls_dir}/library/ssl_tls13_keys.c"
        "${_mbedtls_dir}/library/threading.c"
        "${_mbedtls_dir}/library/version.c"
        "${_mbedtls_dir}/library/version_features.c"
        "${_mbedtls_dir}/library/x509.c"
        "${_mbedtls_dir}/library/x509_create.c"
        "${_mbedtls_dir}/library/x509_crl.c"
        "${_mbedtls_dir}/library/x509_crt.c"
        "${_mbedtls_dir}/library/x509_csr.c"
        "${_mbedtls_dir}/library/x509write_crt.c"
        "${_mbedtls_dir}/library/x509write_csr.c"

        # HiSilicon harden / ALT adaptation
        "${_mbedtls_dir}/harden/src/cipher_adapt.c"
        "${_mbedtls_dir}/harden/src/cipher_common.c"
        "${_mbedtls_dir}/harden/src/cmac_harden.c"
        "${_mbedtls_dir}/harden/src/ecc_harden_common.c"
        "${_mbedtls_dir}/harden/src/ecc_harden.c"
        "${_mbedtls_dir}/harden/src/hkdf_harden.c"
        "${_mbedtls_dir}/harden/src/pbkdf2_hmac_harden.c"
        "${_mbedtls_dir}/harden/src/bignum_harden.c"
        "${_mbedtls_dir}/harden/src/entropy_harden.c"
        "${_mbedtls_dir}/harden/src/dfx.c"
        "${_mbedtls_dir}/harden/src/gcm_harden.c"
        "${_mbedtls_dir}/harden/src/ccm_harden.c"
        "${_mbedtls_dir}/harden/src/hash_harden_common.c"
        "${_mbedtls_dir}/harden/src/connect_src/hash_harden_adapt.c"
        "${_mbedtls_dir}/harden/src/connect_src/sha1.c"
        "${_mbedtls_dir}/harden/src/connect_src/sha256.c"
        "${_mbedtls_dir}/harden/src/connect_src/sha512.c"
        "${_mbedtls_dir}/harden/src/connect_src/aes.c"
        "${_mbedtls_dir}/harden/src/connect_src/aes_harden_adapt.c"
        "${_mbedtls_dir}/harden/src/connect_src/rsa.c"
        "${_mbedtls_dir}/harden/src/connect_src/rsa_harden_adapt.c"
        "${_mbedtls_dir}/harden/src/connect_src/ecp.c"
        "${_mbedtls_dir}/harden/src/connect_src/ecp_harden_adapt.c"
    )

    add_library("${WS63_MBEDTLS_TARGET}" STATIC ${_sources})

    target_include_directories("${WS63_MBEDTLS_TARGET}" PRIVATE
        "${_source_dir}/sdk_integration"
        "${_mbedtls_dir}/compat_inc"
        "${_mbedtls_dir}/include"
        "${_mbedtls_dir}/include/mbedtls"
        "${_mbedtls_dir}/configs"
        "${_mbedtls_dir}/library"
        "${_mbedtls_dir}/3rdparty/everest/include/everest"
        "${_mbedtls_dir}/harden/src/internal_include"
        "${_mbedtls_dir}/harden/platform/connect"
        "${_mbedtls_dir}/harden/cipher_adapt_include"
        "${_mbedtls_dir}/harden/src/connect_src"
        "${_mbedtls_dir}/harden/securec_include"
        "${_source_dir}/include/third_party/lwip"
        "${WS63_MBEDTLS_SDK_INCLUDE_DIR}"
    )

    target_compile_definitions("${WS63_MBEDTLS_TARGET}" PRIVATE
        MBEDTLS_CONFIG_FILE="goldieos_config.h"
        MBEDTLS_USER_CONFIG_FILE="mbedtls_platform_hardware_config.h"
        TD_SUPPORT_STDLIB
        SYSCALLS_H
        WS_IOT_LWIP_C
    )

    # The interface target carries the directory-wide WS63 definitions that
    # the original monolithic build applied to this target.
    target_link_libraries("${WS63_MBEDTLS_TARGET}" PRIVATE
        "${WS63_MBEDTLS_PLATFORM_TARGET}")

    convai_target_enable_warnings("${WS63_MBEDTLS_TARGET}")
    convai_target_enable_ws63_options("${WS63_MBEDTLS_TARGET}")
endfunction()
