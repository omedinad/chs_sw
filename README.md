# CHS — CSV heart-rate statistics

CHS is a small C command-line program for analyzing running workouts exported as
CSV files. It helps answer two questions:

- What is my average pace near a chosen heart rate?
- What is my average heart rate near a chosen pace?

These measures can help compare workouts at a similar effort or speed. Results
depend on the input data and sampling intervals; CHS does not account for terrain,
weather, or other differences between workouts.

## Requirements

- GCC or a compatible C compiler with C99 and POSIX support.
- GNU Make and the standard C and math libraries (`libm`).
- Python 3, for the functional regression tests only.

The project is developed and tested on Linux. It has no external package
libraries, running services, API keys, or configuration files. Native Windows
builds are not supported by the current POSIX-based implementation; use a Linux
environment such as WSL.

## Quick start

From the repository root:

```sh
make
./chs -h 145 -p 5:30.0 test.csv
```

This builds `./chs` and analyzes the included workout, targeting 145 beats per
minute and 5 minutes 30 seconds per kilometer.

Example output for the bundled sample:

```text
HR at           Pace at         File
5:30.00         145             Name
----------------------------------------------
143.8(7)        7:29.38(113)     test.csv(65K, 658 Records)
----------------------------------------------
143.8           7:29.38          1 files
```

Spacing uses tabs and may vary by terminal. The result means:

- Seven samples matched the target pace; their average heart rate was 143.8 bpm.
- 113 samples matched the target heart rate; their average pace was 7:29.38/km.
- The file supplied 658 usable intervals. `65K` is its file size in whole KiB.
- The final line summarizes the available per-file statistics.

## Command-line usage

```text
chs -h HR -p mm:ss.dd [-o output] [--verbose] file.csv ...
```

Both targets and at least one input file are required, except when asking for
help. Options may be supplied in either order.

| Option | Meaning |
| --- | --- |
| `-h HR`, `--heartrate HR` | Target heart rate in bpm: an integer from 46 through 189. |
| `-p PACE`, `--pace PACE` | Target pace in minutes per kilometer, with one or two fractional-second digits. |
| `-o FILE`, `--out FILE` | Write the report to a file, replacing its contents. Default: standard output. |
| `--verbose` | Write processing diagnostics to standard error. |
| `--brief` | Disable verbose diagnostics; this is the default. |
| `--help` | Show usage and exit successfully. |

Pace must be positive, with minutes from 0 through 59 and seconds from 0 through
59. A decimal point and one or two following digits are required: `5:30.5` means
5 minutes and 30.5 seconds; `5:00.0` is valid, while `5:00` is not. Reports always
show hundredths of a second.

The historical range and test options had no implementation. They are no longer
advertised or accepted; unknown options return an error. `-h` selects heart rate,
so use `--help` for usage information.

### Examples

Analyze several workouts in one report:

```sh
./chs -h 145 -p 5:30.0 monday.csv friday.csv
```

Save a report and enable diagnostics:

```sh
./chs --heartrate 145 --pace 5:30.0 --out report.txt --verbose test.csv
```

Redirect the report and errors separately:

```sh
./chs -h 145 -p 5:30.0 test.csv > report.txt 2> errors.txt
```

Use `--` before a filename beginning with a dash:

```sh
./chs -h 145 -p 5:30.0 -- -workout.csv
```

## CSV input format

CHS reads simple, unquoted CSV files with a header row. These exact,
case-sensitive column names are required, in any order:

| Column | Unit and meaning |
| --- | --- |
| `dist` | Cumulative distance in meters, nonnegative and nondecreasing. |
| `time` | Duration of this interval in seconds, including fractional seconds. |
| `heartrate` | Heart-rate reading in beats per minute. |

Other columns are ignored and may be empty. LF and CRLF line endings and a final
line without a newline are supported. Blank lines are ignored. Required numeric
fields must be present, finite, and valid numbers.

**`time` must contain interval durations, not elapsed time since the workout
started.** Convert cumulative timestamps into interval durations before using
CHS. Export distance in meters, rather than kilometers or miles.

A minimal example:

```csv
dist,time,heartrate
0,0,145
100,30,145
200,30,145
```

Each 100-meter interval takes 30 seconds, giving a pace of 5:00/km. Save this as
`example.csv` and run:

```sh
./chs -h 145 -p 5:00.0 example.csv
```

Both usable intervals match both targets, so the report contains
`145.0(2)` and `5:00.00(2)`.

The initial previous distance is zero. A first row at distance zero establishes
that starting point and contributes no running interval. If the first row has a
positive distance and duration, CHS treats it as an interval starting at zero.

### Excluded intervals and rejected files

Intervals with no distance increase, zero duration, or heart rate outside the
open interval `(45, 190)` are excluded. The previous distance is still updated,
so the next interval does not absorb distance from an excluded interval.

Negative durations, decreasing or negative distances, malformed required fields,
and nonfinite values such as `NaN` or `inf` reject the file. Empty files, missing
required columns, and files with no usable intervals are also rejected. Reported
record counts refer to usable intervals, excluding the header and skipped rows.

Quoted fields, embedded commas or newlines, UTF-8 byte-order marks, alternative
column names, and locale-specific decimal separators are not supported. Convert
such exports to the format above first. Input files are read into memory.

## How statistics are calculated

For each usable interval:

```text
interval distance = current cumulative distance − previous cumulative distance
speed (km/h)      = interval distance / interval seconds × 3.6
pace (min/km)     = interval seconds / interval distance × 1000 / 60
```

CHS then smooths heart rate over the usable samples using a normalized,
five-sample Gaussian window with approximate weights:

