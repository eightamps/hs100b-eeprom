

all: build

# Create the output directory
Dist:
		mkdir Dist

# Compile the application
Dist/hs100b-eeprom: Dist *.c *.h
		cc -o Dist/hs100b-eeprom 93Cx6.c main.c -lwiringPi -DC46 -DWORD -DDEBUG

# Build
build: Dist/hs100b-eeprom

# Clean
clean:
		rm -rf Dist

# Telephone EEPROM
telephone-eeprom: build
	./Dist/hs100b-eeprom \
			--vid 0x335e \
			--pid 0x8a02 \
			--manufacturer "Eight Amps" \
			--product "ASI Telephone" \
			--serial "abcd"

# Microphone EEPROM
microphone-eeprom: build
	./Dist/hs100b-eeprom \
			--vid 0x335e \
			--pid 0x8a04 \
			--manufacturer "Eight Amps" \
			--product "ASI Microphone" \
			--serial "efgh"

