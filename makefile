FLAGS+=`pkg-config --cflags --libs glib-2.0`

all:
	gcc focus_fixer.c -fPIC -o focus_fixer.so -shared -lX11 $(FLAGS) -ldl -rdynamic

indent:
	astyle --style=1tbs --indent=spaces=2 --unpad-paren focus_fixer.c 

clean:
	rm focus_fixer.so
