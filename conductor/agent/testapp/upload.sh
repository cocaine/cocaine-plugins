#!/bin/bash


. .env/bin/activate


cocaine-tool app upload --name echo2 --manifest manifest.json --package app.tgz
cocaine-tool profile upload --name fork --profile fork.profile.json
#cocaine-tool profile upload --name rpc-fork --profile rpc-fork.profile.json
