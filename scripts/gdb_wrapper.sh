#!/bin/bash
# GDB wrapper to avoid snap library interference

# Clean environment variables
unset LD_PRELOAD
export LD_LIBRARY_PATH=/usr/local/lib:/usr/lib/x86_64-linux-gnu:/lib/x86_64-linux-gnu

# Set environment for the debugged process
cat > /tmp/gdb_init_$$ << 'EOF'
set environment LD_LIBRARY_PATH /usr/local/lib:/usr/lib/x86_64-linux-gnu:/lib/x86_64-linux-gnu
unset environment LD_PRELOAD
EOF

# Run gdb with cleaned environment and initialization commands
exec /usr/bin/gdb -x /tmp/gdb_init_$$ "$@"
