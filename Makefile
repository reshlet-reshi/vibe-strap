OUT ?= .ignore/vibe-strap
.PHONY: all test clean

all: $(OUT) test

$(OUT): strap0.sh emit.sh
	sh strap0.sh $(OUT)

test: test-stage0.sh strap0.sh emit.sh
	@sh test-stage0.sh $(OUT)

clean:
	rm -f $(OUT)
