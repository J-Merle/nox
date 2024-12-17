main: main.c

grant: main
	sudo setcap cap_net_raw,cap_net_admin=eip ./main
