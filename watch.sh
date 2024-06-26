#!/bin/bash

# while inotifywait -e modify,attrib $1; do make ${2:-}; done
fswatch $1 | xargs -I % make ${2:-}
