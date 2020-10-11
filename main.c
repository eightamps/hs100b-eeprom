#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <wiringPi.h>
#include "93Cx6.h"
#include <unistd.h>

#define pCS	10
#define pSK	14
#define pDI	12	// MOSI
#define pDO	13	// MISO

#define CMEDIA_HEADER 0x6707;
#define MFR_MAX_LEN 30
#define PRODUCT_MAX_LEN 30
#define SERIAL_MAX_LEN 12
#define EEPROM_ADDR_COUNT 64
#define EEPROM_MODEL 46
#define EEPROM_ORG EEPROM_MODE_16BIT
#define EEPROM_BITS 16
#define CMD_PAUSE_US 100
#define HLINE "-------------------------\n"

/**
 * This utility is intended to take a collection of command line parameters,
 * format them appropriately and write them to EEPROM, so that a C-Media
 * HS-100B can later load those values and properly configure itself with the
 * provided USB descriptors.
 */


// Assumes little endian
static void print_bits(char *label, size_t const size, void const * const ptr)
{
		unsigned char *b = (unsigned char*) ptr;
		unsigned char byte;
		int i, j;

  printf("%s :", label);
		for (i = size-1; i >= 0; i--) {
				for (j = 7; j >= 0; j--) {
						byte = (b[i] >> j) & 1;
						printf("%u", byte);
				}
		}
		puts("");
}

void dump(char * title, int bits, uint16_t *dt, uint16_t n) {
  int clm = 0;
  uint16_t data;
  uint16_t saddr =0;
  uint16_t eaddr =n-1;

  printf("-------------------- [%s] --------------------\n", title);
  uint16_t addr;
  for (addr = saddr; addr <= eaddr; addr++) {
    data = dt[addr];
    if (clm == 0) {
      printf("%05x: ",addr);
    }

    if (bits == 8)  printf("%02x ",data);
    if (bits == 16) printf("%04x ",data);

    clm++;
    if (bits == 8 && clm == 16) {
      printf("\n");
      clm = 0;
    }
    if (bits == 16 && clm == 8) {
      printf("\n");
      clm = 0;
    }
  }
  printf("-------------------- [%s] --------------------\n", title);
}

void print_words(uint16_t *entries, size_t len) {
  printf(HLINE);
  printf("LEN: %d\n", len);
  for (int i = 0; i < len; i++) {
    printf("%d : %x \n", i, entries[i]);
  }
  printf("\n");
}

/**
 * Read all bytes on the EEPROM and dump them to output.
 */
void read_all(struct eeprom *dev) {
  uint16_t mem_addr;
  uint16_t data;
  uint16_t rdata[128];
  int i;

  memset(rdata, 0, sizeof(rdata));
  for(i=0;i<64;i++) {
    mem_addr = i;
    rdata[i] = eeprom_read(dev, mem_addr);
  }
  dump("16bit:address 0-64", 16, rdata, 64);
}

typedef struct hs100_params {
  uint16_t vendor_id;
  uint16_t product_id;
  char *manufacturer; // limited to 30 bytes
  char *product; // limited to 30 bytes
  char *serial; // limited to 12 bytes
} hs100_params;

/**
 * Combine the provided chars into a uint16_t word.
 */
uint16_t chars_to_word(char one, char two) {
  return two | (one << 8);
}

/**
 * Aggregate the char array into HS100B EEPROM formatted word entries.
 *
 * This format has a special header, where early versions of the datasheet
 * indicate that the first byte should be len and followed by the first char,
 * while a later version of the datasheet indicates that the first byte
 * should be the first char, followed by the length.
 *
 * Could be confusion related to endianness?
 */
void chars_to_words(char *chars, uint16_t *words) {
  size_t len = strlen(chars);
  int k = 0;
  char next;

  printf(HLINE);
  for (int i = 0; i < len; i+=2) {
    printf("looping with: %d and len: %d\n", i, len);
    if (k == 0) {
      next = chars[0];
      // Process first byte + len as per datasheet.
      words[0] = chars_to_word(next, len);

      printf("Chars to words with %02x\n", words[0]);
      printf("Char byte: %x\n", chars[0]);
      printf("Len  byte: %x | %d\n", len, len);
     
      printf("Bits: chars & len\n");
      print_bits("char: ", 1, &chars[0]);
      print_bits("len : ", 1, &len);

      printf("Bits: combined\n");
      print_bits("1st word:    ", 2, &words[0]);

      // Decrement i by 1, so that the remaining loop has the 1 entry offset
      // that is needed by the HS100B specified header format.
      i -= 1;
    } else {

      if (i + 1 == len) {
        next = '\0';
      } else {
        next = chars[i+1];
      }
      words[k] = chars_to_word(next, chars[i]);
    }

    printf("looping at char index: %d with word index: %d and a: %c and b: %c and entry: 0x%04x\n", i, k, chars[i], next, words[k]);
    k++;
  }
}

