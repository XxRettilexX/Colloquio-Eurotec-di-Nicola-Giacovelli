@echo off
setlocal
python3 -m pip install -r requirements.txt
if errorlevel 1 exit /b 1
python3 -m PyInstaller --onefile --noconsole --name SentinelUtility --clean main.py
if errorlevel 1 exit /b 1
echo Built dist\SentinelUtility.exe
