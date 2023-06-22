#!/bin/bash

echo "Script name: $0"
echo "Input path: $1"
echo "Output path: $2"

# Usage: ./file.sh [-i --input input file path] [-o --output output file
# path]

# AIM: to iterate through the list of files and print all the text files
# in directory and print besides them the number of characters of the file
# name
# ARRAY=($(ls *))
ARRAY=$1

COUNTER=0 # to keep track of the number
mkdir $(pwd)/apply_patches/

for FILE in "$ARRAY"/* #includes all the files
do
        echo $FILE
        cp "$1/$FILE" "$(pwd)/apply_patches/patch_$COUNTER.eml"
        FILE_NAME="$(pwd)/apply_patches/patch_$COUNTER.eml"
        pushd $2
        cat $FILE_NAME | git am
        if [ $? -eq 0 ]; then
                echo "Applied $FILE_NAME successfully."
        else
                echo "Failed to apply $FILE_NAME."
                break
        fi
        popd

let COUNTER++
done
