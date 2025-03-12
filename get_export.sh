#!/bin/bash

# Get the absolute path of the repository base directory
REPO_BASE="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Create or append to .bashrc
echo "# CS140E Path"
echo "export CS140E_PITCP=\"$REPO_BASE\""