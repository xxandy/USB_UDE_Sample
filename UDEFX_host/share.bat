@setlocal
mkdir %userprofile%\vmshare\usbt >nul 2>&1
copy /y driver_both.txt %userprofile%\vmshare\usbt
copy /y exe\x64\Debug\hostudetest.exe %userprofile%\vmshare\usbt
copy /y driver\x64\Debug\*.sys %userprofile%\vmshare\usbt
copy /y driver\x64\Debug\*.inf %userprofile%\vmshare\usbt
copy /y driver\x64\Debug\*.cat %userprofile%\vmshare\usbt