void words_to_chars(uint16_t *words, char *chars, size_t len) {
  int cindex = 0;
  for (int i = 0; i < len; i++) {
    chars[cindex++] = words[i] >> 8;
    chars[cindex++] = words[i] & 0x00ff;
  }
}

void exit_with_usage(char *argv[]) {
  fprintf(stderr, "Usage: %s [ \
      --vid [0x0000] \
      --pid [0x0000] \
      --manufacturer [30 chars max] \
      --product [30 chars max] \
      --serial [12 chars max]", argv[0]);
  exit(1);
}

void validate_params(char *argv[], hs100_params *params) {
  size_t len;

  len = strlen(params->manufacturer);
  if (len >= MFR_MAX_LEN) {
    fprintf(stderr, "ERROR: Manfuacturer is too long at [%d] bytes, should be under [%d] bytes\n", len, MFR_MAX_LEN);
    exit_with_usage(argv);
  }

  len = strlen(params->product);
  if (len >= PRODUCT_MAX_LEN) {
    fprintf(stderr, "ERROR: Product is too long at [%d] bytes, should be under [%d] bytes\n", len, PRODUCT_MAX_LEN);
    exit_with_usage(argv);
  }

  len = strlen(params->serial);
  if (len >= SERIAL_MAX_LEN) {
    fprintf(stderr, "ERROR: Serial is too long at [%d] bytes, should be under [%d] bytes\n", len, SERIAL_MAX_LEN);
    exit_with_usage(argv);
  }
}

void process_args(hs100_params *params, int argc, char *argv[]) {
  int c;
  int option_index = 0;
  int option_count = 0;
  int required_option_count = 5;

  static struct option long_options[] = {
    {"vid", required_argument, NULL, 'v'},
    {"pid", required_argument, 0, 'p'},
    {"manufacturer", required_argument, 0, 'm'},
    {"product", required_argument, 0, 'r'},
    {"serial", required_argument, 0, 's'},
    {0, 0, 0, 0}
  };

  while (1) {
    c = getopt_long(argc, argv, "v:p:m:d:r:s:",
        long_options, &option_index);

    // We're done parsing args, bail and return
    if (c == -1) {
      break;
    }

    switch (c) {
      case 'v':
        params->vendor_id = strtol(optarg, NULL, 16);
        break;
      case 'p':
        params->product_id = strtol(optarg, NULL, 16);
        break;
      case 'm':
        params->manufacturer = optarg;
        break;
      case 'r':
        params->product = optarg;
        break;
      case 's':
        params->serial = optarg;
        break;
      default:
        exit_with_usage(argv);
    }

    option_count++;
  }

  if (option_count < (required_option_count)) {
    fprintf(stderr, "Usage: %s \
--vid [0x0000] \
--pid [0x0000] \
--manufacturer \"[30 chars max]\" \
--product \"[30 chars max]\" \
--serial \"[12 chars max]\"\n", argv[0]);
    exit(1);
  }

  validate_params(argv, params);
}

