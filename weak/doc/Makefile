tex=xelatex
f=-shell-escape
t=tmp

all:
	$(tex) $(f) m

tmp.tex:
	echo "\input{m1}" > $(t)
	echo "\input{m2}" > $(t)

rock-ledger rock-storage rock-executive rock-executor rock-executive-auth evm:
	echo "\input{m1}" > $(t).tex
	cat $@.tex >> $(t).tex
	echo "\input{m2}" >> $(t).tex
	$(tex) $(f) $(t).tex
	mv $(t).pdf $@.pdf
	rm $(t).*

abi-notes:
	$(tex) $(f) $@.tex

# Local Variables:
# mode: makefile
# End:
