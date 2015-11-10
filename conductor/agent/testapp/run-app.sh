#!/bin/bash


cd "$( dirname "${BASH_SOURCE[0]}" )"

. .env/bin/activate

python app.py $@

