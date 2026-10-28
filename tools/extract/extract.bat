@echo off
rem This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
rem distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
rem
rem Sims3DS game-data extractor for Windows: double-click this file, or drag your game folder onto it.
rem It turns YOUR OWN copy of The Sims Complete Collection (your installed game folder, e.g.
rem C:\Program Files (x86)\Maxis\The Sims, or images of your own discs) into a ready 3ds\sims\ folder.
setlocal
cd /d "%~dp0"
echo == Sims3DS extractor ==

set "PY="
py -3 -c "import sys; sys.exit(0 if sys.version_info >= (3, 8) else 1)" >nul 2>&1 && set "PY=py -3"
if not defined PY (
    python -c "import sys; sys.exit(0 if sys.version_info >= (3, 8) else 1)" >nul 2>&1 && set "PY=python"
)
if not defined PY (
    echo Python 3.8 or newer is needed but was not found.
    echo Install it from https://www.python.org/downloads/ ^(tick "Add python.exe to PATH"^), then run this again.
    goto end
)

rem Python helpers for disc images (not needed for an installed game folder)
%PY% -c "import py7zr, pycdlib" >nul 2>&1
if errorlevel 1 (
    echo Installing the Python helpers ^(py7zr, pycdlib^) - one time only ...
    %PY% -m pip install --user --quiet py7zr pycdlib
    if errorlevel 1 echo WARNING: could not install py7zr/pycdlib - fine for an installed game folder.
)

%PY% "%~dp0extract_complete_collection.py" %*

:end
echo.
pause
