all: test

clean:
	rm -rf failures

test:
	@rm -f failures/*
	@echo Running tests:
	smartmet-plugin-test --handler=/q3 --reactor-config=cnf/reactor.conf --timeout 300 --num-threads 1

.PHONY: all clean test
