# Generate serial numbers for devices.
TIMESTAMP    := ${shell date +'%y%m%d'}
GIT_HASH     := ${shell cd ../ && git log -1 --pretty=format:"%H" | cut -c1-4}

PHONE_SERIAL := ${TIMESTAMP}-${GIT_HASH}
MIC_SERIAL   := ${TIMESTAMP}-${GIT_HASH}

all: build

show:
	@echo "TIMESTAMP: ${TIMESTAMP}"
	@echo "GIT_HASH: ${GIT_HASH}"
	@echo "PHONE_SERIAL: ${PHONE_SERIAL}"
	@echo "MIC_SERIAL: ${MIC_SERIAL}"

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
tel: build
	timeout --foreground 10 \
	./Dist/hs100b-eeprom \
		--vid 0x335e \
		--pid 0x8a02 \
		--manufacturer "Eight Amps" \
		--product "ASI Telephone" \
		--serial "${PHONE_SERIAL}" \
		--csel 10

# Microphone EEPROM
mic: build
	timeout --foreground 10 \
	./Dist/hs100b-eeprom \
		--vid 0x335e \
		--pid 0x8a04 \
		--manufacturer "Eight Amps" \
		--product "ASI Microphone" \
		--serial "${MIC_SERIAL}" \
		--csel 11

