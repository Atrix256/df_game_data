@echo off
for %%F in (output_cpp_h output_c_h) do (
    copy /b "%~dp0output_h_prefix.txt" + "%~dp0%%F.h" + "%~dp0output_h_suffix.txt" "%~dp0%%F_string.h" >nul
)
