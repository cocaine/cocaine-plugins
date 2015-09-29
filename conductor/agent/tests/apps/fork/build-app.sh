#!/bin/bash

[ -d .env ] || {
  virtualenv .env
}

. .env/bin/activate


pip install --upgrade https://github.com/cocaine/cocaine-framework-python/archive/v0.12.zip  
pip install --upgrade https://github.com/cocaine/cocaine-tools/archive/v0.12.zip  

