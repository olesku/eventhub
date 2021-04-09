all: release

release:
	mkdir -p build
	cd build && cmake -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo .. && ninja -j0

test: release
	cd build && ./eventhub_tests

debug:
	mkdir -p build-dbg
	cd build-dbg && cmake -GNinja -DCMAKE_BUILD_TYPE=Debug .. && ninja -j0

docker:
	docker build -t eventhub .

docker-debug:
	docker build -t eventhub-debug -f Dockerfile.debug .

clean:
	rm -rf build
	rm -rf build-dbg
