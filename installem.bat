:: Trust driver certificate (the hostude cert suffice, since UDEFX2 is signed with the same cert)
certutil.exe -addstore root hostude.cer
certutil.exe -addstore trustedpublisher hostude.cer

devcon.exe install hostude.inf "USB\VID_1209&PID_0887"

devcon.exe install UDEFX2.inf Root\UDEFX2
