
all:
	mkdir -p build
	cd build && cmake .. && make

docker:
	docker build -t eventhub .

clean:
	rm -rf build
