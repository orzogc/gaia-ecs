#!/bin/bash

PATH_TO_AMALGAMATE_DIR="${@:1}"
PATH_TO_AMALGAMATE=${PATH_TO_AMALGAMATE_DIR}/amalgamate
${PATH_TO_AMALGAMATE} -i ./include ./include/gaia.h ./single_include/gaia.h