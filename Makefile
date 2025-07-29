SRC = main.cpp
OUT = main

all: $(OUT)

$(OUT): $(SRC)
	$(CXX) $(SRC) -o $(OUT)

clean:
	rm -f $(OUT)
