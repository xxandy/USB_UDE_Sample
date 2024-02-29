:: Trust driver certificate (the hostude cert suffice, since UDEFX2 is signed with the same cert)
certutil.exe -addstore root hostude.cer
certutil.exe -addstore trustedpublisher hostude.cer

:: Install drivers
pnputil.exe /add-driver hostude.inf /install
pnputil.exe /add-driver UDEFX2.inf /install

:: Create SW device
devgen.exe /add /instanceid 1 /hardwareid "Root\UDEFX2"
