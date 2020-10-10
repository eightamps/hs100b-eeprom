

hs100-eeprom: Dist *.c *.h
		@cc -Wall -Werror -o Dist/hs100-eeprom main.c
	
clean:
		rm -rf Dist

Dist:
		mkdir Dist

