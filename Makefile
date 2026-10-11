all:
	@make -C build

setup:
	@cmake -DCMAKE_BUILD_TYPE=Release -B ./build
