@echo off
copy /b "%~dp0output_h_prefix.txt" + "%~dp0output.h" + "%~dp0output_h_suffix.txt" "%~dp0output_string.h" >nul
