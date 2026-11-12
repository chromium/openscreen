# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Package declaration for the Open Screen project."""

pkg.declare(
    name = "@openscreen-project",
    lucicfg = "1.46.1",
)

pkg.options.lint_checks([
    "default",
    "-confusing-name",
    "-function-docstring",
    "-function-docstring-args",
    "-function-docstring-return",
    "-function-docstring-header",
    "-module-docstring",
])

pkg.entrypoint("main.star")

pkg.depend(
    name = "@chromium-luci",
    source = pkg.source.googlesource(
        host = "chromium",
        repo = "infra/chromium",
        ref = "refs/heads/main",
        path = "starlark-libs/chromium-luci",
        revision = "1ac486c7d3604e72ef386ec198865877529c342b",
    ),
)

pkg.depend(
    name = "@chromium-targets",
    source = pkg.source.googlesource(
        host = "chromium",
        repo = "chromium/src",
        ref = "refs/heads/main",
        path = "infra/config/targets",
        revision = "4d43e1408e90a96b7f351aea0120928d395dc36e",
    ),
)
