import numpy as np
from collections import defaultdict
import argparse
import subprocess
import re
import matplotlib.pyplot as plt
import sys
from typing import List, Dict, Any, Tuple, Optional
import os
from dataclasses import dataclass


@dataclass
class BenchmarkResult:
    benchmark_name: str
    container_name: str
    problem_size: int
    time_ns: int

def parse_benchmark_line(line: str) -> Optional[BenchmarkResult]:
    pattern = re.compile(
        r"^\s*(?P<bench_name>[^<]+?)"           # 1. Benchmark Name (non-greedy, stops at <)
        r"(?:<(?P<container>.+?)>)?\/"          # 2. Optional Container Name (non-greedy), followed by /
        r"(?P<size>\d+)"                        # 3. Problem Size (integer)
        r"\s+(?P<time>[\d.]+)\s+ns.*$"          # 4. Primary Time (allow digits and decimal point) and rest of line
    )
    match = pattern.match(line.strip())

    if match:
        return BenchmarkResult(
            benchmark_name=match.group("bench_name"),
            container_name=match.group("container"),
            problem_size=int(match.group("size")),
            time_ns=float(match.group("time")),
        )
    else:
        return None

show = True

def execute_binary():
    global show
    """
    Parses arguments for the script and the external binary, 
    and executes the binary using subprocess based on the chosen mode.
    """
    # 1. Setup Argument Parser
    # The parser uses a custom epilog to clearly explain how to separate 
    # arguments intended for the script vs. arguments for the binary.
    parser = argparse.ArgumentParser(
        description="A Python script to execute an external binary with custom arguments.",
        epilog="""
        --- USAGE EXAMPLE ---
        python binary_executor.py --binary /bin/ls --mode save -- -l -a /home/user
        
        Note the double-dash (--) separator. Arguments before it are for 
        this script; arguments after it are for the external binary.
        """
    )

    # 2. Arguments for the Script
    parser.add_argument(
        '--binary',
        type=str,
        required=True,
        help='The required path to the external binary (e.g., /usr/bin/python or /bin/bash).'
    )
    
    # Updated: Replaced --script-flag with --mode, restricting choices
    parser.add_argument(
        '--mode',
        type=str,
        default='show',
        choices=['show', 'save'],
        help='The execution mode for the script. "show" prints output directly; "save" captures and saves the output (default: show).'
    )

    # 3. Arguments for the External Binary
    # argparse.REMAINDER collects all remaining arguments following the special '--' separator.
    parser.add_argument(
        'binary_args',
        nargs=argparse.REMAINDER,
        default=[],
        help='Arguments to be passed to the external binary. Must be preceded by a double-dash (--) separator.'
    )

    args = parser.parse_args()

    # The first element of REMAINDER list is often the separator itself ('--'), 
    # so we strip it out if present.
    if args.binary_args and args.binary_args[0] == '--':
        binary_args_list = args.binary_args[1:]
    else:
        # If the user didn't use '--', all unparsed arguments are here.
        binary_args_list = args.binary_args

    # Check if the binary file exists
    if not os.path.isfile(args.binary):
        print(f"Error: Binary not found at path: {args.binary}", file=sys.stderr)
        sys.exit(1)

    # 4. Construct the Command and Execute
    
    # The full command starts with the binary path, followed by its arguments.
    full_command = [args.binary] + binary_args_list
    
    # Inform the user about the execution details
    print(f"--- Script Settings ---")
    print(f"Binary Path: {args.binary}")
    print(f"Execution Mode: {args.mode}") # Updated printout
    print(f"Command to execute: {' '.join(full_command)}\n")

    show = args.mode == 'show'

    try:
        print(f"--- Running Binary and Capturing Output ---")
        
        # Execute command, ALWAYS capturing output and standard error
        result = subprocess.run(
            full_command, 
            check=True, 
            capture_output=True, # ALWAYS capture output
            text=True
        )

        # 5. Process Output (Always print and store)
        
        # Always print the captured STDOUT to the console
        print(f"\n--- Captured STDOUT Output (Printed) ---")
        sys.stdout.write(result.stdout)
        
        # Always print STDERR if present
        if result.stderr:
            print(f"\n--- Captured STDERR Output (Printed) ---", file=sys.stderr)
            sys.stderr.write(result.stderr)
            
        # Store the STDOUT lines in a variable (array of strings)
        output_lines = result.stdout.splitlines()
        
        # Example of using the stored variable:
        print(f"\n--- Execution Summary ---")
        print(f"Total lines captured in 'output_lines' variable: {len(output_lines)}")
        print(f"First line captured: {'(None)' if not output_lines else output_lines[0]}")
        
        print(f"Binary finished successfully with return code {result.returncode}")
        
        return output_lines
    except FileNotFoundError:
        # This should ideally be caught by the os.path.isfile check, but good practice to keep.
        print(f"Error: The binary '{args.binary}' was not found.", file=sys.stderr)
        sys.exit(1)
    except subprocess.CalledProcessError as e:
        print(f"\n--- Execution Failed (Return Code: {e.returncode}) ---", file=sys.stderr)
        print(f"Command: {e.cmd}", file=sys.stderr)
        # If in 'save' mode, we have captured output available for debugging
        if args.mode == 'save':
            print(f"\n--- Captured STDOUT on Error ---\n{e.stdout}", file=sys.stderr)
            print(f"\n--- Captured STDERR on Error ---\n{e.stderr}", file=sys.stderr)
        sys.exit(e.returncode)
    except Exception as e:
        print(f"An unexpected error occurred during execution: {e}", file=sys.stderr)
        sys.exit(1)

