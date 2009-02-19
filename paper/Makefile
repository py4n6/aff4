all: paper.dvi

%.pdf: %.dvi
	dvips $(*F).dvi -o $(*F).ps
	ps2pdf $(*F).ps $@

%.dvi: %.tex %.bib
	latex $(*F).tex; bibtex $(*F); latex $(*F).tex; latex $(*F).tex

clean:
	rm -f *aux *bbl *blg *log *dvi