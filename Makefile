OUT ?= .ignore/vibe-strap
.PHONY: all test clean

all: $(OUT) test

$(OUT): vibe-strap.sh emit.sh
	sh vibe-strap.sh $(OUT)

test: test.sh vibe-strap.sh emit.sh
	@sh test.sh $(OUT)

clean:
	rm -f $(OUT)