@dataclass
class Results:
    sizes: list[int]
    times: list[int]

def group_benchmark_results(results: list[BenchmarkResult]) -> dict[str, dict[str, Results]]:
    grouped_data = defaultdict(lambda: defaultdict(lambda: Results(sizes=[], times=[])))

    for result in results:
        result_lists = grouped_data[result.benchmark_name][result.container_name]

        result_lists.sizes.append(result.problem_size)
        result_lists.times.append(result.time_ns)
        
    final_dict = {k: dict(v) for k, v in grouped_data.items()}
    return final_dict

def plot_benchmark_comparison(benchmark_name: str, container_data: dict[str, Results]):
    BASE_CONTAINER = "std::any"
    
    if BASE_CONTAINER not in container_data:
        print(f"Error: Cannot calculate speedup for '{benchmark_name}' as the baseline container <{BASE_CONTAINER}> is missing.")
        return

    base_results = container_data[BASE_CONTAINER]
    base_times = np.array(base_results.times)

    # Creates a figure and a set of subplots.
    fig, ax = plt.subplots(figsize=(10, 6))

    for container_name, results_obj in container_data.items():
        if container_name == BASE_CONTAINER:
            continue
        if len(results_obj.times) != len(base_times):
             print(f"Warning: Data point count mismatch ({len(results_obj.times)} vs {len(base_times)}) for {container_name} in {benchmark_name}. Skipping speedup plot.")
             print(results_obj.times)
             print(results_obj.sizes)
             continue

        # Calculate Speedup: Ratio = Base Time / Current Time
        speedups = base_times / results_obj.times 

        # Plot sizes vs. speedup
        ax.plot(results_obj.sizes, speedups, marker='o', linestyle='-', label=container_name)
    
    # Set plot labels and title
    ax.set_xlabel("Vector size")
    ax.set_ylabel(f"Speedup vs. <{BASE_CONTAINER}>, times")
    ax.set_title(f"Speedup Comparison for: {benchmark_name}")
    
    # Add a grid and legend for readability
    ax.grid(True, linestyle='--', alpha=0.6)
    ax.axhline(1.0, color='r', linestyle='--', linewidth=0.8, label=f"Baseline ({BASE_CONTAINER})")
    ax.legend(title="Container", loc='best')
    ax.set_xscale('log', base=2)

    return fig

def read_from_file(path):
    lines = []
    with open(path) as file:
        while line := file.readline():
            lines += [line.rstrip()]
    return lines;

if __name__ == "__main__":
    lines = execute_binary()
    results = list(filter(None,[parse_benchmark_line(line) for line in lines]))
    grouped = group_benchmark_results(results)
    for name,data in grouped.items():
        fig = plot_benchmark_comparison(name, data)
        if (not show and fig):
            # Create a clean filename
            clean_bench_name = name.replace('<', '_').replace('>', '_')
            output_filename = f'{clean_bench_name}_speedup.png'
            
            # 2. Save the current figure to a PNG file
            fig.savefig(output_filename, dpi=300) # Use dpi=300 for high resolution
            # Close the figure to free memory
            plt.close(fig)

            print(f"    -> Plot saved successfully to '{output_filename}' (PNG format).")
    if (show):
        plt.show()

