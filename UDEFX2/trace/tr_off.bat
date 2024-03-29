mkdir %userprofile%\vmshare\usbt >nul 2>&1
tracelog -flush Kahuna -f %userprofile%\vmshare\usbt\kk.etl
tracelog -stop Kahuna
