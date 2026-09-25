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
  'build_revision': '9eae7c82c57b9e64a85bcf3a246821dffd9d4d5e',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling clang-format
  # and whatever else without interference from each other.
  'clang_format_revision': '47dce8466cd71943f64df3860e608957d6e20007',
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
  'tinycbor_revision': '213155c8729b07dea836c5ac6e807a089486bf7b',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling abseil
  # and whatever else without interference from each other.
  'abseil_revision': '1fc748ff47859c3a041b9c2d7cfa5dfb22ae4ec6',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling libfuzzer
  # and whatever else without interference from each other.
  'libfuzzer_revision': '9951014982324338ea932dfdba259aeb1cca70f7',
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
  'perfetto_revision': 'c40e458b6f23e43d5ba180ccf2a53d09d14e4567',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling rust
  # and whatever else without interference from each other.
  'rust_revision': '9200834b7dde809b652b9f0d4561c2bc0e9067c8',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling simple_dns
  # and whatever else without interference from each other.
  'simple_dns_revision': '5c7ec98798d7c9f7d76df7aa3bbe754913ff5159',

  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling clang update.py
  # and whatever else without interference from each other.
  'chrome_version': 'a613b725b06a0561c03ee5b4ceadf8bc6774f884',

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
        'object_name': 'Linux_x64/clang-android-runtime-library-llvmorg-24-init-7747-g62397f8b-27.tar.xz',
        'sha256sum': '9f3076dcb74c53198ccc5cb2db52d548f8c8edce382e4b499f66d5024f34fb11',
        'size_bytes': 6203280,
        'generation': 1789127816319422,
        'condition': 'checkout_android and non_git_source',
      },
      {
        'object_name': 'Linux_x64/clang-llvmorg-24-init-7747-g62397f8b-27.tar.xz',
        'sha256sum': '83f4a4ea3292bfc236d85575fa7af27d39180d88534bc3117b73ad1df34bb959',
        'size_bytes': 57243988,
        'generation': 1789127809113314,
        'condition': 'host_os == "linux" and non_git_source',
      },
      {
        'object_name': 'Linux_x64/clang-tidy-llvmorg-24-init-7747-g62397f8b-27.tar.xz',
        'sha256sum': 'a3a717113cbcdfdbdce6f8f178efe9dbd4352b69b28858cafe9dd12144b4d8b4',
        'size_bytes': 14947636,
        'generation': 1789127808973657,
        'condition': 'host_os == "linux" and checkout_clang_tidy and non_git_source',
      },
      {
        'object_name': 'Linux_x64/clangd-llvmorg-24-init-7747-g62397f8b-27.tar.xz',
        'sha256sum': 'fe21bb9e363dad5cbf3c0e6eff3e38f14eb45b2c61a2642d8ed405f04fc8da1a',
        'size_bytes': 15091424,
        'generation': 1789127808995417,
        'condition': 'host_os == "linux" and checkout_clangd and non_git_source',
      },
      {
        'object_name': 'Linux_x64/llvm-code-coverage-llvmorg-24-init-7747-g62397f8b-27.tar.xz',
        'sha256sum': '8d4396305ff89df80d64ae5073c9aa0c62e587a17411e3fd900b4f2f502d8e17',
        'size_bytes': 2371104,
        'generation': 1789127809277865,
        'condition': 'host_os == "linux" and checkout_clang_coverage_tools and non_git_source',
      },
      {
        'object_name': 'Linux_x64/llvmobjdump-llvmorg-24-init-7747-g62397f8b-27.tar.xz',
        'sha256sum': '4747ad1ae530a94d3d9ae1de3a0533bd82e4afaf588905500bcb65ad3626344e',
        'size_bytes': 5919816,
        'generation': 1789127809398773,
        'condition': '((checkout_linux or checkout_mac or checkout_android) and host_os == "linux") and non_git_source',
      },
      {
        'object_name': 'Mac/clang-llvmorg-24-init-7747-g62397f8b-27.tar.xz',
        'sha256sum': '5b893eb0dc07cb151fee5a19b40f159dd697ba21ca99d0f59e60a80e99acf278',
        'size_bytes': 56729348,
        'generation': 1789127818277683,
        'condition': 'host_os == "mac" and host_cpu == "x64"',
      },
      {
        'object_name': 'Mac/clang-mac-runtime-library-llvmorg-24-init-7747-g62397f8b-27.tar.xz',
        'sha256sum': 'dd5c1da410296c96792bae2c3b79127ae1e53a853521e241088a229fa9a20b3b',
        'size_bytes': 981676,
        'generation': 1789127825117200,
        'condition': 'checkout_mac and not host_os == "mac"',
      },
      {
        'object_name': 'Mac/clang-tidy-llvmorg-24-init-7747-g62397f8b-27.tar.xz',
        'sha256sum': 'd6ddd8ae0c92e46466964936812695cf9020b55dae38a96ba9ab73a49b54af3d',
        'size_bytes': 14927288,
        'generation': 1789127817821775,
        'condition': 'host_os == "mac" and host_cpu == "x64" and checkout_clang_tidy',
      },
      {
        'object_name': 'Mac/clangd-llvmorg-24-init-7747-g62397f8b-27.tar.xz',
        'sha256sum': 'fc78380523c4a1712e38dd54b2fa2e1c5d30af1abd14deaba44b9f7d11c6b5cc',
        'size_bytes': 16813268,
        'generation': 1789127817785803,
        'condition': 'host_os == "mac" and host_cpu == "x64" and checkout_clangd',
      },
      {
        'object_name': 'Mac/llvm-code-coverage-llvmorg-24-init-7747-g62397f8b-27.tar.xz',
        'sha256sum': '69ef58d4655f5df294b3526aef7384568c2a169eeb80ec415aa7265ab4154058',
        'size_bytes': 2419748,
        'generation': 1789127818078978,
        'condition': 'host_os == "mac" and host_cpu == "x64" and checkout_clang_coverage_tools',
      },
      {
        'object_name': 'Mac/llvmobjdump-llvmorg-24-init-7747-g62397f8b-27.tar.xz',
        'sha256sum': '839275989dcef1f956ade1e49e93d78e378e57bc272bbdad3694ba28a3d83d9f',
        'size_bytes': 5902912,
        'generation': 1789127818020417,
        'condition': 'host_os == "mac" and host_cpu == "x64"',
      },
      {
        'object_name': 'Mac_arm64/clang-llvmorg-24-init-7747-g62397f8b-27.tar.xz',
        'sha256sum': 'b1d5d53f8dba60834fc3287ff53276fbed4273eee6a8f31a1ff7825f91db77c1',
        'size_bytes': 47539924,
        'generation': 1789127826521173,
        'condition': 'host_os == "mac" and host_cpu == "arm64"',
      },
      {
        'object_name': 'Mac_arm64/clang-tidy-llvmorg-24-init-7747-g62397f8b-27.tar.xz',
        'sha256sum': '26ed86e035feb6a3085969aec14b6d8f58fa7db1630aa4315e907c85cdce688f',
        'size_bytes': 12996816,
        'generation': 1789127826637982,
        'condition': 'host_os == "mac" and host_cpu == "arm64" and checkout_clang_tidy',
      },
      {
        'object_name': 'Mac_arm64/clangd-llvmorg-24-init-7747-g62397f8b-27.tar.xz',
        'sha256sum': 'dbea6965c94b6afeec10e07843a10011e37bcd0d380552740425ead52e6260ba',
        'size_bytes': 13314464,
        'generation': 1789127826581533,
        'condition': 'host_os == "mac" and host_cpu == "arm64" and checkout_clangd',
      },
      {
        'object_name': 'Mac_arm64/llvm-code-coverage-llvmorg-24-init-7747-g62397f8b-27.tar.xz',
        'sha256sum': '0280149cffa39f09823e21a2a5e3fc53b1d0e83e043647429d69f1547f42db31',
        'size_bytes': 2043396,
        'generation': 1789127826942850,
        'condition': 'host_os == "mac" and host_cpu == "arm64" and checkout_clang_coverage_tools',
      },
      {
        'object_name': 'Mac_arm64/llvmobjdump-llvmorg-24-init-7747-g62397f8b-27.tar.xz',
        'sha256sum': 'f2ea6f562955bbaca69358e83a194ee967886d36f8b5cf18fbae79d99450c4bd',
        'size_bytes': 5641596,
        'generation': 1789127826597258,
        'condition': 'host_os == "mac" and host_cpu == "arm64"',
      },
      {
        'object_name': 'Win/clang-llvmorg-24-init-7747-g62397f8b-27.tar.xz',
        'sha256sum': '57e6aaa0edf5b34a8ac51603112ef346cb11059ccf3e3c91553abb9a7544f619',
        'size_bytes': 51840184,
        'generation': 1789127835497655,
        'condition': 'host_os == "win"',
      },
      {
        'object_name': 'Win/clang-tidy-llvmorg-24-init-7747-g62397f8b-27.tar.xz',
        'sha256sum': '8e2f3fa72b5c63c711083e9c528c9679842864c92e557b866b064364dbdbd95f',
        'size_bytes': 15132748,
        'generation': 1789127835458038,
        'condition': 'host_os == "win" and checkout_clang_tidy',
      },
      {
        'object_name': 'Win/clang-win-runtime-library-llvmorg-24-init-7747-g62397f8b-27.tar.xz',
        'sha256sum': '9f09d592579f262cfd4b57fe5cd9e17b071ec8f80fc9d6bb26c90612e476bdef',
        'size_bytes': 2645936,
        'generation': 1789127842749891,
        'condition': 'checkout_win and not host_os == "win"',
      },
      {
        'object_name': 'Win/clangd-llvmorg-24-init-7747-g62397f8b-27.tar.xz',
        'sha256sum': '05936ceb04719b8f117113dfd0914d863f9187bf7bf1fb2ddb6cda13aac15f4e',
        'size_bytes': 15431860,
        'generation': 1789127835571711,
        'condition': 'host_os == "win" and checkout_clangd',
      },
      {
        'object_name': 'Win/llvm-code-coverage-llvmorg-24-init-7747-g62397f8b-27.tar.xz',
        'sha256sum': '9968f504eed6e4cb4b9d46d05ef742448107532c4f3811c79ff26fd20defd995',
        'size_bytes': 2523680,
        'generation': 1789127835702447,
        'condition': 'host_os == "win" and checkout_clang_coverage_tools',
      },
      {
        'object_name': 'Win/llvmobjdump-llvmorg-24-init-7747-g62397f8b-27.tar.xz',
        'sha256sum': 'bac327770909f1a87080a3d82b54ad3857c9f037e77a2078aadcdbc294f18862',
        'size_bytes': 6005628,
        'generation': 1789127835514996,
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

  # TODO(b/554350196): Host a Git-on-Borg mirror or vendor into Chromium
  # //third_party/rust via gnrt for automated dependency rolling.
  'third_party/simple_dns/src': {
    'url': Var('github') + '/balliegojr/simple-dns.git' +
      '@' + Var('simple_dns_revision'),
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
    # Update the Windows toolchain if necessary.
    'name': 'win_toolchain',
    'pattern': '.',
    'condition': 'checkout_win and not build_with_chromium',
    'action': ['python3', 'build/vs_toolchain.py', 'update', '--force'],
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
  {
    # Update LASTCHANGE.
    'name': 'lastchange',
    'pattern': '.',
    'condition': 'checkout_win and not build_with_chromium',
    'action': ['python3', 'tools/lastchange.py',
               '-o', 'build/util/LASTCHANGE'],
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
