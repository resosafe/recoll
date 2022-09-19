#!/bin/sh

pythondirs="Python37-32 Python38-32 Python39-32"

fatal()
{
    echo $*
    exit 1
}

test -f ../python/recoll/pyrecoll.cpp || fatal must be run in src/windows

cd ../python/recoll || exit 1

test -f pyrecoll.cpp || fatal Not seeing pyrecoll.cpp

rm -rf dist build || exit 1

# Note that the wheel format is the preferred distribution format (vs
# egg), but it requires installation of the wheel package, which does
# not come with the base Python installation. E.g.:
# /c/Program\ Files\ \(x86\)/Python38-32/python -m pip install wheel
for p in $pythondirs; do
    /c/Program\ Files\ \(x86\)/${p}/python.exe setup-win.py bdist_wheel
done

rm -rf build Recoll.egg-info
