CFLAGS=-g

.SUFFIXES: .sa

all:	conv fpsp.h fpsp

install: all

clean:
	rm -f *.h *.s *.o conv a.out

conv:	conv.o conv.c
	cc -o conv conv.o

.sa.o:	$*.sa conv
	rm -f $*.s
	./conv < $*.sa > $*.s
	cc -E ${CFLAGS} $*.s | as -o $*.o_t
	rm -f $*.s $*.o
	mv $*.o_t $*.o

SA = bindec.sa binstr.sa decbin.sa do_func.sa gen_except.sa get_op.sa kernel_ex.sa res_func.sa sacos.sa sasin.sa satan.sa satanh.sa scosh.sa setox.sa sint.sa skeleton.sa slog2.sa slogn.sa srem_mod.sa ssin.sa ssinh.sa stan.sa stanh.sa sto_res.sa stwotox.sa tbldo.sa util.sa x_bsun.sa x_fline.sa x_operr.sa x_ovfl.sa x_snan.sa x_store.sa x_unfl.sa x_unimp.sa x_unsupp.sa

TRANS = bindec.o binstr.o decbin.o do_func.o gen_except.o get_op.o kernel_ex.o res_func.o sacos.o sasin.o satan.o satanh.o scosh.o setox.o sint.o skeleton.o slog2.o slogn.o srem_mod.o ssin.o ssinh.o stan.o stanh.o sto_res.o stwotox.o tbldo.o util.o x_bsun.o x_fline.o x_operr.o x_ovfl.o x_snan.o x_store.o x_unfl.o x_unimp.o x_unsupp.o 

fpsp: ${TRANS}
	ld -r ${TRANS} -o fpsp.o

fpsp.h: fpsp.sa conv
	rm -f fpsp.h
	./conv <fpsp.sa >fpsp.h

depend:
tags:
