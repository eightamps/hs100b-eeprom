

hs100-eeprom: Dist *.c *.h
		cc -o Dist/hs100b-eeprom 93Cx6.c main.c -lwiringPi -DC46 -DWORD
	
clean:
		rm -rf Dist

Dist:
		mkdir Dist

