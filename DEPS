# This file is used to manage the dependencies of the Open Screen repo. It is
# used by gclient to determine what version of each dependency to check out.
#
# For more information, please refer to the official documentation:
#   https://sites.google.com/a/chromium.org/dev/developers/how-tos/get-the-code
#
# When adding a new dependency, please update the top-level .gitignore file
# to list the dependency's destination directory.

use_relative_paths = True
git_dependencies = 'SYNC'

gclient_gn_args_file = 'build/config/gclient_args.gni'
gclient_gn_args = [
  'build_with_chromium',
]

vars = {
  'boringssl_git': 'https://boringssl.googlesource.com',
  'chromium_git': 'https://chromium.googlesource.com',
  'quiche_git': 'https://quiche.googlesource.com',

  # NOTE: we should only reference GitHub directly for dependencies toggled
  # with the "not build_with_chromium" condition.
  'github': 'https://github.com',

  # NOTE: Strangely enough, this will be overridden by any _parent_ DEPS, so
  # in Chromium it will correctly be True.
  'build_with_chromium': False,

  # Needed to download additional clang binaries for processing coverage data
  # (from binaries with GN arg `use_coverage=true`).
  #
  # TODO(issuetracker.google.com/155195126): Change this to False and update
  # buildbot to call tools/download-clang-update-script.py instead.
  'checkout_clang_coverage_tools': True,

 # Fetch clang-tidy into the same bin/ directory as our clang binary.
  'checkout_clang_tidy': False,

  # Fetch clangd into the same bin/ directory as our clang binary.
  'checkout_clangd': False,

  # Fetch instrumented libraries for using MSAN builds.
  'checkout_configuration': 'default',
  'checkout_instrumented_libraries': 'checkout_linux and checkout_configuration == "default"',

  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling gn
  # and whatever else without interference from each other.
  'gn_version': 'git_revision:58933a7cdbc90f70f2381f0c72e76d29be1d43a9',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling ninja
  # and whatever else without interference from each other.
  'ninja_version': 'version:2@1.12.1.chromium.4',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling buildtools
  # and whatever else without interference from each other.
  'buildtools_revision': '4277578aa9c45906e51ad33cac1a5a7ad5288010',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling build
  # and whatever else without interference from each other.
  'build_revision': '034cc53eb28fdbcceb4dc1fa9d218d1d7d35c651',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling clang-format
  # and whatever else without interference from each other.
  'clang_format_revision': '1baf9afe06a7955fd9489d11c7f703475d385926',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling libprotobuf-mutator
  # and whatever else without interference from each other.
  'libprotobuf_mutator_revision': 'c1c950eae0440c3808f2b8bd7c57d0c6a42c1a90',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling zlib
  # and whatever else without interference from each other.
  'zlib_revision': '51b7f2abdade71cd9bb0e7a373ef2610ec6f9daf',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling jsoncpp
  # and whatever else without interference from each other.
  'jsoncpp_revision': '9af09c4a4abe5928d1f7a6e7ec1c73a565bb362e',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling pybind11
  # and whatever else without interference from each other.
  'pybind11_revision': 'd03662f0984f652b60e7ddce53d3868002275197',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling googletest
  # and whatever else without interference from each other.
  'googletest_revision': 'eb2d85edd0bff7a712b6aff147cd9f789f0d7d0b',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling boringssl
  # and whatever else without interference from each other.
  'boringssl_revision': '26e8a8acb91a0cfbd2f95bf7245e2eb87d533a2f',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling quiche
  # and whatever else without interference from each other.
  'quiche_revision': 'b8a4aa531a029737bcd741f85314e89775b923b2',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling instrumented_libs
  # and whatever else without interference from each other.
  'instrumented_libs_revision': 'd15c278eed5d38d9acf2d8054cf37baba93cef8e',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling tinycbor
  # and whatever else without interference from each other.
  'tinycbor_revision': '49d3a238cf4b7b7ff8cba1836803af60ca9c7dc5',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling abseil
  # and whatever else without interference from each other.
  'abseil_revision': '1fc748ff47859c3a041b9c2d7cfa5dfb22ae4ec6',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling libfuzzer
  # and whatever else without interference from each other.
  'libfuzzer_revision': '8c09a8f461575f5fc009c4053a18611e23e3879c',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling libc++
  # and whatever else without interference from each other.
  'libcxx_revision': '97b436da4c33663581d394f4ee0a5977fc38c2f4',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling libc++abi
  # and whatever else without interference from each other.
  'libcxxabi_revision': 'fc1897a2c12aa27e703c3ed48b62eba8abf4ce19',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling llvm-libc
  # and whatever else without interference from each other.
  'llvm_libc_revision': '9da4c296d17f1fdddbef1bafc70a618e6228cc6b',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling modp_b64
  # and whatever else without interference from each other.
  'modp_b64_revision': '50685101d51ef9aabbd60c94f52d9e026d39c509',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling googleurl
  # and whatever else without interference from each other.
  'googleurl_revision': '94ff147fe0b96b4cca5d6d316b9af6210c0b8051',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling perfetto
  # and whatever else without interference from each other.
  'perfetto_revision': '87f0c25cfa45b51c2aaf0e68a41947c23c6d1f0f',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling rust
  # and whatever else without interference from each other.
  'rust_revision': '9200834b7dde809b652b9f0d4561c2bc0e9067c8',

  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling clang update.py
  # and whatever else without interference from each other.
  'chrome_version': '02efa700fe6ba2c66553083b86a1dc8abd7010d9',

  # 'magic' text to tell depot_tools that git submodules should be accepted
  # but parity with DEPS file is expected.
  'SUBMODULE_MIGRATION': 'True',

  # condition to allowlist deps to be synced in Cider. Allowlisting is needed
  # because not all deps are compatible with Cider. Once we migrate everything
  # to be compatible we can get rid of this allowlisting mecahnism and remove
  # this condition. Tracking bug for removing this condition: b/349365433
  'non_git_source': 'True',

  # CPython 3 CIPD package version for Siso hermetic toolchain.
  'cpython3_version': 'version:3@3.11.9.chromium.38',

  # This can be overridden, e.g. with custom_vars, to build clang from HEAD
  # instead of downloading the prebuilt pinned revision.
  'llvm_force_head_revision': False,
}

