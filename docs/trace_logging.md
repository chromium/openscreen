# Trace Logging Library Usage

The idea behind Trace Logging is that we can choose to trace a function and
then, with minimal additional code, determine how long function execution ran
for, whether the function was successful, and connect all of this information to
any existing informational logs. The below provides information about how this
can be achieved with OSP's TraceLogging Infrastructure.

## Compilation

By default, TraceLogging is enabled as part of the build.
To disable TraceLogging, include flag `--args="enable_trace_logging=false"`
when calling `gn gen` as part of building this library.

## Imports

To use TraceLogging, import the following header file:

```c++
#include "util/trace_logging.h"
```

This file will import all other Trace Logging dependencies.

## Trace IDs

When a Trace Log is created, a new unsigned integer is associated with this
specific trace, which will going forward be referred to as a Trace ID.
Associating a Trace ID has the following benefits:

* The same Trace ID can be associated with all Informational Logs generated
  during this traced method’s execution. This will allow for all informational
  logs to easily be associated with a method execution, so that if an unexpected
  failure occurs or edge case pops up we will have additional information useful
  in debugging this issue and can more easily determine the state of the system
  leading to this issue.

* If another Trace Log is created during the execution of this first traced
  method, we will have a hierarchy of Trace IDs. This will allow us to easily
  create an execution tree (through trivial scripting, preexisting tool, or
  database operations) to get a better view of how execution is proceeding.
  Together, the IDs associated with these calls form a Trace ID Hierarchy. The
  Hierarchy has the following format:
  * *Current Trace ID*: The ID of the function currently being traced.
  * *Parent Trace ID*: The ID of the function which was being traced when this
    Trace was initiated.
  * *Root Trace ID*: The ID of the first traced function under which this the
    current Trace was called.

As an example:

``` c++
    public void DoThings() {
      TRACE_SCOPED(category, "outside");
      {
        TRACE_SCOPED(category, "middle");
        {
          TRACE_SCOPED(category, "inside");
        }
      }
    }
```

This could result in the following Trace ID Information:

| NAME    | ROOT ID  | PARENT ID  | TRACE ID  | RESULT  |
|---------|----------|------------|-----------|---------|
| outside | 100      | 0          | 100       | success |
| middle  | 100      | 100        | 101       | success |
| inside  | 100      | 101        | 102       | success |

Note that, prior to any trace calls, the Trace ID is considered to be 0x0 by
convention.

## Trace Results

The "result" of a trace is meant to indicate whether the traced function
completed successfully or not. For scoped traces, the trace is by default
assumed to be successful. If an error state is desired, this should be set using
`TRACE_SET_RESULT(result)` where `result` is some `Error::Code` enum value.

## Tracing Functions

All of the below functions rely on the Platform Layer's IsTraceLoggingEnabled()
function. When logging is disabled, either for the specific category of trace
logging which the Macro specifies or for TraceCategory::Any in all other cases,
the below functions will be treated as a NoOp.

### Synchronous Tracing

#### Tracing the Current Scope

```c++
TRACE_SCOPED(category, name)
TRACE_SCOPED(category, name, traceId, parentId, rootId)
TRACE_SCOPED(category, name, traceIdHierarchy)
```

If logging is enabled for the provided |category|, trace the current scope. The scope
should encompass the operation described by |name|. The latter two uses of this macro are
for manually providing the trace ID hierarchy; the first auto-generates a new trace ID for
this scope and sets its parent trace ID to that of the encompassing scope (if any).

#### Tracing the Current Scope (with default signatures)

```c++
TRACE_DEFAULT_SCOPED(category)
TRACE_DEFAULT_SCOPED(category, traceId, parentId, rootId)
TRACE_DEFAULT_SCOPED(category, traceIdHierarchy)
```

Same as `TRACE_SCOPED()`, but use the current function/method signature as the operation
name.

#### Tracing with Arguments

Synchronous tracing can also be done with either one or two arguments provided
as strings.

```c++
TRACE_SCOPED1(category, name, argname, argval)
TRACE_SCOPED1(category, name, argname, argval, traceId, parentId, rootId)
TRACE_SCOPED1(category, name, argname, argval, traceIdHierarchy)
TRACE_DEFAULT_SCOPED1(category, argname, argval)
TRACE_DEFAULT_SCOPED1(category, argname, argval, traceId, parentId, rootId)
TRACE_DEFAULT_SCOPED1(category, argname, argval, traceIdHierarchy)

TRACE_SCOPED2(category, name, argname, argval, argname_two, argval_two)
TRACE_SCOPED2(category, name, argname, argval, argname_two, argval_two, traceId, parentId, rootId)
TRACE_SCOPED2(category, name, argname, argval, argname_two, argval_two, traceIdHierarchy)
TRACE_DEFAULT_SCOPED2(category, argname, argval, argname_two, argval_two)
TRACE_DEFAULT_SCOPED2(category, argname, argval, argname_two, argval_two, traceId, parentId, rootId)
TRACE_DEFAULT_SCOPED2(category, argname, argval, argname_two, argval_two, traceIdHierarchy)
```

