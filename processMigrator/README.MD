# Smart Process Migration System

## Overview
This project implements a distributed system that monitors resource usage, detects overloaded nodes, and migrates processes dynamically to balance the load.

## Components
1. **Resource Monitor**: Monitors CPU and memory usage and sends data to the central node.
2. **Migration Manager**: Receives resource data and decides when to trigger migrations.
3. **Process Migrator**: Handles checkpointing and restoring processes using CRIU.

## How to Build
1. Install dependencies:
   ```bash
   sudo apt install gcc criu