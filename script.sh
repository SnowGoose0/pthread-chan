#!/bin/bash

# Create the directory if it doesn't exist
mkdir -p meta

# Change to the meta directory
cd meta || exit

# Array of filenames
files=(
  "meta1.txt"
  "meta2.txt"
  "meta3.txt"
  "meta4.txt"
  "meta5.txt"
  "meta6.txt"
  "meta7.txt"
  "meta8.txt"
  "meta9.txt"
  "meta10.txt"
  "meta11.txt"
  "meta12.txt"
  "meta13.txt"
  "meta14.txt"
  "meta15.txt"
  "meta16.txt"
  "meta17.txt"
  "meta18.txt"
  "meta19.txt"
  "meta20.txt"
)

# Create the files
for file in "${files[@]}"; do
  touch "$file"
done

echo "Files created successfully in the 'meta' directory."
