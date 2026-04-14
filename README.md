# Distributed Process Monitor

A C++ distributed system that monitors running processes across multiple remote machines using TCP/IP sockets and a custom command protocol.

## Overview
Two programs work together:
- **`distpsnotify`** — runs locally, launches remote agents via SSH, coordinates data collection
- **`remoteagent`** — runs on remote machines, executes process queries, streams results back over TCP

## Build
```bash
make
```

## Usage
```bash
./distpsnotify -e "ps -eo pid=,ppid=,user=,args=" -a "/path/to/remoteagent" -s "/usr/bin/ssh" -r "129.128.29.32" -r "129.128.29.33" -q "bash" -n 2 -p 8293
```

| Flag | Description |
|------|-------------|
| `-e` | Execution command to run on remote machines |
| `-a` | Path to `remoteagent` binary on remote systems |
| `-s` | Path to SSH binary (requires public-key auth) |
| `-r` | Remote machine IP (repeatable) |
| `-q` | Query string to match against process output |
| `-n` | Number of polling iterations |
| `-p` | Listening port number |

## Sample Output
```
N=1
129.128.29.32::bash(7626)
129.128.29.33::bash(7626)

N=2
129.128.29.32::bash(7626)
129.128.29.33::bash(7626)
```