### Flow Tracing

Flow events are used to connect trace events across different threads, processes,
or even machines. They share a `flow_id` which links them together visually in
trace viewers (like Perfetto).

#### Flow Macro: `TRACE_FLOW_STEP`

```c++
TRACE_FLOW_STEP(category, name, flow_id)
```

* `TRACE_FLOW_STEP`: Marks an intermediate step in the flow.

The `flow_id` can be an integer, a `FrameId`, or any type that is convertible to
`uint64_t` or has a `.value()` method returning a numeric type that is castable
to `uint64_t`.

#### Flow Tracing with Explicit Timestamps

If you need to log a flow event that occurred in the past (e.g., retroactively
tracing a capture event), use the `_WITH_TIME` variants.

```c++
TRACE_FLOW_BEGIN_WITH_TIME(category, name, flow_id, timestamp)
TRACE_FLOW_STEP_WITH_TIME(category, name, flow_id, timestamp)
TRACE_FLOW_END_WITH_TIME(category, name, flow_id, timestamp)
```

* `timestamp`: A `Clock::time_point` representing when the event occurred.

For an example usage of flows for tracking video/audio frame lifecycles, see
[Frame Flow Tracing](../cast/docs/frame_flow_tracing.md).

### Other Tracing Macros

#### `TRACE_CURRENT_ID`

This macro returns the current Trace ID at this point in time.

#### `TRACE_HIERARCHY`

This macro returns an instance of struct Trace ID Hierarchy containing the
current Trace ID Hierarchy. This is intended to be used with
`TRACE_SET_HIERARCHY` (described below) so that Trace ID Hierarchy can be
maintained when crossing thread or machine boundaries.

#### `TRACE_SET_HIERARCHY(ids)`

This macro sets the current Trace Id Hierarchy without initiating a scoped
trace. The set ids will cease to apply when the current scope ends. This is
intended to be used with `TRACE_HIERARCHY` (described above) so that Trace
ID Hierarchy can be maintained when crossing thread or machine boundaries.

#### `TRACE_SET_RESULT(result)`

Sets the current scoped trace's result to be the same as the Error::Code
argument provided.

### Example Code

Synchronous Tracing:

``` c++
    public void DoThings(int important_value) {
      TRACE_SCOPED(category, "DoThings", "important_value", important_value);

      // Do Things.
      // A trace log is generated when the scope containing the above call ends.

      TRACE_SET_RESULT(Error::Code::kNone);
    }
```

## File Division

The code for Trace Logging is divided up as follows:

* *util/trace_logging.h*: the macros Open Screen library code is expected to
      use to log trace events.
* *platform/base/trace_logging_types.h*: the types/enums used in the platform
      API as well as internal library code.
* *util/trace_logging/*: the internal infrastructure backing the macros in
      *trace_logging.h*, and connecting it to the platform API.
* *platform/api/trace_logging_platform.h*: the platform implementation that is
      used as the trace logging destination while tracing is active.
* *platform/impl/text_trace_logging_platform.h*: a basic implementation that
      logs trace events to the console.
* *platform/impl/perfetto_trace_logging_platform.h*: an implementation that
      logs trace events to a Perfetto trace file.

This information is intended to be only explanatory for embedders - only the one
file mentioned above in Imports must be imported.

## Embedder-Specific Tracing Implementations

For an embedder to create a custom TraceLogging implementation:

1. *Create a TraceLoggingPlatform*
  In platform/api/trace_logging_platform.h, the interface TraceLoggingPlatform
  is defined. An embedder must define a class implementing this interface. The
  methods should be as performance-optimal as possible, since they might be
  called frequently (especially `IsLoggingEnabled(TraceCategory)`) and are often
  in the critical execution path of the library's code.

2. *Call `openscreen::StartTracing()` and `StopTracing()`*
  These activate/deactivate tracing by providing the TraceLoggingPlatform
  instance and later clearing references to it.

**Example implementations of this layer can be seen in
platform/impl/text_trace_logging_platform.cc (console logging) and
platform/impl/perfetto_trace_logging_platform.cc (Perfetto tracing).**

## Perfetto Tracing Support

Open Screen supports [Perfetto](https://perfetto.dev/) for trace logging in
standalone builds. This allows for generating trace files that can be viewed in
the Perfetto UI.

### Enabling Perfetto

To enable Perfetto support, ensure the `use_perfetto` GN argument is set to
`true`. This is the default for standalone builds (i.e., when
`build_with_chromium` is `false`).

```bash
gn gen out/Default --args="use_perfetto=true"
```

### Using Perfetto in Standalone Apps

When Perfetto support is compiled in, you can enable it in the standalone sender
and receiver applications using the `--perfetto` or `-P` flag.

```bash
./out/Default/cast_sender -P ...
./out/Default/cast_receiver -P ...
```

When enabled, a trace file (e.g., `openscreen_<pid>.pftrace`) will be generated
upon termination of the application. This file can be opened at
[ui.perfetto.dev](https://ui.perfetto.dev/).
