#!/bin/bash

C_PROG="./pmms"
LOG_FILE="pmms.log"

# Compile if needed
if [ ! -f "$C_PROG" ]; then
    echo "Compiling C program..."
    gcc -o pmms pmms.c
fi

# Start C program in background
$C_PROG &
PARENT_PID=$!
echo "started pmms with PID $PARENT_PID"

# Menu loop
while kill -0 $PARENT_PID 2>/dev/null; do
    echo ""
    echo "[1] View Process Status"
    echo "[2] Pause/Resume a Process"
    echo "[3] Kill a Process"
    echo "[4] Kill All Processes"
    echo "[5] View Log File"
    echo "[6] Exit"
    echo -n ">> "
    read choice

    case $choice in
        1)
            echo "Child Processes:"
            pgrep -P $PARENT_PID -l
            ;;
        2)
            echo -n "Enter PID to toggle pause/resume: "
            read pid
            kill -SIGUSR1 $pid
            ;;
        3)
            echo -n "Enter PID to kill: "
            read pid
            kill -SIGTERM $pid
            ;;
        4)
            echo "Killing all children..."
            pkill -P $PARENT_PID
            ;;
        5)
            echo "--- Log File ---"
            tail -n 20 $LOG_FILE
            echo "----------------"
            ;;
        6)
            echo "Exiting monitor."
            kill -INT $PARENT_PID
            break
            ;;
        *)
            echo "Invalid option."
            ;;
    esac

done

echo "Parent process exited. Monitor shutting down."
exit 0

