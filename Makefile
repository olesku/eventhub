all: release

release:
	mkdir -p build
	cd build && cmake -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo .. && ninja -j0

test: release
	cd build && ./eventhub_tests

asan:
	mkdir -p build-asan
	cd build-asan && cmake -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
		-DEVENTHUB_SANITIZER=address .. && ninja -j0

asan-test: asan
	cd build-asan && env LD_LIBRARY_PATH="$(PWD)/build-asan:/usr/local/lib:/usr/lib" ./eventhub_tests

tsan:
	mkdir -p build-tsan
	cd build-tsan && cmake -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
		-DEVENTHUB_SANITIZER=thread .. && ninja -j0

tsan-test: tsan
	cd build-tsan && env LD_LIBRARY_PATH="$(PWD)/build-tsan:/usr/local/lib:/usr/lib" ./eventhub_tests

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
