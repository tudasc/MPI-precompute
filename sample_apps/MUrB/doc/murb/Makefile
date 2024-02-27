all : moveUrBody.pdf

moveUrBody.pdf : moveUrBody.tex               \
                 sections/equations.tex       \
                 sections/collisions.tex      \
                 sections/implementations.tex \
                 sections/time.tex            \
                 sections/execution.tex
	pdflatex moveUrBody.tex ; pdflatex moveUrBody.tex

show : moveUrBody.pdf
	evince moveUrBody.pdf&

clean : 
	rm -f *~ *.toc *.aux *.log *.lof *.bbl *.blg *.out 
	rm -f sections/*.aux sections/*~

mrproper : clean
	rm -f moveUrBody.pdf
