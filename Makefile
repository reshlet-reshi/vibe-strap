OUT ?= vibe-stage0
EXPECTED_STAGE0 = vibe-strap stage0

.PHONY: all test clean

all: $(OUT) test

$(OUT): strap0.sh emit
	sh strap0.sh $(OUT)

test:
	@set -eu; \
	sh strap0.sh $(OUT); \
	actual=$$(./$(OUT)); \
	test "$$actual" = "$(EXPECTED_STAGE0)"; \
	printf '%s\n' 'smoke test passed'

clean:
	rm -f $(OUT)
