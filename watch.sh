#!/bin/bash

while inotifywait -e modify,attrib $1; do make; done
