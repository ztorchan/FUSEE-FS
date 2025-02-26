RED  =  "\e[31;1m"
GREEN = "\e[32;1m"
YELLOW = "\e[33;1m"
BLUE  = "\e[34;1m"
RESET = "\e[0m"

.PHONY: build-mdtest
build-mdtest: 
	@echo $(YELLOW)">>> build mdtest..."$(RESET)
	@pushd ./third-party/ior && ./bootstrap && ./configure && make -j`nproc` && popd
	@cp ./third-party/ior/src/mdtest ./build/bin/

.PHONY: build-release
build-release: clean
	@echo $(YELLOW)">>> build test..."$(RESET)
	@mkdir fs/fbs_h
	@mkdir build && cd build && cmake -DPERF=ON -DTRANSPORT=infiniband -DCMAKE_BUILD_TYPE=Release .. && make -j`nproc`

.PHONY: build-debug
build-debug: clean
	@echo $(YELLOW)">>> build test..."$(RESET)
	@mkdir fs/fbs_h
	@mkdir build && cd build && cmake -DPERF=ON -DTRANSPORT=infiniband -DCMAKE_BUILD_TYPE=Debug .. && make -j`nproc`

.PHONY: clean
clean:
	@echo $(YELLOW)">>> clean..."$(RESET)
	@rm -rf build fs/fbs_h