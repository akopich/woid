import os
import subprocess
import shutil
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed
import itertools # Added for Cartesian product

# --- Configuration ---
SOURCE_DIR = "." # The script is assumed to be run from the repository root directory
BUILD_ROOT_DIR = "builds" # Directory to hold all temporary build folders
NUM_THREADS = os.cpu_count() or 4 # Use all available CPU cores, or default to 4

# Define the components of the build matrix
COMPILERS = ["g++", "clang++"]
BUILD_MODES = [
    "Release",
    "AUBSan",
    "AUBSanWithExceptions",
    "ReleaseWithExceptions",
]

# Generate the build matrix using the Cartesian product (compilers x build modes)
BUILD_MATRIX = list(itertools.product(COMPILERS, BUILD_MODES))
# ---------------------

def run_command(command, cwd=None):
    """
    Executes a shell command and raises an exception on failure.
    Output is streamed directly to the console to preserve color and formatting.
    """
    command_str = " ".join(command)
    # Print the command header and flush to ensure it appears before streamed output
    print(f"\n[CMD] Running: {command_str} in {cwd if cwd else os.getcwd()}")
    sys.stdout.flush()
    try:
        # By omitting capture_output, stdout and stderr stream directly to the terminal,
        # which allows compilers to correctly output color codes.
        subprocess.run(
            command,
            cwd=cwd,
            check=True, # Ensure errors raise CalledProcessError
        )
        print(f"\n[SUCCESS] Command completed: {command_str}")
    except subprocess.CalledProcessError as e:
        # Since output is streamed, the full error is already visible.
        print(f"\n[ERROR] Command failed with return code {e.returncode}")
        # Re-raise the exception to be caught by build_target
        raise e
    except FileNotFoundError:
        print(f"\n[FATAL] Command not found. Is {command[0]} in your PATH?")
        sys.exit(1)


def build_target(compiler, mode):
    compiler_map = {
        "g++": "gcc",
        "clang++": "clang",
    }
    comp_name = compiler_map.get(compiler, compiler) # Map g++/clang++ to gcc/clang for folder names, falling back to original if unknown.
    build_name = f"{comp_name}_{mode}"
    build_dir = os.path.join(BUILD_ROOT_DIR, build_name)
    success = False

    try:
        print(f"--- Starting build and test for: {build_name} ---")

        # 1. Clean (Ensure a fresh start for this target)
        if os.path.exists(build_dir):
            shutil.rmtree(build_dir)
            print(f"Removed previous directory: {build_dir}")

        os.makedirs(build_dir, exist_ok=True)

        # 2. Configure with CMake
        config_cmd = [
            "cmake",
            f"-DCMAKE_BUILD_TYPE={mode}",
            f"-DCC_SELECTION={comp_name}",
            f"-S", SOURCE_DIR, # Source directory ('.')
            f"-B", build_dir   # Binary directory (where build files go)
        ]
        run_command(config_cmd)

        # 3. Build the target
        # Using -j{NUM_THREADS} to speed up the compilation phase
        build_cmd = ["cmake", "--build", build_dir, "--", f"-j{NUM_THREADS}"]
        run_command(build_cmd)

        if not os.path.exists(os.path.join(build_dir, "MoveOnlyBench")):
             raise Exception(f"MoveOnlyBench was not found in '{build_dir}'")
        else:
             print("MoveOnlyBench is built successfully.")

        expected_binary = os.path.join(build_dir, "WoidTest")

        if os.path.exists(expected_binary):
            print(f"Successfully built executable: {expected_binary}. Now running tests...")

            # Execute the built test program
            run_command([expected_binary])

            success = True
            print(f"Test run completed for: {build_name}")
        else:
            raise Exception(f"Build succeeded, but expected executable '{os.path.basename(expected_binary)}' was not found in '{build_dir}'")

    except Exception as e:
        print(f"*** Build or Test FAILED for {build_name}: {e} ***")
    finally:
        print(f"--- Finished build and test for: {build_name} ({'SUCCESS' if success else 'FAILURE'}) ---\n")
        return build_name, success


def main():
    """Main function to setup and run parallel builds."""
    print(f"Parallel Build Orchestrator, running from current repository root ('.')")
    print("-" * 50)

    # 1. Setup the root build directory
    os.makedirs(BUILD_ROOT_DIR, exist_ok=True)

    # 2. Run parallel builds
    results = {}
    print(f"Starting {len(BUILD_MATRIX)} builds in parallel using up to {NUM_THREADS} build threads...")

    # Using ThreadPoolExecutor to run independent builds concurrently
    with ThreadPoolExecutor(max_workers=len(BUILD_MATRIX)) as executor:
        futures = {executor.submit(build_target, compiler, mode): (compiler, mode)
                   for compiler, mode in BUILD_MATRIX}

        for future in as_completed(futures):
            target_name, status = future.result()
            results[target_name] = "SUCCESS" if status else "FAILURE"

    print("-" * 50)
    print("--- Summary of Parallel Builds and Tests ---")

    # 3. Print summary
    successful_builds = [k for k, v in results.items() if v == "SUCCESS"]
    failed_builds = [k for k, v in results.items() if v == "FAILURE"]

    for name, status in results.items():
        print(f"{name:<30}: {status}")

    print("-" * 50)
    print(f"Total Builds: {len(BUILD_MATRIX)}")
    print(f"Successful: {len(successful_builds)}")
    print(f"Failed: {len(failed_builds)}")

    if failed_builds:
        print("\nNOTE: One or more builds/tests failed. Check the logs above for detailed errors.")
        sys.exit(1)
    else:
        print("All builds and tests completed successfully!")


if __name__ == "__main__":
    main()
