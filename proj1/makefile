make:
	gcc msgserv.c -o msgserv
	gcc rmb.c -o rmb

msgserv: msgserv.c
	gcc msgserv.c -o msgserv

rmb: rmb.c
	gcc rmb.c -o rmb

compile:
	gcc msgserv.c -pedantic -Wall -Wextra -Werror -o msgserv
	gcc rmb.c -pedantic -Wall -Wextra -Werror -o rmb

clean:
	-rm msgserv rmb client
