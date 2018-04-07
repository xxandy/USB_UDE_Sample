@setlocal
mkdir %userprofile%\vmshare\usbt >nul 2>&1
copy /y x64\Debug\UDEFX2.sys %userprofile%\vmshare\usbt
copy /y x64\Debug\UDEFX2.inf %userprofile%\vmshare\usbt
copy /y x64\Debug\UDEFX2\udefx2.cat %userprofile%\vmshare\usbt
copy /y trace\*.bat %userprofile%\vmshare\usbt

