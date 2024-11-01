#!/bin/bash

HEADER_DIR="../.."
OUTPUT_FILE="all_headers.h"

echo "#ifndef ALL_HEADERS_H" > $OUTPUT_FILE
echo "#define ALL_HEADERS_H" >> $OUTPUT_FILE
echo "" >> $OUTPUT_FILE

# Find all .h files recursively and include them
find $HEADER_DIR -type f -name '*.h' | while read header; do
    relative_path=$(realpath --relative-to="$(dirname $OUTPUT_FILE)" "$header")
    echo "#include \"$relative_path\"" >> $OUTPUT_FILE
done

echo "" >> $OUTPUT_FILE
echo "#endif // ALL_HEADERS_H" >> $OUTPUT_FILE
