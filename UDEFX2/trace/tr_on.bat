tracelog -start Kahuna -buffering

@rem enable device side trace
tracelog -enable Kahuna -guid #b687abf7-f962-4198-aa4d-becfef41a4c6 -flag 0xF -level 5

@rem enable host side trace
tracelog -enable Kahuna -guid #421C10CF-EDE5-4227-BB72-8C479A5EAD60 -flag 0xFF -level 5

