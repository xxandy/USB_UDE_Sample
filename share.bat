pushd %~dp0\udefx2
call share.bat
popd
pushd %~dp0\UDEFX_host
call share.bat
popd

copy /y installem.bat %userprofile%\vmshare\usbt