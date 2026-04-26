OUT ?= vibe-stage0
.PHONY: all test clean

all: $(OUT) test

$(OUT): strap0.sh emit
	sh strap0.sh $(OUT)

test: test-stage0.sh strap0.sh emit
	@sh test-stage0.sh $(OUT)

clean:
	rm -f $(OUT)
