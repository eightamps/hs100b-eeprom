#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#define pCS	10
#define pSK	14
#define pDI	12	// MOSI
#define pDO	13	// MISO

#define MFR_MAX_LEN 30
#define PRODUCT_MAX_LEN 30
#define SERIAL_MAX_LEN 12
#define EEPROM_ADDR_COUNT 64

// NOTE(lbayes): DELETEME
#define EEPROM_MODE_16BIT	2

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
  printf("---------------------------\n");
  printf("LEN: %ld\n", len);
  for (int i = 0; i < len; i++) {
    printf("%d : %x \n", i, entries[i]);
  }
  printf("\n");
}

/**
 * Read all bytes on the EEPROM and dump them to output.
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
*/

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
 * Aggregate the char array into HS100B EEPROM word entries.
 */
void chars_to_words(char *chars, uint16_t *words) {
  size_t len = strlen(chars);
  int k;

  printf("-------------------------\n");
  for (int i = 0; i < len; i+=2) {
    k = i / 2;
    words[k] = chars_to_word(chars[i], chars[i + 1]);
    printf("looping at index: %d with k: %d and a: %c and b: %c and entry: 0x%x\n", i, k, chars[i], chars[i+1], words[k]);
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
  if (len > MFR_MAX_LEN) {
    fprintf(stderr, "ERROR: Manfuacturer is too long at [%ld] bytes\n", len);
    exit_with_usage(argv);
  }

  len = strlen(params->product);
  if (len > PRODUCT_MAX_LEN) {
    fprintf(stderr, "ERROR: Product is too long at [%ld] bytes\n", len);
    exit_with_usage(argv);
  }

  len = strlen(params->serial);
  if (len > SERIAL_MAX_LEN) {
    fprintf(stderr, "ERROR: Serial is too long at [%ld] bytes\n", len);
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
  words[index++] = 0x6706;

  // Vendor and Product IDs
  words[index++] = params->vendor_id;
  words[index++] = params->product_id;

  // Serial Number
  char_len = strlen(params->serial);
  word_len = (char_len / 2);
  // printf("LENs: %ld %ld\n", word_len, char_len);
  uint16_t *serial_words = malloc(word_len * sizeof(uint16_t));
  chars_to_words(params->serial, serial_words); 
  // print_words(serial_words, word_len);

  words[index++] = chars_to_word(params->serial[0], char_len);
  for (int i = 0; i < word_len; i++) {
    words[index++] = serial_words[i];
  }

  // Product
  index = 0x0a;  // Set the index to Product cell.
  char_len = strlen(params->product);
  word_len = (char_len / 2);
  // printf("LENs: %ld %ld\n", word_len, char_len);
  uint16_t *product_words = malloc(word_len * sizeof(uint16_t));
  chars_to_words(params->product, product_words); 
  // print_words(product_words, word_len);

  words[index++] = chars_to_word(params->product[0], char_len);
  for (int i = 0; i < word_len; i++) {
    words[index++] = product_words[i];
  }

  // Manufacturer
  index = 0x1a;  // Set the index to the Manufacturer cell.
  char_len = strlen(params->manufacturer);
  word_len = (char_len / 2);
  // printf("LENs: %ld %ld\n", word_len, char_len);
  uint16_t *mfr_words = malloc(word_len * sizeof(uint16_t));
  chars_to_words(params->manufacturer, mfr_words); 
  // print_words(mfr_words, word_len);
  words[index++] = chars_to_word(params->manufacturer[0], char_len);
  for (int i = 0; i < word_len; i++) {
    words[index++] = mfr_words[i];
  }
}

void write_all_words(uint16_t *words) {
  /*
  // open device
  struct eeprom dev;
  int eeprom_bytes = eeprom_open(eeprom_model, eeprom_org, pCS, pSK, pDI, pDO, &dev);
  printf("EEPROM chip=93C%02d, %dBit Organization, Total=%dWords\n",eeprom_model, bits, eeprom_bytes);

  int is_enabled = eeprom_is_ew_enabled(&dev);
  printf("BEFORE EEPROM EW ENABLED? %d\n", is_enabled);
  eeprom_ew_enable(&dev);
  is_enabled = eeprom_is_ew_enabled(&dev);
  printf("AFTER EEPROM EW ENABLED? %d\n", is_enabled);

  // printf("ERASING ALL\n");
  eeprom_erase_all(&dev);

  // Apply the words collection to the EEPROM NOW!
  for (int i = 0; i < EEPROM_ADDR_COUNT; i++) {
    printf("WORD: 0x%02x 0x%04x %d\n", i, words[i], i); 
    eeprom_write(&dev, i, words[i]);
  }

  // printf("WRITING VID / PID\n");
  // uint16_t vid = 0x335e;
  // uint16_t pid = 0x8a02;
  // eeprom_write(&dev, 0x01, vid);
  // eeprom_write(&dev, 0x02, pid);

  // read_all(&dev);
  // if (eeprom_org == EEPROM_MODE_8BIT) {
  // printf("EEPROM chip=93C%02d, %dBit Organization, Total=%dBytes\n",eeprom_model, bits, eeprom_bytes);
  // } else {
  // }
  // if (eeprom_org == EEPROM_MODE_16BIT) org16Mode(&dev, eeprom_bytes);
  */
}

int main(int argc, char *argv[]) {
  uint16_t *words = malloc(EEPROM_ADDR_COUNT * sizeof(uint16_t));
  hs100_params params = {};
  process_args(&params, argc, argv);
  build_words(words, &params);
  write_all_words(words);


  printf("-------------------------\n");
  printf("vid hex: 0x%x dec: %d\n", params.vendor_id, params.vendor_id);
  printf("pid hex: 0x%x dec: %d\n", params.product_id, params.product_id);
  printf("manufacturer: %s\n", params.manufacturer);
  printf("product: %s\n", params.product);
  printf("serial: %s\n", params.serial);

  // TODO(lbayes): UNCOMMENT FOLLOWING:
  // set EEPROM memory size
  // int eeprom_model = 46;
  // set EEPROM organization
  // int eeprom_org = EEPROM_MODE_16BIT;
  // int bits = 16;

  // start wiringPi
  // if (wiringPiSetup() == -1) {
    // printf("wiringPiSetup Error\n");
    // return 1;
  // }

  return 0;
}

