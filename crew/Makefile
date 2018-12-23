CC = g++ -g -pthread -std=c++11
MAKE = make -w

#All the source files in current directory
CURRENT_DIR_SOURCEs = ${wildcard *.cpp}
CURRENT_DIR_OBJs = ${CURRENT_DIR_SOURCEs:.cpp=.o}
BIN = main

.PHONY : all
all : $(BIN)
	@echo "make all succeed."

$(BIN): $(CURRENT_DIR_OBJs)
	@echo "making" $(BIN)
	$(CC) -o $(BIN) $(CURRENT_DIR_OBJs)

$(CURRENT_DIR_OBJs) : %.o:%.cpp
	$(CC) -c $^ -o $@
	@echo "making" $@

.PHONY : clean
clean :
	-@rm $(CURRENT_DIR_OBJs)
	-@rm $(BIN) 























