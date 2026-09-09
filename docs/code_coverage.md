# Code Coverage

Code coverage can be checked using Clang's source-based coverage tools. You
must use the GN argument `use_clang_coverage=true`. It is recommended to do
this in a separate output directory since the added instrumentation will affect
performance and generate profile data every time a binary is run. You can read
more about Clang coverage in the
[Clang documentation](https://clang.llvm.org/docs/SourceBasedCodeCoverage.html).

---

## Viewing Coverage Reports Online

### Individual CL Coverage (Trybots)

When you upload a change to Gerrit and run CQ tryjobs:
- The **`linux_x64`** trybot instruments the files modified by your CL and
  runs `openscreen_unittests` and `e2e_tests`.
- In the Gerrit review UI, line coverage changes are highlighted directly in
  the diff viewer.
- On the LUCI build page for `try/linux_x64`, click the **`html report`** link
  (under the `calculate code coverage` step or in the build overview) to open
  an interactive HTML report showing coverage for files touched by the CL.

### Entire Repository Coverage (CI Builder)

When changes land on `main`:
- The **`ci/linux_x64`** postsubmit builder instruments all source files in the
  repository and runs `openscreen_unittests` and `e2e_tests`.
- On the LUCI build page for `ci/linux_x64`, an **`html report`** link is
  provided under the `calculate code coverage` step.
- The full repository coverage HTML report can be viewed directly in your
  browser at:
  ```text
  https://storage.cloud.google.com/code-coverage-data/postsubmit/chromium.googlesource.com/openscreen/<commit_hash>/ci/linux_x64/<build_id>/html_report/index.html
  ```

---

## Analyzing Coverage Locally

### Option 1: Build, Run, and Generate Locally

To measure and inspect full repository coverage entirely on your local
workstation:

1. **Configure and build** with coverage enabled:
   ```bash
   gn gen out/coverage --args="use_clang_coverage=true is_debug=false"
   ninja -C out/coverage openscreen_unittests
   ```

2. **Run tests** with `LLVM_PROFILE_FILE` set to produce raw profile data:
   ```bash
   LLVM_PROFILE_FILE="default.profraw" out/coverage/openscreen_unittests
   ```

3. **Merge the raw profile data** into an indexed `.profdata` file:
   ```bash
   third_party/llvm-build/Release+Asserts/bin/llvm-profdata merge \
       -sparse default.profraw -o coverage.profdata
   ```

4. **Generate the HTML report**:
   ```bash
   third_party/llvm-build/Release+Asserts/bin/llvm-cov show \
       out/coverage/openscreen_unittests \
       -instr-profile=coverage.profdata \
       -format=html \
       -output-dir=out/coverage_html \
       [filter paths]
   ```
   *Note: `[filter paths]` is an optional list of subdirectories or source
   files (e.g., `cast/` or `cast/streaming/`) to restrict the report scope.
   If omitted, all repository sources are included.*

5. **View the report**:
   Open `out/coverage_html/index.html` in your browser.

---

### Option 2: Generate Locally from CI Profile Data

If you want to view or query coverage for a specific landed commit without
running all unit tests locally, you can download the merged `.profdata`
produced by the CI builder:

1. **Build the binary locally** at the target commit:
   ```bash
   gn gen out/coverage --args="use_clang_coverage=true is_debug=false"
   ninja -C out/coverage openscreen_unittests
   ```

2. **Locate the build details** on the [LUCI `ci/linux_x64` console][luci-ci]
   to get the `<commit_hash>` and `<build_id>`.

[luci-ci]: https://ci.chromium.org/p/openscreen/builders/ci/linux_x64

3. **Download the merged profile data**:
   ```bash
   gsutil cp \
       gs://code-coverage-data/postsubmit/chromium.googlesource.com/openscreen/<commit_hash>/ci/linux_x64/<build_id>/merged.profdata \
       .
   ```

4. **Generate the HTML report** using your local binary and the downloaded
   profile data:
   ```bash
   third_party/llvm-build/Release+Asserts/bin/llvm-cov show \
       out/coverage/openscreen_unittests \
       -instr-profile=merged.profdata \
       -format=html \
       -output-dir=out/coverage_html \
       [filter paths]
   ```

5. **View the report**:
   Open `out/coverage_html/index.html` in your browser.

---

## Coverage for Fuzzers

The same process can be used to check the coverage of a fuzzer's corpus. Just
add `-runs=0` to the fuzzer arguments to ensure it only runs the existing corpus
and exits:
```bash
LLVM_PROFILE_FILE="fuzzer.profraw" out/coverage/<fuzzer_name> -runs=0
```
