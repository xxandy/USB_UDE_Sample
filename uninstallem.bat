:: Delete SW device
devgen.exe /remove SWD\DEVGEN\1

:: Uninstall drivers
pnputil.exe /delete-driver UDEFX2.inf /uninstall /force
pnputil.exe /delete-driver hostude.inf /uninstall /force
