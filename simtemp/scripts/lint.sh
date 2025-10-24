#!/bin/bash

# --- VARIABLES ---
KERNEL_HEADERS_PATH="/usr/src/linux-headers-$(uname -r)"
CHECKPATCH_PATH="$KERNEL_HEADERS_PATH/scripts/checkpatch.pl"
KERNEL_FILE="../kernel/nxp_simtemp.c"
PYTHON_FILE="../user/cli/main.py"

# Error Handling
fail() {
    echo "--- ERROR: $1 ---" >&2
    exit 1
}

echo "--- 1. Running Code Quality Check (LINT) ---"

# --- 0. PRE-FORMATTING: DOS2UNIX FIX ---
if command -v dos2unix &> /dev/null; then
    echo "Converting file to Unix line endings..."
    dos2unix "$KERNEL_FILE"
else
    # Esto no es un error fatal, pero se recomienda instalarlo.
    echo "WARNING: dos2unix not installed. May encounter DOS line ending errors."
fi

#echo "--- 1. Running Code Quality Check (LINT) ---"

# --- 1. CHECKPATCH (C CODE) ---
if [ -f "$CHECKPATCH_PATH" ]; then
    echo "Checking Kernel Code Style (checkpatch)..."
    # Ejecuta el checkpatch en tu archivo C. El flag -f reporta un resumen.
    perl "$CHECKPATCH_PATH" -f --no-tree "$KERNEL_FILE" || fail "Checkpatch found style violations in C code."
else
    echo "WARNING: checkpatch.pl not found. Skipping Kernel style check."
fi

# --- 2. CLANG-FORMAT (C and Python) ---
if command -v clang-format &> /dev/null; then
    echo "Formatting Python/C files with clang-format..."
    # Usa -i para aplicar el formato directamente al archivo
    clang-format -i "$PYTHON_FILE"
    clang-format -i "$KERNEL_FILE"
    echo "Formatting complete."
else
    fail "clang-format not installed. Please install it."
fi

echo "--- SUCCESS: Linting completed. ---"