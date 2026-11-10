# Runtime options

Runtime options let an embedder change Open Screen's behavior while the program
runs, without a rebuild. For example, an embedder can use them to:

* Roll out a new implementation behind a feature flag, and turn it off if it
  causes problems in the field.
* Pass the settings of a Chromium field trial or other experiment into Open
  Screen.

If the defaults work for you, you don't need to do anything.

## The options

The options are the fields of `openscreen::RuntimeOptions` in
[platform/api/runtime_options.h](../platform/api/runtime_options.h). The default
value of each field is the library's recommended setting.

The comment on each field is the reference for that option. It says what the
option controls, when Open Screen reads it, and which build arguments it
depends on, if any.

## Override the defaults

Start from a default-constructed `RuntimeOptions`, change only the fields that
you want to control, and pass it to `SetRuntimeOptions()`:

```c++
#include "platform/api/runtime_options.h"

void ConfigureOpenScreen(bool use_rust_json) {
  openscreen::RuntimeOptions options;  // Starts with the library defaults.
  options.use_rust_json = use_rust_json;
  openscreen::SetRuntimeOptions(options);
}
```

Because you start from the defaults, any option that is added later keeps its
library default until you choose to set it.

## When to set the options

Open Screen reads some options every time it uses them, and others only once,
when it creates the object that uses them. That object keeps the value for its
whole life, so a later change does not affect it.

To make sure that every option applies everywhere, set the options at startup,
before you create any Open Screen objects. You can call `SetRuntimeOptions()`
again later. Each call replaces all of the options, and the new values apply to
every read after the call. Objects that already read an option keep the value
that they read.

The options are stored per process, so if your program runs Open Screen in more
than one process, set them in each of those processes.

`GetRuntimeOptions()` and `SetRuntimeOptions()` can be called on any thread.

## Example: Chromium

Chromium maps each option to a `base::Feature` in
[//components/openscreen_platform](https://source.chromium.org/chromium/chromium/src/+/main:components/openscreen_platform/).
It sets every option from its feature, so the feature state, not the library
default, decides the value in Chromium. This lets Chromium roll out a new
implementation, or turn it off, with a field trial. Each feature's default
matches the library default.

Chromium's `TaskRunner` implementation copies the features into
`RuntimeOptions` each time it is created. Every Open Screen sender and receiver
in Chromium needs a `TaskRunner`, so each process applies its features before
Open Screen reads them. The browser passes feature state to its child
processes, so all of them get the same values.

## Change options in tests

Use `ScopedRuntimeOptions` from
[platform/test/scoped_runtime_options.h](../platform/test/scoped_runtime_options.h).
It sets the options for its lifetime and then restores the previous options:

```c++
#include "platform/test/scoped_runtime_options.h"

TEST(MyTest, WorksWithOptionDisabled) {
  openscreen::RuntimeOptions options;
  options.use_rust_json = false;
  openscreen::ScopedRuntimeOptions scoped_options(options);

  // Code in this scope sees use_rust_json == false.
}
```

If an option is read only when an object is created, create the object inside
the scope.

## Add an option

These steps are for Open Screen developers:

1. Add a field to `RuntimeOptions` with the recommended default.
2. In the field's comment, say what it controls, when Open Screen reads it,
   and which build arguments it depends on, if any. Also say whether it is
   temporary, such as a rollout or an experiment, or permanent. For a
   temporary option, link the bug that tracks its removal.
3. Read the field with `GetRuntimeOptions()`. If an object must keep the same
   behavior for its whole life, read the field once, when the object is
   created.
4. Test the behavior with the option both on and off. For example, use a
   parameterized test with `ScopedRuntimeOptions`.

`RuntimeOptions` must stay small enough for `std::atomic` to be lock-free. A
`static_assert` in `platform/api/runtime_options.cc` fails if a new field
breaks this.

When an option is no longer needed, delete its field. Embedders that set the
field must stop setting it, so tell them before you remove it.
