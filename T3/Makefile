# Para revisar las opciones de compilacion y ejecucion,
# ingrese en el terminal el comando: make

PROB=pedir
SRCS=$(PROB).c test-$(PROB).c pss.c 
HDRS=$(PROB).h pss.h



SHELL=bash -o pipefail

INCLUDE=
CFLAGS=$(XTRA) -Wall -Werror -Wno-unused-function -pedantic -std=c18 $(INCLUDE)
LDLIBS=-pthread

MAK=make --no-print-directory

readme:
	@less README.txt

$(PROB).bin $(PROB).bin-g $(PROB).bin-mem $(PROB).bin-san $(PROB).bin-thr: $(SRCS) $(HDRS)

run-mem: $(PROB).bin-mem
	./$(PROB).bin-mem

run-g: $(PROB).bin-g
	./$(PROB).bin-g

run: $(PROB).bin
	./$(PROB).bin
	
run-thr: $(PROB).bin-thr
	./$(PROB).bin-thr
run-san: $(PROB).bin-san
	@if grep -P '\t' $(PROB).c ; then echo "Su archivo $(PROB).c contiene tabs.  Reemplacelos por espacios en blanco!" ; false ; else true; fi
	./$(PROB).bin-san
	
ddd: $(PROB).ddd

ddd-san: $(PROB).ddd-san

zip:
	@rm -f resultados.txt $(PROB).zip
	@echo "Sistema operativo utilizado" > resultados.txt
	@uname -a >> resultados.txt

	@cat resultados.txt        
	@echo ==== make run ==== | tee -a resultados.txt
	@$(MAK) -B run | tee -a resultados.txt
	@echo ==== make run-g ==== | tee -a resultados.txt
	@$(MAK) -B run-g | tee -a resultados.txt
#	@echo ==== make run-mem ==== | tee -a resultados.txt
#	@$(MAK) -B run-mem | tee -a resultados.txt
	@echo ==== run-san ==== | tee -a resultados.txt
	@$(MAK) -B run-san | tee -a resultados.txt
	@echo ==== run-thr ==== | tee -a resultados.txt
	@$(MAK) -B run-thr | tee -a resultados.txt
	@echo ==== zip ====
	zip -r $(PROB).zip resultados.txt $(PROB).c
	@echo "Entregue por u-cursos el archivo $(PROB).zip"
	@echo "Descargue de u-cursos lo que entrego, descargue nuevamente los"
	@echo "archivos adjuntos y vuelva a probar la tarea tal cual como"
	@echo "la entrego.  Esto es para evitar que Ud. reciba un 1.0 en su"
	@echo "tarea porque entrego los archivos equivocados.  Creame, sucede"
	@echo "a menudo por ahorrarse esta verificacion."



%.bin: %.c
	gcc -O -DOPT=1 $(CFLAGS) $^ $(LDLIBS) -o $@

%.bin-g: %.c
	gcc -g $(CFLAGS) $^ $(LDLIBS) -o $@

%.bin-san: %.c
	gcc -g -DSAN=1 -fsanitize=thread -DSANITIZE $(CFLAGS) $^ $(LDLIBS) -o $@

%.bin-mem: %.c
	gcc -g -DSAN=1 -fsanitize=address -fsanitize=undefined -DSANITIZE $(CFLAGS) $^ $(LDLIBS) -o $@

  
%.bin-thr: %.c
	gcc -g -DSAN=1 -DSANTHR -fsanitize=thread $(CFLAGS) $(SRCS) $(LDLIBS) -o $@

%.ddd: %.bin-g
	ddd $(*F).bin-g &

%.ddd-san: %.bin-san
	ddd $(*F).bin-san &

FORCE:

clean:
	rm -f *.o *.log *.bin* core
