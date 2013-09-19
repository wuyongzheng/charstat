.PHONY: all
all: charstat uniqc ngram

.PHONY: clean
clean:
	rm -f charstat uniqc ngram

charstat: charstat.c
	gcc -g -O2 -Wall -o charstat charstat.c

uniqc: uniqc.c
	gcc -g -O2 -Wall -o uniqc uniqc.c

ngram: ngram.c
	gcc -g -O2 -Wall -o ngram ngram.c