```text
0.010334, 0.207561, 0.564210, 0.207561, 0.010334
```

Each smoothed value uses the original readings, rather than previously smoothed
values. The first and last two usable samples stay unchanged. Workouts with fewer
than five usable samples are left unchanged. Filtering operates over sample
positions, not a fixed number of seconds; excluded intervals do not participate.

The report computes two independent statistics:

| Statistic | Matching rule | Value averaged |
| --- | --- | --- |
| HR at target pace | Pace within 1% of the target, including boundaries. | Smoothed heart rate. |
| Pace at target HR | Smoothed heart rate within 1 bpm of the target, including boundaries. | Pace in minutes per kilometer. |

These are arithmetic sample averages, not duration- or distance-weighted
averages. Irregular recording intervals can therefore affect comparisons.
Matching counts appear in parentheses. A statistic with no matching samples is
shown as `N/A(0)`; this alone is not a processing error.

The final summary averages each available **per-file statistic**, giving each
contributing file equal weight. Heart rate and pace use separate denominators:
a file with no matching heart-rate samples can still contribute a pace statistic,
and vice versa. The summary's file count includes all successfully processed
files, even if they have no matches for either target.

## Errors and exit status

- **0:** All input files were processed successfully, or help was requested.
- **1:** Invalid arguments, an input-file failure, or a report-output failure.

Errors and verbose diagnostics go to standard error; the report goes to standard
output or the file selected with `-o`. A failed input file is reported and skipped,
and subsequent files are still processed. The command returns failure even if
some other files succeed. If no files succeed, summary statistics are `N/A`.

Reports are buffered in a temporary file before being copied to the destination.
The environment therefore needs a writable temporary directory. Choose an output
path distinct from your input files to avoid replacing workout data.

## Build options and tests

```sh
make             # Build the CLI
make test        # Run the bundled workout as a smoke test
make check       # Run C calculation assertions and Python functional tests
make clean       # Remove normal build outputs
```

`make check` covers known-value statistics, fractional timing, smoothing,
argument validation, multiple files, missing and malformed inputs, empty fields,
CRLF, sensor dropouts, unavailable statistics, report output, replacing an input
with its report after analysis, and output-write failures (on systems with `/dev/full`).

Override the compiler or compiler flags as needed. Force recompilation after
changing flags because Make does not track flag changes:

```sh
make -B CC=gcc CFLAGS='-O0 -g -Wall -Wextra -Werror'
```

To keep generated files outside the checkout, supply output directories with
trailing slashes:

```sh
make -j2 check test ODIR=/tmp/chs-build/obj/ BIN_DIR=/tmp/chs-build/
/tmp/chs-build/chs -h 145 -p 5:30.0 test.csv
```

Run these commands from the repository root so header and fixture paths resolve.
Use the same directory overrides with `make clean` to clean that build.

### Memory and undefined-behavior checks

Use a separate build directory for sanitizer instrumentation:

```sh
make -B check test ODIR=/tmp/chs-sanitized/ BIN_DIR=/tmp/chs-sanitized/ \
  CFLAGS='-O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer' \
  LDFLAGS='-fsanitize=address,undefined'
```

This runs the same calculation tests, functional tests, and bundled sample with
AddressSanitizer and UndefinedBehaviorSanitizer enabled.

## Troubleshooting

| Symptom | What to check |
| --- | --- |
| `gcc` or `make` not found | Install a C build toolchain through your operating system's package manager. |
| Python tests cannot start | Install Python 3 and ensure `python3` is on `PATH`. |
| `Invalid pace` | Include fractional seconds, for example `5:30.0`; seconds must be below 60. |
| `Cannot process ...: No such file or directory` | Check the path and current working directory. |
| `Cannot process ...: Invalid argument` | Check required headers, numeric fields, distance progression, and whether any usable intervals remain. |
| `N/A(0)` | No usable samples match that target; inspect the workout or choose another target. |
| Unexpected pace values | Confirm distance is in meters and time is an interval duration in seconds. |
| Output file cannot be opened | Check that its parent directory exists and is writable. |

## Source layout

| Path | Purpose |
| --- | --- |
| `src/chs.c` | Coordinates option parsing, file analysis, and report delivery. |
| `src/cli.c` | Parses and validates CLI options, including help handling. |
| `src/analysis.c` | Reads each file, runs the analysis, and releases its temporary resources. |
| `src/report.c` | Formats workout rows, accumulates summaries, and publishes buffered output. |
| `src/csv_reader.c` | CSV column lookup, numeric parsing, and workout loading. |
| `src/heart_rate.c` | Heart-rate argument parsing and range validation. |
| `src/pace.c` | Pace parsing, formatting, and conversion to/from decimal minutes. |
| `src/samples.c` | Sample-list memory management. |
| `src/statistics.c` | Heart-rate smoothing and sample averages. |
| `src/chs_debug.c` | Optional diagnostic logging. |
| `include/` | Shared data types and function declarations. |
| `tests/test_calculations.c` | Assertions for conversions, smoothing, and heart-rate matching. |
| `tests/test_chs.py` | CLI regression tests using temporary fixtures. |
| `test.csv` | Bundled workout for smoke testing. |
| `Makefile` | Build, validation, and cleanup targets. |

Each module exposes its public interface through a matching header in `include/`.
Function names use `snake_case`, with module prefixes such as `pace_parse`,
`csv_load_samples`, `samples_free`, and `statistics_smooth_heart_rate`. Parsing
helpers remain private to their source file. Historical parser implementations
are preserved in Git history rather than the source tree.

## License

CHS is released under the [Unlicense](LICENSE), a public-domain dedication with
no warranty. See the license file for the full terms.
