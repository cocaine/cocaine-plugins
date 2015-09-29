#!/bin/bash


. .env/bin/activate

cocaine-tool app start --name echo --profile fork

python appclient.py

