all: release

release:
	mkdir -p build
	cd build && cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo .. && make

debug:
	mkdir -p build-dbg
	cd build-dbg && cmake -DCMAKE_BUILD_TYPE=Debug .. && make

docker:
	docker build -t eventhub .

docker-debug:
	docker build -t eventhub-debug -f Dockerfile.debug .

clean:
	rm -rf build
	rm -rf build-dbg