void build_words(uint16_t *words, hs100_params *params) {
  size_t char_len;
  size_t word_len;
  uint16_t index = 0x00;

  // Magic Word (Header)
  // The last 4 bits of this value indicate that:
  //   * We should use a serial number.
  //   * We do not adjust/change any audio parameters.
  words[index++] = CMEDIA_HEADER;

  // Vendor and Product IDs
  words[index++] = params->vendor_id;
  words[index++] = params->product_id;

  // Serial Number
  char_len = strlen(params->serial);
  word_len = (char_len / 2) + 1;
  // printf("LENs: %d %d\n", word_len, char_len);
  uint16_t *serial_words = malloc(word_len * sizeof(uint16_t));
  chars_to_words(params->serial, serial_words); 
  // print_words(serial_words, word_len);
  // words[index++] = chars_to_word(params->serial[0], char_len);
  for (int i = 0; i < word_len; i++) {
    words[index++] = serial_words[i];
  }

  // Product
  index = 0x0a;  // Set the index to Product cell.
  char_len = strlen(params->product);
  word_len = (char_len / 2) + 1;
  // printf("LENs: %d %d\n", word_len, char_len);
  uint16_t *product_words = malloc(word_len * sizeof(uint16_t));
  chars_to_words(params->product, product_words); 
  // print_words(product_words, word_len);

  // words[index++] = chars_to_word(params->product[0], char_len);
  for (int i = 0; i < word_len; i++) {
    words[index++] = product_words[i];
  }

  // Manufacturer
  index = 0x1a;  // Set the index to the Manufacturer cell.
  char_len = strlen(params->manufacturer);
  word_len = (char_len / 2) + 1;
  // printf("LENs: %d %d\n", word_len, char_len);
  uint16_t *mfr_words = malloc(word_len * sizeof(uint16_t));
  chars_to_words(params->manufacturer, mfr_words); 
  // print_words(mfr_words, word_len);
  // words[index++] = chars_to_word(params->manufacturer[0], char_len);
  for (int i = 0; i < word_len; i++) {
    words[index++] = mfr_words[i];
  }
}

void commit_words(uint16_t *words) {
  // open device
  struct eeprom dev;
  int eeprom_bytes = eeprom_open(EEPROM_MODEL, EEPROM_ORG, pCS, pSK, pDI, pDO, &dev);
  printf(HLINE);
  printf("EEPROM chip=93C%02d, %dBit Organization, Total=%dWords\n", EEPROM_MODEL, EEPROM_BITS, eeprom_bytes);

  int is_enabled = eeprom_is_ew_enabled(&dev);
  printf("BEFORE EEPROM EW (Erase/Write) ENABLED? %d\n", is_enabled);
  eeprom_ew_enable(&dev);
  is_enabled = eeprom_is_ew_enabled(&dev);
  usleep(CMD_PAUSE_US);
  printf("AFTER EEPROM EW (Erase/Write) ENABLED? %d\n", is_enabled);

  printf("READING ALL BEFORE CHANGES\n");
  read_all(&dev);
  usleep(CMD_PAUSE_US);

  printf("ERASING ALL\n");
  eeprom_erase_all(&dev);
  usleep(CMD_PAUSE_US);

  bool print_chars = false;
  char one, two;
  // Apply the words collection to the EEPROM NOW!
  for (int i = 0; i < EEPROM_ADDR_COUNT; i++) {
    switch (i) {
      case 0x03:
        print_chars = true;
        printf(HLINE);
        printf("SERIAL\n");
        break;
      case 0x0a:
        print_chars = true;
        printf(HLINE);
        printf("PRODUCT\n");
        break;
      case 0x1a:
        print_chars = true;
        printf(HLINE);
        printf("MANUFACTURER\n");
        break;
      case 0x2a:
        print_chars = false;
        printf(HLINE);
        printf("AUDIO DEVICE\n");
        break;

    }
    printf("WORD: 0x%02x 0x%04x %d", i, words[i], i); 
    if (print_chars) {
      one = words[i] & 0xff;
      two = words[i] >> 8;
      printf(" left: %c, right: %c", one, two);
    }
    printf("\n");

    // NOTE(lbayes): UNCOMMENT
    eeprom_write(&dev, i, words[i]);
    usleep(CMD_PAUSE_US);
  }

  read_all(&dev);
}

void init_words(uint16_t *words) {
  for (int i = 0; i < EEPROM_ADDR_COUNT; i++) {
    words[i] = 0xffff;
  }
}

void print_params(hs100_params *params) {
  printf(HLINE);
  printf("vid hex: 0x%x dec: %d\n", params->vendor_id, params->vendor_id);
  printf("pid hex: 0x%x dec: %d\n", params->product_id, params->product_id);
  printf("manufacturer: %s\n", params->manufacturer);
  printf("product: %s\n", params->product);
  printf("serial: %s\n", params->serial);
  printf(HLINE);
}

int main(int argc, char *argv[]) {
  hs100_params params = {};

  // start wiringPi
  if (wiringPiSetup() == -1) {
    printf("wiringPiSetup Error\n");
  }

  // uint16_t *words = malloc(EEPROM_ADDR_COUNT * sizeof(uint16_t));
  uint16_t words[64] = {};
  init_words(words);
  process_args(&params, argc, argv);
  print_params(&params);
  build_words(words, &params);
  commit_words(words);

  return 0;
}

