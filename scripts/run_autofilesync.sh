#!/bin/bash
# Wrapper script to run autofilesync without snap library interference

# Remove snap paths from LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/usr/local/lib:/usr/lib/x86_64-linux-gnu:/lib/x86_64-linux-gnu

# Execute the program with all arguments
exec "$@"
