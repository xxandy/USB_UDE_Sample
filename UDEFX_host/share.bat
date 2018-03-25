@setlocal
copy /y driver_both.txt %userprofile%\vmshare\usbt
copy /y exe\x64\Debug\osrusbfx2.exe %userprofile%\vmshare\usbt
copy /y driver\x64\Debug\*.sys %userprofile%\vmshare\usbt
copy /y driver\x64\Debug\*.inf %userprofile%\vmshare\usbt
copy /y driver\x64\Debug\*.cat %userprofile%\vmshare\usbt

