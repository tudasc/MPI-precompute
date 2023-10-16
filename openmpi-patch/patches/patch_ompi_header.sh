#!/bin/bash

# patch the header

start_marker="// START INTERFACE MPIOPT"
end_marker="// END INTERFACE MPIOPT"

input_file=$1
output_file=$2

# Find the line numbers of the start and end markers
start_line=$(grep -n "$start_marker" "$input_file" | cut -d ":" -f 1)
end_line=$(grep -n "$end_marker" "$input_file" | cut -d ":" -f 1)

# Check if the markers are present in the input file
if [ -z "$start_line" ] || [ -z "$end_line" ]; then
    echo "Header Begin/end Markers not found"
    exit 1
fi

# Extract the content between the markers
extracted_content=$(sed -n "${start_line},${end_line}p" "$input_file")

# May need to change with different openmpi version:
# the insertion line is an empty line and will be duplicated (before and after the inserted content)
insertion_line=2858

temp_file=$(mktemp)
head -n $insertion_line "$output_file" > "$temp_file"
echo "$extracted_content" >> "$temp_file"
tail -n +$insertion_line "$output_file" >> "$temp_file"
mv "$temp_file" "$output_file"

# end patch header





