#!/bin/bash

while inotifywait -e modify,attrib $1; do make ${2:-}; done
