OUT ?= vibe-stage0
EXPECTED_STRAP = wrote $(OUT)
EXPECTED_STAGE0 = vibe-strap stage0

.PHONY: all test clean

all: $(OUT) test

$(OUT): strap0.sh emit
	sh strap0.sh $(OUT)

test:
	@set -eu; \
	actual=$$(sh strap0.sh $(OUT)); \
	test "$$actual" = "$(EXPECTED_STRAP)"; \
	actual=$$(./$(OUT)); \
	test "$$actual" = "$(EXPECTED_STAGE0)"; \
	printf '%s\n' 'smoke test passed'

clean:
	rm -f $(OUT)