deps = {
  # A mirror of the corresponding folder in Chromium maintained here:
  # https://chromium.googlesource.com/chromium/src/buildtools/+/refs/heads/main
  #
  # IMPORTANT: Read the instructions at docs/roll_deps.md
  'buildtools': {
    'url': Var('chromium_git') + '/chromium/src/buildtools' +
      '@' + Var('buildtools_revision'),
  },

  # A mirror of the corresponding folder in Chromium maintained here:
  # https://chromium.googlesource.com/chromium/src/build/+/refs/heads/main
  'build': {
    'url': Var('chromium_git') + '/chromium/src/build' +
      '@' + Var('build_revision'),
    'condition': 'not build_with_chromium',
  },

  'third_party/clang-format/script': {
    'url': Var('chromium_git') +
      '/external/github.com/llvm/llvm-project/clang/tools/clang-format.git' +
      '@' + Var('clang_format_revision'),
    'condition': 'not build_with_chromium',
  },
  'buildtools/linux64': {
    'packages': [
      {
        'package': 'gn/gn/linux-amd64',
        'version': Var('gn_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "linux" and not build_with_chromium',
  },
  'buildtools/mac': {
    'packages': [
      {
        'package': 'gn/gn/mac-${{arch}}',
        'version': Var('gn_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "mac" and not build_with_chromium',
  },
  'buildtools/win': {
    'packages': [
      {
        'package': 'gn/gn/windows-amd64',
        'version': Var('gn_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "win"',
  },

  # Always download Linux x64 package regardless of host OS for RBE workers.
  'third_party/cpython3/linux-amd64': {
    'packages': [
      {
        'package': 'infra/3pp/tools/cpython3/linux-amd64',
        'version': Var('cpython3_version'),
      },
    ],
    'condition': 'not build_with_chromium and non_git_source',
    'dep_type': 'cipd',
  },

  # Host platform package.
  'third_party/cpython3/host': {
    'packages': [
      {
        'package': 'infra/3pp/tools/cpython3/${{platform}}',
        'version': Var('cpython3_version'),
      },
    ],
    'condition': 'not build_with_chromium and non_git_source',
    'dep_type': 'cipd',
  },

  'third_party/ninja': {
    'packages': [
      # https://chrome-infra-packages.appspot.com/p/infra/3pp/tools/ninja
      {
        'package': 'infra/3pp/tools/ninja/${{platform}}',
        'version': Var('ninja_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'not build_with_chromium',
  },

  'third_party/libprotobuf-mutator/src': {
    'url': Var('chromium_git') +
      '/external/github.com/google/libprotobuf-mutator.git' +
      '@' + Var('libprotobuf_mutator_revision'),
    'condition': 'not build_with_chromium',
  },

  'third_party/zlib/src': {
    'url': Var('github') +
      '/madler/zlib.git' +
      '@' + Var('zlib_revision'), # version 1.3.1
    'condition': 'not build_with_chromium',
  },

  'third_party/jsoncpp/src': {
    'url': Var('chromium_git') +
      '/external/github.com/open-source-parsers/jsoncpp.git' +
      '@' + Var('jsoncpp_revision'),
    'condition': 'not build_with_chromium',
  },

  'third_party/pybind11/src': {
    'url': Var('github') +
      '/pybind/pybind11.git' +
      '@' + Var('pybind11_revision'), # 3.0.4
    'condition': 'not build_with_chromium',
  },

  # googletest now recommends "living at head," which is a bit of a crapshoot
  # because regressions land upstream frequently.  This is a known good revision.
  'third_party/googletest/src': {
    'url': Var('chromium_git') +
      '/external/github.com/google/googletest.git' +
      '@' + Var('googletest_revision'),
    'condition': 'not build_with_chromium',
  },

  # Make sure to also update ./third_party/boringssl/README.chromium's
  # `Revision:` field when updating this dependency.
  'third_party/boringssl/src': {
    'url' : Var('boringssl_git') + '/boringssl.git' +
      '@' + Var('boringssl_revision'),
    'condition': 'not build_with_chromium',
  },

  # To roll forward, typically it is best to match Chrome's version by using
  # quiche_revision from chromium/src/DEPS. Coordination with the QUICHE
  # maintainers may be needed for some breaking changes.
  'third_party/quiche/src': {
    'url': Var('quiche_git') + '/quiche.git' +
      '@' + Var('quiche_revision'),  # 2026-05-07
    'condition': 'not build_with_chromium',
  },

  'third_party/instrumented_libs': {
    'url': Var('chromium_git') + '/chromium/third_party/instrumented_libraries.git' +
      '@' + Var('instrumented_libs_revision'),
    'condition': 'not build_with_chromium',
  },

  'third_party/tinycbor/src':
    Var('chromium_git') + '/external/github.com/intel/tinycbor.git' +
    '@' + Var('tinycbor_revision'),

  # Abseil recommends living at head; we take a revision from one of the LTS
  # tags.  Chromium has forked abseil for reasons and it seems to be rolled
  # frequently, but LTS should generally be safe for interop with Chromium code.
  'third_party/abseil/src': {
    'url': Var('chromium_git') +
      '/external/github.com/abseil/abseil-cpp.git' + '@' +
      Var('abseil_revision'),
    'condition': 'not build_with_chromium',
  },

  'third_party/libfuzzer/src': {
    'url': Var('chromium_git') +
      '/external/github.com/llvm/llvm-project/compiler-rt/lib/fuzzer.git' +
      '@' + Var('libfuzzer_revision'),
    'condition': 'not build_with_chromium',
  },

  # IMPORTANT: Read the instructions at docs/roll_deps.md
  'third_party/libc++/src': {
    'url': Var('chromium_git') +
    '/external/github.com/llvm/llvm-project/libcxx.git' + '@' + Var('libcxx_revision'),
    'condition': 'not build_with_chromium',
  },

  # IMPORTANT: Read the instructions at docs/roll_deps.md
  'third_party/libc++abi/src': {
    'url': Var('chromium_git') +
    '/external/github.com/llvm/llvm-project/libcxxabi.git' + '@' + Var('libcxxabi_revision'),
    'condition': 'not build_with_chromium',
  },
  'third_party/llvm-build/Release+Asserts': {
    'dep_type': 'gcs',
    'bucket': 'chromium-browser-clang',
    'condition': 'not llvm_force_head_revision',
    'objects': [
      {
        'object_name': 'Linux_x64/clang-android-runtime-library-llvmorg-24-init-3796-g20e97c4b-2.tar.xz',
        'sha256sum': 'df24340a4efe4d34cdf9a14dd9441d0576b01b18ef8313247f8972d28c104f75',
        'size_bytes': 2745584,
        'generation': 1786821109912347,
        'condition': 'checkout_android and non_git_source',
      },
      {
        'object_name': 'Linux_x64/clang-llvmorg-24-init-3796-g20e97c4b-2.tar.xz',
        'sha256sum': '86d53b33007a35695645e3867589a20f6f8aeedd146331e1a2ec8fd371e05ea2',
        'size_bytes': 59131228,
        'generation': 1786821103254815,
        'condition': 'host_os == "linux" and non_git_source',
      },
      {
        'object_name': 'Linux_x64/clang-tidy-llvmorg-24-init-3796-g20e97c4b-2.tar.xz',
        'sha256sum': '5f1f647be6760435917118693e380281f413c0ceb49ba0636aa2daa9811e3090',
        'size_bytes': 14864124,
        'generation': 1786821103236504,
        'condition': 'host_os == "linux" and checkout_clang_tidy and non_git_source',
      },
      {
        'object_name': 'Linux_x64/clangd-llvmorg-24-init-3796-g20e97c4b-2.tar.xz',
        'sha256sum': '60b575e202525354d09c22794c4c27829de54ce41566dc6dda2bc9b274179a4a',
        'size_bytes': 15044588,
        'generation': 1786821103240780,
        'condition': 'host_os == "linux" and checkout_clangd and non_git_source',
      },
      {
        'object_name': 'Linux_x64/llvm-code-coverage-llvmorg-24-init-3796-g20e97c4b-2.tar.xz',
        'sha256sum': 'ac7c90ee1eb797cbee2502f452c8c37c14b1bd399bdfe7ca2a94c49325f7a15e',
        'size_bytes': 2354368,
        'generation': 1786821103370588,
        'condition': 'host_os == "linux" and checkout_clang_coverage_tools and non_git_source',
      },
      {
        'object_name': 'Linux_x64/llvmobjdump-llvmorg-24-init-3796-g20e97c4b-2.tar.xz',
        'sha256sum': '3732c020f26bfaf63a851e61022e9d1bcea5764aeadf5715b3020851405c0058',
        'size_bytes': 5895216,
        'generation': 1786821103316462,
        'condition': '((checkout_linux or checkout_mac or checkout_android) and host_os == "linux") and non_git_source',
      },
      {
        'object_name': 'Mac/clang-llvmorg-24-init-3796-g20e97c4b-2.tar.xz',
        'sha256sum': 'ec510fb1db5e96d127eaeb55ff61daf49d58647c653a65c6ffec42c0b59e3d84',
        'size_bytes': 56450032,
        'generation': 1786821111761283,
        'condition': 'host_os == "mac" and host_cpu == "x64"',
      },
      {
        'object_name': 'Mac/clang-mac-runtime-library-llvmorg-24-init-3796-g20e97c4b-2.tar.xz',
        'sha256sum': '659113c27f4d45fa21d991038072a1fe931c28eccf6f3a79f339089e4f9c3af7',
        'size_bytes': 1013300,
        'generation': 1786821118134066,
        'condition': 'checkout_mac and not host_os == "mac"',
      },
      {
        'object_name': 'Mac/clang-tidy-llvmorg-24-init-3796-g20e97c4b-2.tar.xz',
        'sha256sum': 'ce79c0ea03d91ab2f8b902872ecd03ab2df5d02e8a7790bdc998f304d26b41a7',
        'size_bytes': 14897164,
        'generation': 1786821111829674,
        'condition': 'host_os == "mac" and host_cpu == "x64" and checkout_clang_tidy',
      },
      {
        'object_name': 'Mac/clangd-llvmorg-24-init-3796-g20e97c4b-2.tar.xz',
        'sha256sum': '7e57c1131e71bb782c83b1f99bfe36b2ce15c0651916d2b6183efcd683ea7f75',
        'size_bytes': 16775664,
        'generation': 1786821111764923,
        'condition': 'host_os == "mac" and host_cpu == "x64" and checkout_clangd',
      },
      {
        'object_name': 'Mac/llvm-code-coverage-llvmorg-24-init-3796-g20e97c4b-2.tar.xz',
        'sha256sum': '83b01a9a2ec07795ef4c7c60e7351c9c01b9bd66bf5f3e57f9fb30fafafe99f6',
        'size_bytes': 2411064,
        'generation': 1786821111807997,
        'condition': 'host_os == "mac" and host_cpu == "x64" and checkout_clang_coverage_tools',
      },
      {
        'object_name': 'Mac/llvmobjdump-llvmorg-24-init-3796-g20e97c4b-2.tar.xz',
        'sha256sum': '662170dce8cec74c9bf32a97c80522c624ad712445986563ef6086c33b3745a9',
        'size_bytes': 5879520,
        'generation': 1786821111792653,
        'condition': 'host_os == "mac" and host_cpu == "x64"',
      },
      {
        'object_name': 'Mac_arm64/clang-llvmorg-24-init-3796-g20e97c4b-2.tar.xz',
        'sha256sum': '50946ba4d9fef901a4a5f7cbf3893103ef4c82c569d088082f5497c7ae038515',
        'size_bytes': 47226060,
        'generation': 1786821120118643,
        'condition': 'host_os == "mac" and host_cpu == "arm64"',
      },
      {
        'object_name': 'Mac_arm64/clang-tidy-llvmorg-24-init-3796-g20e97c4b-2.tar.xz',
        'sha256sum': '69c0991e4b404f01fc2cd90496a0484250568d7432c51a555a4d353d74184227',
        'size_bytes': 12959144,
        'generation': 1786821120103758,
        'condition': 'host_os == "mac" and host_cpu == "arm64" and checkout_clang_tidy',
      },
      {
        'object_name': 'Mac_arm64/clangd-llvmorg-24-init-3796-g20e97c4b-2.tar.xz',
        'sha256sum': 'fd8c3d3fb93d58a6556905408c807f7879d373fb69e7dc69fcb67abcdc3dde80',
        'size_bytes': 13310300,
        'generation': 1786821120257240,
        'condition': 'host_os == "mac" and host_cpu == "arm64" and checkout_clangd',
      },
      {
        'object_name': 'Mac_arm64/llvm-code-coverage-llvmorg-24-init-3796-g20e97c4b-2.tar.xz',
        'sha256sum': 'cbe6a0e51caabee3e5de5e957867914c34cb742e365b1c8c62826f71cdb7d7d0',
        'size_bytes': 2031248,
        'generation': 1786821120280478,
        'condition': 'host_os == "mac" and host_cpu == "arm64" and checkout_clang_coverage_tools',
      },
      {
        'object_name': 'Mac_arm64/llvmobjdump-llvmorg-24-init-3796-g20e97c4b-2.tar.xz',
        'sha256sum': '0c5d279fe3c1aa4876f98ed59063ac51e0139e55127c7677feeed8e03512eae3',
        'size_bytes': 5601008,
        'generation': 1786821120150557,
        'condition': 'host_os == "mac" and host_cpu == "arm64"',
      },
      {
        'object_name': 'Win/clang-llvmorg-24-init-3796-g20e97c4b-2.tar.xz',
        'sha256sum': 'f977a6e2889a599afd36b4f7125577612198e00686720d865d32c764e110a9e0',
        'size_bytes': 51459508,
        'generation': 1786821128765775,
        'condition': 'host_os == "win"',
      },
      {
        'object_name': 'Win/clang-tidy-llvmorg-24-init-3796-g20e97c4b-2.tar.xz',
        'sha256sum': 'bbcbde20f5318f6eb8ebdbde5bd017bd03030c312be66bfa49fe8a63a51e874c',
        'size_bytes': 15131824,
        'generation': 1786821128762008,
        'condition': 'host_os == "win" and checkout_clang_tidy',
      },
      {
        'object_name': 'Win/clang-win-runtime-library-llvmorg-24-init-3796-g20e97c4b-2.tar.xz',
        'sha256sum': 'f605f88677363686e9fc6dfc3305db36717e3c169015484f277f9aeadb20ae86',
        'size_bytes': 2639484,
        'generation': 1786821135267843,
        'condition': 'checkout_win and not host_os == "win"',
      },
      {
        'object_name': 'Win/clangd-llvmorg-24-init-3796-g20e97c4b-2.tar.xz',
        'sha256sum': '56e33158dee9ed7412598b4c52754470256c829e619d4e5c694053ef46571920',
        'size_bytes': 15458116,
        'generation': 1786821128775871,
       'condition': 'host_os == "win" and checkout_clangd',
      },
      {
        'object_name': 'Win/llvm-code-coverage-llvmorg-24-init-3796-g20e97c4b-2.tar.xz',
        'sha256sum': 'fe540470e9e830b6ebf9c17e1abef717ae1b18f5ecd87fa5442a7a6fd7d651d0',
        'size_bytes': 2514332,
        'generation': 1786821128934305,
        'condition': 'host_os == "win" and checkout_clang_coverage_tools',
      },
      {
        'object_name': 'Win/llvmobjdump-llvmorg-24-init-3796-g20e97c4b-2.tar.xz',
        'sha256sum': '62414b66e12171355237d121be31349664006574419eb4f4b0be4c9438e6a1ef',
        'size_bytes': 5989348,
        'generation': 1786821128627416,
        'condition': '(checkout_linux or checkout_mac or checkout_android) and host_os == "win"',
      },
    ]
  },

  'third_party/llvm-libc/src': {
    'url': Var('chromium_git') +
      '/external/github.com/llvm/llvm-project/libc.git' + '@' + Var('llvm_libc_revision'),
    'condition': 'not build_with_chromium',
  },

  'third_party/modp_b64': {
    'url': Var('chromium_git') + '/chromium/src/third_party/modp_b64' +
      '@' + Var('modp_b64_revision'),
    'condition': 'not build_with_chromium',
  },

  # Googleurl recommends living at head. This is a copy of Chrome's URL parsing
  # library. It is meant to be used by QUICHE.
  #
  # Make sure to also update ./third_party/googleurl/README.chromium's
  # `Revision:` field when updating this dependency.
  'third_party/googleurl/src': {
    'url': Var('quiche_git') + '/googleurl.git' +
      '@' + Var('googleurl_revision'),  #2025-11-11
    'condition': 'not build_with_chromium',
  },

  'third_party/perfetto/src': {
    'url': Var('chromium_git') + '/external/github.com/google/perfetto.git' +
      '@' + Var('perfetto_revision'),
    'condition': 'not build_with_chromium',
  },

  'third_party/rust': {
    'url': Var('chromium_git') + '/chromium/src/third_party/rust' +
      '@' + Var('rust_revision'),
    'condition': 'not build_with_chromium',
  },
}

hooks = [
  {
    'name': 'clang_update_script',
    'pattern': '.',
    'condition': 'not build_with_chromium',
    'action': [ 'python3', 'tools/download-chromium-file.py',
                '--revision', Var('chrome_version'),
                '--path', 'tools/clang/scripts/update.py',
                '--output', 'tools/clang/scripts/update.py' ],
    # NOTE: This file appears in .gitignore, as it is not a part of the
    # openscreen repo.
  },
  {
    'name': 'rust_update_script',
    'pattern': '.',
    'condition': 'not build_with_chromium',
    'action': [ 'python3', 'tools/download-chromium-file.py',
                '--revision', Var('chrome_version'),
                '--path', 'tools/rust/update_rust.py',
                '--output', 'tools/rust/update_rust.py' ],
  },
  {
    'name': 'rust_toolchain',
    'pattern': '.',
    'condition': 'not build_with_chromium',
    'action': [ 'python3', 'tools/rust/update_rust.py' ],
  },
  {
    'name': 'licenses_script',
    'pattern': '.',
    'condition': 'not build_with_chromium',
    'action': [ 'python3', 'tools/download-chromium-file.py',
                '--revision', Var('chrome_version'),
                '--path', 'tools/licenses/licenses.py',
                '--output', 'tools/licenses/licenses.py' ],
  },
  {
    'name': 'licenses_spdx_writer',
    'pattern': '.',
    'condition': 'not build_with_chromium',
    'action': [ 'python3', 'tools/download-chromium-file.py',
                '--revision', Var('chrome_version'),
                '--path', 'tools/licenses/spdx_writer.py',
                '--output', 'tools/licenses/spdx_writer.py' ],
  },
  {
    'name': 'protoc_wrapper_script',
    'pattern': '.',
    'condition': 'not build_with_chromium',
    'action': [ 'python3', 'tools/download-chromium-file.py',
                '--revision', Var('chrome_version'),
                '--path', 'tools/protoc_wrapper/protoc_wrapper.py',
                '--output', 'tools/protoc_wrapper/protoc_wrapper.py' ],
  },
]

# This exists to allow Google Cloud Storage blobs in these DEPS to be fetched.
# Do not add any additional recursedeps entries without consulting
# mfoltz@chromium.org!
recursedeps = [
  'build',
  'buildtools',
  'third_party/instrumented_libs',
]

include_rules = [
  '+util',
  '+platform/api',
  '+platform/base',
  '+platform/test',
  '+testing/util',
  '+third_party',

  # Inter-module dependencies must be through public APIs.
  '-discovery',
  '+discovery/common',
  '+discovery/dnssd/public',
  '+discovery/mdns/public',
  '+discovery/public',

  # Don't include Abseil.
  '-third_party/abseil',
  '-absl',

  # Similar to abseil, don't include boringssl using root path.  Instead,
  # explicitly allow 'openssl' where needed.
  '-third_party/boringssl',

  # Test framework includes.
  '-third_party/googletest',
  '+gtest',
  '+gmock',

  # Can use generic Chromium buildflags.
  '+build/build_config.h',
]
