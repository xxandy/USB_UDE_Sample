mkdir %userprofile%\vmshare\usbt\temp >nul 2>&1
tracefmt %userprofile%\vmshare\usbt\kk.etl -r UDEFX_host\driver\x64\Debug;UDEFX2\x64\Debug -o %userprofile%\vmshare\usbt\temp\kko.txt
type %userprofile%\vmshare\usbt\temp\kko.txt
